#include "Controls.h"

#include "Config.h"
#include "Debug.h"
#include "ListRows.h"
#include "Text.h"
#include "Translations.h"

#include <mutex>

namespace Controls
{
    namespace
    {
        struct Row
        {
            std::string           event;
            Context               context = Context::kGameplay;
            std::string           label;
            int                   defaultKey = -1;
            int                   defaultGamepad = -1;
            std::function<void()> onPress;
            std::string           description;
            std::string           owner;
            bool                  created = false;
            bool                  filled = false;
        };

        std::recursive_mutex g_mutex;
        std::vector<Row>     g_rows;

        std::unordered_map<std::string, std::string> g_labels;

        // Defaults as first seen, so only real changes get written down.
        std::unordered_map<std::string, int> g_defaults;

        using ContextID = RE::UserEvents::INPUT_CONTEXT_ID;

        ContextID Resolve(Context a_context)
        {
            switch (a_context) {
            case Context::kMenu:        return ContextID::kMenuMode;
            case Context::kInventory:   return ContextID::kInventory;
            case Context::kFavorites:   return ContextID::kFavorites;
            case Context::kMap:         return ContextID::kMap;
            case Context::kStats:       return ContextID::kStats;
            case Context::kBook:        return ContextID::kBook;
            case Context::kJournal:     return ContextID::kJournal;
            case Context::kLockpicking: return ContextID::kLockpicking;
            default:                    return ContextID::kGameplay;
            }
        }

        RE::ControlMap::InputContext* ContextFor(Context a_context)
        {
            auto* controls = RE::ControlMap::GetSingleton();
            return controls ? controls->controlMap[Resolve(a_context)] : nullptr;
        }

        bool                         g_hooked = false;
        RE::FxDelegate::CallbackDefn g_originalReset{};
        RE::FxDelegate::CallbackDefn g_originalSave{};

        std::string KeyName(std::string_view a_event, std::size_t a_device)
        {
            return std::string(a_event) + "|" + std::to_string(a_device);
        }

        // Rows sort by this, so a new action lands after the game's own.
        std::int8_t NextIndex(RE::ControlMap::InputContext& a_context)
        {
            std::int8_t next = 0;
            for (std::size_t device = 0; device <= RE::INPUT_DEVICES::kGamepad; ++device) {
                for (const auto& mapping : a_context.deviceMappings[device])
                    next = std::max(next, mapping.indexInContext);
            }
            return static_cast<std::int8_t>(next < 127 ? next + 1 : 127);
        }

        int DefaultFor(const Row& a_row, std::size_t a_device)
        {
            switch (a_device) {
            case RE::INPUT_DEVICES::kKeyboard: return a_row.defaultKey;
            case RE::INPUT_DEVICES::kGamepad:  return a_row.defaultGamepad;
            default:                           return -1;
            }
        }

        RE::ControlMap::UserEventMapping* FirstMapping(
            RE::ControlMap::InputContext& a_context, std::size_t a_device, std::string_view a_event)
        {
            // A second key on the same device would list the row twice.
            for (auto& mapping : a_context.deviceMappings[a_device]) {
                if (std::string_view(mapping.eventID.c_str()) == a_event)
                    return &mapping;
            }
            return nullptr;
        }

        constexpr int kUnbound = RE::ControlMap::kInvalid;

        // Like vanilla's Left Attack: an unbound entry on the missing device
        // lets a remap land there, and adds no row.
        int FallbackFor(RE::ControlMap::InputContext& a_context, const Row& a_row, std::size_t a_device, bool a_created)
        {
            if (a_created)
                return DefaultFor(a_row, a_device);
            if (a_device == RE::INPUT_DEVICES::kGamepad)
                return -1;

            const auto partner = a_device == RE::INPUT_DEVICES::kKeyboard ? RE::INPUT_DEVICES::kMouse
                                                                         : RE::INPUT_DEVICES::kKeyboard;
            return FirstMapping(a_context, partner, a_row.event) ? kUnbound : -1;
        }

