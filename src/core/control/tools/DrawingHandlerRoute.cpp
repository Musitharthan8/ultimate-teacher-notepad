/*
 * Ultimate Teacher Notepad
 *
 * Based on Xournal++ GPLv2+
 */

#include "DrawingHandlerRoute.h"

#include "control/ToolEnums.h"    // for ToolType, DrawingType
#include "control/ToolHandler.h"  // for ToolHandler

auto drawingHandlerRoute(const ToolHandler& tools) -> DrawingHandlerRoute {
    const ToolType tool = tools.getToolType();

    if (tool == TOOL_ERASER) {
        // Whiteout erasing is drawn as a white stroke; other erasers use the eraser path
        return tools.getEraserType() == ERASER_TYPE_WHITEOUT ? DrawingHandlerRoute::Stroke : DrawingHandlerRoute::None;
    }
    if (tool != TOOL_PEN && tool != TOOL_HIGHLIGHTER) {
        return DrawingHandlerRoute::None;
    }

    switch (tools.getDrawingType()) {
        case DRAWING_TYPE_LINE:
            return DrawingHandlerRoute::Ruler;
        case DRAWING_TYPE_RECTANGLE:
            return DrawingHandlerRoute::Rectangle;
        case DRAWING_TYPE_ELLIPSE:
            return DrawingHandlerRoute::Ellipse;
        case DRAWING_TYPE_ARROW:
            return DrawingHandlerRoute::Arrow;
        case DRAWING_TYPE_DOUBLE_ARROW:
            return DrawingHandlerRoute::DoubleArrow;
        case DRAWING_TYPE_COORDINATE_SYSTEM:
            return DrawingHandlerRoute::CoordinateSystem;
        case DRAWING_TYPE_SPLINE:
            return DrawingHandlerRoute::Spline;
        default:
            // Freehand and the shape recogniser both start a StrokeHandler
            return DrawingHandlerRoute::Stroke;
    }
}
