#include "EraseHandler.h"

#include <algorithm>
#include <cmath>

#include <memory>   // for make_unique, unique_ptr
#include <utility>  // for move
#include <vector>   // for vector

#include <gdk/gdk.h>  // for GdkRectangle
#include <glib.h>     // for gint

#include "control/ToolEnums.h"            // for ERASER_TYPE_DELETE_STROKE
#include "model/eraser/CircularEraser.h"  // for utn::eraser::circleHitsStroke
#include "control/ToolHandler.h"          // for ToolHandler
#include "control/UtnPerf.h"              // for utn::perf
#include "gui/LegacyRedrawable.h"         // for Redrawable
#include "model/Document.h"               // for Document
#include "model/Element.h"                // for Element, ELEMENT_STROKE
#include "model/Layer.h"                  // for Layer
#include "model/Stroke.h"                 // for Stroke
#include "model/XojPage.h"                // for XojPage
#include "model/eraser/ErasableStroke.h"  // for ErasableStroke
#include "undo/DeleteUndoAction.h"        // for DeleteUndoAction
#include "undo/EraseUndoAction.h"         // for EraseUndoAction
#include "undo/UndoRedoHandler.h"         // for UndoRedoHandler
#include "util/Range.h"                   // for Range
#include "util/SmallVector.h"             // for SmallVector
#include "view/StrokeView.h"