        bool Surface(RE::ControlMap::InputContext& a_context, const Row& a_row,
            const std::unordered_map<std::string, int>& a_saved, bool& a_created, bool& a_filled)
        {
            // One action, one index, or the screen lists it twice.
            std::int8_t index = -1;
            for (std::size_t device = 0; device <= RE::INPUT_DEVICES::kGamepad && index < 0; ++device) {
                if (const auto* known = FirstMapping(a_context, device, a_row.event))
                    index = known->indexInContext;
            }

            a_created = index < 0;
            if (a_created)
                index = NextIndex(a_context);

            bool shown = false;
            for (std::size_t device = 0; device <= RE::INPUT_DEVICES::kGamepad; ++device) {
                const auto name = KeyName(a_row.event, device);
                const auto saved = a_saved.find(name);
                const auto wanted = saved != a_saved.end() && saved->second >= 0 ? saved->second : -1;

                if (auto* mapping = FirstMapping(a_context, device, a_row.event)) {
                    const auto original = g_defaults.try_emplace(name, mapping->inputKey).first->second;
                    mapping->remappable = true;
                    mapping->inputKey = static_cast<std::uint16_t>(wanted >= 0 ? wanted : original);
                    shown = true;
                    continue;
                }

                const auto fallback = FallbackFor(a_context, a_row, device, a_created);
                if (fallback < 0)
                    continue;
                g_defaults.try_emplace(name, fallback);
                a_filled = a_filled || !a_created;

                RE::ControlMap::UserEventMapping mapping{};
                mapping.eventID = a_row.event.c_str();
                mapping.inputKey = static_cast<std::uint16_t>(wanted >= 0 ? wanted : fallback);
                mapping.indexInContext = index;
                mapping.remappable = true;
                a_context.deviceMappings[device].push_back(mapping);
                shown = true;
            }
            return shown;
        }

        // Keeps entries the game won't have next launch out of its indexed
        // file, which only takes what is remappable.
        void HideAdded(bool a_hide)
        {
            for (const auto& row : g_rows) {
                if (!row.created && !row.filled)
                    continue;

                auto* context = ContextFor(row.context);
                if (!context)
                    continue;

                for (std::size_t device = 0; device <= RE::INPUT_DEVICES::kGamepad; ++device) {
                    if (!a_hide) {
                        if (auto* mapping = FirstMapping(*context, device, row.event))
                            mapping->remappable = true;
                        continue;
                    }
                    for (auto& mapping : context->deviceMappings[device]) {
                        if (std::string_view(mapping.eventID.c_str()) == row.event)
                            mapping.remappable = false;
                    }
                }
            }
        }

        // Vanilla reloads its defaults here, flags included.
        void OnResetControls(const RE::FxDelegateArgs& a_params)
        {
            if (g_originalReset.callback)
                g_originalReset.callback(a_params);

            Config::SaveControlKeys({});
            Apply();
        }

        // The game has just written the player's keys into the ControlMap.
        void OnSaveControls(const RE::FxDelegateArgs& a_params)
        {
            const std::lock_guard lock(g_mutex);

            std::unordered_map<std::string, int> keys;
            for (const auto& row : g_rows) {
                auto* context = ContextFor(row.context);
                if (!context)
                    continue;

                for (std::size_t device = 0; device <= RE::INPUT_DEVICES::kGamepad; ++device) {
                    for (const auto& mapping : context->deviceMappings[device]) {
                        if (!mapping.remappable || std::string_view(mapping.eventID.c_str()) != row.event)
                            continue;

                        const auto name = KeyName(row.event, device);
                        const auto original = g_defaults.find(name);
                        if (original == g_defaults.end() || original->second != mapping.inputKey)
                            keys[name] = mapping.inputKey;
                        break;
                    }
                }
            }

            Config::SaveControlKeys(keys);
            logger::debug("Controls: saved {} key(s)", keys.size());

            HideAdded(true);
            if (g_originalSave.callback)
                g_originalSave.callback(a_params);
            HideAdded(false);
        }

        // The game dispatches nothing for actions it doesn't own.
        class InputSink : public RE::BSTEventSink<RE::InputEvent*>
        {
        public:
            RE::BSEventNotifyControl ProcessEvent(
                RE::InputEvent* const* a_event, RE::BSTEventSource<RE::InputEvent*>*) override
            {
                if (!a_event)
                    return RE::BSEventNotifyControl::kContinue;

                for (auto* event = *a_event; event; event = event->next) {
                    auto* button = event->AsButtonEvent();
                    if (!button || !button->IsDown())
                        continue;

                    const std::lock_guard lock(g_mutex);
                    const std::string_view fired(button->QUserEvent().c_str());
                    for (const auto& row : g_rows) {
                        if (row.onPress && row.event == fired)
                            row.onPress();
                    }
                }
                return RE::BSEventNotifyControl::kContinue;
            }
        };
        InputSink g_inputSink;
        bool      g_listening = false;

        void ListenForPresses()
        {
            if (g_listening)
                return;
            if (std::none_of(g_rows.begin(), g_rows.end(), [](const Row& a_row) { return a_row.onPress != nullptr; }))
                return;

            auto* input = RE::BSInputDeviceManager::GetSingleton();
            if (!input)
                return;

            input->AddEventSink(&g_inputSink);
            g_listening = true;
            logger::debug("Controls: listening for presses");
        }

