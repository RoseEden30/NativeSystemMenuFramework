#pragma once

// A real engine ini setting by name - Skyrim.ini or SkyrimPrefs.ini,
// whichever actually holds it. Live only: WriteSetting doesn't reliably
// persist to disk, so a consumer must save the value in its own ini and
// reapply it here on load.
namespace IniSettings
{
    // 0.0f if a_name isn't a real setting.
    float GetFloat(const char* a_name);

    void SetFloat(const char* a_name, float a_value);
}
