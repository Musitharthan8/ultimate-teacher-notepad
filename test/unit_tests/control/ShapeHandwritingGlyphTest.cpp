/*
 * Ultimate Teacher Notepad
 *
 * Sprint 31 Oct: the Stroke recognizer must not tidy handwriting. ShapeRecognizerCorpusTest holds doodles (V, Z, waves,
 * spirals...). This file adds what a teacher actually writes with the recognizer switched on: letters, numerals and
 * underlines, as one-stroke renditions at classroom sizes, at three levels of sloppiness. Deterministic (own PRNG).
 *
 * "o", "O" and "0" are deliberately NOT in the zero-tolerance group: a handwritten o IS a circle, so no geometry can
 * tell them apart. The Stroke recognizer is an explicit mode for that reason. They are reported, not gated.
 *
 * Based on Xournal++ GPLv2+
 */
#include <gtest/gtest.h>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <iomanip>
#include <iostream>
#include <map>
#include <memory>
#include <string>
#include <vector>

#include "control/shaperecognizer/ShapeRecognizer.h"
#include "model/Point.h"
#include "model/Stroke.h"

namespace {
struct Rng {
    uint64_t s;
    explicit Rng(uint64_t seed): s(seed * 2685821657736338717ULL + 88172645463325252ULL) {}
    uint64_t next() {
        s ^= s >> 12;
        s ^= s << 25;
        s ^= s >> 27;
        return s * 2685821657736338717ULL;
    }
    double uniform() { return static_cast<double>(next() >> 11) / 9007199254740992.0; }
    double range(double a, double b) { return a + (b - a) * uniform(); }
    double gauss() {
        const double u1 = std::max(1e-12, uniform()), u2 = uniform();
        return std::sqrt(-2.0 * std::log(u1)) * std::cos(2.0 * M_PI * u2);
    }
};

using Pt = std::pair<double, double>;
using Path = std::vector<Pt>;

Path arc(double cx, double cy, double rx, double ry, double a0, double a1, int n = 36) {
    Path p;
    for (int i = 0; i <= n; ++i) {
        const double a = a0 + (a1 - a0) * i / n;
        p.push_back({cx + rx * std::cos(a), cy + ry * std::sin(a)});
    }
    return p;
}
Path join(std::initializer_list<Path> parts) {
    Path out;
    for (const auto& part: parts) out.insert(out.end(), part.begin(), part.end());
    return out;
}
Path poly(std::initializer_list<Pt> pts) { return Path(pts); }

struct Glyph {
    std::string name;
    Path path;           // unit square, y down
    bool mayBecomeLine;  // a straight glyph: a perfectly straight line is an acceptable result
    bool ambiguous;      // geometrically a real shape (o, O, 0): reported, not gated
    double aspect;       // width / height of the glyph box
};

std::vector<Glyph> glyphs() {
    const double PI = M_PI;
    std::vector<Glyph> g;
    // Letters with a loop and a tail or stem
    g.push_back({"a", join({arc(0.45, 0.6, 0.32, 0.38, -0.4, 2 * PI - 0.4), poly({{0.78, 0.25}, {0.8, 0.98}})}), false, false, 0.7});
    g.push_back({"d", join({arc(0.42, 0.62, 0.32, 0.36, 0, 2 * PI), poly({{0.74, 0.0}, {0.74, 1.0}})}), false, false, 0.7});
    g.push_back({"b", join({poly({{0.2, 0.0}, {0.2, 1.0}}), arc(0.5, 0.65, 0.3, 0.33, PI, 3 * PI)}), false, false, 0.7});
    g.push_back({"p", join({poly({{0.2, 0.3}, {0.2, 1.1}, {0.2, 0.4}}), arc(0.5, 0.58, 0.3, 0.28, PI, 3 * PI)}), false, false, 0.7});
    g.push_back({"q", join({arc(0.42, 0.58, 0.3, 0.28, 0, 2 * PI), poly({{0.72, 0.3}, {0.72, 1.1}})}), false, false, 0.7});
    g.push_back({"e", join({poly({{0.1, 0.5}, {0.9, 0.5}}), arc(0.5, 0.5, 0.4, 0.4, 0, 1.7 * PI)}), false, false, 0.8});
    g.push_back({"6", join({poly({{0.75, 0.0}, {0.45, 0.2}, {0.28, 0.5}}), arc(0.5, 0.7, 0.26, 0.26, PI, 3 * PI)}), false, false, 0.6});
    g.push_back({"9", join({arc(0.5, 0.3, 0.26, 0.26, 0, 2 * PI), poly({{0.76, 0.3}, {0.7, 0.7}, {0.4, 1.0}})}), false, false, 0.6});
    g.push_back({"g", join({arc(0.42, 0.45, 0.3, 0.3, 0, 2 * PI), poly({{0.72, 0.2}, {0.72, 0.95}, {0.5, 1.15}, {0.2, 1.05}})}), false, false, 0.7});
    // Letters made of a stem and a bowl, closed or almost closed outlines
    g.push_back({"D", join({poly({{0.1, 0.0}, {0.1, 1.0}}), arc(0.1, 0.5, 0.78, 0.5, PI / 2, -PI / 2, 24)}), false, false, 0.9});
    g.push_back({"P", join({poly({{0.15, 1.0}, {0.15, 0.0}}), arc(0.15, 0.27, 0.62, 0.27, -PI / 2, PI / 2, 20), poly({{0.15, 0.54}})}), false, false, 0.7});
    g.push_back({"B", join({poly({{0.1, 1.0}, {0.1, 0.0}}), arc(0.1, 0.25, 0.65, 0.25, -PI / 2, PI / 2, 18), arc(0.1, 0.75, 0.75, 0.25, -PI / 2, PI / 2, 18)}), false, false, 0.8});
    g.push_back({"R", join({poly({{0.15, 1.0}, {0.15, 0.0}}), arc(0.15, 0.25, 0.6, 0.25, -PI / 2, PI / 2, 18), poly({{0.15, 0.5}, {0.8, 1.0}})}), false, false, 0.7});
    g.push_back({"A", poly({{0.05, 1.0}, {0.5, 0.0}, {0.95, 1.0}, {0.8, 0.65}, {0.2, 0.65}}), false, false, 0.9});
    g.push_back({"4", poly({{0.7, 0.0}, {0.05, 0.7}, {0.95, 0.7}, {0.7, 0.7}, {0.7, 1.0}}), false, false, 0.9});
    // Numerals and letters with strokes
    g.push_back({"2", join({arc(0.5, 0.28, 0.35, 0.28, PI, 2.25 * PI, 18), poly({{0.1, 1.0}, {0.9, 1.0}})}), false, false, 0.7});
    g.push_back({"3", join({arc(0.45, 0.25, 0.33, 0.25, -0.9 * PI, 0.5 * PI, 18), arc(0.45, 0.72, 0.4, 0.27, -0.5 * PI, 0.9 * PI, 20)}), false, false, 0.7});
    g.push_back({"5", join({poly({{0.85, 0.0}, {0.2, 0.0}, {0.15, 0.45}}), arc(0.45, 0.72, 0.4, 0.27, -0.6 * PI, 0.8 * PI, 22)}), false, false, 0.7});
    g.push_back({"7", poly({{0.05, 0.0}, {0.95, 0.0}, {0.4, 1.0}}), false, false, 0.8});
    g.push_back({"M", poly({{0.0, 1.0}, {0.0, 0.0}, {0.5, 0.6}, {1.0, 0.0}, {1.0, 1.0}}), false, false, 1.0});
    g.push_back({"K", poly({{0.15, 0.0}, {0.15, 1.0}, {0.15, 0.55}, {0.85, 0.0}, {0.15, 0.55}, {0.85, 1.0}}), false, false, 0.7});
    g.push_back({"h", join({poly({{0.15, 0.0}, {0.15, 1.0}, {0.15, 0.55}}), arc(0.5, 0.65, 0.35, 0.3, PI, 2 * PI, 14), poly({{0.85, 1.0}})}), false, false, 0.7});
    g.push_back({"m", join({poly({{0.1, 1.0}, {0.1, 0.4}}), arc(0.3, 0.55, 0.2, 0.15, PI, 2 * PI, 10), arc(0.7, 0.55, 0.2, 0.15, PI, 2 * PI, 10), poly({{0.9, 1.0}})}), false, false, 1.0});
    g.push_back({"tick-tick", poly({{0.0, 0.55}, {0.3, 1.0}, {0.55, 0.3}, {0.7, 1.0}, {1.0, 0.0}}), false, false, 1.0});
    // Straight glyphs: becoming a perfectly straight line is fine
    g.push_back({"l", poly({{0.5, 0.0}, {0.5, 1.0}}), true, false, 0.1});
    g.push_back({"I", poly({{0.5, 0.0}, {0.5, 1.0}}), true, false, 0.1});
    g.push_back({"slash", poly({{0.1, 1.0}, {0.9, 0.0}}), true, false, 0.8});
    g.push_back({"dash", poly({{0.0, 0.5}, {1.0, 0.5}}), true, false, 1.0});
    g.push_back({"1", poly({{0.25, 0.25}, {0.6, 0.0}, {0.6, 1.0}}), true, false, 0.35});
    // Round glyphs: ambiguous with a real circle
    g.push_back({"o", arc(0.5, 0.5, 0.5, 0.5, -1.5, 2 * PI - 1.5 + 0.12, 36), false, true, 1.0});
    g.push_back({"O", arc(0.5, 0.5, 0.5, 0.5, 0.3, 2 * PI + 0.45, 40), false, true, 1.0});
    g.push_back({"0", arc(0.5, 0.5, 0.36, 0.5, 1.0, 2 * PI + 1.2, 40), false, true, 0.7});
    return g;
}

struct Quality {
    const char* name;
    double wobble;
    double tremor;
};
const Quality QUALITIES[] = {{"neat", 0.004, 0.2}, {"typical", 0.012, 0.45}, {"sloppy", 0.025, 0.8}};
// Letter heights in page units: a small note, normal board writing, large headline
const double HEIGHTS[] = {30.0, 60.0, 120.0};

Path densify(const Path& v, double spacing, Rng& rng) {
    std::vector<double> cum{0};
    for (size_t i = 1; i < v.size(); ++i) {
        cum.push_back(cum.back() + std::hypot(v[i].first - v[i - 1].first, v[i].second - v[i - 1].second));
    }
    const double len = cum.back();
    Path out;
    for (double d = 0; d < len; d += spacing * rng.range(0.6, 1.4)) {
        size_t i = 1;
        while (i + 1 < cum.size() && cum[i] < d) ++i;
        const double t = (d - cum[i - 1]) / std::max(1e-9, cum[i] - cum[i - 1]);
        out.push_back({v[i - 1].first + (v[i].first - v[i - 1].first) * t,
                       v[i - 1].second + (v[i].second - v[i - 1].second) * t});
    }
    out.push_back(v.back());
    return out;
}

std::unique_ptr<Stroke> render(const Glyph& glyph, double height, const Quality& q, double slant, Rng& rng) {
    // The glyph box is height tall; slant is handwriting italic, plus a little rotation
    Path scaled;
    for (const auto& p: glyph.path) {
        const double x = (p.first - 0.5) * height * glyph.aspect + (0.5 - p.second) * height * slant;
        const double y = (p.second - 0.5) * height;
        scaled.push_back({100 + x, 100 + y});
    }
    const double spacing = height < 40 ? 1.2 : (height < 90 ? 2.0 : 3.5);
    Path dense = densify(scaled, spacing, rng);
    const double f1 = rng.range(0.7, 2.0), p1 = rng.range(0, 2 * M_PI);
    auto s = std::make_unique<Stroke>();
    for (size_t i = 0; i < dense.size(); ++i) {
        const double u = static_cast<double>(i) / std::max<size_t>(1, dense.size() - 1);
        const double drift = q.wobble * height * std::sin(2 * M_PI * f1 * u + p1);
        s->addPoint(Point(dense[i].first + drift + q.tremor * rng.gauss(), dense[i].second + drift + q.tremor * rng.gauss()));
    }
    return s;
}

enum class Outcome { Freehand, Line, Shape };
Outcome classify(const Stroke* result) {
    if (!result) return Outcome::Freehand;
    return result->getPointCount() == 2 ? Outcome::Line : Outcome::Shape;
}
}  // namespace

