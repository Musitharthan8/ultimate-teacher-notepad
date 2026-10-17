/*
 * Ultimate Teacher Notepad
 *
 * Audit 16 October 2026: Feedback › Good inherited the bold, italic, underline and strikethrough last used for text.
 *
 * Based on Xournal++ GPLv2+
 */

#include <gtest/gtest.h>
#include <pango/pango.h>

#include "control/tools/FeedbackTypography.h"

namespace {
struct Description {
    PangoFontDescription* desc;
    explicit Description(const XojFont& font): desc(pango_font_description_from_string(font.asString().c_str())) {}
    ~Description() { pango_font_description_free(desc); }
};
}  // namespace

TEST(FeedbackTypography, dropsBoldAndItalicButKeepsFamilyAndSize) {
    const XojFont styled("Sans Bold Italic 14");
    const XojFont plain = utn::feedbackFont(styled);

    Description d(plain);
    EXPECT_EQ(pango_font_description_get_weight(d.desc), PANGO_WEIGHT_NORMAL);
    EXPECT_EQ(pango_font_description_get_style(d.desc), PANGO_STYLE_NORMAL);
    EXPECT_STREQ(pango_font_description_get_family(d.desc), "Sans");
    EXPECT_DOUBLE_EQ(plain.getSize(), 14.0);
}

TEST(FeedbackTypography, plainFontIsUnchanged) {
    const XojFont regular("Serif 12");
    EXPECT_EQ(utn::feedbackFont(regular), regular);
}
