#pragma once

#include <functional>

// BSScrollingList only creates as many "EntryN" clips as it ever shows at
// once, so an entry appended past that count stays in the data and never
// appears on screen.
namespace ListRows
{
    // Duplicates Entry0 until the list has a_needed clips, placing each one
    // and copying the input handlers BSScrollingList assigns at creation.
    // a_setup runs on every clip created.
    void Ensure(RE::GFxValue& a_list, std::uint32_t a_needed, const char* a_tag,
        const std::function<void(RE::GFxValue&)>& a_setup = {});
}
