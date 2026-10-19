#include <gtest/gtest.h>
#include <cairo.h>
#include "model/eraser/StrokeErasure.h"

using utn::eraser::StrokeErasure;

TEST(StrokeErasureGeometry, CapsuleIncludesRoundEndsButNotSquareCorners) {
    auto* surface = cairo_image_surface_create(CAIRO_FORMAT_A8, 100, 100);
    auto* cr = cairo_create(surface);
    utn::eraser::appendErasurePath(cr, {{20, 50}, {80, 50}, 5, {}});
    EXPECT_TRUE(cairo_in_fill(cr, 50, 50));
    EXPECT_TRUE(cairo_in_fill(cr, 16, 50));
    EXPECT_TRUE(cairo_in_fill(cr, 84, 50));
    EXPECT_FALSE(cairo_in_fill(cr, 15.1, 54.9));
    EXPECT_FALSE(cairo_in_fill(cr, 50, 55.1));
    cairo_destroy(cr);
    cairo_surface_destroy(surface);
}

TEST(StrokeErasureGeometry, OverlappingErasedRegionsRemainErased) {
    auto* surface = cairo_image_surface_create(CAIRO_FORMAT_A8, 100, 100);
    auto* cr = cairo_create(surface);
    const std::vector<StrokeErasure> masks{{{20, 50}, {80, 50}, 10, {}}, {{50, 20}, {50, 80}, 10, {}}};
    utn::eraser::clipErasures(cr, masks, {0, 0, 100, 100});
    EXPECT_FALSE(cairo_in_clip(cr, 50, 50));
    EXPECT_FALSE(cairo_in_clip(cr, 50, 25));
    EXPECT_FALSE(cairo_in_clip(cr, 25, 50));
    EXPECT_TRUE(cairo_in_clip(cr, 20, 20));
    cairo_destroy(cr);
    cairo_surface_destroy(surface);
}

TEST(StrokeErasureGeometry, AffineResizeFollowsTheHoleWithoutUsingZoomPixels) {
    auto* surface = cairo_image_surface_create(CAIRO_FORMAT_A8, 100, 100);
    auto* cr = cairo_create(surface);
    StrokeErasure disc{{0, 0}, {0, 0}, 2, xoj::util::Matrix::TRANSLATION(50, 50).scale(3, 0.5)};
    utn::eraser::appendErasurePath(cr, disc);
    EXPECT_TRUE(cairo_in_fill(cr, 55, 50));
    EXPECT_TRUE(cairo_in_fill(cr, 50, 50.8));
    EXPECT_FALSE(cairo_in_fill(cr, 50, 51.2));
    EXPECT_FALSE(cairo_in_fill(cr, 56.2, 50));
    cairo_destroy(cr);
    cairo_surface_destroy(surface);
}
