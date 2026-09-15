#include "Controls.h"

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
        };

        std::recursive_mutex g_mutex;
        std::vector<Row>     g_rows;

        std::unordered_map<std::string, std::string> g_labels;

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

        // Some actions carry a second key, and unlocking both lists the row twice.
        bool Unlock(RE::ControlMap::InputContext& a_context, std::string_view a_event)
        {
            bool found = false;
            for (std::size_t device = 0; device <= RE::INPUT_DEVICES::kGamepad; ++device) {
                bool first = true;
                for (auto& mapping : a_context.deviceMappings[device]) {
                    if (std::string_view(mapping.eventID.c_str()) != a_event)
                        continue;
                    found = true;
                    if (first)
                        mapping.remappable = true;
                    first = false;
                }
            }
            return found;
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

        int shown = 0;
        for (const auto& row : g_rows) {
            auto* context = ContextFor(row.context);
            if (!context) {
                logger::warn("Controls: '{}' asked for a context this game has no map for", row.event);
                continue;
            }

            if (!Unlock(*context, row.event)) {
                logger::warn("Controls: '{}' isn't in that context", row.event);
                continue;
            }
            ++shown;
            if (!row.label.empty())
                g_labels[row.event] = Translations::Resolve(row.label);
            logger::debug("Controls: '{}' [{}] shown", row.event, row.owner.empty() ? "unnamed" : row.owner.c_str());
        }

        if (!g_rows.empty())
            logger::info("Controls: {} of {} row(s) shown", shown, g_rows.size());
    }

    void Tick(RE::GFxValue& a_page)
    {
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
            // Clips are recycled as the list scrolls.
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
