#include "NativeMenu.h"

#include "Config.h"
#include "ListRows.h"
#include "Ordering.h"
#include "Pages.h"
#include "Translations.h"
#include "Text.h"
#include "VanillaSettings.h"

#include <deque>
#include <mutex>

namespace NativeMenu
{
    namespace
    {
        constexpr const char* kMenuRootPath = "_root.QuestJournalFader.Menu_mc";

        struct Entry
        {
            std::string text;
            void(__stdcall* callback)() = nullptr;
            std::string jumpToTab;
            // The page this entry opens, if it is a page entry.
            std::string page;
            // Which mod registered this, so entries can be grouped and
            // ordered by mod rather than by plugin load order.
            std::string owner;
        };

        std::deque<Entry> g_entries;
        // Same reason as the settings list: a mod may register off the menu
        // thread, and the tick calls back into mod code while holding this.
        std::recursive_mutex g_entriesMutex;

        RE::FxDelegate::CallbackDefn g_originalSetSaveDisabled{};
        bool                         g_delegateHooked = false;

        std::atomic<bool> g_injected{ false };
        // Set by a visibility box, applied on the next tick - the System list
        // is off screen while its own settings are open, so rebuilding it
        // there is never seen.
        std::atomic<bool> g_visibilityDirty{ false };
        std::atomic<int>  g_injectTicks{ 0 };
        std::atomic<int>  g_pendingEntry{ -1 };
        // The entries the game may grey out, in the order vanilla passes them
        // to SetSaveDisabled. It picks them by position, which shifts with
        // hiding, reordering or a version without the Creations entry - the
        // six are always the same, so naming them sidesteps all of that.
        // These are Bethesda's own translation keys, so nothing renames them.
        constexpr const char* kSaveDisabledOrder[] = { "$QUICKSAVE", "$SAVE", "$LOAD", "$INSTALLED CONTENT",
            "$SETTINGS", "$QUIT" };

        // Entries hiding must never take away: one leads to the setting that
        // would undo it, the other is the way out.
        constexpr const char* kAlwaysShown[] = { "$SETTINGS", "$QUIT" };

        constexpr const char* kVanillaEntriesMember = "__nsmf_vanillaEntries";

        // Parked on the menu root once found, fetched back each tick. Nothing
        // here holds a GFxValue between ticks: one kept past the menu's life
        // crashes on destruction.
        constexpr const char* kSystemPageMember = "__nsmf_systemPage";

        bool IsSystemPage(const RE::GFxValue& a_value)
        {
            return a_value.IsObject() && a_value.HasMember("CategoryList") && a_value.HasMember("SettingsList") &&
                a_value.HasMember("MappingList");
        }

        // Breadth-first search under the movie root - the System page's exact
        // path isn't stable enough to hardcode, so it's found by structural
        // signature instead.
        bool FindSystemPage(const RE::GFxValue& a_root, RE::GFxValue& a_out, int a_maxDepth)
        {
            std::vector<RE::GFxValue> current{ a_root };
            int                       budget = 2500;
            for (int depth = 0; depth <= a_maxDepth && !current.empty(); ++depth) {
                std::vector<RE::GFxValue> next;
                for (auto& node : current) {
                    if (--budget <= 0)
                        return false;
                    if (IsSystemPage(node)) {
                        a_out = node;
                        return true;
                    }
                    if (depth == a_maxDepth)
                        continue;
                    node.VisitMembers([&next](const char* a_name, const RE::GFxValue& a_val) {
                        static constexpr std::string_view skip[] = { "_parent", "_root", "_global", "_level0",
                            "__proto__", "prototype", "constructor", "_listeners", "stage" };
                        for (auto s : skip)
                            if (s == a_name)
                                return;
                        if (a_val.IsObject() || a_val.IsArray() || a_val.IsDisplayObject())
                            next.push_back(a_val);
                    });
                }
                current.swap(next);
            }
            return false;
        }