TEST(ShapeGlyphs, handwritingStaysHandwriting) {
    struct Tally {
        size_t total = 0, freehand = 0, line = 0, shape = 0;
    };
    std::map<std::string, Tally> byGlyph;
    double worstMs = 0;
    size_t tidyWritingConverted = 0;  // neat or typical handwriting turned into a shape: never acceptable
    uint64_t seed = 9000;
    for (const auto& glyph: glyphs()) {
        for (double height: HEIGHTS) {
            for (const auto& q: QUALITIES) {
                for (int rep = 0; rep < 10; ++rep) {
                    Rng rng(seed++);
                    auto stroke = render(glyph, height, q, rng.range(-0.25, 0.25), rng);
                    ShapeRecognizer reco;
                    const auto start = std::chrono::steady_clock::now();
                    auto result = reco.recognizePatterns(stroke.get(), 0.0);
                    worstMs = std::max(worstMs, std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count());
                    auto& t = byGlyph[glyph.name];
                    ++t.total;
                    switch (classify(result.get())) {
                        case Outcome::Freehand: ++t.freehand; break;
                        case Outcome::Line: ++t.line; break;
                        case Outcome::Shape:
                            ++t.shape;
                            if (!glyph.ambiguous) {
                                std::cout << "  shape from " << glyph.name << " height " << height << " " << q.name
                                          << " rep " << rep << " -> " << result->getPointCount() << " points\n";
                                if (std::string(q.name) != "sloppy") {
                                    ++tidyWritingConverted;
                                }
                            }
                            break;
                    }
                }
            }
        }
    }

    std::cout << std::left << std::setw(11) << "glyph" << std::setw(7) << "total" << std::setw(10) << "freehand"
              << std::setw(7) << "line" << "shape\n";
    for (const auto& [name, t]: byGlyph) {
        std::cout << std::setw(11) << name << std::setw(7) << t.total << std::setw(10) << t.freehand << std::setw(7)
                  << t.line << t.shape << "\n";
    }
    std::cout << "slowest recognizePatterns: " << worstMs << " ms\n";

    for (const auto& glyph: glyphs()) {
        const auto& t = byGlyph[glyph.name];
        if (glyph.ambiguous) {
            continue;  // reported above, not gated
        }
        // A small, sloppy D really is a circle with a flat side. At most one such case per glyph is tolerated (measured:
        // D, 1 in 90); neat and typical writing must never change (checked below).
        EXPECT_LE(t.shape, 1U) << glyph.name << " was turned into a rectangle, triangle or circle too often";
        if (!glyph.mayBecomeLine) {
            EXPECT_EQ(t.line, 0U) << glyph.name << " was turned into a straight line";
        }
    }
    EXPECT_EQ(tidyWritingConverted, 0U) << "neat or typical handwriting was turned into a shape";
    // Recognition must stay cheap enough for pen-up on the UI thread
    EXPECT_LT(worstMs, 25.0);
}

