/*
 * Ultimate Teacher Notepad
 * Continuous erasure regressions, based on Xournal++ GPLv2+.
 */
#include <gtest/gtest.h>

#include <algorithm>
#include <fstream>
#include <string>
#include <cmath>
#include <memory>
#include <filesystem>
#include <string>
#include <system_error>
#include <vector>

#include "control/ToolHandler.h"
#include "control/xojfile/LoadHandler.h"
#include "control/xojfile/SaveHandler.h"
#include "control/tools/EraseHandler.h"
#include "control/ExportHelper.h"
#include "control/jobs/BaseExportJob.h"
#include "control/tools/InputHandler.h"
#include "model/eraser/CircularEraser.h"
#include "gui/LegacyRedrawable.h"
#include "model/Document.h"
#include "model/Image.h"
#include "model/Layer.h"
#include "model/Stroke.h"
#include "model/XojPage.h"
#include "undo/UndoRedoHandler.h"
#include "util/Color.h"
#include "util/Matrix.h"
#include "filesystem.h"

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

namespace {
void addLine(const PageRef& page, double x1, double y1, double x2, double y2) {
    auto stroke = std::make_unique<Stroke>();
    stroke->setToolType(StrokeTool::PEN);
    stroke->setWidth(1);
    stroke->addPoint(Point(x1, y1));
    stroke->addPoint(Point(x2, y2));
    page->getSelectedLayer()->addElement(std::move(stroke));
}
}  // namespace

// Whole-stroke mode is a disc of radius 10 centred at (50, 50). The stroke's nearest point is (58, 58),
// at distance 11.3, outside the disc (a square of half size 10 would catch it). It must survive.
TEST(EraseHandler, wholeStrokeModeIsCircularNotSquare) {
    auto page = std::make_shared<XojPage>(200, 200);
    addLine(page, 58, 58, 200, 200);
    Document document(nullptr);
    UndoRedoHandler undo(nullptr);
    ToolHandler tools(nullptr, nullptr, nullptr);
    tools.setEraserType(ERASER_TYPE_DELETE_STROKE);
    tools.setEraserThickness(10);
    tools.selectTool(TOOL_ERASER);
    EraseTestView view;
    EraseHandler eraser(&undo, &document, page, &tools, &view);
    eraser.erase(50, 50);
    eraser.finalize();
    EXPECT_EQ(page->getSelectedLayer()->getElements().size(), 1U);
}

// The same disc deletes a stroke whose centreline passes within the radius (distance 2 from (50, 50))
TEST(EraseHandler, wholeStrokeModeDeletesStrokeInsideDisc) {
    auto page = std::make_shared<XojPage>(200, 200);
    addLine(page, 0, 52, 200, 52);
    Document document(nullptr);
    UndoRedoHandler undo(nullptr);
    ToolHandler tools(nullptr, nullptr, nullptr);
    tools.setEraserType(ERASER_TYPE_DELETE_STROKE);
    tools.setEraserThickness(10);
    tools.selectTool(TOOL_ERASER);
    EraseTestView view;
    EraseHandler eraser(&undo, &document, page, &tools, &view);
    eraser.erase(50, 50);
    eraser.finalize();
    EXPECT_TRUE(page->getSelectedLayer()->getElements().empty());
}

namespace {
/// Distance from q to the segment (x0,y0)-(x1,y1), using the same primitive as the eraser
double distanceToPath(const Point& q, double x0, double y0, double x1, double y1) {
    return utn::eraser::distancePointSegment({q.x, q.y}, {x0, y0}, {x1, y1});
}

/// All centreline points of the strokes left on the selected layer
std::vector<Point> remainingPoints(const PageRef& page) {
    std::vector<Point> pts;
    for (auto& e: page->getSelectedLayer()->getElements()) {
        if (e->getType() == ELEMENT_STROKE) {
            for (const Point& p: static_cast<Stroke*>(e.get())->getPointVector()) {
                pts.push_back(p);
            }
        }
    }
    return pts;
}

/// Erase along the horizontal path y = 100, x from 0 to 200, with a single fast pointer jump (the sweep fills in)
void sweepHorizontalPath(const PageRef& page, double radius) {
    Document document(nullptr);
    UndoRedoHandler undo(nullptr);
    ToolHandler tools(nullptr, nullptr, nullptr);
    tools.setEraserThickness(radius);
    ASSERT_DOUBLE_EQ(tools.getEraserThickness(), radius) << "eraser thickness was clamped";
    tools.selectTool(TOOL_ERASER);
    EraseTestView view;
    EraseHandler eraser(&undo, &document, page, &tools, &view);
    eraser.erase(0, 100);
    eraser.erase(200, 100);
    eraser.finalize();
}
}  // namespace