        class EntryPressHandler : public RE::GFxFunctionHandler
        {
        public:
            void Call(Params& a_params) override
            {
                if (!a_params.thisPtr)
                    return;
                RE::GFxValue itemIndex, parent;
                if (!a_params.thisPtr->GetMember("itemIndex", &itemIndex) || itemIndex.IsUndefined())
                    return;
                if (!a_params.thisPtr->GetMember("_parent", &parent))
                    return;
                RE::GFxValue kbOrMouse{ 0.0 };
                if (a_params.argCount >= 2 && a_params.args)
                    kbOrMouse = a_params.args[1];
                parent.Invoke("onItemPress", nullptr, &kbOrMouse, 1);
            }
        };
        EntryPressHandler g_entryPressHandler;

        class EntryRollOverHandler : public RE::GFxFunctionHandler
        {
        public:
            void Call(Params& a_params) override
            {
                if (!a_params.thisPtr)
                    return;
                RE::GFxValue itemIndex, parent;
                if (!a_params.thisPtr->GetMember("itemIndex", &itemIndex) || itemIndex.IsUndefined())
                    return;
                if (!a_params.thisPtr->GetMember("_parent", &parent))
                    return;
                RE::GFxValue anim, dis;
                const bool   animating = parent.GetMember("listAnimating", &anim) && anim.IsBool() && anim.GetBool();
                const bool   disabled = parent.GetMember("bDisableInput", &dis) && dis.IsBool() && dis.GetBool();
                if (animating || disabled)
                    return;
                const RE::GFxValue args[2] = { itemIndex, RE::GFxValue(0.0) };
                parent.Invoke("doSetSelectedIndex", nullptr, args, 2);
                parent.SetMember("bMouseDrivenNav", RE::GFxValue(true));
            }
        };
        EntryRollOverHandler g_entryRollOverHandler;

        // Vanilla picks the entries it may grey out by fixed position, so
        // moving or hiding one would send the game the wrong object. Each
        // argument is put back to the entry vanilla meant, by name.
        void OnSetSaveDisabled(const RE::FxDelegateArgs& a_params)
        {
            if (!g_originalSetSaveDisabled.callback)
                return;

            RE::GFxValue root, page, catList, vanilla;
            auto*        movie = a_params.GetMovie();
            if (!movie || !movie->GetVariable(&root, kMenuRootPath) ||
                !root.GetMember(kSystemPageMember, &page) || !page.IsObject() ||
                !page.GetMember("CategoryList", &catList) || !catList.IsObject() ||
                !catList.GetMember(kVanillaEntriesMember, &vanilla) || !vanilla.IsArray()) {
                g_originalSetSaveDisabled.callback(a_params);
                return;
            }

            const auto                count = a_params.GetArgCount();
            std::vector<RE::GFxValue> args(count);
            for (std::uint32_t i = 0; i < count; ++i) {
                args[i] = a_params[i];
                if (i >= std::size(kSaveDisabledOrder))
                    continue;

                // Whatever vanilla picked, hand it the entry it meant.
                for (std::uint32_t j = 0; j < vanilla.GetArraySize(); ++j) {
                    RE::GFxValue entry, text;
                    if (vanilla.GetElement(j, &entry) && entry.IsObject() && entry.GetMember("text", &text) &&
                        text.IsString() && std::string_view(text.GetString()) == kSaveDisabledOrder[i]) {
                        args[i] = entry;
                        break;
                    }
                }
            }

            // Undefined response id: the script calls this without a callback,
            // so nothing is waiting on an answer.
            const RE::FxDelegateArgs forwarded(
                RE::GFxValue(), a_params.GetHandler(), a_params.GetMovie(), args.data(), count);
            g_originalSetSaveDisabled.callback(forwarded);
        }

