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

#include <optional>

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
    void eraseObject(Layer* l, Element* e, double x, double y, double radius, Range& range);

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

};
