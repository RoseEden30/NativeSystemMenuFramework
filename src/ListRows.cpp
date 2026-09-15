#include "ListRows.h"

#include "Debug.h"
#include "GFx.h"

#include <cmath>

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

    // Filling in ListScrollbar hands sizing, moving and hiding back to
    // BSScrollingList, as for the lists that ship with one.
    void EnsureScrollbar(RE::GFxValue& a_list)
    {
        RE::GFxValue shownV, maxScrollV, row, rowW, rowH, rowY, rowX;
        if (!a_list.GetMember("iListItemsShown", &shownV) || !shownV.IsNumber() || shownV.GetNumber() < 1.0 ||
            !a_list.GetMember("iMaxScrollPosition", &maxScrollV) || !maxScrollV.IsNumber() ||
            !a_list.GetMember("Entry0", &row) || !row.IsObject() ||
            !row.GetMember("_x", &rowX) || !rowX.IsNumber() || !row.GetMember("_y", &rowY) || !rowY.IsNumber() ||
            !row.GetMember("_width", &rowW) || !rowW.IsNumber() ||
            !row.GetMember("_height", &rowH) || !rowH.IsNumber())
            return;

        RE::GFxValue bar;
        if (!a_list.GetMember("ListScrollbar", &bar) || !bar.IsObject()) {
            // Not SettingsScrollbar: that one is the slider inside a row,
            // and its thumb has no scaling grid.
            const RE::GFxValue attach[3] = { RE::GFxValue("JournalScrollBar"),
                RE::GFxValue("__nsmf_scrollbar"), RE::GFxValue(22000.0) };
            if (!a_list.Invoke("attachMovie", &bar, attach, 3) || !bar.IsObject()) {
                // A replacer interface may not export it.
                static bool loggedOnce = false;
                if (!loggedOnce) {
                    loggedOnce = true;
                    logger::warn("ListRows: no JournalScrollBar in this interface - lists keep their arrows");
                }
                return;
            }

            a_list.SetMember("ListScrollbar", bar);

            // What BSScrollingList::onLoad does for the lists that come
            // with a scrollbar already attached.
            bar.SetMember("position", RE::GFxValue(0.0));
            const RE::GFxValue listen[3] = { RE::GFxValue("scroll"), a_list, RE::GFxValue("onScroll") };
            bar.Invoke("addEventListener", nullptr, listen, 3);
            logger::debug("ListRows: scrollbar attached");
        }

        // Only once the bar exists, or a list would lose both.
        RE::GFxValue up, down;
        if (a_list.GetMember("ScrollUp", &up) && up.IsObject())
            GFx::SetIfChanged(up, "_visible", false);
        if (a_list.GetMember("ScrollDown", &down) && down.IsObject())
            GFx::SetIfChanged(down, "_visible", false);

        // Only InvalidateData calls SetScrollbarVisibility, and it ran
        // before this bar existed.
        GFx::SetIfChanged(bar, "_visible", maxScrollV.GetNumber() > 0.0);
        if (maxScrollV.GetNumber() > 0.0) {
            // A UIComponent reports no usable width until the frame after
            // attachMovie.
            RE::GFxValue barW;
            if (bar.GetMember("_width", &barW) && barW.IsNumber() && barW.GetNumber() > 0.0) {
                // The border frames the list at a fixed width; a row's own
                // is its content's, so it shifts with the label.
                double     edge = rowW.GetNumber();
                RE::GFxValue border, borderW;
                if (a_list.GetMember("border", &border) && border.IsObject() &&
                    border.GetMember("_width", &borderW) && borderW.IsNumber() && borderW.GetNumber() > 0.0)
                    edge = borderW.GetNumber();

                GFx::SetIfChanged(bar, "_x", rowX.GetNumber() + edge, 0.5);
                GFx::SetIfChanged(bar, "_y", rowY.GetNumber(), 0.5);

                // setSize, not _height, which would stretch the arrows too.
                RE::GFxValue sizedH;
                const auto   height = rowH.GetNumber() * shownV.GetNumber();
                if (!bar.GetMember("__height", &sizedH) || !sizedH.IsNumber() ||
                    std::abs(sizedH.GetNumber() - height) > 0.5) {
                    const RE::GFxValue size[2] = { barW, RE::GFxValue(height) };
                    bar.Invoke("setSize", nullptr, size, 2);
                }
            }

            // pageSize is rows on screen, not iMaxItemsShown, which counts
            // clips - more are created than fit, and the thumb would come out
            // sized as if nothing scrolled. Read before writing:
            // setScrollProperties redraws the thumb on every call.
            RE::GFxValue pageSize, maxPosition;
            if (!bar.GetMember("pageSize", &pageSize) || !pageSize.IsNumber() ||
                !bar.GetMember("maxPosition", &maxPosition) || !maxPosition.IsNumber() ||
                pageSize.GetNumber() != shownV.GetNumber() || maxPosition.GetNumber() != maxScrollV.GetNumber()) {
                const RE::GFxValue props[3] = { shownV, RE::GFxValue(0.0), maxScrollV };
                bar.Invoke("setScrollProperties", nullptr, props, 3);
            }
        }

        Debug::LogScrollbarGeometry(a_list, bar, row);
    }
}
