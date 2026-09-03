#pragma once

// A real engine ini setting by name - Skyrim.ini or SkyrimPrefs.ini,
// whichever actually holds it.
namespace IniSettings
{
    // 0.0f if a_name isn't a real setting.
    float GetFloat(const char* a_name);

    // Live only, cheap - call from a SettingSetter.
    void SetFloat(const char* a_name, float a_value);

    // Writes to disk, like vanilla's own menu does on commit. Call from
    // SettingCommit, not SettingSetter.
    bool SaveFloat(const char* a_name);
}