// --- robustness of the shapes themselves -------------------------------------------------------------------------

namespace {
Path circlePath(double radius, int n) {
    Path p;
    for (int i = 0; i <= n; ++i) {
        const double a = 2 * M_PI * i / n;
        p.push_back({100 + radius * std::cos(a), 100 + radius * std::sin(a)});
    }
    return p;
}
Path rectanglePath(double w, double h, int perSide) {
    Path p;
    auto side = [&](Pt a, Pt b) {
        for (int i = 0; i < perSide; ++i) {
            const double t = static_cast<double>(i) / perSide;
            p.push_back({a.first + (b.first - a.first) * t, a.second + (b.second - a.second) * t});
        }
    };
    const Pt a{100 - w / 2, 100 - h / 2}, b{100 + w / 2, 100 - h / 2}, c{100 + w / 2, 100 + h / 2}, d{100 - w / 2, 100 + h / 2};
    side(a, b);
    side(b, c);
    side(c, d);
    side(d, a);
    p.push_back(a);
    return p;
}
Path trianglePath(double s, int perSide) {
    Path p;
    const Pt a{100, 100 - s / 2}, b{100 + s / 2, 100 + s / 2}, c{100 - s / 2, 100 + s / 2};
    auto side = [&](Pt u, Pt v) {
        for (int i = 0; i < perSide; ++i) {
            const double t = static_cast<double>(i) / perSide;
            p.push_back({u.first + (v.first - u.first) * t, u.second + (v.second - u.second) * t});
        }
    };
    side(a, b);
    side(b, c);
    side(c, a);
    p.push_back(a);
    return p;
}
std::unique_ptr<Stroke> strokeOf(const Path& path, double scale = 1.0) {
    auto s = std::make_unique<Stroke>();
    for (const auto& p: path) s->addPoint(Point(100 + (p.first - 100) * scale, 100 + (p.second - 100) * scale));
    return s;
}
size_t pointsOf(const std::unique_ptr<Stroke>& result) { return result ? result->getPointCount() : 0; }
}  // namespace

