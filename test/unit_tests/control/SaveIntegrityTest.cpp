/*
 * Ultimate Teacher Notepad
 *
 * Sprint 31 Oct: a save must either keep every annotation or say that it did not, the expensive raster work for erased
 * strokes must not happen under the Document lock, and long eraser sweeps must stay cheap to draw and to store.
 *
 * Based on Xournal++ GPLv2+
 */
#include <cmath>
#include <limits>
#include <memory>
#include <string>
#include <vector>

#include <gtest/gtest.h>

#include "control/UtnPerf.h"
#include "control/xojfile/LoadHandler.h"
#include "control/xojfile/SaveHandler.h"
#include "model/Document.h"
#include "model/ExportAudience.h"
#include "model/Layer.h"
#include "model/Stroke.h"
#include "model/XojPage.h"
#include "model/eraser/CircularEraser.h"
#include "model/eraser/MaskedStrokeCodec.h"
#include "model/eraser/StrokeErasure.h"
#include "util/PathUtil.h"

namespace {
struct SavePage: XojPage {
    SavePage(): XojPage(400, 300) {}
};

std::unique_ptr<Stroke> erasedBar(double y = 50) {
    auto s = std::make_unique<Stroke>();
    s->setWidth(8);
    s->setColor(Color{20, 40, 60, 255});
    s->addPoint(Point(20, y));
    s->addPoint(Point(180, y));
    s->setErasures({{{100, y - 20}, {100, y + 20}, 12, {}}});
    return s;
}

std::unique_ptr<Document> loadBack(SaveHandler& saver, const fs::path& file) {
    saver.saveTo(file);
    if (!saver.getErrorMessage().empty()) {
        return nullptr;
    }
    auto reopened = LoadHandler{}.loadDocument(file);
    fs::remove(file);
    return reopened;
}
}  // namespace

// prepareSave runs under the shared Document lock, so it must only snapshot an erased stroke. If it held a pointer
// instead, a change between prepareSave and saveTo would corrupt the file (and the raster would be drawn from live data
// on a thread that holds no lock).
TEST(SaveIntegrity, erasedStrokeIsSnapshottedWhenPrepared) {
    Document doc(nullptr);
    auto page = std::make_shared<SavePage>();
    auto bar = erasedBar();
    Stroke* live = bar.get();
    page->getSelectedLayer()->addElement(std::move(bar));
    doc.addPage(page);

    const auto file = Util::getTmpDirSubfolder() / "utn-snapshot.xopp";
    SaveHandler saver;
    saver.prepareSave(&doc, file);
    live->move(1000, 1000);  // the document changes after the lock was released
    live->setErasures({});

    auto reopened = loadBack(saver, file);
    ASSERT_TRUE(reopened);
    EXPECT_TRUE(saver.getWarnings().empty());
    const auto& elements = reopened->getPage(0)->getSelectedLayer()->getElements();
    ASSERT_EQ(elements.size(), 1U);
    ASSERT_EQ(elements[0]->getType(), ELEMENT_STROKE);
    const auto* stroke = static_cast<const Stroke*>(elements[0].get());
    EXPECT_NEAR(stroke->getPointVector().front().x, 20, 1e-6) << "saved the state at prepareSave time";
    EXPECT_EQ(stroke->getErasures().size(), 1U) << "the erasure survived";
}

TEST(SaveIntegrity, resolvingTwiceIsHarmless) {
    Document doc(nullptr);
    auto page = std::make_shared<SavePage>();
    page->getSelectedLayer()->addElement(erasedBar());
    doc.addPage(page);
    const auto file = Util::getTmpDirSubfolder() / "utn-twice.xopp";
    SaveHandler saver;
    saver.prepareSave(&doc, file);
    saver.resolveDeferredStrokes();
    saver.resolveDeferredStrokes();
    auto reopened = loadBack(saver, file);
    ASSERT_TRUE(reopened);
    EXPECT_EQ(reopened->getPage(0)->getSelectedLayer()->getElements().size(), 1U);
}

namespace {
/// A stroke whose raster fallback cannot be drawn: damaged data with an infinite coordinate, which makes its bounding
/// box infinite (a NaN would be ignored by the min/max of the bounding box and draw fine)
std::unique_ptr<Stroke> damagedErasedStroke(bool leaveTwoUsablePoints) {
    auto s = std::make_unique<Stroke>();
    s->setWidth(6);
    s->addPoint(Point(10, 10));
    s->addPoint(Point(std::numeric_limits<double>::infinity(), 40));
    if (leaveTwoUsablePoints) {
        s->addPoint(Point(120, 60));
    }
    s->setErasures({{{50, 10}, {50, 80}, 9, {}}});
    return s;
}
}  // namespace

