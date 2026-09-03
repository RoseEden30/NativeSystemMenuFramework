#pragma once

// Adds entries to Skyrim's own System (Escape) menu list, alongside Save/
// Load/Settings/... - by walking and editing the live Scaleform object tree
// of the already-loaded menu, not by replacing any interface file.
namespace NativeMenu
{
    // Owner, and the name of the translation files.
    inline constexpr const char* kFrameworkName = "NativeSystemMenuFramework";

    // Its settings tab, as a key so the label is translatable.
    inline constexpr const char* kFrameworkTab = "$NSMF_TAB_FRAMEWORK";

    void InstallHooks();

    // a_callback runs on the menu's own update tick when the entry is
    // selected - not the render thread, not mid-frame.
    //
    // a_jumpToTab, if given, opens that tab's settings directly instead of
    // the category list. a_callback still runs if given.
    //
    // With neither, the entry opens a_owner's own tabs as a list.
    //
    // Returns false if a_text is empty, or the entry has nothing to do.
    bool AddEntry(std::string a_text, void(__stdcall* a_callback)(), std::string a_jumpToTab = {},
        std::string a_owner = {});

    // An entry that opens a page of its own - see Pages. Registered by Pages
    // itself when a page's first item arrives, so a mod never adds it.
    bool AddPageEntry(std::string a_page, std::string a_owner);
}
