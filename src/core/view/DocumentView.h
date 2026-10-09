/*
 * Xournal++
 *
 * Paints a page to a cairo context
 *
 * @author Xournal++ Team
 * https://github.com/xournalpp/xournalpp
 *
 * @license GNU GPLv2 or later
 */

#pragma once

#include <cairo.h>  // for cairo_t

#include <functional>  // for function

#include "model/PageRef.h"  // for ConstPageRef
#include "util/ElementRange.h"
#include "view/background/BackgroundFlags.h"

class PdfCache;
class Layer;

namespace xoj::view {
struct BackgroundFlags;
};

class DocumentView {
public:
    DocumentView() = default;
    virtual ~DocumentView() = default;

public:
    /**
     * Draw the full page, usually you would like to call this method
     * @param page The page to draw
     * @param cr Draw to thgis context
     * @param dontRenderEditingStroke false to draw currently drawing stroke
     * @param flags show/hide various background components
     */
    void drawPage(ConstPageRef page, cairo_t* cr, bool dontRenderEditingStroke,
                  xoj::view::BackgroundFlags flags = xoj::view::BACKGROUND_SHOW_ALL);

    /**
     * Only draws the prescribed layers of the given page, regardless of the layer's current visibility.
     * @param layerRange Range of layers to draw
     * @param page The page to draw
     * @param cr Draw to this context
     * @param dontRenderEditingStroke false to draw currently drawing stroke
     * @param flags show/hide various background components
     */
    void drawLayersOfPage(const LayerRangeVector& layerRange, ConstPageRef page, cairo_t* cr,
                          bool dontRenderEditingStroke,
                          xoj::view::BackgroundFlags flags = xoj::view::BACKGROUND_SHOW_ALL);

    /**
     * Mark stroke with Audio
     */
    void setMarkAudioStroke(bool markAudioStroke);

    /**
     * UTN: draw hidden answers layers faintly at this opacity (0 = not at all, the default).
     * Only the teacher's own canvas sets it, and only while students watch a separate display.
     */
    void setHiddenAnswersOpacity(double opacity);

    /**
     * UTN: replace the usual "layer is visible" rule, e.g. with utn::visibleToStudents for Student View.
     * The filter sees every layer; it alone decides what drawPage draws.
     */
    void setLayerFilter(std::function<bool(const Layer&)> filter);

    // API for special drawing, usually you won't call this methods
public:
    void setPdfCache(PdfCache* cache);

    /**
     * Drawing first step
     * @param page The page to draw
     * @param cr Draw to thgis context
     * @param dontRenderEditingStroke false to draw currently drawing stroke
     */
    void initDrawing(ConstPageRef page, cairo_t* cr, bool dontRenderEditingStroke);

    /**
     * Draw the background
     */
    void drawBackground(xoj::view::BackgroundFlags bgFlags) const;

    /**
     * Draw background if there is no background shown, like in GIMP etc.
     */
    void drawTransparentBackgroundPattern();

    /**
     * Last step in drawing
     */
    void finializeDrawing();

private:
    cairo_t* cr = nullptr;
    ConstPageRef page = nullptr;
    PdfCache* pdfCache = nullptr;
    bool dontRenderEditingStroke = false;
    bool markAudioStroke = false;
    double hiddenAnswersOpacity = 0.0;
    std::function<bool(const Layer&)> layerFilter;

};
