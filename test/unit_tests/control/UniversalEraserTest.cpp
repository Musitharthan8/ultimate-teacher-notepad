/*
 * Ultimate Teacher Notepad
 *
 * M2-02: the default eraser must clear every rendered pixel of pen ink, highlighter ink and drawn shapes inside the
 * eraser disc, and must not touch ink outside it. Instead of trusting the hit-test geometry, the oracle renders the
 * layer before and after the gesture and compares pixels against the exact swept capsule.
 * Based on Xournal++ GPLv2+.
 */
#include <gtest/gtest.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <iostream>
#include <map>
#include <memory>
#include <string>
#include <vector>

#include <cairo.h>

#include "control/ToolHandler.h"
#include "control/tools/EraseHandler.h"
#include "gui/LegacyRedrawable.h"
#include "model/Document.h"
#include "model/Layer.h"
#include "model/Stroke.h"
#include "model/XojPage.h"
#include "model/eraser/CircularEraser.h"
#include "undo/UndoRedoHandler.h"
#include "view/StrokeView.h"
#include "view/View.h"

namespace {
class NullView: public LegacyRedrawable {
public:
    void repaintArea(double, double, double, double) const override {}
    void repaintPage() const override {}
    void rerenderPage(bool) override {}
    void rerenderRect(double, double, double, double) override {}
    GdkRGBA getSelectionColor() override { return {}; }
    void deleteViewBuffer() override {}
};

constexpr int SIZE = 200;  // page units
constexpr int SCALE = 2;   // pixels per unit

struct Shape {
    std::string name;
    std::vector<Point> pts;
    int fill;  // -1 unfilled
};

std::vector<Point> circlePoints(double cx, double cy, double r, int n) {
    std::vector<Point> v;
    for (int i = 0; i <= n; ++i) {
        const double a = 2 * M_PI * i / n;
        v.emplace_back(cx + r * std::cos(a), cy + r * std::sin(a));
    }
    return v;
}

std::vector<Shape> shapes() {
    return {
            {"line", {{40, 100}, {160, 100}}, -1},
            {"diagonal", {{40, 40}, {160, 160}}, -1},
            {"rectangle", {{50, 50}, {150, 50}, {150, 150}, {50, 150}, {50, 50}}, -1},
            {"filledRectangle", {{50, 50}, {150, 50}, {150, 150}, {50, 150}, {50, 50}}, 120},
            {"circle", circlePoints(100, 100, 50, 48), -1},
            {"zigzag", {{30, 60}, {70, 140}, {110, 60}, {150, 140}, {180, 60}}, -1},
    };
}

std::unique_ptr<Stroke> makeStroke(const Shape& s, StrokeTool tool, double width, StrokeCapStyle cap, bool pressure) {
    auto stroke = std::make_unique<Stroke>();
    stroke->setToolType(tool);
    stroke->setWidth(width);
    stroke->setStrokeCapStyle(cap);
    stroke->setColor(Color(0x336699U));
    if (s.fill != -1) {
        stroke->setFill(s.fill);
    }
    for (size_t i = 0; i < s.pts.size(); ++i) {
        Point p = s.pts[i];
        if (pressure) {
            p.z = width * (0.4 + 0.6 * static_cast<double>(i % 3) / 2.0);
        }
        stroke->addPoint(p);
    }
    return stroke;
}

std::vector<uint8_t> renderAlpha(const PageRef& page) {
    auto* surface = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, SIZE * SCALE, SIZE * SCALE);
    auto* cr = cairo_create(surface);
    cairo_scale(cr, SCALE, SCALE);
    for (const auto& e: page->getSelectedLayer()->getElements()) {
        if (e->getType() == ELEMENT_STROKE) {
            xoj::view::StrokeView(static_cast<Stroke*>(e.get())).draw(xoj::view::Context::createDefault(cr));
        }
    }
    cairo_surface_flush(surface);
    std::vector<uint8_t> alpha(SIZE * SCALE * SIZE * SCALE);
    const auto* data = cairo_image_surface_get_data(surface);
    const int stride = cairo_image_surface_get_stride(surface);
    for (int y = 0; y < SIZE * SCALE; ++y) {
        for (int x = 0; x < SIZE * SCALE; ++x) {
            alpha[static_cast<size_t>(y * SIZE * SCALE + x)] = data[y * stride + x * 4 + 3];
        }
    }
    cairo_destroy(cr);
    cairo_surface_destroy(surface);
    return alpha;
}

