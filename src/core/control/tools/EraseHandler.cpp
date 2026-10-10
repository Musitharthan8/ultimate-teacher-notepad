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
/// Pointer samples per eraser radius along a swept path (see EraseHandler::erase)
constexpr double SAMPLES_PER_RADIUS = 4.0;
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
    const xoj::util::Rectangle<double> eraserRect{
            std::min(start.x, x) - halfEraserSize, std::min(start.y, y) - halfEraserSize,
            std::abs(x - start.x) + 2 * halfEraserSize, std::abs(y - start.y) + 2 * halfEraserSize};

    Range rerenderRange;
    Layer* l = page->getSelectedLayer();
    std::vector<Stroke*> candidates;
    // Removing whole strokes invalidates the layer's iterators.
    for (Element* e: xoj::refElementContainer(l->getElements())) {
        if (e->getType() == ELEMENT_STROKE && e->getBoundingBox().intersects(eraserRect)) {
            candidates.push_back(static_cast<Stroke*>(e));
        }
    }
    for (Stroke* stroke: candidates) {
        for (size_t i = 1; i <= steps; ++i) {
            const double t = static_cast<double>(i) / steps;
            eraseStroke(l, stroke, start.x + (x - start.x) * t, start.y + (y - start.y) * t, sampleRadius,
                        rerenderRange);
            if (handler->getEraserType() == ERASER_TYPE_DELETE_STROKE && l->indexOf(stroke) == -1) {
                break;
            }
        }
    }

    if (!rerenderRange.empty()) {
        this->view->rerenderRange(rerenderRange);
    }
}

namespace {
/// Centreline knots of a stroke with the half width of its ink at each point (pressure-aware, like Stroke::distanceTo)
std::vector<utn::eraser::Knot> knotsOf(const Stroke& s) {
    std::vector<utn::eraser::Knot> knots;
    knots.reserve(s.getPointVector().size());
    for (const Point& p: s.getPointVector()) {
        const double width = p.z == Point::NO_PRESSURE ? s.getWidth() : p.z;
        knots.push_back(utn::eraser::Knot{utn::eraser::Vec{p.x, p.y}, 0.5 * width});
    }
    return knots;
}
}  // namespace

void EraseHandler::eraseStroke(Layer* l, Stroke* s, double x, double y, double radius, Range& range) {
    ErasableStroke* erasable = s->getErasable();
    if (!erasable) {
        if (this->handler->getEraserType() == ERASER_TYPE_DELETE_STROKE) {
            // Whole-stroke mode: the eraser is a disc, so hit it with the exact circle test
            if (!utn::eraser::circleHitsStroke(utn::eraser::Vec{x, y}, radius, knotsOf(*s))) {
                // The stroke's ink does not reach the eraser disc
                return;
            }

            // delete the entire stroke
            this->doc->lock();
            auto [stroke, pos] = l->removeElement(s);
            this->doc->unlock();

            if (pos == -1) {
                return;
            }
            range = range.unite(Range(s->getBoundingBox()));

            // removed the if statement - this prevents us from putting multiple elements into a
            // stroke erase operation, but it also prevents the crashing and layer issues!
            if (!this->eraseDeleteUndoAction) {
                auto eraseDel = std::make_unique<DeleteUndoAction>(this->page, true);
                // Todo check dangerous: this->eraseDeleteUndoAction could be a dangling reference
                this->eraseDeleteUndoAction = eraseDel.get();
                this->undo->addUndoAction(std::move(eraseDel));
            }

            this->eraseDeleteUndoAction->addElement(l, std::move(stroke), pos);
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
                auto eraseUndo = std::make_unique<EraseUndoAction>(this->page);
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

void EraseHandler::finalize() {
    previousPoint.reset();
    if (this->eraseUndoAction) {
        std::unique_lock<Document> lock(*doc);
        this->eraseUndoAction->finalize();
        this->eraseUndoAction = nullptr;
    } else if (this->eraseDeleteUndoAction) {
        this->eraseDeleteUndoAction = nullptr;
    }
}
