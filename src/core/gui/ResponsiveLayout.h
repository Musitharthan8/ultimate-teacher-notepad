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

}  // namespace utn::layout
