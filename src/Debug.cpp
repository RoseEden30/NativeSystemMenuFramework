#include "Debug.h"

#include "Config.h"
#include "NativeMenu.h"
#include "Pages.h"
#include "VanillaSettings.h"

#include <cmath>
#include <format>

namespace Debug
{
    namespace
    {
        void __stdcall GetFillerLabel(char* a_buffer, int a_bufferSize)
        {
            std::snprintf(a_buffer, a_bufferSize, "Filler row");
        }

        // Long enough to need the panel's own scrolling, and distinct per item
        // so the wrong one showing is obvious.
        void __stdcall GetShortPageText(char* a_buffer, int a_bufferSize)
        {
            std::snprintf(a_buffer, a_bufferSize, "Short item. If this reads back, the page list dispatched to it.");
        }

        void __stdcall GetLongPageText(char* a_buffer, int a_bufferSize)
        {
            std::string text = "Long item, repeated so the panel has to scroll.\n\n";
            for (int i = 1; i <= 40; ++i)
                text += std::format("Line {} of 40.\n", i);
            std::snprintf(a_buffer, a_bufferSize, "%s", text.c_str());
        }

    }

    void RegisterDevMenu()
    {
        if (!Config::IsVerboseLoggingEnabled())
            return;

        // Opens the framework's own tabs as a list, the path a consuming mod
        // takes.
        NativeMenu::AddEntry("NSMF Debug", nullptr, {}, NativeMenu::kFrameworkName);

        Pages::AddItem("NSMF Debug Page", "Short text", &GetShortPageText, NativeMenu::kFrameworkName);
        Pages::AddItem("NSMF Debug Page", "Long text", &GetLongPageText, NativeMenu::kFrameworkName);

        // The tab list only scrolls once it outgrows the screen, which the
        // real tabs never do on their own. These push it over.
        for (const auto* tab : { "NSMF Filler 1", "NSMF Filler 2", "NSMF Filler 3", "NSMF Filler 4" })
            VanillaSettings::AddLabel(
                tab, &GetFillerLabel, VanillaSettings::Align::kLeft, NativeMenu::kFrameworkName);
    }

    // What a clip actually exposes, read off the live tree rather than assumed
    // from the decompiled swf. Once per menu open, never per tick.
    void LogMembers(RE::GFxValue& a_value, const char* a_tag)
    {
        if (!a_value.IsObject()) {
            logger::debug("Debug: {} is not an object", a_tag);
            return;
        }

        std::string names;
        a_value.VisitMembers([&names](const char* a_name, const RE::GFxValue& a_val) {
            if (!names.empty())
                names += ", ";
            names += a_name;
            if (a_val.IsDisplayObject())
                names += ":clip";
            else if (a_val.IsArray())
                names += ":array";
            else if (a_val.IsObject())
                names += ":obj";
        });
        logger::debug("Debug: {} members = [{}]", a_tag, names);
    }
    // What vanilla's own rows carry, so a description can be keyed to
    // something stable rather than to their translated label.
    std::vector<std::string> g_dumpedTabs;
    // Only logged when it changes, per list: two of them refresh every tick.
    std::unordered_map<std::string, std::string> g_lastScrollbarLine;

    const char* WidgetName(RE::GFxValue& a_movieType)
    {
        if (!a_movieType.IsNumber())
            return "?";
        switch (static_cast<int>(a_movieType.GetNumber())) {
        case 0:
            return "slider";
        case 1:
            return "dropdown";
        case 2:
            return "checkbox";
        default:
            return "other";
        }
    }

