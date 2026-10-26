#include "MaskedStrokeCodec.h"

#include <algorithm>
#include <atomic>
#include <cmath>
#include <iomanip>
#include <limits>
#include <locale>
#include <sstream>

#include "model/LineStyle.h"
#include "view/StrokeView.h"
#include "view/View.h"

namespace utn::eraser {
namespace {
bool finite(const xoj::util::Matrix& m) {
    const double determinant = m.xx * m.yy - m.xy * m.yx;
    return std::isfinite(m.xx) && std::isfinite(m.xy) && std::isfinite(m.yx) && std::isfinite(m.yy) &&
           std::isfinite(m.shift.x) && std::isfinite(m.shift.y) && std::isfinite(determinant) && std::abs(determinant) > 1e-14;
}
void writeMatrix(std::ostream& out, const xoj::util::Matrix& m) {
    out << m.xx << ' ' << m.yx << ' ' << m.xy << ' ' << m.yy << ' ' << m.shift.x << ' ' << m.shift.y;
}
bool readMatrix(std::istream& in, xoj::util::Matrix& m) {
    return static_cast<bool>(in >> m.xx >> m.yx >> m.xy >> m.yy >> m.shift.x >> m.shift.y) && finite(m);
}
bool validPoint(const Point& p) {
    return std::isfinite(p.x) && std::isfinite(p.y) && std::abs(p.x) <= 1e7 && std::abs(p.y) <= 1e7 &&
           std::isfinite(p.z) && (p.z == Point::NO_PRESSURE || (p.z > 0 && p.z <= 100000));
}
bool validDashes(const std::vector<double>& dashes) {
    return dashes.size() <= 256 &&
           std::ranges::none_of(dashes, [](double d) { return !std::isfinite(d) || d < 0; }) &&
           (dashes.empty() || std::ranges::any_of(dashes, [](double d) { return d > 0; }));
}
}

std::string encodeMaskedStroke(const Stroke& s, const xoj::util::Matrix& fallback) {
    if (!finite(fallback) || s.getPointCount() < 2 || s.getPointCount() > MASKED_STROKE_MAX_POINTS ||
        s.getErasures().empty() || s.getErasures().size() > MASKED_STROKE_MAX_ERASURES ||
        !std::isfinite(s.getWidth()) || s.getWidth() <= 0 || s.getWidth() > 100000 || s.getFill() < -1 ||
        s.getFill() > 255 || s.getToolType() > StrokeTool::HIGHLIGHTER || s.getStrokeCapStyle() > StrokeCapStyle::SQUARE)
        return {};
    const auto& dashes = s.getLineStyle().getDashes();
    if (!validDashes(dashes)) return {};
    for (const auto& p: s.getPointVector()) if (!validPoint(p)) return {};
    for (const auto& e: s.getErasures()) {
        if (!std::isfinite(e.start.x) || !std::isfinite(e.start.y) || !std::isfinite(e.end.x) ||
            !std::isfinite(e.end.y) || !std::isfinite(e.radius) || e.radius <= 0 || !finite(e.transform)) return {};
    }
    std::ostringstream out;
    out.imbue(std::locale::classic());
    out << std::setprecision(std::numeric_limits<double>::max_digits10);
    out << "UTN-STROKE 1 "; writeMatrix(out, fallback);
    out << ' ' << static_cast<int>(s.getToolType()) << ' ' << static_cast<int>(s.getStrokeCapStyle()) << ' '
        << s.getFill() << ' ' << static_cast<uint32_t>(s.getColor()) << ' ' << s.getWidth() << ' ' << s.getTimestamp()
        << ' ' << std::quoted(s.getAudioFilename().generic_u8string().empty() ? std::string{} :
             reinterpret_cast<const char*>(s.getAudioFilename().generic_u8string().c_str()));
    out << ' ' << dashes.size(); for (double d: dashes) out << ' ' << d;
    out << ' ' << s.getPointCount(); for (const auto& p: s.getPointVector()) out << ' ' << p.x << ' ' << p.y << ' ' << p.z;
    out << ' ' << s.getErasures().size();
    for (const auto& e: s.getErasures()) {
        out << ' ' << e.start.x << ' ' << e.start.y << ' ' << e.end.x << ' ' << e.end.y << ' ' << e.radius << ' ';
        writeMatrix(out, e.transform);
    }
    auto text = out.str();
    return text.size() <= MASKED_STROKE_MAX_TEXT ? text : std::string{};
}

std::optional<DecodedMaskedStroke> decodeMaskedStroke(std::string_view text) {
    if (text.empty() || text.size() > MASKED_STROKE_MAX_TEXT) return std::nullopt;
    std::istringstream in{std::string(text)}; in.imbue(std::locale::classic());
    std::string magic; int version, tool, cap, fill; uint32_t color; double width; size_t timestamp;
    xoj::util::Matrix fallback;
    if (!(in >> magic >> version) || magic != "UTN-STROKE" || version != 1 || !readMatrix(in, fallback) ||
        !(in >> tool >> cap >> fill >> color >> width >> timestamp) || tool < 0 || tool > 2 || cap < 0 || cap > 2 ||
        fill < -1 || fill > 255 || !std::isfinite(width) || width <= 0 || width > 100000) return std::nullopt;
    std::string audio; if (!(in >> std::quoted(audio)) || audio.size() > 1024 * 1024) return std::nullopt;
    auto stroke = std::make_unique<Stroke>();
    stroke->setToolType(static_cast<StrokeTool::Value>(tool)); stroke->setStrokeCapStyle(static_cast<StrokeCapStyle::Value>(cap));
    stroke->setFill(fill); stroke->setColor(Color{color}); stroke->setWidth(width); stroke->setTimestamp(timestamp);
    if (!audio.empty()) stroke->setAudioFilename(fs::path(std::u8string(reinterpret_cast<const char8_t*>(audio.data()), audio.size())));
    size_t count;
    if (!(in >> count) || count > 256) return std::nullopt;
    std::vector<double> dashes(count);
    for (double& d: dashes) if (!(in >> d)) return std::nullopt;
    if (!validDashes(dashes)) return std::nullopt;
    LineStyle style; style.setDashes(std::move(dashes)); stroke->setLineStyle(style);
    if (!(in >> count) || count < 2 || count > MASKED_STROKE_MAX_POINTS) return std::nullopt;
    std::vector<Point> points; points.reserve(count);
    for (size_t i = 0; i < count; ++i) { double x, y, z; if (!(in >> x >> y >> z)) return std::nullopt; Point p{x,y,z}; if (!validPoint(p)) return std::nullopt; points.push_back(p); }
    stroke->setPointVector(std::move(points));
    if (!(in >> count) || count == 0 || count > MASKED_STROKE_MAX_ERASURES) return std::nullopt;
    std::vector<StrokeErasure> masks(count);
    for (auto& e: masks) if (!(in >> e.start.x >> e.start.y >> e.end.x >> e.end.y >> e.radius) ||
        !std::isfinite(e.start.x) || !std::isfinite(e.start.y) || !std::isfinite(e.end.x) || !std::isfinite(e.end.y) ||
        !std::isfinite(e.radius) || e.radius <= 0 || !readMatrix(in, e.transform)) return std::nullopt;
    in >> std::ws; if (!in.eof()) return std::nullopt;
    stroke->setErasures(std::move(masks));
    return DecodedMaskedStroke{std::move(stroke), fallback};
}

bool matchesFallbackMatrix(const xoj::util::Matrix& a, const xoj::util::Matrix& b) {
    if (!finite(a) || !finite(b)) return false;
    constexpr double epsilon = 1e-7;
    return std::abs(a.xx-b.xx)<=epsilon && std::abs(a.xy-b.xy)<=epsilon && std::abs(a.yx-b.yx)<=epsilon &&
           std::abs(a.yy-b.yy)<=epsilon && std::abs(a.shift.x-b.shift.x)<=epsilon && std::abs(a.shift.y-b.shift.y)<=epsilon;
}

namespace {
std::optional<MaskedStrokeFallback> rasterizeAtScale(const Stroke& stroke, const xoj::util::Rectangle<double>& b,
                                                     double scale) {
    const int width = std::max(1, static_cast<int>(std::ceil(b.width * scale)));
    const int height = std::max(1, static_cast<int>(std::ceil(b.height * scale)));
    auto* raw = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, width, height);
    if (cairo_surface_status(raw) != CAIRO_STATUS_SUCCESS) { cairo_surface_destroy(raw); return std::nullopt; }
    std::shared_ptr<cairo_surface_t> surface(raw, cairo_surface_destroy);
    auto* cr = cairo_create(raw); cairo_set_operator(cr, CAIRO_OPERATOR_CLEAR); cairo_paint(cr); cairo_set_operator(cr, CAIRO_OPERATOR_OVER);
    cairo_scale(cr, scale, scale); cairo_translate(cr, -b.x, -b.y);
    xoj::view::StrokeView(&stroke).draw(xoj::view::Context::createDefault(cr));
    cairo_destroy(cr); cairo_surface_flush(raw);
    return MaskedStrokeFallback{std::move(surface), {1/scale, 0, 0, 1/scale, {b.x, b.y}}};
}
}  // namespace