// Partial erasing is a disc. The stroke's nearest point (58,58) is at distance 11.3 from (50,50), beyond R + h = 10.5,
// but inside the square of half size 10.4 that the old eraser used, so a square eraser would have cut it.
TEST(EraseHandler, partialEraseIsCircularNotSquare) {
    auto page = std::make_shared<XojPage>(200, 200);
    addLine(page, 58, 58, 200, 200);
    Document document(nullptr);
    UndoRedoHandler undo(nullptr);
    ToolHandler tools(nullptr, nullptr, nullptr);
    tools.setEraserThickness(10);
    tools.selectTool(TOOL_ERASER);
    EraseTestView view;
    EraseHandler eraser(&undo, &document, page, &tools, &view);
    eraser.erase(50, 50);
    eraser.finalize();
    EXPECT_EQ(page->getSelectedLayer()->getElements().size(), 1U);
    EXPECT_FALSE(undo.canUndo());
}

// A vertical stroke crossing the sweep is cut exactly where its ink reaches the disc: |y - 100| = R + h.
// Checked for several pen widths, so the cut respects the thickness of the ink.
TEST(EraseHandler, partialEraseCutsAtDiscBoundaryForAnyPenWidth) {
    for (double width: {1.0, 4.0, 10.0}) {
        // ToolHandler clamps the eraser thickness to [0.5, 30]; the radii must stay inside that range
        for (double radius: {3.0, 10.0, 25.0}) {
            auto page = std::make_shared<XojPage>(400, 400);
            auto stroke = std::make_unique<Stroke>();
            stroke->setToolType(StrokeTool::PEN);
            stroke->setWidth(width);
            stroke->addPoint(Point(100, 0));
            stroke->addPoint(Point(100, 200));
            page->getSelectedLayer()->addElement(std::move(stroke));

            sweepHorizontalPath(page, radius);

            const double h = 0.5 * width;
            const double reach = radius + h;
            ASSERT_GE(page->getSelectedLayer()->getElements().size(), 1U) << "w " << width << " R " << radius;
            // No ink survives within reach of the path: every remaining point is at least R + h away
            // Sampling over-erases by less than R / 100 (see EraseHandler::erase), never under-erases
            const double tol = 0.01 * radius + 0.05;
            for (const Point& p: remainingPoints(page)) {
                EXPECT_GE(distanceToPath(p, 0, 100, 200, 100), reach - 1e-6)
                        << "w " << width << " R " << radius << " point (" << p.x << ", " << p.y << ")";
            }
            // The cut lands at exactly R + h above and below the path (the far ends are left in place)
            double highestAbove = -1e9, lowestBelow = 1e9;
            for (const Point& p: remainingPoints(page)) {
                if (p.y < 100) highestAbove = std::max(highestAbove, p.y);
                if (p.y > 100) lowestBelow = std::min(lowestBelow, p.y);
            }
            EXPECT_NEAR(highestAbove, 100 - reach, tol) << "w " << width << " R " << radius;
            EXPECT_NEAR(lowestBelow, 100 + reach, tol) << "w " << width << " R " << radius;
        }
    }
}

// A stroke whose ink lies outside the path's reach is left alone. The second line is 25 away from the path, and the
// disc reaches only 10.5 from it.
TEST(EraseHandler, partialEraseLeavesNeighbouringStrokeOutsideDisc) {
    auto page = std::make_shared<XojPage>(400, 400);
    addLine(page, 0, 125, 200, 125);
    sweepHorizontalPath(page, 10);
    ASSERT_EQ(page->getSelectedLayer()->getElements().size(), 1U);
    auto pts = remainingPoints(page);
    ASSERT_EQ(pts.size(), 2U);
    EXPECT_DOUBLE_EQ(pts[0].y, 125);
    EXPECT_DOUBLE_EQ(pts[1].y, 125);
}

