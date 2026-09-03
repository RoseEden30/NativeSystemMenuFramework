#pragma once

// A page of a mod's own in the System menu: a list of items, each opening a
// panel of text. Built on vanilla's Help screen, which is that shape already
// and brings a scrolling list and text panel with it.
//
// Adding the first item to a name creates the page and its System menu
// entry.
namespace Pages
{
    bool AddItem(std::string a_page, std::string a_label,
        void(__stdcall* a_getText)(char* a_buffer, int a_bufferSize), std::string a_owner = {});

    // Fills the Help list with a_name's items and opens it. False if no page
    // by that name has any item.
    bool Show(RE::GFxMovieView* a_view, RE::GFxValue& a_systemPage, const std::string& a_name);

    // Hands the panel back to vanilla, topics and press handler both. Call
    // before letting a System menu press reach the game's own Help entry.
    void ClearScope(RE::GFxValue& a_systemPage);

    // Clears per-open state. Call whenever the System menu opens or closes.
    void Reset();
}
