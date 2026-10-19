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

/// Pointer samples per eraser radius along a swept path (see EraseHandler::erase)
constexpr double SAMPLES_PER_RADIUS = 4.0;

// Match StrokeViewHelper/StrokeContour: pressure is the width of the outgoing
// segment, and the final point's pressure is unused. Highlighters render at the
// fallback width. Avoid rebuilding a knot vector at every pointer sample.
bool wholeStrokeTouchesDisc(const Stroke& stroke, utn::eraser::Vec centre, double radius) {
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
        return std::hypot(p.x - centre.x, p.y - centre.y) <= radius + halfWidth(p);
    }
    for (size_t i = 1; i < points.size(); ++i) {
        const Point& a = points[i - 1];
        const Point& b = points[i];
        const double reach = radius + halfWidth(a);
        if (utn::eraser::distancePointSegment(centre, {a.x, a.y}, {b.x, b.y}) <= reach) {
            return true;
        }
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
    const auto start = previousPoint.value_or(xoj::util::Point<double>{x, y});
    previousPoint = xoj::util::Point<double>{x, y};
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
        std::unique_lock<Document> lock(*doc);
        this->eraseUndoAction->finalize();
        this->eraseUndoAction = nullptr;
    } else if (this->eraseDeleteUndoAction) {
        this->eraseDeleteUndoAction = nullptr;
    }
}