// A stroke lying on the sweep is removed completely, with no gap left by the jump
TEST(EraseHandler, partialEraseSweepLeavesNoGapAlongStroke) {
    auto page = std::make_shared<XojPage>(400, 400);
    addLine(page, 0, 100, 200, 100);
    sweepHorizontalPath(page, 10);
    EXPECT_TRUE(page->getSelectedLayer()->getElements().empty());
}

// Curved stroke (a circle of radius 60 centred on (100,100)) swept by the horizontal path: every remaining point must
// be outside the disc's reach of the path.
TEST(EraseHandler, partialEraseCurvedStrokeRespectsDisc) {
    auto page = std::make_shared<XojPage>(400, 400);
    auto stroke = std::make_unique<Stroke>();
    stroke->setToolType(StrokeTool::PEN);
    stroke->setWidth(2);
    for (int k = 0; k <= 360; k += 4) {
        const double a = k * std::acos(-1.0) / 180.0;
        stroke->addPoint(Point(100 + 60 * std::cos(a), 100 + 60 * std::sin(a)));
    }
    page->getSelectedLayer()->addElement(std::move(stroke));
    sweepHorizontalPath(page, 10);
    const double reach = 10 + 1;
    for (const Point& p: remainingPoints(page)) {
        EXPECT_GE(distanceToPath(p, 0, 100, 200, 100), reach - 1e-6) << p.x << ", " << p.y;
    }
    EXPECT_FALSE(remainingPoints(page).empty());
}

TEST(EraseHandler, wholeStrokeModeAccountsForStylusPressure) {
    auto page = std::make_shared<XojPage>(250, 250);
    auto stroke = std::make_unique<Stroke>();
    stroke->setToolType(StrokeTool::PEN);
    stroke->setWidth(1);  // fallback width is narrow; pressure creates a much wider stroke
    stroke->addPoint(Point(80, 100, 20));
    stroke->addPoint(Point(120, 100, 20));
    page->getSelectedLayer()->addElement(std::move(stroke));

    Document document(nullptr);
    UndoRedoHandler undo(nullptr);
    ToolHandler tools(nullptr, nullptr, nullptr);
    tools.setEraserType(ERASER_TYPE_DELETE_STROKE);
    tools.setEraserThickness(1);
    tools.selectTool(TOOL_ERASER);
    EraseTestView view;
    EraseHandler eraser(&undo, &document, page, &tools, &view);
    // Centreline distance 11, but the pressure-based half width 10 reaches the radius 1 disc.
    eraser.erase(100, 111);
    eraser.finalize();
    EXPECT_TRUE(page->getSelectedLayer()->getElements().empty());
    EXPECT_TRUE(undo.canUndo());
}

TEST(EraseHandler, partialErasePrefilterIncludesExpandedSweepSamples) {
    auto page = std::make_shared<XojPage>(200, 200);
    auto stroke = std::make_unique<Stroke>();
    stroke->setToolType(StrokeTool::PEN);
    stroke->setWidth(0.02);
    // The final pointer is (10,100) with radius 10. The nominal disc misses x=20.06,
    // but sampling at spacing 2.5 expands the disc to sqrt(100+1.5625) = 10.0778.
    // A prefilter based on the nominal radius incorrectly drops this stroke.
    stroke->addPoint(Point(20.06, 90));
    stroke->addPoint(Point(20.06, 110));
    page->getSelectedLayer()->addElement(std::move(stroke));

    Document document(nullptr);
    UndoRedoHandler undo(nullptr);
    ToolHandler tools(nullptr, nullptr, nullptr);
    tools.setEraserThickness(10);
    tools.selectTool(TOOL_ERASER);
    EraseTestView view;
    EraseHandler eraser(&undo, &document, page, &tools, &view);
    eraser.erase(0, 100);
    EXPECT_FALSE(undo.canUndo());
    eraser.erase(10, 100);
    eraser.finalize();
    EXPECT_TRUE(undo.canUndo()) << "Expanded sampling radius must also expand candidate prefilter";
    EXPECT_GT(page->getSelectedLayer()->getElements().size(), 0U);
}