        // Vanilla's onCategoryButtonPress plays this itself; our entries never
        // reach that function, so they'd otherwise click silently.
        void PlayUiSound(RE::GFxMovie* a_movie, const char* a_sound)
        {
            RE::GFxValue gameDelegate;
            if (!a_movie->GetVariable(&gameDelegate, "gfx.io.GameDelegate"))
                return;

            RE::GFxValue params;
            a_movie->CreateArray(&params);
            RE::GFxValue soundName(a_sound);
            params.Invoke("push", nullptr, &soundName, 1);

            const RE::GFxValue args[2] = { RE::GFxValue("PlaySound"), params };
            gameDelegate.Invoke("call", nullptr, args, 2);
        }

        std::string TextOf(RE::GFxValue& a_entry)
        {
            RE::GFxValue text;
            return a_entry.GetMember("text", &text) && text.IsString() ? text.GetString() : std::string{};
        }

        bool IsEssential(const std::string& a_text)
        {
            return std::find(std::begin(kAlwaysShown), std::end(kAlwaysShown), a_text) != std::end(kAlwaysShown);
        }

        bool IsHidden(RE::GFxValue& a_entry)
        {
            const auto text = TextOf(a_entry);
            return !text.empty() && !IsEssential(text) && Config::IsEntryHidden(text);
        }

        // What the player sees: vanilla's entries minus the hidden ones, the
        // mod entries, then vanilla's last one - the way out of the menu,
        // which belongs at the bottom. Taken as "the last one" rather than by
        // name, so it holds whatever the interface calls it.
        //
        // Runs again whenever a visibility box is ticked.
        void BuildDisplayList(RE::GFxMovieView* a_view, RE::GFxValue& a_catList, RE::GFxValue& a_vanilla)
        {
            RE::GFxValue displayList;
            a_view->CreateArray(&displayList);
            const auto push = [&displayList](RE::GFxValue& a_entry) {
                displayList.Invoke("push", nullptr, &a_entry, 1);
            };

            const auto count = a_vanilla.GetArraySize();
            for (std::uint32_t i = 0; i + 1 < count; ++i) {
                RE::GFxValue entry;
                if (a_vanilla.GetElement(i, &entry) && entry.IsObject() && !IsHidden(entry))
                    push(entry);
            }

            for (std::size_t i = 0; i < g_entries.size(); ++i) {
                if (Config::IsEntryHidden(g_entries[i].text))
                    continue;
                RE::GFxValue entry;
                a_view->CreateObject(&entry);
                entry.SetMember("text", Text::MakeGFxString(g_entries[i].text));
                entry.SetMember("__nsmf_eidx", RE::GFxValue(static_cast<double>(i)));
                push(entry);
            }

            RE::GFxValue last;
            if (count > 0 && a_vanilla.GetElement(count - 1, &last) && last.IsObject() && !IsHidden(last))
                push(last);

            a_catList.SetMember("entryList", displayList);
            // Vanilla's own row handlers resolve a click through the
            // list's press handler, which a duplicate never reaches - so our
            // own stand in for them.
            ListRows::Ensure(a_catList, displayList.GetArraySize(), "CategoryList", [a_view](RE::GFxValue& a_clip) {
                RE::GFxValue fnPress, fnRoll;
                a_view->CreateFunction(&fnPress, &g_entryPressHandler);
                a_view->CreateFunction(&fnRoll, &g_entryRollOverHandler);
                a_clip.SetMember("onPress", fnPress);
                a_clip.SetMember("onRollOver", fnRoll);
            });
            a_catList.Invoke("InvalidateData");
        }

