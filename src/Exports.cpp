#include "IniSettings.h"
#include "NativeMenu.h"
#include "Pages.h"
#include "Translations.h"
#include "VanillaSettings.h"

// Resolved by consumers via GetProcAddress. Exported names are part of the
// API and must stay stable across releases.

extern "C" __declspec(dllexport) bool __cdecl AddSystemMenuEntry(
    const char* a_text, void(__stdcall* a_callback)(), const char* a_jumpToTab, const char* a_owner)
{
    return NativeMenu::AddEntry(
        a_text ? a_text : "", a_callback, a_jumpToTab ? a_jumpToTab : "", a_owner ? a_owner : "");
}

extern "C" __declspec(dllexport) bool __cdecl AddVanillaSetting(const char* a_tab, int a_type, const char* a_label,
    float(__stdcall* a_getValue)(), void(__stdcall* a_onChange)(float), float a_defaultValue,
    const char* const* a_options, int a_optionCount, bool(__stdcall* a_isEnabled)(),
    void(__stdcall* a_formatValue)(float, char*, int), const char* a_description, const char* a_owner,
    void(__stdcall* a_onCommit)(float))
{
    std::vector<std::string> options;
    for (int i = 0; a_options && i < a_optionCount; ++i)
        options.emplace_back(a_options[i] ? a_options[i] : "");

    return VanillaSettings::AddSetting(a_tab ? a_tab : "", static_cast<VanillaSettings::Type>(a_type),
        a_label ? a_label : "", a_getValue, a_onChange, a_defaultValue, std::move(options), a_isEnabled,
        a_formatValue, a_description ? a_description : "", a_owner ? a_owner : "", a_onCommit);
}

// For text a mod assembles itself, which never reaches the framework as a
// whole key.
extern "C" __declspec(dllexport) void __cdecl TranslateString(
    const char* a_key, char* a_buffer, int a_bufferSize)
{
    if (!a_buffer || a_bufferSize <= 0)
        return;
    // Named, not a temporary: Resolve returns its argument back when the key
    // is unknown, so the string has to outlive the call.
    const std::string key = a_key ? a_key : "";
    std::snprintf(a_buffer, static_cast<std::size_t>(a_bufferSize), "%s", Translations::Resolve(key).c_str());
}

extern "C" __declspec(dllexport) bool __cdecl AddSystemMenuPageItem(
    const char* a_page, const char* a_label, void(__stdcall* a_getText)(char*, int), const char* a_owner)
{
    return Pages::AddItem(a_page ? a_page : "", a_label ? a_label : "", a_getText, a_owner ? a_owner : "");
}

extern "C" __declspec(dllexport) bool __cdecl AddVanillaLabel(
    const char* a_tab, void(__stdcall* a_getText)(char*, int), int a_align, const char* a_owner)
{
    return VanillaSettings::AddLabel(
        a_tab ? a_tab : "", a_getText, static_cast<VanillaSettings::Align>(a_align), a_owner ? a_owner : "");
}

extern "C" __declspec(dllexport) bool __cdecl AddVanillaButton(
    const char* a_tab, const char* a_label, void(__stdcall* a_onPress)(), const char* a_owner)
{
    return VanillaSettings::AddButton(
        a_tab ? a_tab : "", a_label ? a_label : "", a_onPress, a_owner ? a_owner : "");
}

extern "C" __declspec(dllexport) bool __cdecl SetVanillaTabDescription(
    const char* a_tab, const char* a_description)
{
    return VanillaSettings::SetTabDescription(a_tab ? a_tab : "", a_description ? a_description : "");
}

extern "C" __declspec(dllexport) float __cdecl GetIniSetting(const char* a_name)
{
    return IniSettings::GetFloat(a_name);
}

extern "C" __declspec(dllexport) void __cdecl SetIniSetting(const char* a_name, float a_value)
{
    IniSettings::SetFloat(a_name, a_value);
}
