#include "Pages.h"

#include "NativeMenu.h"
#include "SystemState.h"
#include "Text.h"

#include <deque>
#include <mutex>

namespace Pages
{
    namespace
    {
        struct Item
        {
            std::string page;
            std::string label;
            void(__stdcall* getText)(char*, int) = nullptr;
        };

        std::deque<Item>         g_items;
        std::vector<std::string> g_pages;
        std::recursive_mutex     g_mutex;

        // Indices into g_items for the page on screen. Empty while vanilla's
        // own topics are.
        std::vector<std::size_t> g_shown;

        // Vanilla's topics, and the object the listener was registered with,
        // are parked on the list clip: a GFxValue in a global outlives the
        // movie, and destroying it afterwards crashes.
        //
        // EventDispatcher matches a listener on scope and handler name, so add
        // and remove have to be handed the same object.
        constexpr const char* kTopicsMember = "__nsmf_vanillaTopics";
        constexpr const char* kScopeMember = "__nsmf_pageScope";

        // Read while showing the list, where a GFxMovieView is at hand - the
        // press handler only gets a GFxMovie.
        double g_helpTextState = 8.0;

        // Long enough for a page of prose.
        constexpr int kTextBufferSize = 4096;

        bool GetHelpList(RE::GFxValue& a_systemPage, RE::GFxValue& a_out)
        {
            RE::GFxValue panel;
            return a_systemPage.GetMember("HelpListPanel", &panel) && panel.IsObject() &&
                panel.GetMember("List_mc", &a_out) && a_out.IsObject();
        }

        std::string TextOf(const Item& a_item)
        {
            if (!a_item.getText)
                return {};
            std::string buffer(kTextBufferSize, '\0');
            a_item.getText(buffer.data(), kTextBufferSize);
            buffer.resize(std::strlen(buffer.c_str()));
            return buffer;
        }

        // Mirrors onHelpItemPress: fill the two text fields, tell the list
        // panel not to close to the main menu, then swap states. Vanilla's
        // RequestHelpText is the only part left out - the text comes from the
        // mod instead.
        class ItemPressHandler : public RE::GFxFunctionHandler
        {
        public:
            void Call(Params& a_params) override
            {
                const std::lock_guard lock(g_mutex);

                RE::GFxValue page;
                if (!a_params.thisPtr || !a_params.thisPtr->GetMember("__nsmf_page", &page))
                    return;

                RE::GFxValue list, idxVal;
                if (!GetHelpList(page, list) || !list.GetMember("selectedIndex", &idxVal) || !idxVal.IsNumber())
                    return;

                const auto index = idxVal.GetNumber();
                if (index < 0.0 || static_cast<std::size_t>(index) >= g_shown.size())
                    return;
                const auto& item = g_items[g_shown[static_cast<std::size_t>(index)]];
                logger::debug("Pages: item press [{}] '{}' in '{}'", index, item.label, item.page);

                RE::GFxValue holder, title, text;
                if (!page.GetMember("HelpTextPanel", &holder) || !holder.IsObject() ||
                    !holder.GetMember("HelpTextHolder", &holder) || !holder.IsObject()) {
                    logger::warn("Pages: no HelpTextHolder - cannot show '{}'", item.label);
                    return;
                }
                if (holder.GetMember("TitleText", &title) && title.IsObject())
                    title.SetMember("text", Text::MakeGFxString(item.label));
                if (holder.GetMember("HelpText", &text) && text.IsObject())
                    text.SetMember("htmlText", Text::MakeGFxString(TextOf(item)));

                // Without this, Back from the text would leave the whole
                // screen instead of returning to the list.
                RE::GFxValue listPanel;
                if (page.GetMember("HelpListPanel", &listPanel) && listPanel.IsObject())
                    listPanel.SetMember("bCloseToMainState", RE::GFxValue(false));

                RE::GFxValue state(g_helpTextState);
                page.Invoke("EndState");
                page.Invoke("StartState", nullptr, &state, 1);
            }
        };
        ItemPressHandler g_itemPressHandler;

        void ListenVanilla(RE::GFxValue& a_list, RE::GFxValue& a_systemPage, bool a_listening)
        {
            const RE::GFxValue args[3] = { RE::GFxValue("itemPress"), a_systemPage,
                RE::GFxValue("onHelpItemPress") };
            a_list.Invoke(a_listening ? "addEventListener" : "removeEventListener", nullptr, args, 3);
        }

