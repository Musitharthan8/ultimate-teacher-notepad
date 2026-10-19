#pragma once

#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <cairo.h>
#include "model/Stroke.h"
#include "util/Matrix.h"

namespace utn::eraser {
inline constexpr size_t MASKED_STROKE_MAX_POINTS = 100000;
inline constexpr size_t MASKED_STROKE_MAX_ERASURES = 4096;
inline constexpr size_t MASKED_STROKE_MAX_TEXT = 8 * 1024 * 1024;

struct DecodedMaskedStroke {
    std::unique_ptr<Stroke> stroke;
    xoj::util::Matrix fallbackMatrix;
};
struct MaskedStrokeFallback {
    std::shared_ptr<cairo_surface_t> surface;
    xoj::util::Matrix matrix;
};
// Plain versioned text, never clipboard/native binary serialization.
std::string encodeMaskedStroke(const Stroke& stroke, const xoj::util::Matrix& fallbackMatrix);
std::optional<DecodedMaskedStroke> decodeMaskedStroke(std::string_view text);
bool matchesFallbackMatrix(const xoj::util::Matrix& actual, const xoj::util::Matrix& expected);
std::optional<MaskedStrokeFallback> rasterizeMaskedStroke(const Stroke& stroke);
}  // namespace utn::eraser