        // One checkbox per System menu entry, in the framework's own tab.
        // Built here rather than at load because the entries are only known
        // once the menu exists.
        void RegisterVisibilityRows(std::vector<RE::GFxValue>& a_vanilla)
        {
            static bool registered = false;
            if (registered)
                return;
            registered = true;

            std::vector<std::string> names;
            for (auto& entry : a_vanilla) {
                if (auto text = TextOf(entry); !text.empty())
                    names.push_back(std::move(text));
            }
            for (const auto& entry : g_entries)
                names.push_back(entry.text);
            if (names.empty())
                return;

            VanillaSettings::AddLabel(
                kFrameworkTab,
                [](char* a_buffer, int a_size) { std::snprintf(a_buffer, a_size, "$NSMF_SYSTEM_MENU_ENTRIES"); },
                VanillaSettings::Align::kCenter, kFrameworkName);

            for (const auto& name : names) {
                const bool essential = IsEssential(name);
                VanillaSettings::AddSetting(
                    kFrameworkTab, VanillaSettings::Type::kCheckbox, name,
                    [name] { return Config::IsEntryHidden(name) ? 0.0f : 1.0f; },
                    [name](float a_value) {
                        Config::SetEntryHidden(name, a_value == 0.0f);
                        g_visibilityDirty.store(true);
                    },
                    1.0f, {},
                    [essential] { return !essential; }, nullptr,
                    essential ? "$NSMF_ENTRY_LOCKED_DESC" : "$NSMF_ENTRY_DESC",
                    kFrameworkName);
            }
            logger::info("NativeSystemMenuFramework: {} entr{} can be shown or hidden", names.size(),
                names.size() == 1 ? "y" : "ies");
        }

        void HookDelegate(RE::FxDelegate* a_fxDelegate)
        {
            if (g_delegateHooked || !a_fxDelegate)
                return;

            RE::GString name("SetSaveDisabled");
            if (!a_fxDelegate->callbacks.Get(name, &g_originalSetSaveDisabled)) {
                logger::warn("NativeSystemMenuFramework: SetSaveDisabled not registered - vanilla greying may follow the "
                             "wrong entries");
                return;
            }
            a_fxDelegate->callbacks.Set(
                name, RE::FxDelegate::CallbackDefn{ g_originalSetSaveDisabled.handler, &OnSetSaveDisabled });
            g_delegateHooked = true;
            logger::debug("NativeSystemMenuFramework: SetSaveDisabled hooked");
        }

        // Every click on the category list runs through this once injected.
        // Our entries carry an __nsmf_eidx marker and are queued for Tick to
        // dispatch; anything else is forwarded to vanilla untouched.
        class PressHandler : public RE::GFxFunctionHandler
        {
        public:
            void Call(Params& a_params) override
            {
                if (a_params.argCount < 1 || !a_params.args)
                    return;
                RE::GFxValue& evt = a_params.args[0];

                RE::GFxValue entry, eidx;
                if (evt.GetMember("entry", &entry) && entry.GetMember("__nsmf_eidx", &eidx) && eidx.IsNumber()) {
                    const int idx = static_cast<int>(eidx.GetNumber());
                    logger::debug("NativeSystemMenuFramework: itemPress on our entry {}", idx);
                    PlayUiSound(a_params.movie, "UIMenuOK");
                    g_pendingEntry.store(idx);
                    return;
                }

                // Vanilla's handler switches on the position it sees, so it
                // has to be told where the entry used to be, not where it is
                // now.
                RE::GFxValue vidx;
                if (entry.IsObject() && entry.GetMember("__nsmf_vidx", &vidx) && vidx.IsNumber())
                    evt.SetMember("index", vidx);

                if (!a_params.thisPtr)
                    return;
                RE::GFxValue page;
                if (a_params.thisPtr->GetMember("__nsmf_page", &page)) {
                    // Vanilla's own Settings and Help entries must find their
                    // own lists, not whatever a mod entry left in them.
                    VanillaSettings::ClearOwnerScope(page);
                    Pages::ClearScope(page);
                    page.Invoke("onCategoryButtonPress", nullptr, &evt, 1);
                }
            }
        };
        PressHandler g_pressHandler;