// A pen at 240 Hz on a slow circle produces thousands of points. Recognition runs at pen-up on the UI thread.
TEST(ShapeRobustness, veryDenseStrokesAreRecognisedQuicklyAndCorrectly) {
    for (int n: {200, 2000, 8000}) {
        struct Case {
            const char* name;
            Path path;
            bool circle;
        } cases[] = {{"circle", circlePath(60, n), true},
                     {"rectangle", rectanglePath(120, 80, n / 4), false},
                     {"triangle", trianglePath(120, n / 3), false}};
        for (auto& c: cases) {
            auto stroke = strokeOf(c.path);
            ShapeRecognizer reco;
            const auto start = std::chrono::steady_clock::now();
            auto result = reco.recognizePatterns(stroke.get(), 0.0);
            const double ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count();
            EXPECT_TRUE(result) << c.name << " with " << n << " points was not recognised";
            EXPECT_LT(ms, 50.0) << c.name << " with " << n << " points took " << ms << " ms";
            if (result && !c.circle) {
                EXPECT_LE(pointsOf(result), 5U) << c.name;
            }
        }
    }
}

// A quick flick leaves very few samples. Sparse input may be left alone but must never become a different shape.
TEST(ShapeRobustness, sparseFastStrokesNeverBecomeADifferentShape) {
    for (int perSide: {1, 2, 3, 5}) {
        auto rect = strokeOf(rectanglePath(120, 80, perSide));
        auto tri = strokeOf(trianglePath(120, perSide));
        ShapeRecognizer r1, r2;
        auto rectResult = r1.recognizePatterns(rect.get(), 0.0);
        auto triResult = r2.recognizePatterns(tri.get(), 0.0);
        if (rectResult) {
            EXPECT_TRUE(pointsOf(rectResult) == 5U || pointsOf(rectResult) == 4U)
                    << "a rectangle flick with " << perSide << " points per side became " << pointsOf(rectResult);
        }
        if (triResult) {
            EXPECT_EQ(pointsOf(triResult), 4U) << "a triangle flick with " << perSide << " points per side";
        }
    }
}

