#include "IniSettings.h"

namespace IniSettings
{
    namespace
    {
        RE::Setting* Find(const char* a_name)
        {
            auto* setting = a_name ? RE::GetINISetting(a_name) : nullptr;
            if (!setting) {
                logger::warn("IniSettings: no setting named '{}'", a_name ? a_name : "(null)");
                return nullptr;
            }
            if (setting->GetType() != RE::Setting::Type::kFloat) {
                logger::warn("IniSettings: '{}' isn't a float setting", a_name);
                return nullptr;
            }
            return setting;
        }
    }

    float GetFloat(const char* a_name)
    {
        auto* setting = Find(a_name);
        return setting ? setting->GetFloat() : 0.0f;
    }

    void SetFloat(const char* a_name, float a_value)
    {
        if (auto* setting = Find(a_name))
            setting->SetFloat(a_value);
    }
}
