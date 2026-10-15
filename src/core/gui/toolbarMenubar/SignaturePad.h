/* Ultimate Teacher Notepad, based on Xournal++ GPLv2+. */
#pragma once

#include <vector>
#include <cairo.h>

namespace utn {
// Inserted signatures use the existing embedded image format and undo path.
struct SignaturePad {
    struct Point { double x; double y; };
    std::vector<std::vector<Point>> strokes;
    bool drawing = false;
};

inline void paintSignature(cairo_t* cr, const SignaturePad& pad) {
    cairo_set_source_rgba(cr, 0.05, 0.05, 0.05, 1);
    cairo_set_line_width(cr, 2.5);
    cairo_set_line_cap(cr, CAIRO_LINE_CAP_ROUND);
    cairo_set_line_join(cr, CAIRO_LINE_JOIN_ROUND);
    for (const auto& stroke: pad.strokes) {
        if (stroke.empty()) continue;
        cairo_move_to(cr, stroke.front().x, stroke.front().y);
        for (const auto& point: stroke) cairo_line_to(cr, point.x, point.y);
        // A tap should still leave a dot.
        cairo_line_to(cr, stroke.back().x + 0.01, stroke.back().y);
        cairo_stroke(cr);
    }
}
}  // namespace utn
