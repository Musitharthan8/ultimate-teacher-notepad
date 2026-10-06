/*
 * Ultimate Teacher Notepad
 * Continuous erasure regressions, based on Xournal++ GPLv2+.
 */
#include <gtest/gtest.h>

#include "control/ToolHandler.h"
#include "control/tools/EraseHandler.h"
#include "control/tools/InputHandler.h"
#include "gui/LegacyRedrawable.h"
#include "model/Document.h"
#include "model/Layer.h"
#include "model/Stroke.h"
#include "model/XojPage.h"
#include "undo/UndoRedoHandler.h"

namespace {
class EraseTestView: public LegacyRedrawable {
public:
    unsigned redraws = 0;
    void repaintArea(double, double, double, double) const override {}
    void repaintPage() const override {}
    void rerenderPage(bool) override {}
    void rerenderRect(double, double, double, double) override { ++redraws; }
    GdkRGBA getSelectionColor() override { return {}; }
    void deleteViewBuffer() override {}
};

void addHighlight(const PageRef& page, double x1, double x2) {
    auto stroke = std::make_unique<Stroke>();
    stroke->setToolType(StrokeTool::HIGHLIGHTER);
    stroke->setWidth(3);
    stroke->addPoint(Point(x1, 50));
    stroke->addPoint(Point(x2, 50));
    page->getSelectedLayer()->addElement(std::move(stroke));
}
}  // namespace

TEST(EraseHandler, sweepsAcrossSparsePointerEvents) {
    auto page = std::make_shared<XojPage>(200, 200);
    addHighlight(page, 20, 180);
    Document document(nullptr);
    UndoRedoHandler undo(nullptr);
    ToolHandler tools(nullptr, nullptr, nullptr);
    tools.selectTool(TOOL_ERASER);
    EraseTestView view;
    EraseHandler eraser(&undo, &document, page, &tools, &view);
    eraser.erase(20, 50);
    eraser.erase(180, 50);
    eraser.finalize();
    EXPECT_TRUE(page->getSelectedLayer()->getElements().empty());
    EXPECT_TRUE(undo.canUndo());
    EXPECT_EQ(view.redraws, 2U);
}

TEST(EraseHandler, doesNotSweepBetweenSeparateGestures) {
    auto page = std::make_shared<XojPage>(200, 200);
    addHighlight(page, 80, 120);
    Document document(nullptr);
    UndoRedoHandler undo(nullptr);
    ToolHandler tools(nullptr, nullptr, nullptr);
    tools.selectTool(TOOL_ERASER);
    EraseTestView view;
    EraseHandler eraser(&undo, &document, page, &tools, &view);
    eraser.erase(20, 50);
    eraser.finalize();
    eraser.erase(180, 50);
    eraser.finalize();
    EXPECT_EQ(page->getSelectedLayer()->getElements().size(), 1U);
    EXPECT_FALSE(undo.canUndo());
}

TEST(TeacherTools, leavingMarkupClearsSpecialMode) {
    ToolHandler tools(nullptr, nullptr, nullptr);
    tools.selectTool(TOOL_HIGHLIGHTER);
    tools.setSmartHighlighterEnabled(true);
    tools.selectTool(TOOL_ERASER);
    EXPECT_EQ(tools.getToolType(), TOOL_ERASER);
    EXPECT_FALSE(tools.isSmartHighlighterEnabled());
    tools.selectTool(TOOL_PEN);
    tools.selectTool(TOOL_ERASER);
    EXPECT_EQ(tools.getToolType(), TOOL_ERASER);
}

namespace {
class StrokeFactory: public InputHandler {
public:
    using InputHandler::createStroke;
};

void enableShapeStyle(ToolHandler& tools, ToolType type) {
    tools.selectTool(type);
    auto& tool = tools.getTool(type);
    tool.setFill(true);
    tool.setFillAlpha(128);
    LineStyle dashed;
    dashed.setDashes({4, 2});
    tool.setLineStyle(dashed);
}
}  // namespace

TEST(TeacherTools, handwritingDoesNotInheritShapeStyle) {
    ToolHandler tools(nullptr, nullptr, nullptr);
    for (auto type: {TOOL_PEN, TOOL_HIGHLIGHTER}) {
        enableShapeStyle(tools, type);
        tools.setDrawingType(DRAWING_TYPE_DEFAULT);
        auto stroke = StrokeFactory::createStroke(&tools, true);
        EXPECT_EQ(stroke->getFill(), -1);
        EXPECT_FALSE(stroke->getLineStyle().hasDashes());
        EXPECT_TRUE(tools.getTool(type).getFill());
    }
}

TEST(TeacherTools, shapesRetainFillAndDashes) {
    ToolHandler tools(nullptr, nullptr, nullptr);
    enableShapeStyle(tools, TOOL_PEN);
    tools.setDrawingType(DRAWING_TYPE_RECTANGLE);
    auto stroke = StrokeFactory::createStroke(&tools, true);
    EXPECT_EQ(stroke->getFill(), 128);
    EXPECT_TRUE(stroke->getLineStyle().hasDashes());
}

TEST(TeacherTools, classicHandwritingRetainsUpstreamStyle) {
    ToolHandler tools(nullptr, nullptr, nullptr);
    enableShapeStyle(tools, TOOL_PEN);
    tools.setDrawingType(DRAWING_TYPE_DEFAULT);
    auto stroke = StrokeFactory::createStroke(&tools, false);
    EXPECT_EQ(stroke->getFill(), 128);
    EXPECT_TRUE(stroke->getLineStyle().hasDashes());
}
