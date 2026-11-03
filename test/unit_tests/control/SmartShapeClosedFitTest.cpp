/*
 * UTN Smart Shape: scale invariance, gapped and rotated contours, handwriting negatives.
 * Pure deterministic geometry; no GTK and no real profile writes. GPLv2+.
 */
#include <gtest/gtest.h>
#include <cmath>
#include <vector>
#include "control/shaperecognizer/ClosedStrokeFit.h"

namespace {
using utn::smartshape::Point2;
using utn::smartshape::Kind;
using utn::smartshape::fitClosedStroke;

std::vector<Point2> polygon(const std::vector<Point2>& corners, double scale, double rotate,
                            double missing, double jitter) {
    std::vector<Point2> out;
    for (size_t e = 0; e < corners.size(); ++e) {
        Point2 a = corners[e], b = corners[(e+1)%corners.size()];
        for (int i = 0; i < 28; ++i) {
            if (e+1 == corners.size() && double(i)/28 > 1-missing) break;
            double t=double(i)/28, x=scale*(a.x+t*(b.x-a.x)), y=scale*(a.y+t*(b.y-a.y));
            double n=out.size()+1.0;
            out.push_back({100 + x*std::cos(rotate)-y*std::sin(rotate)+jitter*std::sin(7.2*n),
                           100 + x*std::sin(rotate)+y*std::cos(rotate)+jitter*std::cos(6.5*n)});
        }
    }
    return out;
}
std::vector<Point2> oval(double rx, double ry, double rotation, double turns, double jitter) {
    std::vector<Point2> out;
    for (int i=0; i<150; ++i) {
        double t=turns*2*utn::smartshape::detail::PI*i/149;
        double x=rx*std::cos(t)+jitter*std::cos(i*1.3);
        double y=ry*std::sin(t)+jitter*std::sin(i*1.7);
        out.push_back({100+x*std::cos(rotation)-y*std::sin(rotation),
                       100+x*std::sin(rotation)+y*std::cos(rotation)});
    }
    return out;
}
void expectKind(const std::vector<Point2>& trace, Kind expected) {
    const auto fitted=fitClosedStroke(trace);
    ASSERT_TRUE(fitted.has_value());
    EXPECT_EQ(fitted->kind,expected);
    EXPECT_LT(fitted->relativeError,0.034);
    ASSERT_GE(fitted->outline.size(),4U);
    EXPECT_NEAR(fitted->outline.front().x,fitted->outline.back().x,1e-7);
    EXPECT_NEAR(fitted->outline.front().y,fitted->outline.back().y,1e-7);
}
}
TEST(SmartShapeClosedFit, SmallGappedRotatedRectangle) {
    expectKind(polygon({{0,0},{1,0},{1,.55},{0,.55}},24,.45,.08,.14),Kind::Rectangle);
}
TEST(SmartShapeClosedFit, LargeNoisyRectangleIsScaleInvariant) {
    expectKind(polygon({{0,0},{1,0},{1,.55},{0,.55}},120,.68,.08,.25),Kind::Rectangle);
}
TEST(SmartShapeClosedFit, SmallGappedRotatedTriangle) {
    expectKind(polygon({{0,-1},{.9,.8},{-.9,.8}},15,.35,.10,.10),Kind::Triangle);
}
TEST(SmartShapeClosedFit, NoisyGappedCircle) {
    expectKind(oval(9,9,.1,.92,.4),Kind::Circle);
}
TEST(SmartShapeClosedFit, RotatedEllipseIsNotFlattenedIntoCircle) {
    expectKind(oval(15,9,.55,.92,.22),Kind::Ellipse);
}
TEST(SmartShapeClosedFit, RejectsHandwritingAndScribbles) {
    EXPECT_FALSE(fitClosedStroke({{0,0},{3,5},{6,10},{9,15},{12,20},{15,15},{18,10},{21,5},{24,0}}));
    EXPECT_FALSE(fitClosedStroke({{0,0},{10,9},{20,0},{30,9},{40,0},{50,9}}));
    EXPECT_FALSE(fitClosedStroke({{0,0},{12,0},{0,12},{12,12},{0,0},{15,15}}));
}
TEST(SmartShapeClosedFit, DoesNotPromoteTinyDots) {
    EXPECT_FALSE(fitClosedStroke(oval(2,2,0,1,0)));
}
