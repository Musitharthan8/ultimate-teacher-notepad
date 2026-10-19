#include <gtest/gtest.h>
#include <cmath>
#include <chrono>
#include <memory>
#include <vector>

#include "control/ToolHandler.h"
#include "control/tools/EraseHandler.h"
#include "gui/LegacyRedrawable.h"
#include "model/Document.h"
#include "model/Layer.h"
#include "model/Stroke.h"
#include "model/XojPage.h"
#include "undo/UndoRedoHandler.h"
#include "view/StrokeView.h"

namespace {
class PrecisionView: public LegacyRedrawable {
public:
    void repaintArea(double, double, double, double) const override {}
    void repaintPage() const override {}
    void rerenderPage(bool) override {}
    void rerenderRect(double, double, double, double) override {}
    GdkRGBA getSelectionColor() override { return {}; }
    void deleteViewBuffer() override {}
};

std::vector<unsigned char> pixels(const PageRef& page, double zoom) {
    int size = static_cast<int>(200 * zoom);
    auto* surface = cairo_image_surface_create(CAIRO_FORMAT_A8, size, size);
    auto* cr = cairo_create(surface);
    cairo_scale(cr, zoom, zoom);
    for (const auto& e: page->getSelectedLayer()->getElements()) {
        xoj::view::StrokeView(static_cast<const Stroke*>(e.get())).draw(xoj::view::Context::createDefault(cr));
    }
    cairo_surface_flush(surface);
    std::vector<unsigned char> result(static_cast<size_t>(size * size));
    const auto* data = cairo_image_surface_get_data(surface);
    for (int y = 0; y < size; ++y) {
        std::copy_n(data + y * cairo_image_surface_get_stride(surface), size, result.data() + y * size);
    }
    cairo_destroy(cr);
    cairo_surface_destroy(surface);
    return result;
}

void addWideLine(const PageRef& page, bool vertical = false) {
    auto stroke = std::make_unique<Stroke>();
    stroke->setToolType(StrokeTool::PEN);
    stroke->setWidth(20);
    stroke->addPoint(vertical ? Point(100, 20) : Point(20, 100));
    stroke->addPoint(vertical ? Point(100, 180) : Point(180, 100));
    page->getSelectedLayer()->addElement(std::move(stroke));
}
} // namespace

TEST(PartialErasePrecision, TinyDiscRemovesOnlyInkInsideItsBoundaryAtEveryZoom) {
    for (double radius: {0.5, 1.0, 3.0}) {
        auto page = std::make_shared<XojPage>(200, 200);
        addWideLine(page);
        Document doc(nullptr);
        UndoRedoHandler undo(nullptr);
        ToolHandler tools(nullptr, nullptr, nullptr);
        tools.setEraserThickness(radius);
        tools.selectTool(TOOL_ERASER);
        PrecisionView view;
        std::vector<std::vector<unsigned char>> before;
        for (double zoom: {1.0, 2.0, 4.0}) before.push_back(pixels(page, zoom));
        EraseHandler eraser(&undo, &doc, page, &tools, &view);
        eraser.erase(100, 100);
        eraser.finalize();
        size_t scaleIndex = 0;
        for (double zoom: {1.0, 2.0, 4.0}) {
            const auto after = pixels(page, zoom);
            const int size = static_cast<int>(200 * zoom);
            unsigned lostOutside = 0;
            unsigned remainingInside = 0;
            for (int y = 0; y < size; ++y) for (int x = 0; x < size; ++x) {
                const double d = std::hypot((x + 0.5) / zoom - 100, (y + 0.5) / zoom - 100);
                const size_t i = static_cast<size_t>(y * size + x);
                // Cairo's clipped stroke tessellation can round distant cap
                // edge coverage by up to two alpha units (<0.8%). No visible ink may be lost.
                if (d > radius + 1.5 / zoom && std::abs(int(before[scaleIndex][i]) - int(after[i])) > 2) ++lostOutside;
                if (d < radius - 1.5 / zoom && after[i] != 0) ++remainingInside;
            }
            EXPECT_EQ(lostOutside, 0U) << "radius=" << radius << " zoom=" << zoom;
            EXPECT_EQ(remainingInside, 0U);
            ++scaleIndex;
        }
        ASSERT_TRUE(undo.canUndo());
        undo.undo();
        EXPECT_EQ(pixels(page, 1), before[0]);
        undo.redo();
        EXPECT_NE(pixels(page, 1), before[0]);
    }
}

