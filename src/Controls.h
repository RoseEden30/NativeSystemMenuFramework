#pragma once

#include <functional>

// Rows on the System menu's Controls screen. Vanilla lists the Gameplay rows
// flagged remappable in the live ControlMap, so flipping that flag surfaces a
// row with vanilla's own remap, conflict check and persistence.
namespace Controls
{
    // Resolved against the running game's INPUT_CONTEXT_ID, which isn't the
    // same list on SE, AE and VR.
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
    // created from the defaults, which are scan codes or -1 for none. a_label
    // overrides the row's text where the game has no wording for it.
    bool Add(std::string a_event, Context a_context, std::string a_label, int a_defaultKey, int a_defaultGamepad,
        std::function<void()> a_onPress, std::string a_description, std::string a_owner);

    // Call once the game has read controlmap.txt.
    void Apply();

    // The list redraws its own rows, so labels are reapplied every tick.
    void Tick(RE::GFxValue& a_page);
}