// One undrawable stroke used to make every later save and autosave fail, leaving the user's previous file the only copy.
// It must not stop the save, and it must never remove the other annotations on the page.
TEST(SaveIntegrity, unrenderableStrokeNeverBlocksTheSaveOrDropsOtherInk) {
    Document doc(nullptr);
    auto page = std::make_shared<SavePage>();
    page->getSelectedLayer()->addElement(erasedBar(80));
    page->getSelectedLayer()->addElement(damagedErasedStroke(true));
    page->getSelectedLayer()->addElement(erasedBar(200));
    doc.addPage(page);

    const auto file = Util::getTmpDirSubfolder() / "utn-damaged.xopp";
    SaveHandler saver;
    saver.prepareSave(&doc, file);
    auto reopened = loadBack(saver, file);
    ASSERT_TRUE(saver.getErrorMessage().empty()) << saver.getErrorMessage();
    ASSERT_TRUE(reopened);
    EXPECT_EQ(reopened->getPage(0)->getSelectedLayer()->getElements().size(), 3U) << "no annotation was dropped";
    EXPECT_EQ(saver.getWarnings().size(), 1U) << "the teacher is told that one stroke came back unerased";
}

TEST(SaveIntegrity, strokeWithNoUsablePointsIsReportedNotSilentlyLost) {
    Document doc(nullptr);
    auto page = std::make_shared<SavePage>();
    page->getSelectedLayer()->addElement(erasedBar());
    page->getSelectedLayer()->addElement(damagedErasedStroke(false));
    doc.addPage(page);

    const auto file = Util::getTmpDirSubfolder() / "utn-nopoints.xopp";
    SaveHandler saver;
    saver.prepareSave(&doc, file);
    auto reopened = loadBack(saver, file);
    ASSERT_TRUE(reopened);
    EXPECT_EQ(reopened->getPage(0)->getSelectedLayer()->getElements().size(), 1U);
    ASSERT_EQ(saver.getWarnings().size(), 1U);
}

// The erased part of a stroke may have been private. A student copy must never get it back, even as a fallback.
TEST(SaveIntegrity, studentCopyNeverRestoresErasedInk) {
    Document doc(nullptr);
    auto page = std::make_shared<SavePage>();
    page->getSelectedLayer()->addElement(damagedErasedStroke(true));
    doc.addPage(page);

    const auto file = Util::getTmpDirSubfolder() / "utn-student-damaged.xopp";
    SaveHandler saver;
    saver.setExportAudience(ExportAudience::Student);
    saver.prepareSave(&doc, file);
    auto reopened = loadBack(saver, file);
    ASSERT_TRUE(saver.getErrorMessage().empty()) << saver.getErrorMessage();
    ASSERT_TRUE(reopened);
    EXPECT_TRUE(reopened->getPage(0)->getSelectedLayer()->getElements().empty());
    EXPECT_EQ(saver.getWarnings().size(), 1U);
}

TEST(SaveIntegrity, rasterFallbackStillWorksForOrdinaryStrokes) {
    auto bar = erasedBar();
    auto fallback = utn::eraser::rasterizeMaskedStroke(*bar);
    ASSERT_TRUE(fallback.has_value());
    EXPECT_GT(cairo_image_surface_get_width(fallback->surface.get()), 0);
}

// --- eraser sweeps -----------------------------------------------------------------------------------------------

namespace {
/// Capsules of a curved sweep along a circular arc, one per pointer event as EraseHandler adds them
void sweepArc(Stroke& stroke, double radius, double arcRadius, int samples, double sweepRadians) {
    xoj::util::Point<double> previous{arcRadius + 200, 200};
    for (int i = 1; i <= samples; ++i) {
        const double angle = sweepRadians * i / samples;
        const xoj::util::Point<double> next{200 + arcRadius * std::cos(angle), 200 + arcRadius * std::sin(angle)};
        stroke.addErasure({previous, next, radius, {}});
        previous = next;
    }
}

double axisDistance(const xoj::util::Point<double>& p, const Stroke& stroke) {
    double best = 1e300;
    for (const auto& e: stroke.getErasures()) {
        best = std::min(best, utn::eraser::distancePointSegment({p.x, p.y}, {e.start.x, e.start.y}, {e.end.x, e.end.y}));
    }
    return best;
}
}  // namespace

