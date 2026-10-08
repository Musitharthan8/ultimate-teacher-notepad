/*
 * Ultimate Teacher Notepad
 *
 * Regressions for "Highlight draws arrows": tool transitions must decide which page input handler a press creates.
 * Based on Xournal++ GPLv2+.
 */
#include <gtest/gtest.h>

#include "control/ToolEnums.h"
#include "control/ToolHandler.h"
#include "control/UtnLayout.h"
#include "control/settings/SettingsEnums.h"
#include "control/tools/DrawingHandlerRoute.h"

namespace teacher_tool_test {

struct ShapeCase {
    DrawingType type;
    DrawingHandlerRoute route;
};

// Every shape the Shapes & Lines launcher, the Tools menu or Ctrl+1..8 can set.
const ShapeCase SHAPES[] = {
        {DRAWING_TYPE_ARROW, DrawingHandlerRoute::Arrow},
        {DRAWING_TYPE_DOUBLE_ARROW, DrawingHandlerRoute::DoubleArrow},
        {DRAWING_TYPE_RECTANGLE, DrawingHandlerRoute::Rectangle},
        {DRAWING_TYPE_ELLIPSE, DrawingHandlerRoute::Ellipse},
        {DRAWING_TYPE_LINE, DrawingHandlerRoute::Ruler},
        {DRAWING_TYPE_COORDINATE_SYSTEM, DrawingHandlerRoute::CoordinateSystem},
        {DRAWING_TYPE_SPLINE, DrawingHandlerRoute::Spline},
        {DRAWING_TYPE_SHAPE_RECOGNIZER, DrawingHandlerRoute::Stroke},
};

}  // namespace teacher_tool_test

using teacher_tool_test::ShapeCase;
using teacher_tool_test::SHAPES;

class TeacherShapeTransition: public ::testing::TestWithParam<ShapeCase> {};

namespace {
// Shapes & Lines: select Pen, then apply the shape (ToolTeachingKit::chooseDrawingTool)
void chooseShape(ToolHandler& tools, DrawingType type) {
    tools.selectTool(TOOL_PEN);
    tools.setDrawingType(type);
}

}  // namespace

TEST(UtnLayout, recognisesOnlyTeacherLayouts) {
    EXPECT_TRUE(utn::isTeacherLayout("UTN Teacher"));
    EXPECT_TRUE(utn::isTeacherLayout("UTN Teacher Tablet"));
    EXPECT_TRUE(utn::isTeacherLayout("UTN Marking"));
    EXPECT_FALSE(utn::isTeacherLayout("Xournal++"));
    EXPECT_FALSE(utn::isTeacherLayout("Tablet mode"));
    EXPECT_FALSE(utn::isTeacherLayout(""));
}

TEST_P(TeacherShapeTransition, shapeThenHighlightDrawsFreehandHighlight) {
    ToolHandler tools(nullptr, nullptr, nullptr);
    tools.setTeacherToolPolicy(true);

    chooseShape(tools, GetParam().type);
    ASSERT_EQ(drawingHandlerRoute(tools), GetParam().route);

    tools.selectTool(TOOL_HIGHLIGHTER);
    EXPECT_EQ(tools.getToolType(), TOOL_HIGHLIGHTER);
    EXPECT_EQ(tools.getDrawingType(), DRAWING_TYPE_DEFAULT);
    EXPECT_EQ(drawingHandlerRoute(tools), DrawingHandlerRoute::Stroke);
}

TEST_P(TeacherShapeTransition, shapeThenPenDrawsFreehand) {
    ToolHandler tools(nullptr, nullptr, nullptr);
    tools.setTeacherToolPolicy(true);

    chooseShape(tools, GetParam().type);
    tools.selectTool(TOOL_HIGHLIGHTER);
    tools.selectTool(TOOL_PEN);
    EXPECT_EQ(tools.getDrawingType(), DRAWING_TYPE_DEFAULT);
    EXPECT_EQ(drawingHandlerRoute(tools), DrawingHandlerRoute::Stroke);

    // Re-selecting Pen directly while it is in a shape mode also returns to handwriting
    chooseShape(tools, GetParam().type);
    tools.selectTool(TOOL_PEN);
    EXPECT_EQ(drawingHandlerRoute(tools), DrawingHandlerRoute::Stroke);
}

TEST_P(TeacherShapeTransition, shapeShortcutOnHighlightMovesToPen) {
    // Tools menu / Ctrl+1..8 apply to the toolbar tool; on Highlight this used to leave a hidden shape behind.
    ToolHandler tools(nullptr, nullptr, nullptr);
    tools.setTeacherToolPolicy(true);
    tools.selectTool(TOOL_HIGHLIGHTER);

    tools.setDrawingType(GetParam().type);
    EXPECT_EQ(tools.getToolType(), TOOL_PEN);
    EXPECT_EQ(drawingHandlerRoute(tools), GetParam().route);
    EXPECT_EQ(tools.getTool(TOOL_HIGHLIGHTER).getDrawingType(), DRAWING_TYPE_DEFAULT);

    tools.selectTool(TOOL_HIGHLIGHTER);
    EXPECT_EQ(drawingHandlerRoute(tools), DrawingHandlerRoute::Stroke);
}

