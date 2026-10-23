/*
 * Ultimate Teacher Notepad
 *
 * One source for the visible Eraser modes in the contextual panel.
 * The entries mirror the saved eraser enum values and toolbar mode menu.
 *
 * Based on Xournal++ GPLv2+
 */

#pragma once

#include <array>

#include "control/ToolEnums.h"
#include "util/i18n.h"  // for N_

namespace utn {

struct EraserMode {
    const char* label;   ///< untranslated
    const char* hint;    ///< untranslated
    EraserType type;
};

inline constexpr std::array<EraserMode, 4> ERASER_MODES{{
        {N_("Eraser (pen, highlighter, shapes)"), N_("Everyday eraser: rubs out parts of pen ink, highlighter and drawn shapes"), ERASER_TYPE_DEFAULT},
        {N_("Whole stroke"), N_("Remove an entire pen, highlighter or shape stroke in one touch"), ERASER_TYPE_DELETE_STROKE},
        {N_("Delete object"), N_("Deliberately remove a whole text box, Answer Box or picture. Ink stays. Page-sized pictures are not deleted by sweeping."), ERASER_TYPE_DELETE_OBJECT},
        {N_("Whiteout"), N_("Cover ink with white rather than deleting it"), ERASER_TYPE_WHITEOUT},
}};

inline constexpr int eraserModeIndex(EraserType type) {
    for (int i = 0; i < static_cast<int>(ERASER_MODES.size()); ++i) {
        if (ERASER_MODES[static_cast<size_t>(i)].type == type) {
            return i;
        }
    }
    return 0;  // NONE / unknown: default mode
}

}  // namespace utn
