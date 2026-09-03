#pragma once

#include "Translations.h"

#include <unordered_set>

// UTF-8 to the wide string Scaleform wants - the narrow constructor doesn't
// render non-Latin scripts reliably.
//
// GFxValue points at the string data instead of copying it, so converted text
// is interned and kept for the process lifetime. Translation happens before
// interning, or the key would be what gets cached.
namespace Text
{
    inline RE::GFxValue MakeGFxString(const std::string& a_utf8)
    {
        static std::unordered_set<std::wstring> interned;

        const auto& text = Translations::Resolve(a_utf8);

        std::wstring wide;
        const int    wideLen = MultiByteToWideChar(CP_UTF8, 0, text.c_str(), -1, nullptr, 0);
        if (wideLen > 1) {
            wide.resize(static_cast<std::size_t>(wideLen) - 1);
            MultiByteToWideChar(CP_UTF8, 0, text.c_str(), -1, wide.data(), wideLen);
        }

        return RE::GFxValue(interned.insert(std::move(wide)).first->c_str());
    }
}
