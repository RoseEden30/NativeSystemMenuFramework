#pragma once

// Skyrim's translation files, read by us rather than by the engine, which
// only loads them for plugins in the load order - a DLL-only mod would
// otherwise need a dummy esp to be translatable.
//
// The format is the one translators and xTranslator already use: UTF-16LE
// with a BOM, one "$KEY<tab>Value" per line, in
// Interface/Translations/<mod>_<language>.txt where <mod> is the name passed
// to SetModName.
namespace Translations
{
    // <mod>_english.txt as a fallback, then <mod>_<game language>.txt over
    // it. Later calls for the same mod do nothing.
    void Load(const std::string& a_mod);

    // The translation for a "$key", or the text unchanged when nothing knows
    // it - the engine resolves the game's own keys and the framework leans on
    // that, so a made-up fallback would break the labels that work today.
    //
    // Returns a_text itself on a miss, so a_text must outlive the result.
    const std::string& Resolve(const std::string& a_text);
}
