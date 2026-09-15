#pragma once

#include <functional>

// Rows on the Controls screen. Vanilla lists the Gameplay actions flagged
// remappable in the live ControlMap, so flipping that flag surfaces a row.
namespace Controls
{
    // Resolved against INPUT_CONTEXT_ID, which differs across SE, AE and VR.
    enum class Context
    {
        kGameplay = 0,
        kMenu,
        kInventory,
        kFavorites,
        kMap,
        kStats,
        kBook,
        kJournal,
        kLockpicking,
    };

    // An action already in a_context is unlocked, one the game doesn't know is
    // created from the defaults. a_label overrides the row's text.
    bool Add(std::string a_event, Context a_context, std::string a_label, int a_defaultKey, int a_defaultGamepad,
        std::function<void()> a_onPress, std::string a_description, std::string a_owner);

    // Call once the game has read controlmap.txt.
    void Apply();

    // The list redraws its own rows, so labels are reapplied every tick.
    void Tick(RE::JournalMenu* a_this, RE::GFxValue& a_page);

    void Reset();
}