        bool Inject(RE::GFxMovieView* a_view, RE::GFxValue& a_page)
        {
            RE::GFxValue catList;
            if (!a_page.GetMember("CategoryList", &catList) || !catList.IsObject())
                return false;

            RE::GFxValue entryList;
            if (!catList.GetMember("entryList", &entryList) || !entryList.IsArray() || entryList.GetArraySize() == 0)
                return false;

            if (catList.HasMember("__nsmf_injected"))
                return true;

            // Vanilla's list as it stands, before anything moves, each entry
            // stamped with where it was. Local: a GFxValue kept past the
            // menu's life crashes on destruction.
            std::vector<RE::GFxValue> vanilla;
            for (std::uint32_t i = 0; i < entryList.GetArraySize(); ++i) {
                RE::GFxValue entry;
                if (!entryList.GetElement(i, &entry) || !entry.IsObject())
                    continue;
                entry.SetMember("__nsmf_vidx", RE::GFxValue(static_cast<double>(i)));
                vanilla.push_back(entry);
            }
            if (vanilla.empty())
                return false;

            // Same rule as the settings tabs; an entry whose mod never named
            // itself is its own group. Safe to sort here and nowhere else:
            // the index the press handler reads is written below, once the
            // order is settled.
            const auto key = [](const Entry& a_entry) -> const std::string& {
                return a_entry.owner.empty() ? a_entry.text : a_entry.owner;
            };
            std::stable_sort(g_entries.begin(), g_entries.end(), [&key](const Entry& a_lhs, const Entry& a_rhs) {
                return Ordering::OwnerPrecedes(key(a_lhs), key(a_rhs));
            });

            // Vanilla's list untouched, kept where the movie owns it, so the
            // greying translation can find an entry the player has hidden.
            RE::GFxValue vanillaList;
            a_view->CreateArray(&vanillaList);
            for (auto& entry : vanilla)
                vanillaList.Invoke("push", nullptr, &entry, 1);
            catList.SetMember(kVanillaEntriesMember, vanillaList);
            RegisterVisibilityRows(vanilla);

            BuildDisplayList(a_view, catList, vanillaList);

            RE::GFxValue scope;
            a_view->CreateObject(&scope);
            RE::GFxValue fn;
            a_view->CreateFunction(&fn, &g_pressHandler);
            scope.SetMember("onNSMFPress", fn);
            scope.SetMember("__nsmf_page", a_page);

            const RE::GFxValue rm[3] = { RE::GFxValue("itemPress"), a_page, RE::GFxValue("onCategoryButtonPress") };
            catList.Invoke("removeEventListener", nullptr, rm, 3);
            const RE::GFxValue add[3] = { RE::GFxValue("itemPress"), scope, RE::GFxValue("onNSMFPress") };
            catList.Invoke("addEventListener", nullptr, add, 3);

            catList.SetMember("__nsmf_scope", scope);
            catList.SetMember("__nsmf_injected", RE::GFxValue(1.0));

            if (g_entries.empty())
                logger::info("NativeSystemMenuFramework: no mod entry to add, hooked for hiding only");
            else
                logger::info("NativeSystemMenuFramework: injected {} entr{}", g_entries.size(),
                    g_entries.size() == 1 ? "y" : "ies");
            return true;
        }

        void Tick(RE::JournalMenu* a_this)
        {
            const std::lock_guard lock(g_entriesMutex);

            if (!a_this || !a_this->uiMovie)
                return;
            auto* view = a_this->uiMovie.get();

            if (!g_injected.load()) {
                if (g_injectTicks.load() <= 0)
                    return;
                g_injectTicks.fetch_sub(1);

                RE::GFxValue root, page;
                if (view->GetVariable(&root, kMenuRootPath) && FindSystemPage(root, page, 10) &&
                    Inject(view, page)) {
                    root.SetMember(kSystemPageMember, page);
                    g_injected.store(true);
                }
                return;
            }

            RE::GFxValue root, systemPage;
            if (!view->GetVariable(&root, kMenuRootPath) ||
                !root.GetMember(kSystemPageMember, &systemPage) || !systemPage.IsObject())
                return;

            if (g_visibilityDirty.exchange(false)) {
                RE::GFxValue catList, vanilla;
                if (systemPage.GetMember("CategoryList", &catList) && catList.IsObject() &&
                    catList.GetMember(kVanillaEntriesMember, &vanilla) && vanilla.IsArray())
                    BuildDisplayList(view, catList, vanilla);
            }

            HookDelegate(a_this->fxDelegate.get());
            VanillaSettings::Tick(a_this, view, systemPage);

            const int pending = g_pendingEntry.exchange(-1);
            if (pending < 0)
                return;
            if (pending >= static_cast<int>(g_entries.size())) {
                logger::warn("NativeSystemMenuFramework: pending entry {} out of range", pending);
                return;
            }
            // Copied: the callback below is mod code, and may register an
            // entry in turn.
            const Entry entry = g_entries[pending];
            logger::debug("NativeSystemMenuFramework: dispatching entry {} ('{}')", pending, entry.text);
            // In order of how specific the entry was about what it opens.
            if (!entry.page.empty())
                Pages::Show(view, systemPage, entry.page);
            else if (!entry.jumpToTab.empty())
                VanillaSettings::JumpToTab(view, systemPage, entry.jumpToTab);
            else if (!entry.callback)
                VanillaSettings::ShowOwnerTabs(view, systemPage, entry.owner);
            if (entry.callback)
                entry.callback();
        }