TEST(EraseHandler, circularPartialEraseSupportsUndoRedo) {
    auto page = std::make_shared<XojPage>(300, 300);
    addLine(page, 20, 100, 220, 100);

    Document document(nullptr);
    UndoRedoHandler undo(nullptr);
    ToolHandler tools(nullptr, nullptr, nullptr);
    tools.setEraserThickness(10);
    tools.selectTool(TOOL_ERASER);
    EraseTestView view;
    EraseHandler eraser(&undo, &document, page, &tools, &view);

    eraser.erase(120, 100);
    eraser.finalize();
    ASSERT_EQ(page->getSelectedLayer()->getElements().size(), 2U);
    ASSERT_TRUE(undo.canUndo());
    EXPECT_FALSE(undo.canRedo());

    for (int repeat = 0; repeat < 3; ++repeat) {
        undo.undo();
        ASSERT_EQ(page->getSelectedLayer()->getElements().size(), 1U);
        ASSERT_TRUE(undo.canRedo());
        const auto* original = static_cast<Stroke*>(page->getSelectedLayer()->getElements().front().get());
        ASSERT_EQ(original->getPointCount(), 2U);
        EXPECT_DOUBLE_EQ(original->getPointVector().front().x, 20.0);
        EXPECT_DOUBLE_EQ(original->getPointVector().back().x, 220.0);

        undo.redo();
        ASSERT_EQ(page->getSelectedLayer()->getElements().size(), 2U);
        ASSERT_TRUE(undo.canUndo());
    }
}

TEST(EraseHandler, circularWholeStrokeEraseSupportsUndoRedo) {
    auto page = std::make_shared<XojPage>(300, 300);
    addLine(page, 20, 100, 220, 100);

    Document document(nullptr);
    UndoRedoHandler undo(nullptr);
    ToolHandler tools(nullptr, nullptr, nullptr);
    tools.setEraserType(ERASER_TYPE_DELETE_STROKE);
    tools.setEraserThickness(10);
    tools.selectTool(TOOL_ERASER);
    EraseTestView view;
    EraseHandler eraser(&undo, &document, page, &tools, &view);

    eraser.erase(120, 100);
    eraser.finalize();
    ASSERT_TRUE(page->getSelectedLayer()->getElements().empty());
    ASSERT_TRUE(undo.canUndo());

    for (int repeat = 0; repeat < 3; ++repeat) {
        undo.undo();
        ASSERT_EQ(page->getSelectedLayer()->getElements().size(), 1U);
        ASSERT_TRUE(undo.canRedo());
        undo.redo();
        ASSERT_TRUE(page->getSelectedLayer()->getElements().empty());
        ASSERT_TRUE(undo.canUndo());
    }
}

TEST(EraseHandler, separateCircularGesturesCreateSeparateUndoSteps) {
    auto page = std::make_shared<XojPage>(300, 300);
    addLine(page, 20, 100, 220, 100);

    Document document(nullptr);
    UndoRedoHandler undo(nullptr);
    ToolHandler tools(nullptr, nullptr, nullptr);
    tools.setEraserThickness(8);
    tools.selectTool(TOOL_ERASER);
    EraseTestView view;
    EraseHandler eraser(&undo, &document, page, &tools, &view);

    eraser.erase(80, 100);
    eraser.finalize();
    ASSERT_EQ(page->getSelectedLayer()->getElements().size(), 2U);

    eraser.erase(160, 100);
    eraser.finalize();
    ASSERT_EQ(page->getSelectedLayer()->getElements().size(), 3U);

    undo.undo();
    ASSERT_EQ(page->getSelectedLayer()->getElements().size(), 2U);
    undo.undo();
    ASSERT_EQ(page->getSelectedLayer()->getElements().size(), 1U);
    ASSERT_FALSE(undo.canUndo());

    undo.redo();
    ASSERT_EQ(page->getSelectedLayer()->getElements().size(), 2U);
    undo.redo();
    ASSERT_EQ(page->getSelectedLayer()->getElements().size(), 3U);
}