namespace {
/**
 * Whether the ink of a stroke can touch rect. The stroke's bounding box is padded by the fallback width only, so a
 * pressure-widened stroke can reach past it. The cheap box test runs first, and the pressure-aware test only runs
 * for strokes with pressure whose box misses.
 */
bool inkMayIntersect(const Stroke& s, const xoj::util::Rectangle<double>& rect) {
    const auto box = s.getBoundingBox();
    if (box.intersects(rect).has_value()) {
        return true;
    }
    if (!s.hasPressure()) {
        return false;
    }
    double maxHalfWidth = 0.5 * s.getWidth();
    for (const Point& p: s.getPointVector()) {
        if (p.z != Point::NO_PRESSURE) {
            maxHalfWidth = std::max(maxHalfWidth, 0.5 * p.z);
        }
    }
    const double extra = maxHalfWidth - 0.5 * s.getWidth();
    const xoj::util::Rectangle<double> padded{box.x - extra, box.y - extra, box.width + 2 * extra,
                                              box.height + 2 * extra};
    return padded.intersects(rect).has_value();
}

/// Whether an element covers at least half of the page area (see the Object eraser)
bool isPageSized(const xoj::util::Rectangle<double>& box, double pageWidth, double pageHeight) {
    return pageWidth > 0 && pageHeight > 0 && box.width * box.height >= 0.5 * pageWidth * pageHeight;
}

/// Pointer samples per eraser radius along a swept path (see EraseHandler::erase)
constexpr double SAMPLES_PER_RADIUS = 4.0;
/// How far, as a fraction of the radius, a new sweep may stick out of earlier erasures and still count as erased already
constexpr double REDUNDANT_PASS_TOLERANCE = 0.05;
bool alreadyErased(const Stroke& stroke, utn::eraser::Vec a, utn::eraser::Vec b, double radius);

bool maskedInkTouchesDisc(const Stroke& stroke, utn::eraser::Vec centre, double radius) {
    if (stroke.getErasures().empty()) return true;
    // Only masked candidates need this local visibility check. It also handles
    // holes formed by the union of overlapping, transformed erasures.
    constexpr int size = 128;
    auto* surface = cairo_image_surface_create(CAIRO_FORMAT_A8, size, size);
    if (cairo_surface_status(surface) != CAIRO_STATUS_SUCCESS) { cairo_surface_destroy(surface); return false; }
    auto* cr = cairo_create(surface);
    cairo_scale(cr, size / (2 * radius), size / (2 * radius));
    cairo_translate(cr, radius - centre.x, radius - centre.y);
    cairo_arc(cr, centre.x, centre.y, radius, 0, 2 * std::acos(-1.0));
    cairo_clip(cr);
    xoj::view::StrokeView(&stroke).draw(xoj::view::Context::createColorBlind(cr));
    cairo_surface_flush(surface);
    bool visible = false;
    const auto* data = cairo_image_surface_get_data(surface);
    const int stride = cairo_image_surface_get_stride(surface);
    for (int y = 0; y < size && !visible; ++y)
        for (int x = 0; x < size; ++x) if (data[y * stride + x]) { visible = true; break; }
    cairo_destroy(cr);
    cairo_surface_destroy(surface);
    return visible;
}

// Match StrokeViewHelper/StrokeContour: pressure is the width of the outgoing
// segment, and the final point's pressure is unused. Highlighters render at the
// fallback width. Avoid rebuilding a knot vector at every pointer sample.
bool wholeStrokeTouchesDisc(const Stroke& stroke, utn::eraser::Vec centre, double radius) {
    // A whole-stroke click inside an existing erased region cannot delete
    // surviving ink: the clicked disc is already transparent.
    const auto& points = stroke.getPointVector();
    if (points.empty()) {
        return false;
    }
    const bool usePressure = stroke.hasPressure() && stroke.getToolType() != StrokeTool::HIGHLIGHTER;
    auto halfWidth = [&stroke, usePressure](const Point& p) {
        return 0.5 * (!usePressure || p.z == Point::NO_PRESSURE ? stroke.getWidth() : p.z);
    };
    if (points.size() == 1) {
        const Point& p = points.front();
        return std::hypot(p.x - centre.x, p.y - centre.y) <= radius + halfWidth(p) &&
               !alreadyErased(stroke, centre, centre, radius) && maskedInkTouchesDisc(stroke, centre, radius);
    }
    for (size_t i = 1; i < points.size(); ++i) {
        const Point& a = points[i - 1];
        const Point& b = points[i];
        const double reach = radius + halfWidth(a);
        if (utn::eraser::distancePointSegment(centre, {a.x, a.y}, {b.x, b.y}) <= reach) {
            return !alreadyErased(stroke, centre, centre, radius) && maskedInkTouchesDisc(stroke, centre, radius);
        }
    }
    return false;
}

double segmentDistance(utn::eraser::Vec a, utn::eraser::Vec b, utn::eraser::Vec p, utn::eraser::Vec q) {
    auto cross = [](utn::eraser::Vec x, utn::eraser::Vec y, utn::eraser::Vec z) {
        return (y.x - x.x) * (z.y - x.y) - (y.y - x.y) * (z.x - x.x);
    };
    const double abp = cross(a, b, p), abq = cross(a, b, q);
    const double pqa = cross(p, q, a), pqb = cross(p, q, b);
    if (((abp < 0 && abq > 0) || (abp > 0 && abq < 0)) &&
        ((pqa < 0 && pqb > 0) || (pqa > 0 && pqb < 0))) return 0;
    return std::min({utn::eraser::distancePointSegment(a, p, q), utn::eraser::distancePointSegment(b, p, q),
                     utn::eraser::distancePointSegment(p, a, b), utn::eraser::distancePointSegment(q, a, b)});
}

bool sweptDiscTouchesStroke(const Stroke& stroke, utn::eraser::Vec a, utn::eraser::Vec b, double radius) {
    if (stroke.getFill() != -1) return true; // Broad phase bounds clip harmless masks outside a filled shape.
    const auto& points = stroke.getPointVector();
    const bool pressure = stroke.hasPressure() && stroke.getToolType() != StrokeTool::HIGHLIGHTER;
    for (size_t i = 1; i < points.size(); ++i) {
        const auto& p = points[i - 1];
        const auto& q = points[i];
        const double halfWidth = 0.5 * (pressure && p.z != Point::NO_PRESSURE ? p.z : stroke.getWidth());
        if (segmentDistance(a, b, {p.x, p.y}, {q.x, q.y}) <= radius + halfWidth) return true;
    }
    return false;
}

bool alreadyErased(const Stroke& stroke, utn::eraser::Vec a, utn::eraser::Vec b, double radius) {
    for (const auto& e: stroke.getErasures()) {
        const auto& m = e.transform;
        const double det = m.xx * m.yy - m.xy * m.yx;
        if (!std::isfinite(det) || std::abs(det) < 1e-14) continue;
        const auto inverse = m.inverse();
        const auto localA = inverse * xoj::util::Point<double>{a.x, a.y};
        const auto localB = inverse * xoj::util::Point<double>{b.x, b.y};
        // Largest singular value bounds the inverse-transformed disc, including
        // nonuniform resizing. Containment is conservative and zoom independent.
        const double sum = inverse.xx * inverse.xx + inverse.xy * inverse.xy + inverse.yx * inverse.yx + inverse.yy * inverse.yy;
        const double inverseDet = 1 / det;
        const double localRadius = radius * std::sqrt((sum + std::sqrt(std::max(0.0, sum * sum - 4 * inverseDet * inverseDet))) / 2);
        // Merged capsules (Stroke::addErasure) follow the pointer samples only to within ERASURE_MERGE_TOLERANCE of the
        // radius, and a second pass over the same ink is never pixel exact either, so it lands near, not on, an old
        // capsule's axis. Without slack here a repeated pass is never recognised as already erased and every scrub back
        // and forth adds capsules that cost a fill on each redraw. 5% of the radius is under a pixel for any usable eraser.
        const double slack = REDUNDANT_PASS_TOLERANCE * localRadius;
        if (e.radius + slack >= localRadius &&
            utn::eraser::distancePointSegment({localA.x, localA.y}, {e.start.x, e.start.y}, {e.end.x, e.end.y}) <= e.radius - localRadius + slack &&
            utn::eraser::distancePointSegment({localB.x, localB.y}, {e.start.x, e.start.y}, {e.end.x, e.end.y}) <= e.radius - localRadius + slack) return true;
    }
    return false;
}
}  // namespace