struct Outcome {
    size_t inkLeftInDisc = 0;
    size_t overErased = 0;
};

/// Run one gesture (click or drag) and compare against the exact swept capsule.
Outcome gesture(const PageRef& page, utn::eraser::Vec a, utn::eraser::Vec b, double radius) {
    const auto before = renderAlpha(page);
    Document document(nullptr);
    UndoRedoHandler undo(nullptr);
    ToolHandler tools(nullptr, nullptr, nullptr);
    tools.setEraserThickness(radius);
    tools.setEraserType(ERASER_TYPE_DEFAULT);
    tools.selectTool(TOOL_ERASER);
    NullView view;
    EraseHandler eraser(&undo, &document, page, &tools, &view);
    eraser.erase(a.x, a.y);
    if (a.x != b.x || a.y != b.y) {
        eraser.erase(b.x, b.y);
    }
    eraser.finalize();
    const auto after = renderAlpha(page);

    Outcome out;
    const double margin = 1.5 / SCALE;  // antialiasing tolerance
    for (int py = 0; py < SIZE * SCALE; ++py) {
        for (int px = 0; px < SIZE * SCALE; ++px) {
            const size_t i = static_cast<size_t>(py * SIZE * SCALE + px);
            const utn::eraser::Vec c{(px + 0.5) / SCALE, (py + 0.5) / SCALE};
            const double d = utn::eraser::distancePointSegment(c, a, b);
            if (d <= radius - margin && after[i] > 8) {
                ++out.inkLeftInDisc;
            } else if (d > radius + margin && after[i] + 8 < before[i]) {
                ++out.overErased;
            }
        }
    }
    return out;
}

struct Gesture {
    const char* name;
    utn::eraser::Vec a, b;
};
}  // namespace

TEST(UniversalEraser, defaultEraserClearsEveryInkKindInsideTheDiscAndNothingOutside) {
    std::map<std::string, size_t> missed, over;
    size_t cases = 0;
    for (const auto& shape: shapes()) {
        for (StrokeTool tool: {StrokeTool::PEN, StrokeTool::HIGHLIGHTER}) {
            if (shape.fill != -1 && tool != StrokeTool::HIGHLIGHTER) continue;
            for (double width: {1.5, 3.0, 12.0, 30.0}) {
                for (StrokeCapStyle cap: {StrokeCapStyle::ROUND, StrokeCapStyle::BUTT, StrokeCapStyle::SQUARE}) {
                    for (bool pressure: {false, true}) {
                        if (pressure && tool == StrokeTool::HIGHLIGHTER) continue;
                        for (double radius: {2.0, 5.0, 15.0}) {
                            // Clicks: on the first segment's centre line, and offset to the rendered ink edge
                            const Point p0 = shape.pts[0], p1 = shape.pts[1];
                            const utn::eraser::Vec mid{(p0.x + p1.x) / 2, (p0.y + p1.y) / 2};
                            const double len = std::hypot(p1.x - p0.x, p1.y - p0.y);
                            const utn::eraser::Vec normal{-(p1.y - p0.y) / len, (p1.x - p0.x) / len};
                            const double edge = width / 2 + radius * 0.5;
                            const utn::eraser::Vec atEdge{mid.x + normal.x * edge, mid.y + normal.y * edge};
                            const Gesture gestures[] = {
                                    {"clickOnCentre", mid, mid},
                                    {"clickAtInkEdge", atEdge, atEdge},
                                    {"dragAcross",
                                     {mid.x - normal.x * 25, mid.y - normal.y * 25},
                                     {mid.x + normal.x * 25, mid.y + normal.y * 25}},
                            };
                            for (const auto& g: gestures) {
                                auto page = std::make_shared<XojPage>(SIZE, SIZE);
                                page->getSelectedLayer()->addElement(makeStroke(shape, tool, width, cap, pressure));
                                const auto result = gesture(page, g.a, g.b, radius);
                                ++cases;
                                const std::string key = shape.name + "/" +
                                                        (tool == StrokeTool::PEN ? "pen" : "highlighter") + "/w" +
                                                        std::to_string(static_cast<int>(width)) + "/cap" +
                                                        std::to_string(static_cast<int>(cap)) +
                                                        (pressure ? "/pressure" : "") + "/r" +
                                                        std::to_string(static_cast<int>(radius)) + "/" + g.name;
                                if (result.inkLeftInDisc) missed[key] = result.inkLeftInDisc;
                                if (result.overErased) over[key] = result.overErased;
                            }
                        }
                    }
                }
            }
        }
    }
    for (const auto& [k, v]: missed) ADD_FAILURE() << "ink left inside the eraser disc (" << v << " px): " << k;
    for (const auto& [k, v]: over) ADD_FAILURE() << "ink erased outside the eraser disc (" << v << " px): " << k;
    std::cout << cases << " cases, " << missed.size() << " missed, " << over.size() << " over-erased\n";
}

