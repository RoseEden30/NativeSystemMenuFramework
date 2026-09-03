#include "Config.h"
#include "Translations.h"
#include "Debug.h"
#include "Logging.h"
#include "NativeMenu.h"
#include "VanillaSettings.h"

#include <cstdio>

namespace
{
    // The framework's own settings, in a custom tab like any other, rather
    // than buried in an ini the player has to find and hand-edit.
    void __stdcall GetVersionLabel(char* a_buffer, int a_bufferSize)
    {
        const auto* plugin = SKSE::PluginDeclaration::GetSingleton();
        std::snprintf(a_buffer, a_bufferSize, "NativeSystemMenuFramework %s", plugin->GetVersion().string().c_str());
    }

    float __stdcall GetVerboseLogging() { return Config::IsVerboseLoggingEnabled() ? 1.0f : 0.0f; }
    void  __stdcall SetVerboseLogging(float a_value)
    {
        const bool enabled = a_value != 0.0f;
        Config::SetVerboseLoggingEnabled(enabled);
        Logging::SetVerbose(enabled);
    }

    // The ScrollBar has 21 stops, and Config's range spans exactly 10 ticks
    // per stop. A range that doesn't makes the readout drift off the stored
    // value - 150 would show as 152.
    constexpr float kRetryMin = static_cast<float>(Config::kInjectionRetryMin);
    constexpr float kRetrySpan = static_cast<float>(Config::kInjectionRetryMax - Config::kInjectionRetryMin);

    int RetryTicksFor(float a_value) { return static_cast<int>(kRetryMin + a_value * kRetrySpan + 0.5f); }

    float __stdcall GetInjectionRetries()
    {
        return (static_cast<float>(Config::GetInjectionRetryTicks()) - kRetryMin) / kRetrySpan;
    }
    void __stdcall SetInjectionRetries(float a_value) { Config::SetInjectionRetryTicks(RetryTicksFor(a_value)); }

    void __stdcall CommitInjectionRetries(float) { Config::SaveInjectionRetryTicks(); }

    void __stdcall FormatInjectionRetries(float a_value, char* a_buffer, int a_bufferSize)
    {
        const auto ticks = RetryTicksFor(a_value);
        // Resolved here rather than left as a key: the label and the value are
        // glued together before display, and the pair matches nothing.
        std::snprintf(a_buffer, a_bufferSize, "%d %s", ticks, Translations::Resolve("$NSMF_TICKS").c_str());
    }

    void RegisterOwnSettings()
    {
        using NativeMenu::kFrameworkName;
        using NativeMenu::kFrameworkTab;

        VanillaSettings::SetTabDescription("Gameplay", "$NSMF_TAB_GAMEPLAY_DESC");
        VanillaSettings::SetTabDescription("Display", "$NSMF_TAB_DISPLAY_DESC");
        VanillaSettings::SetTabDescription("Audio", "$NSMF_TAB_AUDIO_DESC");
        VanillaSettings::SetTabDescription(kFrameworkTab, "$NSMF_TAB_FRAMEWORK_DESC");

        VanillaSettings::AddLabel(
            kFrameworkTab, &GetVersionLabel, VanillaSettings::Align::kLeft, kFrameworkName);
        VanillaSettings::AddSetting(kFrameworkTab, VanillaSettings::Type::kCheckbox, "$NSMF_VERBOSE_LOGGING", &GetVerboseLogging,
            &SetVerboseLogging, 0.0f, {}, nullptr, nullptr, "$NSMF_VERBOSE_LOGGING_DESC", kFrameworkName);
        VanillaSettings::AddSetting(
            kFrameworkTab, VanillaSettings::Type::kDropdown, "$NSMF_DESCRIPTION_SPACE",
            [] { return static_cast<float>(Config::GetDescriptionRows()); },
            [](float a_value) { Config::SetDescriptionRows(static_cast<int>(a_value)); }, 0.0f,
            { "$NSMF_SPACE_NONE", "$NSMF_SPACE_SMALL", "$NSMF_SPACE_LARGE" }, nullptr, nullptr,
            "$NSMF_DESCRIPTION_SPACE_DESC", kFrameworkName);
        VanillaSettings::AddSetting(kFrameworkTab, VanillaSettings::Type::kSlider, "$NSMF_INJECTION_RETRIES",
            &GetInjectionRetries, &SetInjectionRetries, (150.0f - kRetryMin) / kRetrySpan, {}, nullptr,
            &FormatInjectionRetries, "$NSMF_INJECTION_RETRIES_DESC", kFrameworkName, &CommitInjectionRetries);

        Debug::RegisterDevMenu();
    }

    void OnMessage(SKSE::MessagingInterface::Message* message)
    {
        switch (message->type) {
        case SKSE::MessagingInterface::kPostPostLoad:
            // Before any consumer registers. The hook doesn't depend on
            // entries existing - injection no-ops until one is.
            NativeMenu::InstallHooks();
            RegisterOwnSettings();
            break;

        default:
            break;
        }
    }
}

SKSEPluginLoad(const SKSE::LoadInterface* skse)
{
    SKSE::Init(skse);
    Logging::Init();
    Config::LoadSettings();
    if (Config::IsVerboseLoggingEnabled())
        Logging::SetVerbose();

    const auto* plugin = SKSE::PluginDeclaration::GetSingleton();
    logger::info("{} {} loaded", plugin->GetName(), plugin->GetVersion());

    SKSE::GetMessagingInterface()->RegisterListener(OnMessage);

    return true;
}