TEST(PartialErasePrecision, FastSweepAtIntersectionPreservesInkBesideThePath) {
    auto page = std::make_shared<XojPage>(200, 200);
    addWideLine(page);
    addWideLine(page, true);
    auto before = pixels(page, 2);
    Document doc(nullptr);
    UndoRedoHandler undo(nullptr);
    ToolHandler tools(nullptr, nullptr, nullptr);
    tools.setEraserThickness(0.5);
    tools.selectTool(TOOL_ERASER);
    PrecisionView view;
    EraseHandler eraser(&undo, &doc, page, &tools, &view);
    eraser.erase(20, 100);
    eraser.erase(180, 100);
    eraser.finalize();
    auto after = pixels(page, 2);
    // The narrow path cuts a slit through both wide strokes, not a wide centreline interval.
    EXPECT_EQ(after[214 * 400 + 200], before[214 * 400 + 200]);
    EXPECT_GT(after[214 * 400 + 200], 0);
    EXPECT_EQ(after[200 * 400 + 200], 0);
    undo.undo();
    EXPECT_EQ(pixels(page, 2), before);
    undo.redo();
    EXPECT_EQ(pixels(page, 2), after);
}

TEST(PartialErasePrecision, HighlighterBoundsFollowRenderedFallbackWidth) {
    auto page = std::make_shared<XojPage>(200, 200);
    auto stroke = std::make_unique<Stroke>();
    stroke->setToolType(StrokeTool::HIGHLIGHTER);
    stroke->setWidth(30);
    stroke->addPoint(Point(20, 100, 1));
    stroke->addPoint(Point(180, 100, 1));
    page->getSelectedLayer()->addElement(std::move(stroke));
    const auto before = pixels(page, 2);
    Document doc(nullptr);
    UndoRedoHandler undo(nullptr);
    ToolHandler tools(nullptr, nullptr, nullptr);
    tools.setEraserThickness(3);
    tools.selectTool(TOOL_ERASER);
    PrecisionView view;
    EraseHandler eraser(&undo, &doc, page, &tools, &view);
    eraser.erase(100, 100);
    eraser.finalize();
    const auto after = pixels(page, 2);
    EXPECT_GT(before[220 * 400 + 200], 0);
    EXPECT_EQ(after[220 * 400 + 200], before[220 * 400 + 200]);
}

TEST(PartialErasePrecision, WholeStrokeEraserInsideAnErasedHoleIsANoOp) {
    auto page = std::make_shared<XojPage>(200, 200);
    addWideLine(page);
    Document doc(nullptr);
    UndoRedoHandler undo(nullptr);
    ToolHandler tools(nullptr, nullptr, nullptr);
    tools.setEraserThickness(3);
    tools.selectTool(TOOL_ERASER);
    PrecisionView view;
    EraseHandler eraser(&undo, &doc, page, &tools, &view);
    eraser.erase(100, 100);
    eraser.finalize();
    const auto afterPartial = pixels(page, 2);
    tools.selectTool(TOOL_PEN);
    tools.setEraserThickness(0.5);
    tools.setEraserType(ERASER_TYPE_DELETE_STROKE);
    tools.selectTool(TOOL_ERASER);
    eraser.erase(100, 100);
    eraser.finalize();
    EXPECT_EQ(pixels(page, 2), afterPartial);
    // Only the partial edit belongs in history.
    undo.undo();
    EXPECT_FALSE(undo.canUndo());
}

TEST(PartialErasePrecision, WholeStrokeRespectsTransformedAndOverlappingHoles) {
    for (bool transformed: {false, true}) {
        auto page = std::make_shared<XojPage>(200, 200);
        addWideLine(page);
        auto* stroke = static_cast<Stroke*>(page->getSelectedLayer()->getElements().front().get());
        stroke->setErasures({{{99, 100}, {99, 100}, 2, {}}, {{101, 100}, {101, 100}, 2, {}}});
        double x = 100, y = 100;
        if (transformed) {
            stroke->move(10, 20);
            stroke->scale(110, 120, 2, .75, 0, false);
            x = 110; y = 120;
        }
        const auto before = pixels(page, 2);
        Document doc(nullptr);
        UndoRedoHandler undo(nullptr);
        ToolHandler tools(nullptr, nullptr, nullptr);
        tools.setEraserThickness(1);
        tools.setEraserType(ERASER_TYPE_DELETE_STROKE);
        tools.selectTool(TOOL_ERASER);
        PrecisionView view;
        EraseHandler eraser(&undo, &doc, page, &tools, &view);
        eraser.erase(x, y);
        eraser.finalize();
        EXPECT_FALSE(undo.canUndo());
        EXPECT_EQ(pixels(page, 2), before);
        eraser.erase(x, y + 5);
        eraser.finalize();
        EXPECT_TRUE(page->getSelectedLayer()->getElements().empty());
        undo.undo();
        EXPECT_EQ(pixels(page, 2), before);
    }
}

