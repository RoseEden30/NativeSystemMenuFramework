#pragma once

// Development only, behind [Debug] Verbose: dumps of the live Scaleform tree,
// and placeholder entries that exercise each menu shape.
namespace Debug
{
    void RegisterDevMenu();

    void LogMembers(RE::GFxValue& a_value, const char* a_tag);
    void LogVanillaEntries(RE::GFxValue& a_list, const std::string& a_tab);
    // The Help panel, which is the screen a mod page is built on.
    void LogHelpPanel(RE::GFxValue& a_page);

    void LogScrollbarGeometry(RE::GFxValue& a_list, RE::GFxValue& a_bar, RE::GFxValue& a_row);

    // Drops what is only dumped once per menu open.
    void Reset();
}
