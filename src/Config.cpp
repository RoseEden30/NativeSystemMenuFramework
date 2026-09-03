#include "Config.h"

#include <SimpleIni.h>

#include <unordered_set>

namespace Config
{
    namespace
    {
        bool g_verbose = false;
        int  g_injectionRetryTicks = 150;
        int  g_descriptionRows = 0;

        // Only the hidden ones are listed, so the file stays readable and an
        // entry nobody has touched needs no key at all.
        std::unordered_set<std::string> g_hiddenEntries;

        std::filesystem::path GetIniPath()
        {
            const auto* plugin = SKSE::PluginDeclaration::GetSingleton();
            return std::filesystem::path(std::format("Data/SKSE/Plugins/{}.ini", plugin->GetName()));
        }
    }

    void LoadSettings()
    {
        const auto path = GetIniPath();

        CSimpleIniA ini;
        ini.SetUnicode();

        if (ini.LoadFile(path.string().c_str()) >= 0) {
            g_verbose = ini.GetBoolValue("Debug", "Verbose", g_verbose);
            g_injectionRetryTicks = std::clamp(
                static_cast<int>(ini.GetLongValue("General", "InjectionRetryTicks", g_injectionRetryTicks)),
                kInjectionRetryMin, kInjectionRetryMax);
            g_descriptionRows = std::clamp(
                static_cast<int>(ini.GetLongValue("General", "DescriptionRows", g_descriptionRows)), 0, 2);
            std::list<CSimpleIniA::Entry> keys;
            ini.GetAllKeys("Hidden", keys);
            for (const auto& key : keys) {
                if (ini.GetBoolValue("Hidden", key.pItem, false))
                    g_hiddenEntries.emplace(key.pItem);
            }

            logger::info("Settings loaded from {}", path.string());
            if (!g_hiddenEntries.empty())
                logger::info("{} System menu entr{} hidden", g_hiddenEntries.size(),
                    g_hiddenEntries.size() == 1 ? "y" : "ies");
            return;
        }

        ini.SetBoolValue("Debug", "Verbose", g_verbose);
        ini.SetLongValue("General", "InjectionRetryTicks", g_injectionRetryTicks);
        ini.SetLongValue("General", "DescriptionRows", g_descriptionRows);
        if (ini.SaveFile(path.string().c_str()) < 0)
            logger::warn("Couldn't write default settings to {}", path.string());
    }

    bool IsVerboseLoggingEnabled() { return g_verbose; }
    int  GetInjectionRetryTicks() { return g_injectionRetryTicks; }
    int  GetDescriptionRows() { return g_descriptionRows; }

    bool IsEntryHidden(const std::string& a_entry) { return g_hiddenEntries.contains(a_entry); }

    void SetEntryHidden(const std::string& a_entry, bool a_hidden)
    {
        if (a_entry.empty() || IsEntryHidden(a_entry) == a_hidden)
            return;

        if (a_hidden)
            g_hiddenEntries.emplace(a_entry);
        else
            g_hiddenEntries.erase(a_entry);

        const auto  path = GetIniPath();
        CSimpleIniA ini;
        ini.SetUnicode();
        ini.LoadFile(path.string().c_str());
        if (a_hidden)
            ini.SetBoolValue("Hidden", a_entry.c_str(), true);
        else
            ini.Delete("Hidden", a_entry.c_str(), true);
        if (ini.SaveFile(path.string().c_str()) < 0)
            logger::warn("Couldn't save [Hidden] {} to {}", a_entry, path.string());
    }

    void SetVerboseLoggingEnabled(bool a_enabled)
    {
        g_verbose = a_enabled;
        const auto path = GetIniPath();
        CSimpleIniA ini;
        ini.SetUnicode();
        ini.LoadFile(path.string().c_str());
        ini.SetBoolValue("Debug", "Verbose", g_verbose);
        if (ini.SaveFile(path.string().c_str()) < 0)
            logger::warn("Couldn't save [Debug] Verbose to {}", path.string());
    }

    void SetInjectionRetryTicks(int a_ticks)
    {
        g_injectionRetryTicks = std::clamp(a_ticks, kInjectionRetryMin, kInjectionRetryMax);
    }

    void SaveInjectionRetryTicks()
    {
        const auto path = GetIniPath();
        CSimpleIniA ini;
        ini.SetUnicode();
        ini.LoadFile(path.string().c_str());
        ini.SetLongValue("General", "InjectionRetryTicks", g_injectionRetryTicks);
        if (ini.SaveFile(path.string().c_str()) < 0)
            logger::warn("Couldn't save [General] InjectionRetryTicks to {}", path.string());
    }

    void SetDescriptionRows(int a_rows)
    {
        g_descriptionRows = std::clamp(a_rows, 0, 2);

        const auto  path = GetIniPath();
        CSimpleIniA ini;
        ini.SetUnicode();
        ini.LoadFile(path.string().c_str());
        ini.SetLongValue("General", "DescriptionRows", g_descriptionRows);
        if (ini.SaveFile(path.string().c_str()) < 0)
            logger::warn("Couldn't save [General] DescriptionRows to {}", path.string());
    }
}
