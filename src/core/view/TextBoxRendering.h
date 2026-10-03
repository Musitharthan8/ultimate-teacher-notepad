/*
 * Ultimate Teacher Notepad
 *
 * Answer box rendering helpers
 *
 * Based on Xournal++ GPLv2+
 */

#pragma once

#include <algorithm>
#include <cmath>

#include <cairo.h>

#include "model/Text.h"
#include "util/Color.h"
#include "util/Rectangle.h"

namespace xoj::view {

inline void appendRoundedRectangle(cairo_t* cr, const xoj::util::Rectangle<double>& box, double radius) {
    radius = std::clamp(radius, 0.0, 0.5 * std::min(box.width, box.height));

    if (radius <= 0.0) {
        cairo_rectangle(cr, box.x, box.y, box.width, box.height);
        return;
    }

    const double x2 = box.x + box.width;
    const double y2 = box.y + box.height;

    cairo_new_sub_path(cr);
    cairo_arc(cr, x2 - radius, box.y + radius, radius, -M_PI_2, 0);
    cairo_arc(cr, x2 - radius, y2 - radius, radius, 0, M_PI_2);
    cairo_arc(cr, box.x + radius, y2 - radius, radius, M_PI_2, M_PI);
    cairo_arc(cr, box.x + radius, box.y + radius, radius, M_PI, 3 * M_PI_2);
    cairo_close_path(cr);
}

inline void drawAnswerBox(cairo_t* cr, const Text* text, const xoj::util::Rectangle<double>& box) {
    if (!text->isBoxEnabled()) {
        return;
    }

    cairo_set_operator(cr, CAIRO_OPERATOR_OVER);
    appendRoundedRectangle(cr, box, text->getBoxCornerRadius());

    Util::cairo_set_source_argb(cr, text->getBoxBackgroundColor());
    cairo_fill_preserve(cr);

    if (text->getBoxBorderWidth() > 0.0 && text->getBoxBorderColor().alpha > 0U) {
        Util::cairo_set_source_argb(cr, text->getBoxBorderColor());
        cairo_set_line_width(cr, text->getBoxBorderWidth());
        cairo_stroke(cr);
    } else {
        cairo_new_path(cr);
    }
}

}  // namespace xoj::view
