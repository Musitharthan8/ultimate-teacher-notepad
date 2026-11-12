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

    /// A rectangle of the page, in page units
    struct Region {
        double x = 0.0, y = 0.0, width = 0.0, height = 0.0;
    };

    /**
     * Follow Teacher: show the part of the page the teacher's window shows. The region is scaled to fill the window as
     * far as it fits both ways and centred, so a teacher looking at the whole page gives plain fit page, and a teacher
     * scrolled down a zoomed-in page gives the same part of the page. An empty or whole-page region is fit page.
     */
    void showRegion(const Region& region, double width, double height, double pageW, double pageH) {
        if (width <= 0 || height <= 0 || pageW <= 0 || pageH <= 0 || !(region.width > 0) || !(region.height > 0)) {
            setFitPage();
            return;
        }
        const double fitPage = std::min(width / pageW, height / pageH);
        const double wanted = std::min(width / region.width, height / region.height);
        const double zoom = std::clamp(wanted / fitPage, 1.0, MAX_MAGNIFICATION);
        if (zoom < 1.0 + 1e-3) {
            setFitPage();
            return;
        }
        mode = Mode::Custom;
        magnification = zoom;
        const double scale = fitPage * zoom;
        // The page point at the centre of the region goes to the centre of the window; resolve() clamps to the page
        originX = region.x + 0.5 * region.width - 0.5 * width / scale;
        originY = region.y + 0.5 * region.height - 0.5 * height / scale;
        resolve(width, height, pageW, pageH);
    }

    /**
     * The part of a page that a scrolled and zoomed window shows, in page units. @p viewX/@p viewY/@p viewWidth/
     * @p viewHeight are the window's visible rectangle in the document layout (pixels); @p pageX/@p pageY the page's
     * top-left in the same layout; @p zoom the layout's pixels per page unit. False when none of the page is visible.
     */
    static bool visibleRegionOfPage(double viewX, double viewY, double viewWidth, double viewHeight, double pageX,
                                    double pageY, double zoom, double pageW, double pageH, Region& out) {
        if (!(zoom > 0) || viewWidth <= 0 || viewHeight <= 0 || pageW <= 0 || pageH <= 0) {
            return false;
        }
        const double left = std::max(0.0, (viewX - pageX) / zoom);
        const double top = std::max(0.0, (viewY - pageY) / zoom);
        const double right = std::min(pageW, (viewX + viewWidth - pageX) / zoom);
        const double bottom = std::min(pageH, (viewY + viewHeight - pageY) / zoom);
        if (!(right > left) || !(bottom > top)) {
            return false;
        }
        out = {left, top, right - left, bottom - top};
        return true;
    }

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
