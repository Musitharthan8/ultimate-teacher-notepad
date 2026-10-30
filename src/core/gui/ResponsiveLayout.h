/*
 * Ultimate Teacher Notepad
 *
 * Responsive layout decisions of the main window, kept free of GTK so they can be tested at any logical width.
 * Widths are GTK logical pixels (already divided by the display scale factor), not device pixels.
 *
 * Based on Xournal++ GPLv2+
 */

#pragma once

#include <cstddef>
#include <string_view>
#include <vector>

namespace utn::layout {

// The Paper & Ink teacher rail is a fixed-width icon card. Keep this in sync with
// MainWindow's GtkScrolledWindow sizing and the corresponding GTK layout test.
inline constexpr int TEACHER_RAIL_WIDTH = 60;

/// Enter the compact (icon-only rail) layout below this width
constexpr int COMPACT_ENTER_BELOW = 1080;
/// Leave the compact layout only above this width. The gap avoids flicker while the window edge is dragged.
constexpr int COMPACT_LEAVE_ABOVE = 1120;
/// Smaller allocations are the placeholder size before the window is shown and carry no information
constexpr int MIN_MEANINGFUL_WIDTH = 200;

/// Values of the "Tool labels" setting
enum ToolLabelMode : int { LABELS_AUTOMATIC = 0, LABELS_SHOW = 1, LABELS_HIDE = 2 };

/// @return The compact state to use after the window has been allocated `width`, given the state before
constexpr bool nextCompactLayout(bool wasCompact, int width) {
    if (!wasCompact && width >= MIN_MEANINGFUL_WIDTH && width < COMPACT_ENTER_BELOW) {
        return true;
    }
    if (wasCompact && width > COMPACT_LEAVE_ABOVE) {
        return false;
    }
    return wasCompact;
}

/// @return Whether the captions under the rail icons are shown. Hidden captions stay available as tooltips and
/// accessible names.
constexpr bool railLabelsVisible(int toolLabelMode, bool compact) {
    return toolLabelMode == LABELS_SHOW || (toolLabelMode == LABELS_AUTOMATIC && !compact);
}

/// How much the interface explains itself. Both modes offer exactly the same tools and actions; only the amount of
/// guidance differs, never what is possible, and never the document, Student View privacy or annotation behaviour.
/// Custom is whatever the teacher had before choosing a mode, and what any later individual choice leaves behind.
enum class InterfaceMode { Custom, Beginner, Expert };

/// @return The "Tool labels" value a mode sets, or -1 when it sets nothing (Custom). Beginner names every tool, Expert
/// shows icons only; the names stay in tooltips and accessible names either way.
constexpr int toolLabelModeFor(InterfaceMode mode) {
    return mode == InterfaceMode::Beginner ? LABELS_SHOW : mode == InterfaceMode::Expert ? LABELS_HIDE : -1;
}

/// @return Whether the short explanations under the tool settings are shown. Expert drops them.
constexpr bool contextHintsShown(InterfaceMode mode) { return mode != InterfaceMode::Expert; }

/// @return The mode to keep after the teacher picks a "Tool labels" value by hand. A choice that agrees with the mode
/// keeps it; a different one is the teacher's own arrangement, so the mode becomes Custom instead of being overridden.
constexpr InterfaceMode modeAfterLabelChoice(InterfaceMode mode, int toolLabelMode) {
    return toolLabelModeFor(mode) == toolLabelMode ? mode : InterfaceMode::Custom;
}

/// The saved form. Anything unknown, including nothing saved, is Custom: an existing profile keeps what it had.
inline InterfaceMode parseInterfaceMode(std::string_view saved) {
    return saved == "beginner" ? InterfaceMode::Beginner :
           saved == "expert"   ? InterfaceMode::Expert :
                                 InterfaceMode::Custom;
}

constexpr const char* interfaceModeName(InterfaceMode mode) {
    return mode == InterfaceMode::Beginner ? "beginner" : mode == InterfaceMode::Expert ? "expert" : "custom";
}


/// The page comes first: below these widths the panels that sit beside it give way. Each panel has a lower threshold
/// to hide at and a higher one to return at, so dragging the window edge does not make it flicker.
/// Thumbnail sidebar: hidden below 1400 px, back above 1440 px.
constexpr int THUMBNAILS_HIDE_BELOW = 1400;
constexpr int THUMBNAILS_RETURN_ABOVE = 1440;
/// Settings inspector: folded to its "Show settings" handle below 960 px, back above 1000 px.
constexpr int INSPECTOR_FOLD_BELOW = 960;
constexpr int INSPECTOR_RETURN_ABOVE = 1000;

/// @return Whether the thumbnail sidebar should be out of the way after the window has been allocated `width`
constexpr bool nextThumbnailsAway(bool wereAway, int width) {
    if (!wereAway && width >= MIN_MEANINGFUL_WIDTH && width < THUMBNAILS_HIDE_BELOW) {
        return true;
    }
    if (wereAway && width > THUMBNAILS_RETURN_ABOVE) {
        return false;
    }
    return wereAway;
}

/// @return Whether the settings inspector should be folded after the window has been allocated `width`
constexpr bool nextInspectorFolded(bool wasFolded, int width) {
    if (!wasFolded && width >= MIN_MEANINGFUL_WIDTH && width < INSPECTOR_FOLD_BELOW) {
        return true;
    }
    if (wasFolded && width > INSPECTOR_RETURN_ABOVE) {
        return false;
    }
    return wasFolded;
}

/// The app bar is measured, never assumed: its requisition depends on the font, the display scale and the language, so
/// no width is safe to hard-code. When the bar asks for more than the window has, the least important buttons move into
/// the "More" menu, in the order given, until what is left plus the "More" button fits.
/// @param naturalWidth the width the bar asks for with every item shown and no "More" button
/// @param availableWidth the width the window gives it
/// @param movableWidths the widths of the items that may move, first to move first
/// @param moreButtonWidth the width of the "More" button, which only appears when something moves
/// @return how many of the movable items (from the front) move; 0 when the bar already fits, all of them when even
/// that is not enough (the caller then falls back to scrolling)
inline std::size_t appBarItemsToOverflow(int naturalWidth, int availableWidth, const std::vector<int>& movableWidths,
                                         int moreButtonWidth) {
    if (naturalWidth <= availableWidth) {
        return 0;
    }
    int saved = 0;
    for (std::size_t i = 0; i < movableWidths.size(); ++i) {
        saved += movableWidths[i];
        if (naturalWidth - saved + moreButtonWidth <= availableWidth) {
            return i + 1;
        }
    }
    return movableWidths.size();
}
}  // namespace utn::layout