        class AdvanceHook
        {
        public:
            static void Install()
            {
                REL::Relocation<std::uintptr_t> vtbl{ RE::JournalMenu::VTABLE[0] };
                _Advance = vtbl.write_vfunc(0x5, Advance);
            }

        private:
            static void Advance(RE::JournalMenu* a_this, float a_interval, std::uint32_t a_currentTime)
            {
                _Advance(a_this, a_interval, a_currentTime);
                Tick(a_this);
            }
            static inline REL::Relocation<decltype(Advance)> _Advance;
        };

        // The System menu tears down and rebuilds its Scaleform tree every
        // time it opens, so injection state has to reset on each open - and
        // the movie needs a few ticks after opening before CategoryList is
        // actually populated, hence the tick delay rather than injecting
        // immediately.
        class JournalSink : public RE::BSTEventSink<RE::MenuOpenCloseEvent>
        {
        public:
            static JournalSink* GetSingleton()
            {
                static JournalSink singleton;
                return &singleton;
            }

            RE::BSEventNotifyControl ProcessEvent(
                const RE::MenuOpenCloseEvent* a_event, RE::BSTEventSource<RE::MenuOpenCloseEvent>*) override
            {
                if (a_event && std::string_view(a_event->menuName.c_str()) == RE::JournalMenu::MENU_NAME) {
                    g_injected.store(false);
                    g_pendingEntry.store(-1);
                    g_injectTicks.store(a_event->opening ? Config::GetInjectionRetryTicks() : 0);
                    g_visibilityDirty.store(false);
                    g_delegateHooked = false;
                    g_originalSetSaveDisabled = {};
                    VanillaSettings::Reset();
                    Pages::Reset();
                }
                return RE::BSEventNotifyControl::kContinue;
            }
        };
    }

    void InstallHooks()
    {
        AdvanceHook::Install();
        if (auto* ui = RE::UI::GetSingleton())
            ui->AddEventSink<RE::MenuOpenCloseEvent>(JournalSink::GetSingleton());
        logger::info("NativeSystemMenuFramework: hooks installed");
    }

    bool AddEntry(std::string a_text, void(__stdcall* a_callback)(), std::string a_jumpToTab, std::string a_owner)
    {
        const std::lock_guard lock(g_entriesMutex);

        if (a_text.empty() || (!a_callback && a_jumpToTab.empty() && a_owner.empty()))
            return false;
        Translations::Load(a_owner);
        g_entries.push_back({ std::move(a_text), a_callback, std::move(a_jumpToTab), {}, std::move(a_owner) });
        return true;
    }

    bool AddPageEntry(std::string a_page, std::string a_owner)
    {
        const std::lock_guard lock(g_entriesMutex);

        if (a_page.empty())
            return false;
        Translations::Load(a_owner);
        // The entry is named after the page it opens.
        Entry entry{ a_page, nullptr, {}, a_page, std::move(a_owner) };
        g_entries.push_back(std::move(entry));
        return true;
    }
}