namespace {
size_t inkPixels(const std::vector<uint8_t>& alpha) {
    return static_cast<size_t>(std::count_if(alpha.begin(), alpha.end(), [](uint8_t a) { return a > 8; }));
}

void runGesture(const PageRef& page, EraserType type, double radius, utn::eraser::Vec a, utn::eraser::Vec b,
                UndoRedoHandler& undo, Document& document) {
    ToolHandler tools(nullptr, nullptr, nullptr);
    tools.setEraserThickness(radius);
    tools.setEraserType(type);
    tools.selectTool(TOOL_ERASER);
    NullView view;
    EraseHandler eraser(&undo, &document, page, &tools, &view);
    eraser.erase(a.x, a.y);
    if (a.x != b.x || a.y != b.y) {
        eraser.erase(b.x, b.y);
    }
    eraser.finalize();
}
}  // namespace

// A pen stroke written over a highlighter band: one drag removes both where the disc touches them, neither is immune.
TEST(UniversalEraser, penOverHighlighterBothErasedUndoRestoresPixelsExactly) {
    auto page = std::make_shared<XojPage>(SIZE, SIZE);
    Shape band{"band", {{20, 100}, {180, 100}}, -1};
    Shape pen{"pen", {{20, 96}, {60, 104}, {100, 96}, {140, 104}, {180, 96}}, -1};
    page->getSelectedLayer()->addElement(makeStroke(band, StrokeTool::HIGHLIGHTER, 20, StrokeCapStyle::BUTT, false));
    page->getSelectedLayer()->addElement(makeStroke(pen, StrokeTool::PEN, 2, StrokeCapStyle::ROUND, false));
    const auto before = renderAlpha(page);
    ASSERT_GT(inkPixels(before), 0U);

    Document document(nullptr);
    UndoRedoHandler undo(nullptr);
    runGesture(page, ERASER_TYPE_DEFAULT, 6, {100, 80}, {100, 120}, undo, document);
    runGesture(page, ERASER_TYPE_DEFAULT, 6, {40, 100}, {60, 100}, undo, document);  // second, separate gesture
    const auto after = renderAlpha(page);

    // Everything within the first drag's capsule (x 94..106) is clear of both inks, and the second gesture too.
    for (int py = 0; py < SIZE * SCALE; ++py) {
        for (int px = 0; px < SIZE * SCALE; ++px) {
            const double x = (px + 0.5) / SCALE, y = (py + 0.5) / SCALE;
            const bool inFirst = x >= 94 + 1 && x <= 106 - 1 && y >= 80 && y <= 120;
            const bool inSecond = utn::eraser::distancePointSegment({x, y}, {40, 100}, {60, 100}) <= 6 - 1;
            if (inFirst || inSecond) {
                ASSERT_LE(after[static_cast<size_t>(py * SIZE * SCALE + px)], 8) << x << "," << y;
            }
        }
    }
    EXPECT_LT(inkPixels(after), inkPixels(before));

    ASSERT_TRUE(undo.canUndo());
    undo.undo();
    undo.undo();
    EXPECT_EQ(renderAlpha(page), before) << "undo must restore every pixel";
    undo.redo();
    undo.redo();
    EXPECT_EQ(renderAlpha(page), after) << "redo must reproduce the erased page exactly";
}

