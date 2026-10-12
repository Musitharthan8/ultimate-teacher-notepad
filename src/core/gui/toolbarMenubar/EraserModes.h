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
        {N_("Ink eraser"), N_("Erase parts of handwritten strokes"), ERASER_TYPE_DEFAULT},
        {N_("Stroke eraser"), N_("Remove an entire handwritten stroke"), ERASER_TYPE_DELETE_STROKE},
        {N_("Object eraser"), N_("Remove an entire text box, Answer Box or picture. Ink stays."), ERASER_TYPE_DELETE_OBJECT},
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