TEST_P(TeacherShapeTransition, storedHighlighterShapeIsDroppedWhenTeacherLayoutLoads) {
    // A Classic layout or imported Xournal++ settings stored a shape on the highlighter.
    ToolHandler tools(nullptr, nullptr, nullptr);
    tools.selectTool(TOOL_HIGHLIGHTER);
    tools.setDrawingType(GetParam().type);
    ASSERT_EQ(drawingHandlerRoute(tools), GetParam().route);

    tools.setTeacherToolPolicy(true);
    EXPECT_EQ(drawingHandlerRoute(tools), DrawingHandlerRoute::Stroke);
    // The stored value is cleared too, so it is not saved back to settings and restored next launch
    EXPECT_EQ(tools.getTool(TOOL_HIGHLIGHTER).getDrawingType(), DRAWING_TYPE_DEFAULT);
}

TEST_P(TeacherShapeTransition, highlighterButtonToolNeverDrawsShape) {
    // Stylus/touch button tools receive the toolbar highlighter's stored shape through ButtonConfig's
    // "don't change" copy (applyNoChangeSettings) or an explicit button configuration.
    ToolHandler tools(nullptr, nullptr, nullptr);
    tools.resetButtonTool(TOOL_HIGHLIGHTER, Button::BUTTON_STYLUS_ONE);
    tools.setButtonDrawingType(GetParam().type, Button::BUTTON_STYLUS_ONE);
    ASSERT_TRUE(tools.pointActiveToolToButtonTool(Button::BUTTON_STYLUS_ONE));
    ASSERT_EQ(drawingHandlerRoute(tools), GetParam().route);
    ASSERT_TRUE(tools.pointActiveToolToToolbarTool());

    tools.setTeacherToolPolicy(true);
    tools.selectTool(TOOL_PEN);

    ASSERT_TRUE(tools.pointActiveToolToButtonTool(Button::BUTTON_STYLUS_ONE));
    EXPECT_EQ(tools.getToolType(), TOOL_HIGHLIGHTER);
    EXPECT_EQ(drawingHandlerRoute(tools), DrawingHandlerRoute::Stroke);
}

TEST_P(TeacherShapeTransition, classicLayoutKeepsUpstreamShapeBehaviour) {
    ToolHandler tools(nullptr, nullptr, nullptr);
    ASSERT_FALSE(tools.hasTeacherToolPolicy());

    // Highlighter shapes are a visible, intentional mode in Classic Xournal++ layouts
    tools.selectTool(TOOL_HIGHLIGHTER);
    tools.setDrawingType(GetParam().type);
    EXPECT_EQ(tools.getToolType(), TOOL_HIGHLIGHTER);
    EXPECT_EQ(drawingHandlerRoute(tools), GetParam().route);

    // Pen keeps its shape when re-selected, as upstream
    chooseShape(tools, GetParam().type);
    tools.selectTool(TOOL_HIGHLIGHTER);
    tools.selectTool(TOOL_PEN);
    EXPECT_EQ(drawingHandlerRoute(tools), GetParam().route);
}

INSTANTIATE_TEST_SUITE_P(AllShapes, TeacherShapeTransition, ::testing::ValuesIn(SHAPES));

TEST(TeacherToolTransitions, shapesStillWorkAfterHighlight) {
    ToolHandler tools(nullptr, nullptr, nullptr);
    tools.setTeacherToolPolicy(true);
    tools.selectTool(TOOL_HIGHLIGHTER);
    chooseShape(tools, DRAWING_TYPE_ARROW);
    EXPECT_EQ(tools.getToolType(), TOOL_PEN);
    EXPECT_EQ(drawingHandlerRoute(tools), DrawingHandlerRoute::Arrow);

    // "Back to Pen"
    tools.setDrawingType(DRAWING_TYPE_DEFAULT);
    EXPECT_EQ(drawingHandlerRoute(tools), DrawingHandlerRoute::Stroke);
}

TEST(TeacherToolTransitions, smartMarkupModeIsIndependentOfShapes) {
    ToolHandler tools(nullptr, nullptr, nullptr);
    tools.setTeacherToolPolicy(true);
    chooseShape(tools, DRAWING_TYPE_DOUBLE_ARROW);
    tools.selectTool(TOOL_HIGHLIGHTER);
    tools.setSmartHighlighterEnabled(true);
    tools.setSmartHighlighterSnapMode(SmartHighlighterSnapMode::Underline);
    EXPECT_EQ(drawingHandlerRoute(tools), DrawingHandlerRoute::Stroke);

    // Leaving Highlight clears Smart Markup, but not Pen's freehand state
    tools.selectTool(TOOL_PEN);
    EXPECT_FALSE(tools.isSmartHighlighterEnabled());
    EXPECT_EQ(drawingHandlerRoute(tools), DrawingHandlerRoute::Stroke);
}

TEST(TeacherToolTransitions, nonDrawingToolsDoNotRouteToDrawingHandlers) {
    ToolHandler tools(nullptr, nullptr, nullptr);
    tools.setTeacherToolPolicy(true);
    chooseShape(tools, DRAWING_TYPE_RECTANGLE);
    for (ToolType type: {TOOL_TEXT, TOOL_SELECT_RECT, TOOL_SELECT_OBJECT, TOOL_HAND, TOOL_IMAGE}) {
        tools.selectTool(type);
        EXPECT_EQ(drawingHandlerRoute(tools), DrawingHandlerRoute::None) << toolTypeToString(type);
    }
    tools.selectTool(TOOL_ERASER);
    tools.setEraserType(ERASER_TYPE_DEFAULT);
    EXPECT_EQ(drawingHandlerRoute(tools), DrawingHandlerRoute::None);
    tools.setEraserType(ERASER_TYPE_WHITEOUT);
    EXPECT_EQ(drawingHandlerRoute(tools), DrawingHandlerRoute::Stroke);
}
