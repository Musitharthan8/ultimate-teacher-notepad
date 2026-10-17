/*
 * Ultimate Teacher Notepad
 *
 * Ready-made Feedback comments ("Good", "Check your working") always use plain typography. They used to take the bold,
 * italic, underline and strikethrough last used for ordinary text, which placed a crossed-out "Good" on student work.
 *
 * Based on Xournal++ GPLv2+
 */

#pragma once

#include "model/Font.h"

namespace utn {

/// @p font's family and size with regular weight and upright style
XojFont feedbackFont(const XojFont& font);

}  // namespace utn
