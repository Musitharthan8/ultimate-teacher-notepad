#pragma once

#include <cairo.h>
#include <vector>
#include "util/Matrix.h"
#include "util/Point.h"
#include "util/Rectangle.h"

namespace utn::eraser {
// A swept circular eraser in document units. The affine transform follows
// subsequent selection transforms, including non-uniform resizing.
struct StrokeErasure {
    xoj::util::Point<double> start{};
    xoj::util::Point<double> end{};
    double radius = 0;
    xoj::util::Matrix transform{};
};

void appendErasurePath(cairo_t* cr, const StrokeErasure& erasure);
void clipErasures(cairo_t* cr, const std::vector<StrokeErasure>& erasures,
                  const xoj::util::Rectangle<double>& bounds);
} // namespace utn::eraser
