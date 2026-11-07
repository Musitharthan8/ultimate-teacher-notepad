/*
 * Ultimate Teacher Notepad
 *
 * Student View viewport: how the page is placed in the projector window. GTK-free, so it is unit tested.
 *
 * The Student View used to draw the whole page scaled to fit, with no way to magnify it. The viewport keeps its own
 * zoom, independent of the teacher's canvas (the teacher may work zoomed in while the class sees the full page, or the
 * other way round). Modes: fit page (the default), fit width, or a custom magnification.
 *
 * Based on Xournal++ GPLv2+
 */

#pragma once

#include <algorithm>
#include <cmath>

namespace utn {

class StudentViewport {
public:
    enum class Mode { FitPage, FitWidth, Custom };

    /// Where the page lands in the window: scale (window pixels per page unit) and the page's top-left in pixels
    struct Layout {
        double scale = 1.0;
        double offsetX = 0.0;
        double offsetY = 0.0;
    };

    static constexpr double MAX_MAGNIFICATION = 8.0;  ///< relative to fit page
    static constexpr double BUTTON_STEP = 1.1;        ///< one Zoom in / Zoom out step

    Mode getMode() const { return mode; }
    bool isFit() const { return mode != Mode::Custom; }

    void setFitPage() {
        mode = Mode::FitPage;
        originX = originY = 0.0;
    }
    void setFitWidth() {
        mode = Mode::FitWidth;
        originX = originY = 0.0;
    }

    /// Scroll back to the top of the page, e.g. after another page is selected
    void scrollToTop() { originX = originY = 0.0; }

    /// Resolve the placement for a window of width x height showing a page of pageW x pageH. Clamps the scroll position.
    Layout resolve(double width, double height, double pageW, double pageH) {
        Layout layout;
        if (width <= 0 || height <= 0 || pageW <= 0 || pageH <= 0) {
            return layout;
        }
        const double fitPage = std::min(width / pageW, height / pageH);
        switch (mode) {
            case Mode::FitPage:
                layout.scale = fitPage;
                break;
            case Mode::FitWidth:
                layout.scale = width / pageW;
                break;
            case Mode::Custom:
                layout.scale = fitPage * std::clamp(magnification, 1.0, MAX_MAGNIFICATION);
                break;
        }
        const double contentW = pageW * layout.scale;
        const double contentH = pageH * layout.scale;
        if (contentW <= width + 1e-6) {
            originX = 0.0;
            layout.offsetX = 0.5 * (width - contentW);
        } else {
            originX = std::clamp(originX, 0.0, (contentW - width) / layout.scale);
            layout.offsetX = -originX * layout.scale;
        }
        if (contentH <= height + 1e-6) {
            originY = 0.0;
            layout.offsetY = 0.5 * (height - contentH);
        } else {
            originY = std::clamp(originY, 0.0, (contentH - height) / layout.scale);
            layout.offsetY = -originY * layout.scale;
        }
        return layout;
    }

    /// Multiply the zoom by @p factor, keeping the page point under (anchorX, anchorY) (window pixels) where it is.
    /// Never zooms out past fit page.
    void zoomBy(double factor, double anchorX, double anchorY, double width, double height, double pageW,
                double pageH) {
        if (width <= 0 || height <= 0 || pageW <= 0 || pageH <= 0 || !(factor > 0)) {
            return;
        }
        const Layout before = resolve(width, height, pageW, pageH);
        const double fitPage = std::min(width / pageW, height / pageH);
        const double newScale = std::clamp(before.scale * factor, fitPage, fitPage * MAX_MAGNIFICATION);
        const double pageX = (anchorX - before.offsetX) / before.scale;
        const double pageY = (anchorY - before.offsetY) / before.scale;
        magnification = newScale / fitPage;
        mode = Mode::Custom;
        originX = pageX - anchorX / newScale;
        originY = pageY - anchorY / newScale;
        // A centred axis (content smaller than the window) is handled by resolve()
        resolve(width, height, pageW, pageH);
    }

    /// Scroll by window pixels (positive moves the view right / down)
    void panBy(double dxPixels, double dyPixels, double width, double height, double pageW, double pageH) {
        const Layout layout = resolve(width, height, pageW, pageH);
        originX += dxPixels / layout.scale;
        originY += dyPixels / layout.scale;
        resolve(width, height, pageW, pageH);
    }

private:
    Mode mode = Mode::FitPage;
    double magnification = 1.0;  ///< Custom mode: scale relative to fit page
    double originX = 0.0;        ///< page units at the window's left edge (only when wider than the window)
    double originY = 0.0;        ///< page units at the window's top edge (only when taller than the window)
};

}  // namespace utn
