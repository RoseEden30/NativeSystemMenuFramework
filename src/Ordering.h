#pragma once

// Groups are ordered by mod name rather than by plugin load order, which
// would reshuffle the menu whenever a DLL is renamed.
//
// Always through a stable sort, so a mod's own declaration order survives
// inside its group.
namespace Ordering
{
    // Case-insensitive, or a mod named "iNeed" lands after "Zzz".
    inline bool OwnerPrecedes(const std::string& a_lhs, const std::string& a_rhs)
    {
        return std::lexicographical_compare(a_lhs.begin(), a_lhs.end(), a_rhs.begin(), a_rhs.end(),
            [](unsigned char a_a, unsigned char a_b) { return std::tolower(a_a) < std::tolower(a_b); });
    }
}
