#pragma once

// Data/SKSE/Plugins/NativeSystemMenuFramework.ini - written with defaults if it
// doesn't exist yet. Call LoadSettings once, early in SKSEPluginLoad.
namespace Config
{
    void LoadSettings();

    // [Debug] Verbose - logs menu clicks, callback dispatch and injection
    // details. Off for end users, on when diagnosing an integration issue.
    bool IsVerboseLoggingEnabled();

    // Saves to the ini. Call Logging::SetVerbose alongside to apply it live.
    void SetVerboseLoggingEnabled(bool a_enabled);

    // Bounded either way it is set: a negative value disables the injection,
    // a huge one keeps searching the menu tree all session.
    inline constexpr int kInjectionRetryMin = 30;
    inline constexpr int kInjectionRetryMax = 230;

    // [General] InjectionRetryTicks - how many menu ticks to keep retrying
    // the injection. An upper bound, not a wait: injection runs every tick
    // and stops as soon as it succeeds. Worth raising if entries don't appear
    // alongside other menu-modifying mods.
    int GetInjectionRetryTicks();

    // In memory only - a slider calls this on every step.
    void SetInjectionRetryTicks(int a_ticks);
    void SaveInjectionRetryTicks();

    // [General] DescriptionRows - rows the Settings lists give up so a
    // description has room under them. Vanilla already leaves enough.
    int GetDescriptionRows();

    // Saves to the ini. Applies on the next tick.
    void SetDescriptionRows(int a_rows);

    // [Hidden] - System menu entries the player has hidden, keyed by the
    // entry's own text rather than its position: a replaced interface, or a
    // version without the Creations entry, shifts every position. An unknown
    // key is never read, and an entry the file doesn't mention is visible.
    bool IsEntryHidden(const std::string& a_entry);
    void SetEntryHidden(const std::string& a_entry, bool a_hidden);
}
