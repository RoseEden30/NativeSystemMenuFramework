#pragma once

// SystemPage's state constants, read from the running game rather than
// hardcoded: Anniversary Edition inserted two states, so TRANSITIONING is 13
// on 1.5 and 15 on 1.6, and a replaced interface can move them again.
namespace SystemState
{
    inline std::unordered_map<std::string, double> g_cache;

    // Cached per menu open. The fallback is only used if the lookup fails.
    inline double Read(RE::GFxMovieView* a_view, const char* a_name, double a_fallback)
    {
        const auto cached = g_cache.find(a_name);
        if (cached != g_cache.end())
            return cached->second;

        double       value = a_fallback;
        RE::GFxValue read;
        const auto   path = std::string("_global.SystemPage.") + a_name;
        if (a_view && a_view->GetVariable(&read, path.c_str()) && read.IsNumber())
            value = read.GetNumber();
        else
            logger::warn("SystemState: {} not readable, assuming {}", a_name, a_fallback);

        g_cache[a_name] = value;
        return value;
    }

    inline void Reset() { g_cache.clear(); }
}
