#include "PresentationOverlay.h"

#include <algorithm>

#include "util/raii/CairoWrappers.h"  // for CairoSaveGuard

namespace xoj::view {

void paintPresentationOverlay(cairo_t* cr, bool spotlight, double pageWidth, double pageHeight, double pointerX,
                              double pointerY, double zoom, double curtainAlpha) {
    xoj::util::CairoSaveGuard guard(cr);
    const double safeZoom = zoom > 0.0 ? zoom : 1.0;

    if (spotlight) {
        const double radius = SPOTLIGHT_RADIUS_PX / safeZoom;

        cairo_set_fill_rule(cr, CAIRO_FILL_RULE_EVEN_ODD);
        cairo_rectangle(cr, 0.0, 0.0, pageWidth, pageHeight);
        cairo_arc(cr, pointerX, pointerY, radius, 0.0, 2.0 * 3.14159265358979323846);

        cairo_set_source_rgba(cr, 0.0, 0.0, 0.0, SPOTLIGHT_SHADE_ALPHA);
        cairo_fill(cr);

        cairo_arc(cr, pointerX, pointerY, radius, 0.0, 2.0 * 3.14159265358979323846);
        cairo_set_source_rgba(cr, 1.0, 1.0, 1.0, 0.65);
        cairo_set_line_width(cr, 2.0 / safeZoom);
        cairo_stroke(cr);
    } else {
        const double curtainY = std::clamp(pointerY, 0.0, pageHeight);

        cairo_rectangle(cr, 0.0, curtainY, pageWidth, pageHeight - curtainY);
        cairo_set_source_rgba(cr, 0.0, 0.0, 0.0, curtainAlpha);
        cairo_fill(cr);

        cairo_move_to(cr, 0.0, curtainY);
        cairo_line_to(cr, pageWidth, curtainY);
        cairo_set_source_rgba(cr, 1.0, 1.0, 1.0, 0.75);
        cairo_set_line_width(cr, 2.0 / safeZoom);
        cairo_stroke(cr);
    }
}

}  // namespace xoj::view
