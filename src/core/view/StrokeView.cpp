#include "StrokeView.h"

#include <algorithm>  // for max
#include <cmath>      // for ceil

#include <glib.h>  // for g_warning

#include "model/Stroke.h"    // for Stroke, StrokeTool::HIGHLIGHTER
#include "util/Assert.h"     // for xoj_assert
#include "util/Color.h"      // for cairo_set_source_rgbi
#include "util/Rectangle.h"  // for Rectangle
#include "view/Mask.h"       // for Mask
#include "view/View.h"       // for Context, OPACITY_NO_AUDIO, view

#include "ErasableStrokeView.h"  // for ErasableStrokeView
#include "StrokeViewHelper.h"
#include "filesystem.h"  // for path

class ErasableStroke;

using xoj::util::Rectangle;
using namespace xoj::view;

StrokeView::StrokeView(const Stroke* s): s(s) {}

void StrokeView::draw(const Context& ctx) const {

    if (s->getPointCount() < 2) {
        // Should not happen
        g_warning("View::StrokeView::draw empty stroke...");
        return;
    }

    const bool highlighter = s->getToolType() == StrokeTool::HIGHLIGHTER;
    const bool filledHighlighter = highlighter && s->getFill() != -1;
    const bool drawTranslucent = ctx.fadeOutNonAudio && s->getAudioFilename().empty();
    // Image targets can subtract capsules directly from a bounded alpha mask.
    // Repeated vector clip intersections otherwise grow expensive on long,
    // curved eraser gestures. Keep vector clipping for PDF/vector targets.
    cairo_matrix_t targetMatrix;
    cairo_get_matrix(ctx.cr, &targetMatrix);
    // Selection previews may reflect or rotate the context. The existing Mask
    // helper requires positive diagonal scaling; use vector clips otherwise.
    const bool rasterErasure = targetMatrix.xx > 0 && targetMatrix.yy > 0 &&
            targetMatrix.xy == 0 && targetMatrix.yx == 0 && !s->getErasures().empty() &&
            cairo_surface_get_type(cairo_get_target(ctx.cr)) == CAIRO_SURFACE_TYPE_IMAGE;
    const bool useMask = (!ctx.noColor && filledHighlighter) || drawTranslucent || rasterErasure;

    if (ctx.showCurrentEdition && filledHighlighter && s->getErasable() != nullptr) {
        // Currently being erased filled highlighter strokes need a special treatment
        ErasableStrokeView erasableStrokeView(*s->getErasable());
        erasableStrokeView.paintFilledHighlighter(ctx.cr);
        return;
    }

    // The mask will be colorblind
    const bool noColor = ctx.noColor || useMask;

    xoj::util::CairoSaveGuard saveGuard(ctx.cr);

    if (!rasterErasure) utn::eraser::clipErasures(ctx.cr, s->getErasures(), s->getBoundingBox());

    Mask mask;

    // If not using a mask, draw directly onto the given cairo context
    cairo_t* cr = ctx.cr;

    if (useMask) {
        /**
         * To avoid visual glitches when different translucent cairo_stroke are painted,
         * they are painted without colors to a mask which will in turn be blitted (see below)
         */

        /**
         * Infer the zoom level from context.
         */
        cairo_matrix_t matrix;
        cairo_get_matrix(ctx.cr, &matrix);
        // We assume the matrix is diagonal (i.e. only scaling, no rotation)
        xoj_assert(matrix.xy == 0 && matrix.yx == 0);

        const double zoom = std::max(matrix.xx, matrix.yy);

        /**
         * Create a mask tailored to the stroke's bounding box
         */
        Range extent(s->getBoundingBox());
        if (rasterErasure) {
            double x1, y1, x2, y2;
            cairo_clip_extents(ctx.cr, &x1, &y1, &x2, &y2);
            extent = extent.intersect(Range(x1, y1, x2, y2));
            if (!extent.isValid()) return;
        }
        mask = Mask(cairo_get_target(ctx.cr), extent, zoom);
        cr = mask.get();
    }

    cairo_set_line_join(cr, CAIRO_LINE_JOIN_ROUND);
    cairo_set_line_cap(cr, CAIRO_LINE_CAP[s->getStrokeCapStyle()]);

    if (auto fill = s->getFill(); fill != -1) {
        /**
         * Paint the filling
         */
        if (noColor) {
            // Painting on an Alpha-only mask. No need for colors.
            if (filledHighlighter) {
                cairo_set_source_rgba(cr, 1, 1, 1, 1);
            } else {
                cairo_set_source_rgba(cr, 1, 1, 1, static_cast<double>(fill) / 255.0);
            }
        } else {
            Util::cairo_set_source_rgbi(cr, s->getColor(), static_cast<double>(fill) / 255.0);
        }
        cairo_set_operator(cr, useMask ? CAIRO_OPERATOR_SOURCE : CAIRO_OPERATOR_OVER);

        if (ErasableStroke* erasable = s->getErasable(); erasable != nullptr && ctx.showCurrentEdition) {
            // don't render erasable for previews
            ErasableStrokeView erasableStrokeView(*erasable);
            erasableStrokeView.drawFilling(cr);
        } else {
            StrokeViewHelper::pathToCairo(cr, s->getPointVector());
            cairo_fill(cr);
        }
    }

    /**
     * Picking the colors and alpha values, and the operator
     */
    if (noColor) {
        /**
         * No color: painting onto an alpha mask. Colors will be applied upon blitting.
         */
        cairo_set_source_rgba(cr, 1, 1, 1, 1);
        cairo_set_operator(cr, !useMask && !s->getErasures().empty() ? CAIRO_OPERATOR_OVER : CAIRO_OPERATOR_SOURCE);
    } else if (highlighter) {
        /**
         * Highlighter without filling.
         */
        Util::cairo_set_source_rgbi(cr, s->getColor(), OPACITY_HIGHLIGHTER);
        cairo_set_operator(cr, CAIRO_OPERATOR_MULTIPLY);
    } else {
        /**
         * Normal pen
         */
        Util::cairo_set_source_rgbi(cr, s->getColor());
        // SOURCE with a nonrectangular subtractive clip can clear earlier PDF
        // page content. Masked opaque ink can safely composite with OVER.
        cairo_set_operator(cr, !useMask && !s->getErasures().empty() ? CAIRO_OPERATOR_OVER : CAIRO_OPERATOR_SOURCE);
    }

    if (ErasableStroke* erasable = s->getErasable(); erasable != nullptr && ctx.showCurrentEdition) {
        // don't render erasable for previews
        ErasableStrokeView erasableStrokeView(*erasable);
        erasableStrokeView.draw(cr);
    } else if (s->hasPressure() && !highlighter) {
        StrokeViewHelper::drawWithPressure(cr, s->getPointVector(), s->getLineStyle());
    } else {
        StrokeViewHelper::drawNoPressure(cr, s->getPointVector(), s->getWidth(), s->getLineStyle());
    }

    if (useMask) {
        if (rasterErasure) {
            cairo_set_operator(cr, CAIRO_OPERATOR_DEST_OUT);
            cairo_set_source_rgba(cr, 1, 1, 1, 1);
            for (const auto& erasure: s->getErasures()) {
                cairo_new_path(cr);
                utn::eraser::appendErasurePath(cr, erasure);
                cairo_fill(cr);
            }
        }
        /**
         * Blit the mask onto the target cairo context.
         */

        /**
         * Opacity for the mask's content: the base value depends on the tool:
         * Pen                     : 1
         * Highlighter (no filling): OPACITY_HIGHLIGHTER
         * Highlighter (filled)    : s->getFill() / 255
         */
        double groupAlpha =
                highlighter && !ctx.noColor ? (filledHighlighter ? static_cast<double>(s->getFill()) / 255.0 : OPACITY_HIGHLIGHTER) :
                              1.0;

        // If the stroke has no audio attached, we draw it (even more) translucent
        if (drawTranslucent) {
            groupAlpha *= OPACITY_NO_AUDIO;
            groupAlpha = std::max(MINIMAL_ALPHA, groupAlpha);
        }

        // Blit the mask onto the given cairo context
        cairo_set_operator(ctx.cr, highlighter ? CAIRO_OPERATOR_MULTIPLY : CAIRO_OPERATOR_OVER);

        Util::cairo_set_source_rgbi(ctx.cr, s->getColor(), groupAlpha);

        mask.blitTo(ctx.cr);
    }
}