TEST(EraserCoalescing, aCurvedSweepStaysCompact) {
    Stroke stroke;
    constexpr double radius = 10;
    sweepArc(stroke, radius, 150, 2000, 2.5);
    // 2000 pointer events used to leave 2000 capsules, because only an exactly straight sweep was merged
    EXPECT_LT(stroke.getErasures().size(), 200U);
    EXPECT_LE(stroke.getErasures().size(), utn::eraser::MASKED_STROKE_MAX_ERASURES);
}

TEST(EraserCoalescing, mergedCapsulesStayWithinTheStatedToleranceOfEverySample) {
    Stroke stroke;
    constexpr double radius = 12;
    constexpr double arcRadius = 120;
    constexpr int samples = 1500;
    constexpr double sweep = 3.0;
    sweepArc(stroke, radius, arcRadius, samples, sweep);

    const double tolerance = Stroke::ERASURE_MERGE_TOLERANCE * radius;
    for (int i = 0; i <= samples; ++i) {
        const double angle = sweep * i / samples;
        const xoj::util::Point<double> sample{200 + arcRadius * std::cos(angle), 200 + arcRadius * std::sin(angle)};
        EXPECT_LE(axisDistance(sample, stroke), tolerance + 1e-9) << "sample " << i << " strays from the erased path";
    }
}

TEST(EraserCoalescing, aSharpTurnStartsANewCapsule) {
    Stroke stroke;
    stroke.addErasure({{0, 0}, {100, 0}, 10, {}});
    stroke.addErasure({{100, 0}, {100, 100}, 10, {}});  // 90 degrees
    EXPECT_EQ(stroke.getErasures().size(), 2U);
    stroke.addErasure({{100, 100}, {0, 100}, 10, {}});  // another 90 degrees
    EXPECT_EQ(stroke.getErasures().size(), 3U);
}

TEST(EraserCoalescing, aReversalIsNeverMerged) {
    Stroke stroke;
    stroke.addErasure({{0, 0}, {100, 0}, 10, {}});
    stroke.addErasure({{100, 0}, {0, 0}, 10, {}});
    EXPECT_EQ(stroke.getErasures().size(), 2U);
}

TEST(EraserCoalescing, differentRadiiAreNeverMerged) {
    Stroke stroke;
    stroke.addErasure({{0, 0}, {50, 0}, 10, {}});
    stroke.addErasure({{50, 0}, {100, 0}, 11, {}});
    EXPECT_EQ(stroke.getErasures().size(), 2U);
}

TEST(EraserCoalescing, aStraightSweepIsStillOneCapsule) {
    Stroke stroke;
    for (int i = 0; i < 500; ++i) {
        stroke.addErasure({{i * 2.0, 5}, {(i + 1) * 2.0, 5}, 8, {}});
    }
    // One capsule per ERASURE_MERGE_MAX_VERTICES samples at most, so 500 samples need two or three
    EXPECT_GE(stroke.getErasures().size(), 2U);
    EXPECT_LE(stroke.getErasures().size(), 3U);
}

// --- perf counters -----------------------------------------------------------------------------------------------

TEST(PerfCounters, recordsOnlyWhenEnabledAndReportsPercentiles) {
    utn::perf::setEnabled(false);
    utn::perf::reset();
    utn::perf::record(utn::perf::Metric::PenUp, 5);
    EXPECT_EQ(utn::perf::stats(utn::perf::Metric::PenUp).count, 0U);

    utn::perf::setEnabled(true);
    for (int i = 1; i <= 100; ++i) {
        utn::perf::record(utn::perf::Metric::PenUp, i);
    }
    const auto stats = utn::perf::stats(utn::perf::Metric::PenUp);
    EXPECT_EQ(stats.count, 100U);
    EXPECT_DOUBLE_EQ(stats.max, 100);
    EXPECT_NEAR(stats.mean(), 50.5, 1e-9);
    EXPECT_NEAR(stats.p95, 95, 1.0);
    EXPECT_NE(utn::perf::report().find("pen_up_ms"), std::string::npos);

    utn::perf::reset();
    utn::perf::setEnabled(false);
}