namespace {
std::atomic<size_t> rasterizeCalls{0};
}
size_t rasterizeMaskedStrokeCallCount() { return rasterizeCalls.load(); }

std::optional<MaskedStrokeFallback> rasterizeMaskedStroke(const Stroke& stroke) {
    rasterizeCalls.fetch_add(1);
    const auto b = stroke.getBoundingBox();
    if (!std::isfinite(b.x) || !std::isfinite(b.y) || !std::isfinite(b.width) || !std::isfinite(b.height) ||
        b.width <= 0 || b.height <= 0) return std::nullopt;
    constexpr double maxEdge = 8192, maxPixels = 16 * 1024 * 1024;
    double scale = 4;
    // Reserve enough room for rounding both dimensions up to whole pixels.
    scale = std::min(scale, (maxEdge - 2) / b.width); scale = std::min(scale, (maxEdge - 2) / b.height);
    scale = std::min(scale, std::sqrt((maxPixels - 4 * maxEdge) / (b.width * b.height)));
    if (!std::isfinite(scale) || scale <= 0) return std::nullopt;
    // A surface that cannot be allocated (memory pressure, huge stroke) is retried at a coarser resolution before the
    // caller has to give up; a slightly softer fallback is far better than a failed save.
    for (int attempt = 0; attempt < 4; ++attempt, scale /= 2) {
        if (auto fallback = rasterizeAtScale(stroke, b, scale)) {
            return fallback;
        }
    }
    return std::nullopt;
}
}  // namespace utn::eraser
