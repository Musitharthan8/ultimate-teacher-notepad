/* UTN diagnostic: original code, GPLv2+. Not part of the installed application. */
#include <cmath>
#include <cstdint>
#include <iostream>
#include <memory>
#include <string>
#include <vector>

#include <cairo.h>
#include "model/Stroke.h"
#include "model/eraser/ErasableStroke.h"
#include "util/Range.h"
#include "util/SmallVector.h"
#include "view/StrokeView.h"
#include "view/View.h"

namespace {
struct Surface {
    cairo_surface_t* surface;
    cairo_t* cr;
    explicit Surface(int zoom):
            surface(cairo_image_surface_create(CAIRO_FORMAT_ARGB32, 160 * zoom, 100 * zoom)),
            cr(cairo_create(surface)) {
        cairo_scale(cr, zoom, zoom);
    }
    ~Surface() { cairo_destroy(cr); cairo_surface_destroy(surface); }
    Surface(const Surface&) = delete;
    Surface& operator=(const Surface&) = delete;
    unsigned alpha(int x, int y) const {
        const auto* row = cairo_image_surface_get_data(surface) + y * cairo_image_surface_get_stride(surface);
        return reinterpret_cast<const std::uint32_t*>(row)[x] >> 24;
    }
};

void render(Surface& image, const Stroke& stroke) {
    xoj::view::StrokeView(&stroke).draw(xoj::view::Context::createDefault(image.cr));
    cairo_surface_flush(image.surface);
}

bool probe(const std::string& prefix, const char* name, double width, double radius, double cy, int zoom) {
    Stroke stroke;
    stroke.setWidth(width);
    stroke.setFill(-1);
    stroke.setToolType(StrokeTool::PEN);
    stroke.addPoint({20, 50});
    stroke.addPoint({140, 50});
    const Point center(80, cy);
    Range changed;
    ErasableStroke erased(stroke);
    erased.beginErasure(stroke.intersectWithEraserDisc(center, radius), changed);
    auto remaining = erased.getStrokes();

    Surface before(zoom), actual(zoom), reference(zoom);
    render(before, stroke);
    for (const auto& part: remaining) render(actual, *part);
    render(reference, stroke);
    // Local raster oracle for the intended visible result, not a proposed XOPP representation.
    cairo_set_operator(reference.cr, CAIRO_OPERATOR_DEST_OUT);
    cairo_set_source_rgba(reference.cr, 0, 0, 0, 1);
    cairo_arc(reference.cr, 80, cy, radius, 0, 2 * std::acos(-1.0));
    cairo_fill(reference.cr);
    cairo_surface_flush(reference.surface);

    std::size_t lostOutside = 0, retainedInside = 0, originalInk = 0;
    for (int y = 0; y < 100 * zoom; ++y) {
        for (int x = 0; x < 160 * zoom; ++x) {
            if (before.alpha(x, y) < 200) continue;
            ++originalInk;
            const double distance = std::hypot((x + 0.5) / zoom - 80, (y + 0.5) / zoom - cy);
            // Exclude one device pixel at the circle edge to avoid antialias boundary ambiguity.
            if (distance > radius + 1.0 / zoom && actual.alpha(x, y) < 50) ++lostOutside;
            if (distance < radius - 1.0 / zoom && actual.alpha(x, y) > 200) ++retainedInside;
        }
    }
    bool written = true;
    for (const auto& item: std::vector<std::pair<const char*, Surface*>>{
                 {"before", &before}, {"actual", &actual}, {"reference", &reference}}) {
        const auto path = prefix + "-" + name + "-z" + std::to_string(zoom) + "-" + item.first + ".png";
        written = cairo_surface_write_to_png(item.second->surface, path.c_str()) == CAIRO_STATUS_SUCCESS && written;
    }
    auto* preview = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, 480 * zoom, 120 * zoom);
    auto* previewCr = cairo_create(preview);
    cairo_set_source_rgb(previewCr, 1, 1, 1);
    cairo_paint(previewCr);
    int panel = 0;
    for (const auto& item: std::vector<std::pair<const char*, Surface*>>{
                 {"Original", &before}, {"UTN centreline trim", &actual}, {"Visible-disc reference", &reference}}) {
        cairo_set_source_surface(previewCr, item.second->surface, panel * 160 * zoom, 20 * zoom);
        cairo_paint(previewCr);
        cairo_set_source_rgb(previewCr, 0.1, 0.1, 0.1);
        cairo_set_font_size(previewCr, 10 * zoom);
        cairo_move_to(previewCr, (panel * 160 + 4) * zoom, 14 * zoom);
        cairo_show_text(previewCr, item.first);
        cairo_set_source_rgb(previewCr, 0.9, 0.1, 0.1);
        cairo_set_line_width(previewCr, zoom * 0.5);
        cairo_arc(previewCr, (panel * 160 + 80) * zoom, (cy + 20) * zoom, radius * zoom, 0, 2 * std::acos(-1.0));
        cairo_stroke(previewCr);
        ++panel;
    }
    const auto previewPath = prefix + "-" + name + "-z" + std::to_string(zoom) + "-comparison.png";
    written = cairo_surface_write_to_png(preview, previewPath.c_str()) == CAIRO_STATUS_SUCCESS && written;
    cairo_destroy(previewCr);
    cairo_surface_destroy(preview);
    std::cout << "{\"case\":\"" << name << "\",\"zoom\":" << zoom
              << ",\"width\":" << width << ",\"radius\":" << radius
              << ",\"source_ink_pixels\":" << originalInk
              << ",\"lost_outside_disc_pixels\":" << lostOutside
              << ",\"retained_inside_disc_pixels\":" << retainedInside
              << ",\"remaining_strokes\":" << remaining.size() << "}\n";
    return written;
}
}  // namespace

int main(int argc, char** argv) {
    if (argc != 2) {
        std::cerr << "Usage: utn-eraser-visual-probe OUTPUT_PREFIX (parent directory must exist)\n";
        return 2;
    }
    bool ok = true;
    for (int zoom: {1, 2, 4}) {
        ok = probe(argv[1], "thin", 2, 3, 50, zoom) && ok;
        ok = probe(argv[1], "thick-center", 20, 3, 50, zoom) && ok;
        ok = probe(argv[1], "thick-edge", 20, 3, 59, zoom) && ok;
    }
    return ok ? 0 : 1;
}
