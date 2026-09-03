#include "ListRows.h"

namespace ListRows
{
    namespace
    {
        constexpr double kDepthBase = 21000.0;
    }

    void Ensure(RE::GFxValue& a_list, std::uint32_t a_needed, const char* a_tag,
        const std::function<void(RE::GFxValue&)>& a_setup)
    {
        RE::GFxValue  maxShownV;
        std::uint32_t curClips = 0;
        const bool    haveMaxShown = a_list.GetMember("iMaxItemsShown", &maxShownV) && maxShownV.IsNumber();
        if (haveMaxShown)
            curClips = static_cast<std::uint32_t>(maxShownV.GetNumber());
        logger::debug("ListRows[{}]: iMaxItemsShown found={} value={} needed={}", a_tag, haveMaxShown, curClips,
            a_needed);
        if (curClips == 0 || a_needed <= curClips)
            return;

        RE::GFxValue entry0, entry1;
        if (!a_list.GetMember("Entry0", &entry0) || !entry0.IsObject()) {
            logger::warn("ListRows[{}]: no Entry0 to duplicate", a_tag);
            return;
        }

        // duplicateMovieClip copies _x/_y as-is, so the offset between the
        // first two rows is read rather than assumed - lists vary in
        // direction.
        double originX = 0.0, originY = 0.0, stepX = 0.0, stepY = 0.0;
        bool   haveStep = false;
        if (a_list.GetMember("Entry1", &entry1) && entry1.IsObject()) {
            RE::GFxValue x0, y0, x1, y1;
            if (entry0.GetMember("_x", &x0) && x0.IsNumber() && entry0.GetMember("_y", &y0) && y0.IsNumber() &&
                entry1.GetMember("_x", &x1) && x1.IsNumber() && entry1.GetMember("_y", &y1) && y1.IsNumber()) {
                originX = x0.GetNumber();
                originY = y0.GetNumber();
                stepX = x1.GetNumber() - originX;
                stepY = y1.GetNumber() - originY;
                haveStep = true;
            }
        }
        logger::debug("ListRows[{}]: origin=({},{}) step=({},{}) haveStep={}", a_tag, originX, originY, stepX, stepY,
            haveStep);

        // BSScrollingList assigns these once at list creation, so duplicates
        // never get them and mouse input dies past the original clip count.
        // Sharing the closures is safe - they resolve _parent and itemIndex
        // from the calling clip.
        RE::GFxValue onRollOver, onPress, onPressAux;
        const bool   haveOnRollOver = entry0.GetMember("onRollOver", &onRollOver);
        const bool   haveOnPress = entry0.GetMember("onPress", &onPress);
        const bool   haveOnPressAux = entry0.GetMember("onPressAux", &onPressAux);

        for (std::uint32_t i = curClips; i < a_needed; ++i) {
            // Always Entry0, never the previous duplicate: a clip made by
            // duplicateMovieClip has no children until the next frame, so
            // duplicating one in turn copies an empty shell and the row
            // renders as glyph boxes.
            RE::GFxValue src;
            if (!a_list.GetMember("Entry0", &src))
                break;

            const std::string  newName = "Entry" + std::to_string(i);
            const RE::GFxValue dupArgs[2] = { RE::GFxValue(newName.c_str()), RE::GFxValue(kDepthBase + i) };
            src.Invoke("duplicateMovieClip", nullptr, dupArgs, 2);

            RE::GFxValue clip;
            if (!a_list.GetMember(newName.c_str(), &clip) || !clip.IsObject()) {
                logger::warn("ListRows[{}]: duplicate '{}' not found after creation", a_tag, newName);
                continue;
            }

            clip.SetMember("clipIndex", RE::GFxValue(static_cast<double>(i)));
            if (haveStep) {
                clip.SetMember("_x", RE::GFxValue(originX + stepX * i));
                clip.SetMember("_y", RE::GFxValue(originY + stepY * i));
            }
            if (haveOnRollOver)
                clip.SetMember("onRollOver", onRollOver);
            if (haveOnPress)
                clip.SetMember("onPress", onPress);
            if (haveOnPressAux)
                clip.SetMember("onPressAux", onPressAux);

            if (a_setup)
                a_setup(clip);
        }

        a_list.SetMember("iMaxItemsShown", RE::GFxValue(static_cast<double>(a_needed)));
    }
}
