/*
 * Ultimate Teacher Notepad
 *
 * Plain typography for Feedback comments
 *
 * Based on Xournal++ GPLv2+
 */

#include "FeedbackTypography.h"

#include <pango/pango.h>

namespace utn {

XojFont feedbackFont(const XojFont& font) {
    PangoFontDescription* desc = pango_font_description_from_string(font.asString().c_str());
    pango_font_description_set_weight(desc, PANGO_WEIGHT_NORMAL);
    pango_font_description_set_style(desc, PANGO_STYLE_NORMAL);
    gchar* value = pango_font_description_to_string(desc);
    XojFont plain(value);
    g_free(value);
    pango_font_description_free(desc);
    return plain;
}

}  // namespace utn