    void LogVanillaEntries(RE::GFxValue& a_list, const std::string& a_tab)
    {
        if (std::find(g_dumpedTabs.begin(), g_dumpedTabs.end(), a_tab) != g_dumpedTabs.end())
            return;

        RE::GFxValue entryList;
        if (!a_list.GetMember("EntriesA", &entryList) || !entryList.IsArray() || entryList.GetArraySize() == 0)
            return;
        g_dumpedTabs.push_back(a_tab);

        for (std::uint32_t i = 0; i < entryList.GetArraySize(); ++i) {
            RE::GFxValue entry, id, text, movieType, value, options;
            if (!entryList.GetElement(i, &entry) || !entry.IsObject())
                continue;
            entry.GetMember("ID", &id);
            entry.GetMember("text", &text);
            entry.GetMember("movieType", &movieType);
            entry.GetMember("value", &value);

            // A dropdown's own options say what the row actually offers, which
            // its label rarely does.
            std::string choices;
            if (entry.GetMember("options", &options) && options.IsArray()) {
                for (std::uint32_t o = 0; o < options.GetArraySize(); ++o) {
                    RE::GFxValue option;
                    if (!options.GetElement(o, &option))
                        continue;
                    if (!choices.empty())
                        choices += " | ";
                    choices += option.IsString() ? option.GetString() : "?";
                }
            }

            logger::debug("Debug: {}[{}] '{}' is a {}, value {:.4g}, id {:.0f}{}", a_tab, i,
                text.IsString() ? text.GetString() : "?", WidgetName(movieType),
                value.IsNumber() ? value.GetNumber() : -1.0, id.IsNumber() ? id.GetNumber() : -1.0,
                choices.empty() ? "" : " - options: " + choices);
        }
    }
    // The numbers behind the scrollbar, whenever any of them moves - the only
    // way to tell an initialisation lag from a bad measurement.
    void LogScrollbarGeometry(RE::GFxValue& a_list, RE::GFxValue& a_bar, RE::GFxValue& a_row)
    {
        // Gated here rather than at the call site: unlike the other two this
        // runs every tick, and the reads alone are worth skipping.
        if (!Config::IsVerboseLoggingEnabled())
            return;

        const auto num = [](RE::GFxValue& a_obj, const char* a_name) {
            RE::GFxValue v;
            if (!a_obj.GetMember(a_name, &v))
                return -1.0;
            // _visible and friends come back as booleans.
            return v.IsNumber() ? v.GetNumber() : v.IsBool() ? (v.GetBool() ? 1.0 : 0.0) : -1.0;
        };

        RE::GFxValue track, thumb;
        a_bar.GetMember("track", &track);
        a_bar.GetMember("thumb", &thumb);

        // What setSize was given, reported back only a frame later.
        const auto askedW = num(a_bar, "__width");
        const auto askedH = num(a_bar, "__height");
        const auto asked = std::isnan(askedW) || std::isnan(askedH)
            ? std::string("not sized yet")
            : std::format("asked {:.1f}x{:.1f}", askedW, askedH);

        const auto line = std::format(
            "row {:.1f}x{:.1f} at ({:.1f},{:.1f}) | list: {:.0f} shown, {:.0f} clips, "
            "scrolls {:.0f}, height {:.1f} | bar {:.1f}x{:.1f} at ({:.1f},{:.1f}), {}, {} | "
            "track {:.1f}, thumb {:.1f}",
            num(a_row, "_width"), num(a_row, "_height"), num(a_row, "_x"), num(a_row, "_y"),
            num(a_list, "iListItemsShown"), num(a_list, "iMaxItemsShown"), num(a_list, "iMaxScrollPosition"),
            num(a_list, "fListHeight"), num(a_bar, "_width"), num(a_bar, "_height"), num(a_bar, "_x"),
            num(a_bar, "_y"), asked, num(a_bar, "_visible") > 0.0 ? "visible" : "hidden",
            track.IsObject() ? num(track, "_height") : -1.0, thumb.IsObject() ? num(thumb, "_height") : -1.0);

        RE::GFxValue target;
        const std::string key =
            a_list.GetMember("_target", &target) && target.IsString() ? target.GetString() : "";

        auto& last = g_lastScrollbarLine[key];
        if (line == last)
            return;
        last = line;
        logger::debug("Debug: scrollbar {}", line);
    }

    void LogHelpPanel(RE::GFxValue& a_page)
    {
        const auto dump = [&a_page](const char* a_path) {
            RE::GFxValue node = a_page;
            std::string  remaining = a_path;
            while (!remaining.empty()) {
                const auto dot = remaining.find('.');
                const auto name = remaining.substr(0, dot);
                RE::GFxValue child;
                if (!node.GetMember(name.c_str(), &child) || !child.IsObject()) {
                    logger::debug("Debug: {} missing at '{}'", a_path, name);
                    return;
                }
                node = child;
                remaining = dot == std::string::npos ? "" : remaining.substr(dot + 1);
            }
            LogMembers(node, a_path);
        };

        dump("HelpListPanel");
        dump("HelpListPanel.List_mc");
        dump("HelpTextPanel");
        dump("HelpTextPanel.HelpTextHolder");
        dump("HelpTextPanel.HelpTextHolder.HelpText");
        dump("HelpTextPanel.HelpTextHolder.TitleText");
    }

    void Reset()
    {
        g_dumpedTabs.clear();
        g_lastScrollbarLine.clear();
    }
}