TEST(EraseHandler, partiallyErasedPressureStrokesRoundTripThroughXopp) {
    auto page = std::make_shared<XojPage>(300, 300);
    auto stroke = std::make_unique<Stroke>();
    stroke->setToolType(StrokeTool::PEN);
    stroke->setColor(Color{0x36, 0x78, 0xc9});
    stroke->setWidth(4.0);
    stroke->addPoint(Point(20, 100, 4.0));
    stroke->addPoint(Point(220, 100, 4.0));
    page->getSelectedLayer()->addElement(std::move(stroke));

    Document document(nullptr);
    document.addPage(page);
    UndoRedoHandler undo(nullptr);
    ToolHandler tools(nullptr, nullptr, nullptr);
    tools.setEraserThickness(10);
    tools.selectTool(TOOL_ERASER);
    EraseTestView view;
    {
        EraseHandler eraser(&undo, &document, page, &tools, &view);
        eraser.erase(120, 100);
        eraser.finalize();
    }
    ASSERT_EQ(page->getSelectedLayer()->getElements().size(), 2U);

    // A unique temporary .xopp avoids collisions with the repository's other document tests.
    struct TemporaryXopp {
        fs::path path;
        ~TemporaryXopp() {
            std::error_code ec;
            fs::remove(path, ec);
        }
    } temporary{fs::temp_directory_path() /
                ("utn-circular-eraser-" + std::to_string(g_get_monotonic_time()) + ".xopp")};

    SaveHandler saver;
    saver.prepareSave(&document, temporary.path);
    saver.saveTo(temporary.path);
    ASSERT_TRUE(saver.getErrorMessage().empty()) << saver.getErrorMessage();
    ASSERT_TRUE(fs::exists(temporary.path));

    auto restored = LoadHandler{}.loadDocument(temporary.path);
    ASSERT_NE(restored, nullptr);
    ASSERT_EQ(restored->getPageCount(), 1U);
    auto roundTrippedPage = restored->getPage(0);
    ASSERT_NE(roundTrippedPage, nullptr);

    const auto& before = page->getSelectedLayer()->getElements();
    const auto& after = roundTrippedPage->getSelectedLayer()->getElements();
    ASSERT_EQ(before.size(), after.size());
    for (size_t i = 0; i < before.size(); ++i) {
        auto* original = dynamic_cast<Stroke*>(before[i].get());
        auto* loaded = dynamic_cast<Stroke*>(after[i].get());
        ASSERT_NE(original, nullptr);
        ASSERT_NE(loaded, nullptr);
        EXPECT_EQ(original->getToolType(), loaded->getToolType());
        EXPECT_EQ(original->getColor(), loaded->getColor());
        EXPECT_DOUBLE_EQ(original->getWidth(), loaded->getWidth());
        EXPECT_EQ(original->getStrokeCapStyle(), loaded->getStrokeCapStyle());
        EXPECT_EQ(original->getPointCount(), loaded->getPointCount());
        ASSERT_EQ(original->getPointVector().size(), loaded->getPointVector().size());
        for (size_t j = 0; j < original->getPointCount(); ++j) {
            const Point a = original->getPoint(j), b = loaded->getPoint(j);
            EXPECT_NEAR(a.x, b.x, 1e-7);
            EXPECT_NEAR(a.y, b.y, 1e-7);
            // XOPP intentionally stores pressure for drawable segments only; the last
            // point has no outgoing segment, so its z value is not round-tripped.
            if (j + 1 < original->getPointCount()) {
                EXPECT_NEAR(a.z, b.z, 1e-7);
            }
        }
    }
}

namespace {
/// Temporary file removed at scope exit
struct TemporaryFile {
    fs::path path;
    ~TemporaryFile() {
        std::error_code ec;
        fs::remove(path, ec);
    }
};

std::vector<std::vector<Point>> strokePoints(const PageRef& page) {
    std::vector<std::vector<Point>> out;
    for (auto& e: page->getSelectedLayer()->getElements()) {
        if (auto* st = dynamic_cast<Stroke*>(e.get())) {
            out.push_back(st->getPointVector());
        }
    }
    return out;
}
}  // namespace