EraseHandler::EraseHandler(UndoRedoHandler* undo, Document* doc, const PageRef& page, ToolHandler* handler,
                           LegacyRedrawable* view):
        page(page),
        handler(handler),
        view(view),
        doc(doc),
        undo(undo),
        eraseDeleteUndoAction(nullptr),
        eraseUndoAction(nullptr),
        halfEraserSize(0) {}

EraseHandler::~EraseHandler() {
    if (this->eraseDeleteUndoAction || this->eraseUndoAction) {
        this->finalize();
    }
}

/**
 * Handle eraser event: "Delete Stroke" and "Standard", Whiteout is not handled here
 */
void EraseHandler::erase(double x, double y) {
    utn::perf::ScopedTimer eraseTimer(utn::perf::Metric::EraseEvent);
    if (!std::isfinite(x) || !std::isfinite(y)) {
        previousPoint.reset();
        return;
    }
    // A hidden answers layer can remain selected (for example if the page has
    // no ordinary layer). Never erase invisible content, regardless of mode.
    // Create no undo action here, so a hidden gesture cannot sweep into visible ink.
    // Forget the last point too: if the layer is revealed later in the same drag, the
    // next visible sample must start a new sweep, not join the pre-hide point to it.
    Layer* l = page->getSelectedLayer();
    if (!l || !l->isVisible()) {
        previousPoint.reset();
        return;
    }

    this->halfEraserSize = this->handler->getThickness();
    if (!std::isfinite(halfEraserSize) || halfEraserSize <= 0) { previousPoint.reset(); return; }
    const auto start = previousPoint.value_or(xoj::util::Point<double>{x, y});
    previousPoint = xoj::util::Point<double>{x, y};
    if (handler->getEraserType() == ERASER_TYPE_DEFAULT) {
        eraseWithMask(l, start.x, start.y, x, y, halfEraserSize);
        return;
    }
    const double distance = std::hypot(x - start.x, y - start.y);
    // Sample the pointer path densely. Between two samples a disc of radius R leaves a gap of width up to the spacing,
    // so the samples are at most R / SAMPLES_PER_RADIUS apart and each sample disc is enlarged to cover the gap
    // (see sampleRadius). Then the union of the samples contains the whole swept circle, with no gaps.
    const size_t steps =
            std::max<size_t>(1, static_cast<size_t>(std::ceil(distance / (halfEraserSize / SAMPLES_PER_RADIUS))));
    const double spacing = distance / static_cast<double>(steps);
    // A point of the swept path at distance a <= spacing / 2 from its nearest sample is within R of the swept circle
    // when sqrt(a^2 + d^2) <= sqrt(R^2 + (spacing / 2)^2), so the sample disc needs that radius. The over-erase is at
    // most sqrt(R^2 + (spacing / 2)^2) - R, which is below R / 100 for SAMPLES_PER_RADIUS = 4.
    const double sampleRadius = std::sqrt(halfEraserSize * halfEraserSize + 0.25 * spacing * spacing);
    // Rectangle::intersects requires positive overlap, whereas the circular hit test
    // intentionally includes tangency. Inflate this *broad-phase filter only* by a
    // tiny tolerance so a stroke touching exactly at the outer edge is not discarded.
    // Use the actual enlarged sample radius rather than the nominal cursor radius.
    const double filterRadius = sampleRadius + std::max(1e-8, sampleRadius * 1e-8);
    const xoj::util::Rectangle<double> eraserRect{
            std::min(start.x, x) - filterRadius, std::min(start.y, y) - filterRadius,
            std::abs(x - start.x) + 2 * filterRadius, std::abs(y - start.y) + 2 * filterRadius};

    Range rerenderRange;

    // A drag consists of multiple erase() events. Capture the pre-gesture ordering once,
    // before any whole-element removals. Sorting hits within an individual event is not
    // enough: the first event may have already shifted the indices seen by later events.
    const auto eraserType = handler->getEraserType();
    if ((eraserType == ERASER_TYPE_DELETE_STROKE || eraserType == ERASER_TYPE_DELETE_OBJECT) &&
        originalDeletePositions.empty() && !eraseDeleteUndoAction) {
        std::ptrdiff_t index = 0;
        for (const auto& element: l->getElements()) {
            originalDeletePositions.emplace(element.get(), index++);
        }
    }

    if (handler->getEraserType() == ERASER_TYPE_DELETE_OBJECT) {
        // Object eraser: removes text, images and other non-stroke elements. Strokes are never touched.
        {
            // Decide every hit first (a hit depends only on the object's own box and the pointer samples), then remove
            // from the highest index down. Each removal then happens at the element's original index, which is the
            // position DeleteUndoAction restores, so one undo step puts back every object where it was.
            std::vector<std::pair<Element::Index, Element*>> hits;
            for (Element* e: xoj::refElementContainer(l->getElements())) {
                if (e->getType() == ELEMENT_STROKE || !e->getBoundingBox().intersects(eraserRect).has_value()) {
                    continue;
                }
                const auto box = e->getBoundingBox();
                if (e->getType() == ELEMENT_IMAGE && isPageSized(box, page->getWidth(), page->getHeight())) {
                    // A scanned worksheet or full-page picture is background-like: a stray swipe must not delete it.
                    // It can still be removed deliberately by selecting it.
                    continue;
                }
                for (size_t i = 1; i <= steps; ++i) {
                    const double t = static_cast<double>(i) / steps;
                    const utn::eraser::Vec sample{start.x + (x - start.x) * t, start.y + (y - start.y) * t};
                    if (utn::eraser::circleTouchesRect(sample, sampleRadius, box.x, box.y, box.width, box.height)) {
                        hits.emplace_back(l->indexOf(e), e);
                        break;
                    }
                }
            }
            std::sort(hits.begin(), hits.end(), [](const auto& a, const auto& b) { return a.first > b.first; });
            for (const auto& hit: hits) {
                eraseObject(l, hit.second, rerenderRange);
            }
        }
    } else {
        std::vector<Stroke*> candidates;
        // Removing whole strokes invalidates the layer's iterators.
        for (Element* e: xoj::refElementContainer(l->getElements())) {
            if (e->getType() == ELEMENT_STROKE && inkMayIntersect(*static_cast<Stroke*>(e), eraserRect)) {
                candidates.push_back(static_cast<Stroke*>(e));
            }
        }
        if (handler->getEraserType() == ERASER_TYPE_DELETE_STROKE) {
            // Decide every whole-stroke hit before removing anything, then remove from the highest index down. Each
            // removal then happens at the stroke's original index, which is the position DeleteUndoAction restores.
            std::vector<std::pair<Element::Index, Stroke*>> hits;
            for (Stroke* stroke: candidates) {
                if (stroke->getErasable()) {
                    // Already edited by the partial eraser: keep the established path (in-place erasing, no removal)
                    for (size_t i = 1; i <= steps; ++i) {
                        const double t = static_cast<double>(i) / steps;
                        eraseStroke(l, stroke, start.x + (x - start.x) * t, start.y + (y - start.y) * t, sampleRadius,
                                    rerenderRange);
                    }
                    continue;
                }
                for (size_t i = 1; i <= steps; ++i) {
                    const double t = static_cast<double>(i) / steps;
                    const utn::eraser::Vec sample{start.x + (x - start.x) * t, start.y + (y - start.y) * t};
                    if (wholeStrokeTouchesDisc(*stroke, sample, sampleRadius)) {
                        hits.emplace_back(l->indexOf(stroke), stroke);
                        break;
                    }
                }
            }
            std::sort(hits.begin(), hits.end(), [](const auto& a, const auto& b) { return a.first > b.first; });
            for (const auto& hit: hits) {
                removeWholeStroke(l, hit.second, rerenderRange);
            }
        } else {
            for (Stroke* stroke: candidates) {
                for (size_t i = 1; i <= steps; ++i) {
                    const double t = static_cast<double>(i) / steps;
                    eraseStroke(l, stroke, start.x + (x - start.x) * t, start.y + (y - start.y) * t, sampleRadius,
                                rerenderRange);
                }
            }
        }
    }

    if (!rerenderRange.empty()) {
        this->view->rerenderRange(rerenderRange);
    }
}

