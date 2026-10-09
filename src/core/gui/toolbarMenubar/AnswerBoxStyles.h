/*
 * Ultimate Teacher Notepad
 *
 * One table of Answer Box styles, used by the properties bar and the marking comment command.
 *
 * Based on Xournal++ GPLv2+
 */

#pragma once

#include <array>

#include "control/ToolHandler.h"  // for ToolHandler
#include "util/Color.h"
#include "util/i18n.h"  // for N_

namespace utn {

struct AnswerBoxStyle {
    const char* label;  ///< untranslated; wrap in _() when displayed
    Color text;
    Color background;
    Color border;
};

inline constexpr AnswerBoxStyle MODEL_ANSWER_STYLE{N_("Model answer"), Color{45U, 45U, 45U, 255U},
                                                   Color{255U, 248U, 214U, 230U}, Color{180U, 140U, 20U, 255U}};
inline constexpr AnswerBoxStyle MARKING_COMMENT_STYLE{N_("Marking comment"), Color{160U, 35U, 35U, 255U},
                                                      Color{255U, 248U, 214U, 230U}, Color{160U, 35U, 35U, 255U}};

inline constexpr std::array<AnswerBoxStyle, 5> ANSWER_BOX_STYLES{{
        MODEL_ANSWER_STYLE,
        {N_("Definition"), Color{45U, 45U, 45U, 255U}, Color{224U, 240U, 255U, 230U}, Color{50U, 110U, 180U, 255U}},
        {N_("Warning"), Color{45U, 45U, 45U, 255U}, Color{255U, 228U, 232U, 235U}, Color{190U, 55U, 70U, 255U}},
        {N_("Note"), Color{45U, 45U, 45U, 255U}, Color{232U, 247U, 232U, 230U}, Color{60U, 135U, 75U, 255U}},
        MARKING_COMMENT_STYLE,
}};

/// Apply a style's colours with the standard border, padding and corners
inline void applyAnswerBoxStyle(ToolHandler& tools, const AnswerBoxStyle& style) {
    tools.setAnswerBoxTextColor(style.text);
    tools.setAnswerBoxBackgroundColor(style.background);
    tools.setAnswerBoxBorderColor(style.border);
    tools.setAnswerBoxBorderWidth(1.2);
    tools.setAnswerBoxPadding(6.0);
    tools.setAnswerBoxCornerRadius(5.0);
}

}  // namespace utn