// Several eraser gestures (partial and whole-stroke), with undo and redo in between, then save and reload. The page
// that is saved must match the page that was in memory, stroke for stroke and point for point.
TEST(EraseHandler, repeatedEraseGesturesSurviveUndoRedoAndSaveReopen) {
    auto page = std::make_shared<XojPage>(300, 300);
    for (double y: {100.0, 160.0}) {
        auto stroke = std::make_unique<Stroke>();
        stroke->setToolType(StrokeTool::PEN);
        stroke->setColor(Color{0x20, 0x20, 0x20});
        stroke->setWidth(4.0);
        stroke->addPoint(Point(20, y, 4.0));
        stroke->addPoint(Point(220, y, 4.0));
        page->getSelectedLayer()->addElement(std::move(stroke));
    }
    Document document(nullptr);
    document.addPage(page);
    UndoRedoHandler undo(nullptr);
    ToolHandler tools(nullptr, nullptr, nullptr);
    tools.setEraserThickness(10);
    tools.selectTool(TOOL_ERASER);
    EraseTestView view;

    // Gesture 1: partial erase across the first stroke
    {
        EraseHandler eraser(&undo, &document, page, &tools, &view);
        eraser.erase(120, 100);
        eraser.finalize();
    }
    // Gesture 2: whole-stroke erase of the second stroke
    tools.setEraserType(ERASER_TYPE_DELETE_STROKE);
    {
        EraseHandler eraser(&undo, &document, page, &tools, &view);
        eraser.erase(120, 160);
        eraser.finalize();
    }
    const auto afterBoth = strokePoints(page);
    ASSERT_EQ(afterBoth.size(), 2U);

    // Undo the whole-stroke gesture, then redo it: the state must be the same as before
    undo.undo();
    EXPECT_EQ(strokePoints(page).size(), 3U);
    undo.redo();
    EXPECT_EQ(strokePoints(page), afterBoth);

    // Save, then reload: the saved page matches what was in memory
    TemporaryFile temporary{fs::temp_directory_path() /
                            ("utn-repeated-erase-" + std::to_string(g_get_monotonic_time()) + ".xopp")};
    SaveHandler saver;
    saver.prepareSave(&document, temporary.path);
    saver.saveTo(temporary.path);
    ASSERT_TRUE(saver.getErrorMessage().empty()) << saver.getErrorMessage();

    auto restored = LoadHandler{}.loadDocument(temporary.path);
    ASSERT_NE(restored, nullptr);
    ASSERT_EQ(restored->getPageCount(), 1U);
    const auto reloaded = strokePoints(restored->getPage(0));
    ASSERT_EQ(reloaded.size(), afterBoth.size());
    for (size_t i = 0; i < reloaded.size(); ++i) {
        ASSERT_EQ(reloaded[i].size(), afterBoth[i].size());
        for (size_t j = 0; j < reloaded[i].size(); ++j) {
            EXPECT_NEAR(reloaded[i][j].x, afterBoth[i][j].x, 1e-7);
            EXPECT_NEAR(reloaded[i][j].y, afterBoth[i][j].y, 1e-7);
        }
    }
}

// An erased page exports to a PDF. The check is that a non-empty PDF file with the PDF header is written.
TEST(EraseHandler, erasedPageExportsToPdf) {
    auto page = std::make_shared<XojPage>(300, 300);
    auto stroke = std::make_unique<Stroke>();
    stroke->setToolType(StrokeTool::PEN);
    stroke->setWidth(4.0);
    stroke->addPoint(Point(20, 100, 4.0));
    stroke->addPoint(Point(220, 100, 4.0));
    page->getSelectedLayer()->addElement(std::move(stroke));
    Document document(nullptr);
    document.addPage(page);
    UndoRedoHandler undo(nullptr);
    ToolHandler tools(nullptr, nullptr, nullptr);
    tools.setEraserThickness(10);
    tools.selectTool(TOOL_ERASER);
    EraseTestView view;
    {
        EraseHandler eraser(&undo, &document, page, &tools, &view);
        eraser.erase(120, 100);
        eraser.finalize();
    }
    TemporaryFile pdf{fs::temp_directory_path() /
                      ("utn-erased-export-" + std::to_string(g_get_monotonic_time()) + ".pdf")};
    ExportHelper::exportPdf(&document, pdf.path, nullptr, nullptr, EXPORT_BACKGROUND_NONE, false);
    ASSERT_TRUE(fs::exists(pdf.path));
    std::ifstream in(pdf.path, std::ios::binary);
    char header[5] = {0, 0, 0, 0, 0};
    in.read(header, 4);
    EXPECT_EQ(std::string(header), "%PDF");
    EXPECT_GT(fs::file_size(pdf.path), 100U);
}

namespace {
// Builds a PNG-backed Image of the given size at (x, y), so the object eraser sees a real bounding box.
std::unique_ptr<Image> makePngImage(double x, double y, int w, int h) {
    GdkPixbuf* pixbuf = gdk_pixbuf_new(GDK_COLORSPACE_RGB, TRUE, 8, w, h);
    gchar* buffer = nullptr;
    gsize length = 0;
    gdk_pixbuf_save_to_buffer(pixbuf, &buffer, &length, "png", nullptr, nullptr);
    g_object_unref(pixbuf);
    auto image = std::make_unique<Image>();
    image->setImage(std::string(buffer, length));
    g_free(buffer);
    image->setTransformation(xoj::util::Matrix::TRANSLATION(x, y));
    return image;
}

size_t countType(const PageRef& page, ElementType type) {
    size_t n = 0;
    for (const auto& e: page->getSelectedLayer()->getElements()) {
        n += (e->getType() == type) ? 1U : 0U;
    }
    return n;
}
}  // namespace

