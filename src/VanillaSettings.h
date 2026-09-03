#pragma once

#include <functional>

// Adds rows to the System menu's settings screen using Skyrim's own
// ScrollBar/OptionStepper/CheckBox widgets, through the same
// GameDelegate.call("OptionChange", ...) dispatch vanilla rows use.
//
// a_tab is "Gameplay", "Display" or "Audio" for a native tab, or any other
// name to create one the first time it is used.
namespace VanillaSettings
{
    enum class Type
    {
        kSlider = 0,    // 0.0-1.0
        kDropdown = 1,  // index into a_options
        kCheckbox = 2,  // 0 or 1
        kLabel = 3,     // read-only text - see AddLabel
        kButton = 4,    // fire-and-forget action - see AddButton
    };

    // Label rows only: a setting row carries a widget on the right, so its
    // text has nowhere to move.
    enum class Align
    {
        kLeft = 0,
        kCenter = 1,
        kRight = 2,
    };

    // Called every JournalMenu tick: installs the OptionChange interception
    // once per menu open, then (re)injects rows into the visible tab.
    void Tick(RE::JournalMenu* a_this, RE::GFxMovieView* a_view, RE::GFxValue& a_systemPage);

    // Settles anything still pending, then clears per-open state. Call
    // whenever the System menu opens or closes.
    void Reset();

    // a_defaultValue (same units as a_getValue/a_onChange) hooks the row into
    // vanilla's own "reset settings to default". a_isEnabled greys the row out
    // and blocks its changes while false. a_formatValue (kSlider only) appends
    // the value to the label, since a ScrollBar has no readout of its own.
    // a_description shows under the rows while the row is selected.
    //
    // a_onCommit runs once the value settles; a_onChange runs on every step.
    //
    // std::function rather than plain pointers so the framework's own rows can
    // carry context - one checkbox per System menu entry, where the entries
    // are only known at runtime. The C API still takes bare pointers.
    bool AddSetting(std::string a_tab, Type a_type, std::string a_label, std::function<float()> a_getValue,
        std::function<void(float)> a_onChange, float a_defaultValue, std::vector<std::string> a_options,
        std::function<bool()> a_isEnabled = nullptr,
        std::function<void(float a_value, char* a_buffer, int a_bufferSize)> a_formatValue = nullptr,
        std::string a_description = {}, std::string a_owner = {},
        std::function<void(float)> a_onCommit = nullptr);

    // Read-only row: no widget binds to an unrecognized movieType, leaving
    // just the text. a_getText runs every tick, so keep it cheap.
    bool AddLabel(std::string a_tab, std::function<void(char* a_buffer, int a_bufferSize)> a_getText,
        Align a_align = Align::kLeft, std::string a_owner = {});

    // Vanilla has no button widget - this is a CheckBox reset to unchecked
    // every frame, so a click reads as a momentary press.
    bool AddButton(
        std::string a_tab, std::string a_label, std::function<void()> a_onPress, std::string a_owner = {});

    // Shown while a_tab is highlighted in the category list. Empty clears it.
    bool SetTabDescription(std::string a_tab, std::string a_description);

    // Opens the category list showing only a_owner's tabs, so a System menu
    // entry acts as that mod's own page. False if the mod has no tabs.
    bool ShowOwnerTabs(RE::GFxMovieView* a_view, RE::GFxValue& a_page, const std::string& a_owner);

    // Puts the full tab list back. Call before handing a System menu press to
    // vanilla, or its Settings entry opens the narrowed list.
    void ClearOwnerScope(RE::GFxValue& a_page);

    // Opens a_tab's settings directly, skipping the category list.
    void JumpToTab(RE::GFxMovie* a_view, RE::GFxValue& a_page, const std::string& a_tab);
}
