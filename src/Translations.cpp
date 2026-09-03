#include "Translations.h"

#include <fstream>
#include <mutex>
#include <unordered_map>
#include <unordered_set>

namespace Translations
{
    namespace
    {
        constexpr const char* kFolder = "Data/Interface/Translations";

        std::unordered_map<std::string, std::string> g_strings;
        std::unordered_set<std::string>              g_pending;
        std::unordered_set<std::string>              g_loaded;
        std::mutex                                   g_mutex;

        std::string Lowercase(std::string a_text)
        {
            for (auto& c : a_text)
                c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
            return a_text;
        }

        // sLanguage is what the game itself keys its own translation files on,
        // so it is what a translator's file name will match.
        std::string GameLanguage()
        {
            auto*       collection = RE::INISettingCollection::GetSingleton();
            const auto* setting = collection ? collection->GetSetting("sLanguage:General") : nullptr;
            const auto* value = setting ? setting->GetString() : nullptr;
            return value && *value ? Lowercase(value) : "english";
        }

        // The folder and the file names are spelled every which way in the
        // wild - "Translations" and "translations", "_ENGLISH" and "_english"
        // both ship today - so nothing here compares case-sensitively.
        std::filesystem::path FindFile(const std::string& a_mod, const std::string& a_language)
        {
            std::error_code error;
            if (!std::filesystem::is_directory(kFolder, error))
                return {};

            const auto wanted = Lowercase(a_mod + "_" + a_language + ".txt");
            for (const auto& entry : std::filesystem::directory_iterator(kFolder, error)) {
                if (entry.is_regular_file(error) && Lowercase(entry.path().filename().string()) == wanted)
                    return entry.path();
            }
            return {};
        }

        std::string ToUtf8(const std::string& a_bytes)
        {
            // UTF-16LE with a BOM is what every file surveyed uses. UTF-8 is
            // accepted too, because a translator editing in Notepad and saving
            // with the default encoding would otherwise get broken accents
            // with nothing to explain them.
            if (a_bytes.size() >= 2 && static_cast<unsigned char>(a_bytes[0]) == 0xFF &&
                static_cast<unsigned char>(a_bytes[1]) == 0xFE) {
                const auto* wide = reinterpret_cast<const wchar_t*>(a_bytes.data() + 2);
                const auto  count = static_cast<int>((a_bytes.size() - 2) / sizeof(wchar_t));
                const auto  size = WideCharToMultiByte(CP_UTF8, 0, wide, count, nullptr, 0, nullptr, nullptr);
                if (size <= 0)
                    return {};
                std::string utf8(static_cast<std::size_t>(size), '\0');
                WideCharToMultiByte(CP_UTF8, 0, wide, count, utf8.data(), size, nullptr, nullptr);
                return utf8;
            }

            if (a_bytes.size() >= 3 && static_cast<unsigned char>(a_bytes[0]) == 0xEF &&
                static_cast<unsigned char>(a_bytes[1]) == 0xBB && static_cast<unsigned char>(a_bytes[2]) == 0xBF)
                return a_bytes.substr(3);

            return a_bytes;
        }

        // Later files win, so the game's language overwrites the English
        // fallback key by key rather than replacing the whole table - a
        // half-translated file still shows English for what it is missing.
        std::size_t Merge(const std::filesystem::path& a_path)
        {
            std::ifstream file(a_path, std::ios::binary);
            if (!file)
                return 0;

            const std::string bytes((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
            const auto        text = ToUtf8(bytes);

            std::size_t added = 0;
            std::size_t start = 0;
            while (start < text.size()) {
                auto end = text.find('\n', start);
                if (end == std::string::npos)
                    end = text.size();

                auto line = text.substr(start, end - start);
                start = end + 1;
                if (!line.empty() && line.back() == '\r')
                    line.pop_back();

                const auto tab = line.find('\t');
                if (tab == std::string::npos || line.empty() || line.front() != '$')
                    continue;

                auto key = line.substr(0, tab);
                auto value = line.substr(tab + 1);
                if (value.empty())
                    continue;

                g_strings.insert_or_assign(std::move(key), std::move(value));
                ++added;
            }
            return added;
        }

        // Deferred rather than done on registration: sLanguage is not readable
        // yet at kPostPostLoad, so a mod registering that early silently got
        // the English fallback while one registering at kDataLoaded got the
        // right language. Resolving only happens once a menu is open, by which
        // time the setting is there.
        void FlushLocked()
        {
            if (g_pending.empty())
                return;

            static bool announced = false;
            const auto  language = GameLanguage();
            if (!announced) {
                announced = true;
                logger::info("Translations: game language is {}", language);
            }

            std::vector<std::string> wanted{ "english" };
            if (language != "english")
                wanted.push_back(language);

            for (const auto& mod : g_pending) {
                bool found = false;
                for (const auto& which : wanted) {
                    const auto path = FindFile(mod, which);
                    if (path.empty()) {
                        logger::debug("Translations: no {}_{}.txt", mod, which);
                        continue;
                    }
                    found = true;
                    logger::info("Translations: {} string(s) from {}", Merge(path), path.filename().string());
                }
                // A missing file for the game's language is normal - English
                // covers it. Nothing at all means the file names don't match
                // the name the mod registered under, which is otherwise
                // invisible: its keys just render as keys.
                if (!found)
                    logger::warn("Translations: no file for '{}' - expected {}_<language>.txt in {}", mod, mod,
                        kFolder);
                g_loaded.insert(mod);
            }
            g_pending.clear();
        }
    }

    void Load(const std::string& a_mod)
    {
        const std::lock_guard lock(g_mutex);

        if (!a_mod.empty() && !g_loaded.contains(a_mod))
            g_pending.insert(a_mod);
    }

    const std::string& Resolve(const std::string& a_text)
    {
        if (a_text.empty() || a_text.front() != '$')
            return a_text;

        const std::lock_guard lock(g_mutex);
        FlushLocked();

        const auto found = g_strings.find(a_text);
        return found != g_strings.end() ? found->second : a_text;
    }
}
