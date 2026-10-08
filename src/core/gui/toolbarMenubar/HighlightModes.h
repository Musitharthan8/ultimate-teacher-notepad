/*
 * Ultimate Teacher Notepad
 *
 * One list of Highlight modes for the tool rail menu and the properties bar, so both show the same names
 * in the same order.
 *
 * Based on Xournal++ GPLv2+
 */

#pragma once

#include <array>
#include <optional>

#include "control/ToolHandler.h"  // for SmartHighlighterSnapMode
#include "util/i18n.h"            // for N_

namespace utn {

struct HighlightMode {
    const char* label;  ///< untranslated; wrap in _() when displayed
    const char* hint;   ///< untranslated tooltip
    std::optional<SmartHighlighterSnapMode> snap;  ///< nullopt = freehand
};

inline constexpr std::array<HighlightMode, 6> HIGHLIGHT_MODES{{
        {N_("Freehand"), N_("Highlight exactly where you draw"), std::nullopt},
        {N_("Straight line"), N_("Straighten roughly horizontal strokes"), SmartHighlighterSnapMode::Straight},
        {N_("Snap to word"), N_("Cover the PDF words under the stroke"), SmartHighlighterSnapMode::Word},
        {N_("Snap to line"), N_("Cover whole PDF text lines under the stroke"), SmartHighlighterSnapMode::Line},
        {N_("Underline"), N_("Underline the PDF text under the stroke"), SmartHighlighterSnapMode::Underline},
        {N_("Strikethrough"), N_("Strike through the PDF text under the stroke"), SmartHighlighterSnapMode::Strikethrough},
}};

/// Index into HIGHLIGHT_MODES for the current Highlight state
inline int highlightModeIndex(bool smartEnabled, SmartHighlighterSnapMode snap) {
    if (!smartEnabled) {
        return 0;
    }
    for (int i = 1; i < static_cast<int>(HIGHLIGHT_MODES.size()); ++i) {
        if (HIGHLIGHT_MODES[static_cast<size_t>(i)].snap == snap) {
            return i;
        }
    }
    return 0;
}

}  // namespace utn