TEST(PartialErasePrecision, CurvedGestureKeepsSingleUndoSnapshot) {
    auto page = std::make_shared<XojPage>(200, 200);
    addWideLine(page);
    const auto before = pixels(page, 2);
    Document doc(nullptr);
    UndoRedoHandler undo(nullptr);
    ToolHandler tools(nullptr, nullptr, nullptr);
    tools.setEraserThickness(.5);
    tools.selectTool(TOOL_ERASER);
    PrecisionView view;
    EraseHandler eraser(&undo, &doc, page, &tools, &view);
    const auto start = std::chrono::steady_clock::now();
    for (int i = 0; i < 1024; ++i) eraser.erase(20 + 160.0 * i / 1023, 100 + 7 * std::sin(i * .1));
    eraser.finalize();
    const auto after = pixels(page, 2);
    RecordProperty("gesture_and_render_ms", std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - start).count());
    EXPECT_NE(after, before);
    undo.undo();
    EXPECT_FALSE(undo.canUndo());
    EXPECT_EQ(pixels(page, 2), before);
    undo.redo();
    EXPECT_EQ(pixels(page, 2), after);
}

TEST(PartialErasePrecision, DiagonalSquareCapsSurviveASeparateErase) {
    auto page = std::make_shared<XojPage>(200, 200);
    auto s = std::make_unique<Stroke>();
    s->setWidth(20);
    s->setStrokeCapStyle(StrokeCapStyle::SQUARE);
    s->addPoint(Point(20, 20)); s->addPoint(Point(180, 180));
    page->getSelectedLayer()->addElement(std::move(s));
    const auto before = pixels(page, 2);
    Document doc(nullptr);
    UndoRedoHandler undo(nullptr);
    ToolHandler tools(nullptr, nullptr, nullptr);
    tools.setEraserThickness(.5); tools.selectTool(TOOL_ERASER);
    PrecisionView view;
    EraseHandler eraser(&undo, &doc, page, &tools, &view);
    eraser.erase(100, 100); eraser.finalize();
    const auto after = pixels(page, 2);
    for (int y = 0; y < 80; ++y) for (int x = 0; x < 80; ++x)
        EXPECT_LE(std::abs(int(before[y * 400 + x]) - int(after[y * 400 + x])), 2);
}

TEST(PartialErasePrecision, ReflectedAndRotatedSelectionPreviewKeepsTheHole) {
    auto page = std::make_shared<XojPage>(200, 200);
    addWideLine(page);
    auto* stroke = static_cast<Stroke*>(page->getSelectedLayer()->getElements().front().get());
    stroke->setErasures({{{100, 100}, {100, 100}, 3, {}}});
    for (bool reflected: {true, false}) {
        auto* surface = cairo_image_surface_create(CAIRO_FORMAT_A8, 400, 400);
        auto* cr = cairo_create(surface);
        cairo_translate(cr, 400, reflected ? 400 : 0);
        if (reflected) cairo_scale(cr, -2, -2);
        else { cairo_rotate(cr, std::acos(-1.) / 2); cairo_scale(cr, 2, 2); }
        xoj::view::StrokeView(stroke).draw(xoj::view::Context::createDefault(cr));
        cairo_surface_flush(surface);
        const auto* data = cairo_image_surface_get_data(surface);
        const int stride = cairo_image_surface_get_stride(surface);
        EXPECT_EQ(data[200 * stride + 200], 0);
        EXPECT_GT(data[(reflected ? 200 : 80) * stride + (reflected ? 320 : 200)], 0);
        EXPECT_EQ(cairo_status(cr), CAIRO_STATUS_SUCCESS);
        cairo_destroy(cr); cairo_surface_destroy(surface);
    }
}
