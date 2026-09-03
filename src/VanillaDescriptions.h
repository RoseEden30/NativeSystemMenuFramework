#pragma once

// Descriptions for Skyrim's own settings rows, which ship without any.
//
// Found by the raw "$..." text the game puts in the row data: that is the same
// in every language, unlike the translated label on screen, and unlike the row
// IDs, which are FormIDs on the Audio tab and so move with the load order.
//
// The description is a key of ours, resolved like any other - the wording
// lives in Interface/Translations/NativeSystemMenuFramework_<language>.txt.
namespace VanillaDescriptions
{
    struct Entry
    {
        const char* vanillaKey;
        const char* descriptionKey;
    };

    inline constexpr Entry kEntries[] = {
        // Gameplay
        Entry{ "$Invert Y", "$NSMF_VANILLA_INVERT_Y" },
        Entry{ "$Look Sensitivity", "$NSMF_VANILLA_LOOK_SENSITIVITY" },
        Entry{ "$Vibration", "$NSMF_VANILLA_VIBRATION" },
        Entry{ "$360 Controller", "$NSMF_VANILLA_360_CONTROLLER" },
        Entry{ "$SaveGameMissingCreationsCheck", "$NSMF_VANILLA_SAVEGAMEMISSINGCREATIONSCHECK" },
        Entry{ "$Difficulty", "$NSMF_VANILLA_DIFFICULTY" },
        Entry{ "$Show Floating Markers", "$NSMF_VANILLA_SHOW_FLOATING_MARKERS" },
        Entry{ "$Save on Rest", "$NSMF_VANILLA_SAVE_ON_REST" },
        Entry{ "$Save on Wait", "$NSMF_VANILLA_SAVE_ON_WAIT" },
        Entry{ "$Save on Travel", "$NSMF_VANILLA_SAVE_ON_TRAVEL" },
        Entry{ "$Save on Pause", "$NSMF_VANILLA_SAVE_ON_PAUSE" },

        // Display
        Entry{ "$Brightness", "$NSMF_VANILLA_BRIGHTNESS" },
        Entry{ "$HUD Opacity", "$NSMF_VANILLA_HUD_OPACITY" },
        Entry{ "$Actor Fade", "$NSMF_VANILLA_ACTOR_FADE" },
        Entry{ "$Item Fade", "$NSMF_VANILLA_ITEM_FADE" },
        Entry{ "$Object Fade", "$NSMF_VANILLA_OBJECT_FADE" },
        Entry{ "$Grass Fade", "$NSMF_VANILLA_GRASS_FADE" },
        Entry{ "$Crosshair", "$NSMF_VANILLA_CROSSHAIR" },
        Entry{ "$Dialogue Subtitles", "$NSMF_VANILLA_DIALOGUE_SUBTITLES" },
        Entry{ "$General Subtitles", "$NSMF_VANILLA_GENERAL_SUBTITLES" },
        Entry{ "$DDOF Intensity", "$NSMF_VANILLA_DDOF_INTENSITY" },

        // Audio
        Entry{ "$Master", "$NSMF_VANILLA_MASTER" },
        Entry{ "$Effects", "$NSMF_VANILLA_EFFECTS" },
        Entry{ "$Footsteps", "$NSMF_VANILLA_FOOTSTEPS" },
        Entry{ "$Voice", "$NSMF_VANILLA_VOICE" },
        Entry{ "$Music", "$NSMF_VANILLA_MUSIC" },
    };

    inline const char* Find(const char* a_key)
    {
        if (!a_key)
            return nullptr;
        for (const auto& entry : kEntries)
            if (std::string_view(entry.vanillaKey) == a_key)
                return entry.descriptionKey;
        return nullptr;
    }
}
