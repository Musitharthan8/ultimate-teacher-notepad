/*
 * Ultimate Teacher Notepad
 *
 * Responsive layout decisions of the main window, kept free of GTK so they can be tested at any logical width.
 * Widths are GTK logical pixels (already divided by the display scale factor), not device pixels.
 *
 * Based on Xournal++ GPLv2+
 */

#pragma once

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
}  // namespace utn::layout