// Stroke eraser: any contact with the rendered ink deletes the whole element, for pens, highlighters and shapes.
TEST(UniversalEraser, strokeEraserDeletesAnyInkKindWhenTouchingRenderedInk) {
    std::vector<std::string> failures;
    for (const auto& shape: shapes()) {
        for (StrokeTool tool: {StrokeTool::PEN, StrokeTool::HIGHLIGHTER}) {
            for (double width: {1.5, 3.0, 12.0, 30.0}) {
                for (StrokeCapStyle cap: {StrokeCapStyle::ROUND, StrokeCapStyle::BUTT, StrokeCapStyle::SQUARE}) {
                    for (bool pressure: {false, true}) {
                        if (pressure && tool == StrokeTool::HIGHLIGHTER) continue;
                        const Point p0 = shape.pts[0], p1 = shape.pts[1];
                        const utn::eraser::Vec mid{(p0.x + p1.x) / 2, (p0.y + p1.y) / 2};
                        const double len = std::hypot(p1.x - p0.x, p1.y - p0.y);
                        const utn::eraser::Vec normal{-(p1.y - p0.y) / len, (p1.x - p0.x) / len};
                        // Disc just overlapping the ink edge: its centre sits half a width + 70% radius away
                        const double radius = 5;
                        const double inkHalfWidth = (pressure ? 0.4 * width : width) / 2;  // first segment's width
                        const double off = inkHalfWidth + radius * 0.7;
                        const utn::eraser::Vec at{mid.x + normal.x * off, mid.y + normal.y * off};
                        auto page = std::make_shared<XojPage>(SIZE, SIZE);
                        page->getSelectedLayer()->addElement(makeStroke(shape, tool, width, cap, pressure));
                        Document document(nullptr);
                        UndoRedoHandler undo(nullptr);
                        runGesture(page, ERASER_TYPE_DELETE_STROKE, radius, at, at, undo, document);
                        if (!page->getSelectedLayer()->getElements().empty()) {
                            failures.push_back(shape.name + "/" + (tool == StrokeTool::PEN ? "pen" : "highlighter") +
                                               "/w" + std::to_string(static_cast<int>(width)) + "/cap" +
                                               std::to_string(static_cast<int>(cap)) + (pressure ? "/pressure" : ""));
                        }
                    }
                }
            }
        }
    }
    for (const auto& f: failures) ADD_FAILURE() << "stroke eraser missed touching ink: " << f;
}

// Mode limitation, not a hit-test defect: the Object eraser must never remove ink (documented "Ink stays").
TEST(UniversalEraser, objectEraserLeavesAllInkAlone) {
    for (StrokeTool tool: {StrokeTool::PEN, StrokeTool::HIGHLIGHTER}) {
        auto page = std::make_shared<XojPage>(SIZE, SIZE);
        Shape line{"line", {{40, 100}, {160, 100}}, -1};
        page->getSelectedLayer()->addElement(makeStroke(line, tool, 12, StrokeCapStyle::ROUND, false));
        Document document(nullptr);
        UndoRedoHandler undo(nullptr);
        runGesture(page, ERASER_TYPE_DELETE_OBJECT, 10, {100, 100}, {100, 100}, undo, document);
        EXPECT_EQ(page->getSelectedLayer()->getElements().size(), 1U);
    }
}
