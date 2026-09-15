#pragma once

// A Scaleform write costs far more than a read, and these run on every row of
// every tick.
namespace GFx
{
    inline void SetIfChanged(RE::GFxValue& a_object, const char* a_member, double a_value, double a_epsilon = 0.0001)
    {
        RE::GFxValue current;
        if (a_object.GetMember(a_member, &current) && current.IsNumber() &&
            std::abs(current.GetNumber() - a_value) <= a_epsilon)
            return;
        a_object.SetMember(a_member, RE::GFxValue(a_value));
    }

    inline void SetIfChanged(RE::GFxValue& a_object, const char* a_member, bool a_value)
    {
        RE::GFxValue current;
        if (a_object.GetMember(a_member, &current) && current.IsBool() && current.GetBool() == a_value)
            return;
        a_object.SetMember(a_member, RE::GFxValue(a_value));
    }
}