// Coordinates are page units, whatever the zoom: the same shape drawn bigger or smaller (the pen travels the same
// fraction of the screen) must give the same answer, except below the minimum size where ink is left alone.
TEST(ShapeRobustness, theAnswerDoesNotDependOnTheSizeOfTheShape) {
    for (double scale: {0.5, 1.0, 2.0, 4.0}) {
        struct Case {
            const char* name;
            Path path;
            size_t expectedPoints;
        } cases[] = {{"rectangle", rectanglePath(120, 80, 30), 5}, {"triangle", trianglePath(120, 30), 4}};
        for (auto& c: cases) {
            auto stroke = strokeOf(c.path, scale);
            ShapeRecognizer reco;
            auto result = reco.recognizePatterns(stroke.get(), 0.0);
            ASSERT_TRUE(result) << c.name << " at scale " << scale;
            EXPECT_EQ(pointsOf(result), c.expectedPoints) << c.name << " at scale " << scale;
        }
        auto circle = strokeOf(circlePath(60, 120), scale);
        ShapeRecognizer reco;
        auto result = reco.recognizePatterns(circle.get(), 0.0);
        ASSERT_TRUE(result) << "circle at scale " << scale;
        EXPECT_GT(pointsOf(result), 5U) << "a circle stays a circle at scale " << scale;
    }
}

TEST(ShapeRobustness, shapesBelowTheMinimumSizeAreLeftAlone) {
    constexpr double minimumSize = 18.0;  // Settings default for strokeRecognizerMinSize
    auto tiny = strokeOf(rectanglePath(120, 80, 30), 0.1);  // a 14-unit diagonal
    ShapeRecognizer reco;
    EXPECT_FALSE(reco.recognizePatterns(tiny.get(), minimumSize));
    auto big = strokeOf(rectanglePath(120, 80, 30), 1.0);
    ShapeRecognizer reco2;
    EXPECT_TRUE(reco2.recognizePatterns(big.get(), minimumSize));
}