void EraseHandler::eraseWithMask(Layer* layer, double startX, double startY, double endX, double endY, double radius) {
    if (!std::isfinite(radius) || radius <= 0) return;
    const utn::eraser::Vec a{startX, startY}, b{endX, endY};
    const xoj::util::Rectangle<double> bounds{std::min(startX, endX) - radius - 1e-8,
                                             std::min(startY, endY) - radius - 1e-8,
                                             std::abs(endX - startX) + 2 * radius + 2e-8,
                                             std::abs(endY - startY) + 2 * radius + 2e-8};
    if (originalDeletePositions.empty() && !eraseUndoAction) {
        Element::Index i = 0;
        for (const auto& e: layer->getElements()) originalDeletePositions.emplace(e.get(), i++);
    }
    std::vector<Stroke*> candidates;
    for (Element* e: xoj::refElementContainer(layer->getElements())) {
        if (e->getType() != ELEMENT_STROKE) continue;
        auto* stroke = static_cast<Stroke*>(e);
        if (inkMayIntersect(*stroke, bounds) && sweptDiscTouchesStroke(*stroke, a, b, radius) &&
            !alreadyErased(*stroke, a, b, radius)) candidates.push_back(stroke);
    }
    Range dirty;
    for (Stroke* stroke: candidates) {
        if (!eraseUndoAction) {
            auto action = std::make_unique<EraseUndoAction>(page, doc);
            eraseUndoAction = action.get();
            undo->addUndoAction(std::move(action));
        }
        const auto original = originalDeletePositions.find(stroke);
        const auto originalPos = original == originalDeletePositions.end() ? layer->indexOf(stroke) : original->second;
        const auto box = stroke->getBoundingBox();
        // Only the part of the stroke inside this event's capsule can change. Repainting the whole bounding box made
        // every pointer event rerender (and Student View redraw) the entire page-wide underline or circle it touched.
        if (const auto touched = box.intersects(bounds)) {
            dirty = dirty.unite(Range(*touched));
        }
        {
            std::unique_lock<Document> lock(*doc);
            stroke = eraseUndoAction->beginMaskedEdit(layer, stroke, originalPos);
            originalDeletePositions.emplace(stroke, originalPos);
            stroke->addErasure({{startX, startY}, {endX, endY}, radius, {}});
            // Only remove an element when this capsule provably covers its
            // entire bounds. Small holes keep the editable vector stroke.
            bool covers = true;
            for (double cx: {box.x, box.x + box.width}) for (double cy: {box.y, box.y + box.height}) {
                covers = covers && utn::eraser::distancePointSegment({cx, cy}, a, b) <= radius;
            }
            // Read before the stroke can be removed below
            utn::perf::record(utn::perf::Metric::ErasuresPerStroke, static_cast<double>(stroke->getErasures().size()));
            if (covers) {
                eraseUndoAction->removeEdited(stroke);
                auto removed = layer->removeElement(stroke);
                dirty = dirty.unite(Range(box));  // the whole stroke disappears
            }
        }
    }
    if (!dirty.empty()) view->rerenderRange(dirty);
}