        void InstallHooks(RE::FxDelegate* a_fxDelegate)
        {
            if (!a_fxDelegate)
                return;

            const auto hook = [&](const char* a_name, RE::FxDelegate::CallbackDefn& a_original,
                                   RE::FxDelegateHandler::CallbackFn* a_replacement) {
                RE::GString name(a_name);
                if (!a_fxDelegate->callbacks.Get(name, &a_original)) {
                    logger::warn("Controls: no '{}' callback to hook", a_name);
                    return;
                }
                a_fxDelegate->callbacks.Set(name, RE::FxDelegate::CallbackDefn{ a_original.handler, a_replacement });
                logger::debug("Controls: '{}' hooked", a_name);
            };

            hook("ResetControlsToDefaults", g_originalReset, &OnResetControls);
            hook("SaveControls", g_originalSave, &OnSaveControls);

            g_hooked = true;
        }
    }

    bool Add(std::string a_event, Context a_context, std::string a_label, int a_defaultKey, int a_defaultGamepad,
        std::function<void()> a_onPress, std::string a_description, std::string a_owner)
    {
        const std::lock_guard lock(g_mutex);

        if (a_event.empty())
            return false;

        Translations::Load(a_owner);
        g_rows.push_back({ std::move(a_event), a_context, std::move(a_label), a_defaultKey, a_defaultGamepad,
            std::move(a_onPress), std::move(a_description), std::move(a_owner) });
        return true;
    }

    void Apply()
    {
        const std::lock_guard lock(g_mutex);

        const auto saved = Config::GetControlKeys();

        int shown = 0;
        for (auto& row : g_rows) {
            auto* context = ContextFor(row.context);
            if (!context) {
                logger::warn("Controls: '{}' asked for a context this game has no map for", row.event);
                continue;
            }

            if (!Surface(*context, row, saved, row.created, row.filled)) {
                logger::warn("Controls: '{}' is unknown and has no default key", row.event);
                continue;
            }
            ++shown;
            if (!row.label.empty())
                g_labels[row.event] = Translations::Resolve(row.label);
            logger::debug("Controls: '{}' [{}] shown", row.event, row.owner.empty() ? "unnamed" : row.owner.c_str());
        }

        if (g_rows.empty())
            return;

        ListenForPresses();
        logger::info("Controls: {} of {} row(s) shown", shown, g_rows.size());
    }

    void Reset()
    {
        const std::lock_guard lock(g_mutex);

        g_hooked = false;
        g_originalReset = {};
        g_originalSave = {};
    }

    void Tick(RE::JournalMenu* a_this, RE::GFxValue& a_page)
    {
        if (!g_hooked && a_this && a_this->fxDelegate)
            InstallHooks(a_this->fxDelegate.get());

        RE::GFxValue panel, list;
        if (!a_page.GetMember("InputMappingPanel", &panel) || !panel.IsObject() ||
            !panel.GetMember("List_mc", &list) || !list.IsObject())
            return;

        Debug::LogInputMappings(a_page);
        ListRows::EnsureScrollbar(list);

        const std::lock_guard lock(g_mutex);
        if (g_labels.empty())
            return;

        RE::GFxValue entries, maxShown;
        if (!list.GetMember("EntriesA", &entries) || !entries.IsArray() ||
            !list.GetMember("iMaxItemsShown", &maxShown) || !maxShown.IsNumber())
            return;

        const auto clipCount = static_cast<std::uint32_t>(maxShown.GetNumber());
        const auto entryCount = entries.GetArraySize();
        for (std::uint32_t i = 0; i < clipCount; ++i) {
            RE::GFxValue clip, itemIndex, entry, event;
            if (!list.GetMember(("Entry" + std::to_string(i)).c_str(), &clip) || !clip.IsObject() ||
                !clip.GetMember("itemIndex", &itemIndex) || !itemIndex.IsNumber())
                continue;

            const auto index = static_cast<std::uint32_t>(itemIndex.GetNumber());
            if (index >= entryCount || !entries.GetElement(index, &entry) || !entry.IsObject() ||
                !entry.GetMember("text", &event) || !event.IsString())
                continue;

            const auto label = g_labels.find(event.GetString());
            if (label == g_labels.end())
                continue;

            RE::GFxValue field, current;
            if (!clip.GetMember("textField", &field) || !field.IsObject())
                continue;
            if (field.GetMember("text", &current) && current.IsString() && label->second == current.GetString())
                continue;
            field.SetMember("text", Text::MakeGFxString(label->second));
        }
    }
}
