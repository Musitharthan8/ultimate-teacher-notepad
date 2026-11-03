/*
 * Ultimate Teacher Notepad - scale-aware fit for a single nearly closed Smart Shape stroke.
 * Pure C++17 geometry; no GTK, settings or document state. GPLv2+.
 */
#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <optional>
#include <vector>

namespace utn::smartshape {
struct Point2 {
    double x = 0.0;
    double y = 0.0;
};
enum class Kind { Triangle, Rectangle, Circle, Ellipse };
struct Fit {
    Kind kind;
    std::vector<Point2> outline;
    double relativeError = 0.0;
};
namespace detail {
constexpr double PI = 3.14159265358979323846;
inline Point2 sub(Point2 a, Point2 b) { return {a.x - b.x, a.y - b.y}; }
inline double dot(Point2 a, Point2 b) { return a.x * b.x + a.y * b.y; }
inline double cross(Point2 a, Point2 b) { return a.x * b.y - a.y * b.x; }
inline double len(Point2 a) { return std::hypot(a.x, a.y); }
inline double distance(Point2 a, Point2 b) { return len(sub(a, b)); }
inline double segmentDistance(Point2 p, Point2 a, Point2 b) {
    const Point2 v = sub(b, a);
    const double vv = dot(v, v);
    const double t = vv > 0 ? std::clamp(dot(sub(p, a), v) / vv, 0.0, 1.0) : 0.0;
    return distance(p, {a.x + v.x * t, a.y + v.y * t});
}
inline bool solve5(std::array<std::array<double, 6>, 5>& matrix, std::array<double, 5>& sol) {
    for (int i = 0; i < 5; ++i) {
        int pivot = i;
        for (int j = i + 1; j < 5; ++j)
            if (std::abs(matrix[j][i]) > std::abs(matrix[pivot][i])) pivot = j;
        if (std::abs(matrix[pivot][i]) < 1e-10) return false;
        if (pivot != i) std::swap(matrix[i], matrix[pivot]);
        const double div = matrix[i][i];
        for (int c = i; c < 6; ++c) matrix[i][c] /= div;
        for (int j = 0; j < 5; ++j) {
            if (j == i) continue;
            const double f = matrix[j][i];
            for (int c = i; c < 6; ++c) matrix[j][c] -= f * matrix[i][c];
        }
    }
    for (int i = 0; i < 5; ++i) sol[i] = matrix[i][5];
    return true;
}
inline std::vector<Point2> resample(const std::vector<Point2>& src, double length) {
    constexpr int COUNT = 96;
    std::vector<Point2> out;
    out.reserve(COUNT + 1);
    size_t j = 1;
    double traversed = 0.0;
    for (int i = 0; i < COUNT; ++i) {
        const double target = length * i / (COUNT - 1);
        while (j < src.size() - 1 && traversed + distance(src[j - 1], src[j]) < target) {
            traversed += distance(src[j - 1], src[j]);
            ++j;
        }
        const double section = distance(src[j - 1], src[j]);
        const double t = section > 0 ? std::clamp((target - traversed) / section, 0.0, 1.0) : 0.0;
        out.push_back({src[j - 1].x + t * (src[j].x - src[j - 1].x),
                       src[j - 1].y + t * (src[j].y - src[j - 1].y)});
    }
    return out;
}
inline void rdp(const std::vector<Point2>& pts, size_t left, size_t right, double epsilon,
                std::vector<size_t>& corners) {
    if (right <= left + 1) return;
    double maximum = -1.0;
    size_t at = left;
    for (size_t i = left + 1; i < right; ++i) {
        const double d = segmentDistance(pts[i], pts[left], pts[right]);
        if (d > maximum) { maximum = d; at = i; }
    }
    if (maximum <= epsilon) return;
    rdp(pts, left, at, epsilon, corners);
    corners.push_back(at);
    rdp(pts, at, right, epsilon, corners);
}
inline std::optional<Fit> polygon(const std::vector<Point2>& pts, double diagonal) {
    const size_t n = pts.size();
    constexpr size_t WINDOW = 6;
    size_t start = 0;
    double biggestTurn = 0;
    for (size_t i = 0; i < n; ++i) {
        const Point2 before = sub(pts[i], pts[(i + n - WINDOW) % n]);
        const Point2 after = sub(pts[(i + WINDOW) % n], pts[i]);
        const double turn = std::abs(std::atan2(cross(before, after), dot(before, after)));
        if (turn > biggestTurn) { biggestTurn = turn; start = i; }
    }
    if (biggestTurn < 0.92) return std::nullopt;
    std::vector<Point2> ordered;
    ordered.reserve(n + 1);
    for (size_t i = 0; i < n; ++i) ordered.push_back(pts[(i + start) % n]);
    ordered.push_back(ordered.front());
    std::vector<size_t> corners{0};
    rdp(ordered, 0, n, diagonal * 0.038, corners);
    corners.push_back(n);
    const size_t sides = corners.size() - 1;
    if (sides != 3 && sides != 4) return std::nullopt;
    // Only straight, substantial sides; reject scribbles and handwriting with reversals.
    double sign = 0;
    for (size_t i = 0; i < sides; ++i) {
        const Point2 before = sub(ordered[corners[i]], ordered[corners[(i + sides - 1) % sides]]);
        const Point2 after = sub(ordered[corners[(i + 1) % sides]], ordered[corners[i]]);
        if (len(after) < diagonal * 0.18) return std::nullopt;
        const double c = cross(before, after);
        if (std::abs(c) < 0.12 * len(before) * len(after)) return std::nullopt;
        if (sign != 0 && c * sign < 0) return std::nullopt;
        sign = c;
        if (sides == 4 && std::abs(dot(before, after) / (len(before) * len(after))) > 0.32)
            return std::nullopt;
    }
    // Fit the line along each side by total least squares, then intersect adjacent sides to refine corners.
    struct Line { Point2 p; Point2 d; };
    std::vector<Line> lines;
    for (size_t i = 0; i < sides; ++i) {
        const size_t from = corners[i], to = corners[i + 1];
        if (to - from < 3) return std::nullopt;
        Point2 center{};
        for (size_t j = from; j <= to; ++j) { center.x += ordered[j].x; center.y += ordered[j].y; }
        const double m = static_cast<double>(to - from + 1);
        center.x /= m; center.y /= m;
        double xx = 0, xy = 0, yy = 0;
        for (size_t j = from; j <= to; ++j) {
            const double x = ordered[j].x - center.x, y = ordered[j].y - center.y;
            xx += x*x; xy += x*y; yy += y*y;
        }
        const double theta = 0.5 * std::atan2(2*xy, xx - yy);
        lines.push_back({center, {std::cos(theta), std::sin(theta)}});
    }
    std::vector<Point2> vertices;
    for (size_t i = 0; i < sides; ++i) {
        const auto& a = lines[(i + sides - 1) % sides];
        const auto& b = lines[i];
        const double denom = cross(a.d, b.d);
        if (std::abs(denom) < 0.25) return std::nullopt;
        const double t = cross(sub(b.p, a.p), b.d) / denom;
        vertices.push_back({a.p.x + t * a.d.x, a.p.y + t * a.d.y});
        if (distance(vertices.back(), ordered[corners[i]]) > diagonal * 0.12) return std::nullopt;
    }
    double sumSq = 0;
    for (Point2 p: pts) {
        double nearest = std::numeric_limits<double>::infinity();
        for (size_t k = 0; k < sides; ++k)
            nearest = std::min(nearest, segmentDistance(p, vertices[k], vertices[(k+1)%sides]));
        sumSq += nearest * nearest;
    }
    const double error = std::sqrt(sumSq / pts.size()) / diagonal;
    if (error > 0.034) return std::nullopt;
    vertices.push_back(vertices.front());
    return Fit{sides == 3 ? Kind::Triangle : Kind::Rectangle, std::move(vertices), error};
}
inline std::optional<Fit> ellipse(const std::vector<Point2>& pts, double diagonal, double pathLength) {
    // Normalise before solving the conic. Algebraic least squares recovers rotated ellipses and circles.
    Point2 mean{};
    for (Point2 p: pts) { mean.x += p.x; mean.y += p.y; }
    mean.x /= pts.size(); mean.y /= pts.size();
    std::array<std::array<double, 6>, 5> matrix{};
    for (Point2 p: pts) {
        const double x = (p.x - mean.x) / diagonal, y = (p.y - mean.y) / diagonal;
        const std::array<double, 5> features{x*x, x*y, y*y, x, y};
        for (int i=0;i<5;++i) {
            for (int j=0;j<5;++j) matrix[i][j] += features[i]*features[j];
            matrix[i][5] += features[i];
        }
    }
    std::array<double,5> coeff{};
    if (!solve5(matrix, coeff)) return std::nullopt;
    const double a=coeff[0], b=coeff[1]*0.5, c=coeff[2], d=coeff[3], e=coeff[4];
    const double det=a*c-b*b;
    if (a<=0 || c<=0 || det<=1e-8) return std::nullopt;
    const double cx=-(c*d-b*e)/(2*det), cy=-(-b*d+a*e)/(2*det);
    const double k=1+a*cx*cx+2*b*cx*cy+c*cy*cy;
    if (!(k>0)) return std::nullopt;
    const double delta=std::sqrt((a-c)*(a-c)+4*b*b);
    const double eigenMin=0.5*(a+c-delta), eigenMax=0.5*(a+c+delta);
    if (eigenMin<=0 || eigenMax<=0) return std::nullopt;
    const double major=std::sqrt(k/eigenMin),minor=std::sqrt(k/eigenMax);
    if (minor/major<0.43 || major>1.2 || minor<0.06) return std::nullopt;
    // Eigenvector of the smallest eigenvalue, the ellipse's long axis.
    const double angle=0.5*std::atan2(2*b, a-c) + PI/2;
    const double co=std::cos(angle), si=std::sin(angle);
    Point2 center{mean.x+cx*diagonal, mean.y+cy*diagonal};
    double errorSq=0, signedTurn=0, reverseTurn=0, lastPhi=0;
    for (size_t i=0;i<pts.size();++i) {
        const double x=(pts[i].x-center.x)/diagonal, y=(pts[i].y-center.y)/diagonal;
        const double u=(co*x+si*y)/major, v=(-si*x+co*y)/minor;
        const double radial=std::hypot(u,v);
        errorSq+=std::pow(radial-1,2);
        const double phi=std::atan2(v,u);
        if(i){double diff=std::remainder(phi-lastPhi,2*PI);signedTurn+=diff; if(diff*signedTurn<0)reverseTurn+=std::abs(diff);}
        lastPhi=phi;
    }
    const double error=std::sqrt(errorSq/pts.size())*minor;
    if (error>0.032 || std::abs(signedTurn)<5.0 || reverseTurn>3.0) return std::nullopt;
    // Reject spirals or letters that make several loops: a closed ellipse follows its perimeter once.
    const double perimeter=PI*(3*(major+minor)-std::sqrt((3*major+minor)*(major+3*minor)))*diagonal;
    if (pathLength/perimeter<0.74 || pathLength/perimeter>1.85) return std::nullopt;
    const bool circle=minor/major>0.88;
    const double rx=circle ? 0.5*(major+minor) : major;
    const double ry=circle ? rx : minor;
    std::vector<Point2> outline;
    const int steps=std::max(24,static_cast<int>(std::ceil(perimeter/3.0)));
    for (int i=0;i<=steps;++i) {
        const double t=2*PI*i/steps;
        outline.push_back({center.x+diagonal*(rx*co*std::cos(t)-ry*si*std::sin(t)),
                           center.y+diagonal*(rx*si*std::cos(t)+ry*co*std::sin(t))});
    }
    return Fit{circle?Kind::Circle:Kind::Ellipse,std::move(outline),error};
}
} // namespace detail

inline std::optional<Fit> fitClosedStroke(const std::vector<Point2>& source, double minDiagonal = 7.0) {
    if (source.size() < 9) return std::nullopt;
    double x0=std::numeric_limits<double>::infinity(), y0=x0;
    double x1=-x0, y1=-x0, pathLength=0;
    for (size_t i=0;i<source.size();++i) {
        const Point2 p=source[i];
        if (!std::isfinite(p.x)||!std::isfinite(p.y)) return std::nullopt;
        x0=std::min(x0,p.x);x1=std::max(x1,p.x);y0=std::min(y0,p.y);y1=std::max(y1,p.y);
        if(i)pathLength+=detail::distance(source[i-1],p);
    }
    const double diagonal=std::hypot(x1-x0,y1-y0);
    if(diagonal<minDiagonal || pathLength<1.75*diagonal) return std::nullopt;
    const double gap=detail::distance(source.front(),source.back());
    if(gap>0.14*pathLength || gap>0.36*diagonal) return std::nullopt;
    const auto samples=detail::resample(source,pathLength);
    auto poly=detail::polygon(samples,diagonal);
    auto round=detail::ellipse(samples,diagonal,pathLength);
    if(poly && (!round || poly->relativeError <= 1.3*round->relativeError+0.002)) return poly;
    if(round) return round;
    return poly;
}
} // namespace utn::smartshape