void EraseHandler::eraseStroke(Layer* l, Stroke* s, double x, double y, double radius, Range& range) {
    ErasableStroke* erasable = s->getErasable();
    if (!erasable) {
        if (this->handler->getEraserType() == ERASER_TYPE_DELETE_STROKE) {
            // Whole-stroke mode: the eraser is a disc, so hit it with the exact circle test
            if (!wholeStrokeTouchesDisc(*s, utn::eraser::Vec{x, y}, radius)) {
                // The stroke's ink does not reach the eraser disc
                return;
            }

            removeWholeStroke(l, s, range);
        } else {  // Default eraser
            auto pos = l->indexOf(s);
            if (pos == -1) {
                return;
            }

            // Partial erasing: the eraser is a disc, and the ink of the stroke is removed where it touches that disc
            auto intersectionParameters = s->intersectWithEraserDisc(Point(x, y), radius);

            if (intersectionParameters.empty()) {
                // The stroke's ink does not touch the eraser disc
                return;
            }

            if (this->eraseUndoAction == nullptr) {
                auto eraseUndo = std::make_unique<EraseUndoAction>(this->page, this->doc);
                // Todo check dangerous: this->eraseDeleteUndoAction could be a dangling reference
                this->eraseUndoAction = eraseUndo.get();
                this->undo->addUndoAction(std::move(eraseUndo));
            }

            doc->lock();
            erasable = new ErasableStroke(*s);
            s->setErasable(erasable);
            doc->unlock();
            this->eraseUndoAction->addOriginal(l, s, pos);
            erasable->beginErasure(intersectionParameters, range);
        }
    } else {
        /**
         * This stroke has already been touched by the eraser
         * (Necessarily the default eraser)
         */
        auto pos = l->indexOf(s);
        if (pos == -1) {
            return;
        }
        erasable->erase(Point(x, y), radius, range);
    }
}

