#include "StrokeErasure.h"
#include <cmath>

namespace utn::eraser {
void appendErasurePath(cairo_t* cr, const StrokeErasure& erasure) {
    cairo_save(cr);
    erasure.transform.transformCairo(cr);
    const double angle = std::atan2(erasure.end.y - erasure.start.y, erasure.end.x - erasure.start.x);
    constexpr double pi = 3.14159265358979323846;
    cairo_new_sub_path(cr);
    cairo_arc(cr, erasure.end.x, erasure.end.y, erasure.radius, angle - pi / 2, angle + pi / 2);
    cairo_arc(cr, erasure.start.x, erasure.start.y, erasure.radius, angle + pi / 2, angle + 3 * pi / 2);
    cairo_close_path(cr);
    cairo_restore(cr);
}

void clipErasures(cairo_t* cr, const std::vector<StrokeErasure>& erasures,
                  const xoj::util::Rectangle<double>& bounds) {
    const auto rule = cairo_get_fill_rule(cr);
    // Intersect complements separately: one even-odd path containing several
    // overlapping capsules would wrongly restore their overlapping erased ink.
    for (const auto& erasure: erasures) {
        cairo_new_path(cr);
        cairo_rectangle(cr, bounds.x - 1, bounds.y - 1, bounds.width + 2, bounds.height + 2);
        appendErasurePath(cr, erasure);
        cairo_set_fill_rule(cr, CAIRO_FILL_RULE_EVEN_ODD);
        cairo_clip(cr);
    }
    cairo_set_fill_rule(cr, rule);
}
} // namespace utn::eraser
