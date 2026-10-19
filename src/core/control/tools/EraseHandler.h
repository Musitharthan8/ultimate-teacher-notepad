/*
 * Xournal++
 *
 * Handles the erase of stroke, in special split into different parts etc.
 *
 * @author Xournal++ Team
 * https://github.com/xournalpp/xournalpp
 *
 * @license GNU GPLv2 or later
 */

#pragma once

#include <cstddef>      // for ptrdiff_t
#include <optional>
#include <unordered_map>  // for unordered_map

#include "util/Point.h"

#include "model/PageRef.h"  // for PageRef

class DeleteUndoAction;
class Element;
class Document;
class EraseUndoAction;
class Layer;
class Range;
class LegacyRedrawable;
class Stroke;
class ToolHandler;
class UndoRedoHandler;

class EraseHandler {
public:
    EraseHandler(UndoRedoHandler* undo, Document* doc, const PageRef& page, ToolHandler* handler,
                 LegacyRedrawable* view);
    virtual ~EraseHandler();

public:
    void erase(double x, double y);
    void finalize();

private:
    void eraseStroke(Layer* l, Stroke* s, double x, double y, double radius, Range& range);
    void eraseObject(Layer* l, Element* e, Range& range);
    void removeWholeStroke(Layer* l, Stroke* s, Range& range);
    void eraseWithMask(Layer* layer, double startX, double startY, double endX, double endY, double radius);

private:
    PageRef page;
    ToolHandler* handler;
    LegacyRedrawable* view;
    Document* doc;
    UndoRedoHandler* undo;

    DeleteUndoAction* eraseDeleteUndoAction;
    EraseUndoAction* eraseUndoAction;

    double halfEraserSize;
    std::optional<xoj::util::Point<double>> previousPoint;

    // Original element indices at the start of a delete gesture. Erase() may be called
    // several times during a drag: later removals must not record shifted indices,
    // otherwise one Undo reorders surviving strokes/objects. Reset in finalize().
    std::unordered_map<const Element*, std::ptrdiff_t> originalDeletePositions;

};