void EraseHandler::removeWholeStroke(Layer* l, Stroke* s, Range& range) {
    this->doc->lock();
    auto [stroke, pos] = l->removeElement(s);
    this->doc->unlock();
    if (pos == -1) {
        return;
    }
    range = range.unite(Range(s->getBoundingBox()));

    if (!this->eraseDeleteUndoAction) {
        auto eraseDel = std::make_unique<DeleteUndoAction>(this->page, true, this->doc);
        this->eraseDeleteUndoAction = eraseDel.get();
        this->undo->addUndoAction(std::move(eraseDel));
    }
    const auto original = originalDeletePositions.find(s);
    const auto undoPos = original == originalDeletePositions.end() ? pos : original->second;
    this->eraseDeleteUndoAction->addElement(l, std::move(stroke), undoPos);
}

void EraseHandler::eraseObject(Layer* l, Element* e, Range& range) {
    this->doc->lock();
    auto [element, pos] = l->removeElement(e);
    this->doc->unlock();
    if (pos == -1) {
        return;
    }
    range = range.unite(Range(element->getBoundingBox()));

    // One undo step per gesture, as for whole-stroke deletion
    if (!this->eraseDeleteUndoAction) {
        auto eraseDel = std::make_unique<DeleteUndoAction>(this->page, true, this->doc);
        this->eraseDeleteUndoAction = eraseDel.get();
        this->undo->addUndoAction(std::move(eraseDel));
    }
    const auto original = originalDeletePositions.find(e);
    const auto undoPos = original == originalDeletePositions.end() ? pos : original->second;
    this->eraseDeleteUndoAction->addElement(l, std::move(element), undoPos);
}

void EraseHandler::finalize() {
    previousPoint.reset();
    originalDeletePositions.clear();
    if (this->eraseUndoAction) {
        bool restructured = false;
        {
            std::unique_lock<Document> lock(*doc);
            restructured = this->eraseUndoAction->finalize();
        }
        this->eraseUndoAction = nullptr;
        // Every pointer event already repainted its own area. Rendering the whole page again at the end of each gesture
        // held the shared Document lock for a full page (0.65 s on a dense page in the stress test) just as the next
        // stroke needed the exclusive lock. Only a gesture that replaced elements needs it.
        if (restructured) {
            this->page->firePageChanged();
        }
    } else if (this->eraseDeleteUndoAction) {
        this->eraseDeleteUndoAction = nullptr;
    }
}