        void ListenOurs(RE::GFxMovieView* a_view, RE::GFxValue& a_list, RE::GFxValue& a_systemPage, bool a_listening)
        {
            RE::GFxValue scope;
            if (!a_list.GetMember(kScopeMember, &scope) || !scope.IsObject()) {
                if (!a_listening || !a_view)
                    return;
                RE::GFxValue fn;
                a_view->CreateObject(&scope);
                a_view->CreateFunction(&fn, &g_itemPressHandler);
                scope.SetMember("onNSMFPageItemPress", fn);
                scope.SetMember("__nsmf_page", a_systemPage);
                a_list.SetMember(kScopeMember, scope);
            }

            const RE::GFxValue args[3] = { RE::GFxValue("itemPress"), scope,
                RE::GFxValue("onNSMFPageItemPress") };
            a_list.Invoke(a_listening ? "addEventListener" : "removeEventListener", nullptr, args, 3);
        }
    }

    bool AddItem(std::string a_page, std::string a_label, void(__stdcall* a_getText)(char*, int), std::string a_owner)
    {
        if (a_page.empty() || a_label.empty() || !a_getText)
            return false;

        bool isNewPage = false;
        {
            const std::lock_guard lock(g_mutex);
            if (std::find(g_pages.begin(), g_pages.end(), a_page) == g_pages.end()) {
                g_pages.push_back(a_page);
                isNewPage = true;
            }
            g_items.push_back({ a_page, std::move(a_label), a_getText });
        }

        // Outside the lock: NativeMenu takes its own, and its tick takes the
        // two in the opposite order.
        if (isNewPage)
            NativeMenu::AddPageEntry(std::move(a_page), std::move(a_owner));
        return true;
    }

    bool Show(RE::GFxMovieView* a_view, RE::GFxValue& a_systemPage, const std::string& a_name)
    {
        const std::lock_guard lock(g_mutex);

        RE::GFxValue list;
        if (!a_view || !GetHelpList(a_systemPage, list))
            return false;

        std::vector<std::size_t> shown;
        for (std::size_t i = 0; i < g_items.size(); ++i) {
            if (g_items[i].page == a_name)
                shown.push_back(i);
        }
        if (shown.empty()) {
            logger::warn("Pages: '{}' has no items", a_name);
            return false;
        }

        // Only on the way in: a page opened twice would otherwise save its own
        // list over vanilla's and lose the topics for good.
        if (g_shown.empty()) {
            RE::GFxValue topics;
            if (list.GetMember("entryList", &topics) && topics.IsArray())
                list.SetMember(kTopicsMember, topics);
            ListenVanilla(list, a_systemPage, false);
            ListenOurs(a_view, list, a_systemPage, true);
        }

        RE::GFxValue entryList;
        a_view->CreateArray(&entryList);
        for (const auto index : shown) {
            RE::GFxValue entry;
            a_view->CreateObject(&entry);
            entry.SetMember("text", Text::MakeGFxString(g_items[index].label));
            entryList.Invoke("push", nullptr, &entry, 1);
        }
        list.SetMember("entryList", entryList);
        g_shown = std::move(shown);
        list.Invoke("InvalidateData");

        g_helpTextState = SystemState::Read(a_view, "HELP_TEXT_STATE", 8.0);

        // What vanilla's own Help entry does once its topics are in.
        RE::GFxValue state(SystemState::Read(a_view, "HELP_LIST_STATE", 7.0));
        a_systemPage.Invoke("StartState", nullptr, &state, 1);

        logger::info("Pages: showing '{}' ({} item(s))", a_name, g_shown.size());
        return true;
    }

    void ClearScope(RE::GFxValue& a_systemPage)
    {
        const std::lock_guard lock(g_mutex);

        if (g_shown.empty())
            return;
        g_shown.clear();

        RE::GFxValue list;
        if (!GetHelpList(a_systemPage, list))
            return;

        RE::GFxValue topics;
        if (list.GetMember(kTopicsMember, &topics) && topics.IsArray())
            list.SetMember("entryList", topics);
        ListenOurs(nullptr, list, a_systemPage, false);
        ListenVanilla(list, a_systemPage, true);
        list.Invoke("InvalidateData");
        logger::debug("Pages: vanilla help topics restored");
    }

    void Reset()
    {
        const std::lock_guard lock(g_mutex);
        g_shown.clear();
    }
}