// Object eraser: an element is removed only when the eraser disc touches its bounding box, and strokes are untouched.
TEST(EraseHandler, objectEraserRemovesImageButNotStrokes) {
    auto page = std::make_shared<XojPage>(300, 300);
    addLine(page, 20, 50, 280, 50);
    page->getSelectedLayer()->addElement(makePngImage(100, 100, 40, 20));

    Document document(nullptr);
    UndoRedoHandler undo(nullptr);
    ToolHandler tools(nullptr, nullptr, nullptr);
    tools.setEraserThickness(5);
    tools.setEraserType(ERASER_TYPE_DELETE_OBJECT);
    tools.selectTool(TOOL_ERASER);
    EraseTestView view;
    EraseHandler eraser(&undo, &document, page, &tools, &view);

    // Sweep through the image, and also across the stroke at y = 50 (which must survive)
    eraser.erase(60, 110);
    eraser.erase(160, 110);
    eraser.erase(160, 50);
    eraser.erase(200, 50);
    eraser.finalize();

    EXPECT_EQ(countType(page, ELEMENT_IMAGE), 0U);
    EXPECT_EQ(countType(page, ELEMENT_STROKE), 1U);
    const auto* stroke = static_cast<Stroke*>(page->getSelectedLayer()->getElements().front().get());
    EXPECT_EQ(stroke->getPointCount(), 2U);
    ASSERT_TRUE(undo.canUndo());
}

TEST(EraseHandler, objectEraserSupportsUndoRedo) {
    auto page = std::make_shared<XojPage>(300, 300);
    addLine(page, 20, 50, 280, 50);
    page->getSelectedLayer()->addElement(makePngImage(100, 100, 40, 20));

    Document document(nullptr);
    UndoRedoHandler undo(nullptr);
    ToolHandler tools(nullptr, nullptr, nullptr);
    tools.setEraserThickness(5);
    tools.setEraserType(ERASER_TYPE_DELETE_OBJECT);
    tools.selectTool(TOOL_ERASER);
    EraseTestView view;
    EraseHandler eraser(&undo, &document, page, &tools, &view);

    eraser.erase(60, 110);
    eraser.erase(160, 110);
    eraser.finalize();
    ASSERT_EQ(countType(page, ELEMENT_IMAGE), 0U);

    for (int repeat = 0; repeat < 3; ++repeat) {
        undo.undo();
        EXPECT_EQ(countType(page, ELEMENT_IMAGE), 1U);
        EXPECT_EQ(countType(page, ELEMENT_STROKE), 1U);
        ASSERT_TRUE(undo.canRedo());

        undo.redo();
        EXPECT_EQ(countType(page, ELEMENT_IMAGE), 0U);
        EXPECT_EQ(countType(page, ELEMENT_STROKE), 1U);
        ASSERT_TRUE(undo.canUndo());
    }
}

// A gesture that touches nothing must not create an undo step.
TEST(EraseHandler, objectEraserFarAwayIsNoOp) {
    auto page = std::make_shared<XojPage>(300, 300);
    addLine(page, 20, 50, 280, 50);
    page->getSelectedLayer()->addElement(makePngImage(100, 100, 40, 20));

    Document document(nullptr);
    UndoRedoHandler undo(nullptr);
    ToolHandler tools(nullptr, nullptr, nullptr);
    tools.setEraserThickness(5);
    tools.setEraserType(ERASER_TYPE_DELETE_OBJECT);
    tools.selectTool(TOOL_ERASER);
    EraseTestView view;
    EraseHandler eraser(&undo, &document, page, &tools, &view);

    eraser.erase(250, 250);
    eraser.erase(270, 260);
    eraser.finalize();

    EXPECT_EQ(countType(page, ELEMENT_IMAGE), 1U);
    EXPECT_EQ(countType(page, ELEMENT_STROKE), 1U);
    EXPECT_FALSE(undo.canUndo());
}
