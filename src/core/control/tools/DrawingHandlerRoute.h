/*
 * Ultimate Teacher Notepad
 *
 * Chooses which page input handler a press starts with the current tool state.
 *
 * Based on Xournal++ GPLv2+
 */

#pragma once

class ToolHandler;

/// The input handler a page press creates for drawing tools.
enum class DrawingHandlerRoute {
    None,  ///< Not a drawing press (eraser, selection, text, ...); handled elsewhere
    Stroke,
    Spline,
    Ruler,
    Rectangle,
    Ellipse,
    Arrow,
    DoubleArrow,
    CoordinateSystem,
};

/**
 * @brief Single decision point for drawing-handler creation on a page press.
 *
 * Uses ToolHandler::getDrawingType(), so the UTN teacher tool policy applies: a highlighter in a teacher layout
 * always routes to Stroke, never to a shape handler.
 */
DrawingHandlerRoute drawingHandlerRoute(const ToolHandler& tools);
