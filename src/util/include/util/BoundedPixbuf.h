/*
 * Xournal++
 *
 * Bound decoded raster dimensions passed through GdkPixbuf loaders.
 *
 * @license GNU GPLv2 or later
 */

#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <utility>

#include <gdk-pixbuf/gdk-pixbuf.h>

namespace xoj::util {

inline constexpr uint64_t MAX_DECODED_PIXBUF_PIXELS = uint64_t{1} << 25;

inline auto boundedPixbufDimensions(int width, int height) -> std::pair<int, int> {
    if (width <= 0 || height <= 0) {
        return {width, height};
    }
    const uint64_t pixels = static_cast<uint64_t>(width) * static_cast<uint64_t>(height);
    if (pixels <= MAX_DECODED_PIXBUF_PIXELS) {
        return {width, height};
    }
    const double scale = std::sqrt(static_cast<double>(MAX_DECODED_PIXBUF_PIXELS) / static_cast<double>(pixels));
    int boundedWidth = std::max(1, static_cast<int>(std::floor(width * scale)));
    int boundedHeight = std::max(1, static_cast<int>(std::floor(height * scale)));
    if (static_cast<uint64_t>(boundedWidth) * static_cast<uint64_t>(boundedHeight) > MAX_DECODED_PIXBUF_PIXELS) {
        if (boundedWidth >= boundedHeight) {
            boundedWidth = static_cast<int>(MAX_DECODED_PIXBUF_PIXELS / static_cast<uint64_t>(boundedHeight));
        } else {
            boundedHeight = static_cast<int>(MAX_DECODED_PIXBUF_PIXELS / static_cast<uint64_t>(boundedWidth));
        }
    }
    return {boundedWidth, boundedHeight};
}

inline void boundedPixbufSizePrepared(GdkPixbufLoader* loader, gint width, gint height, gpointer) {
    const auto [boundedWidth, boundedHeight] = boundedPixbufDimensions(width, height);
    if (boundedWidth > 0 && boundedHeight > 0 && (boundedWidth != width || boundedHeight != height)) {
        gdk_pixbuf_loader_set_size(loader, boundedWidth, boundedHeight);
    }
}

inline void limitGdkPixbufLoaderPixels(GdkPixbufLoader* loader) {
    g_signal_connect(loader, "size-prepared", G_CALLBACK(boundedPixbufSizePrepared), nullptr);
}

}  // namespace xoj::util
