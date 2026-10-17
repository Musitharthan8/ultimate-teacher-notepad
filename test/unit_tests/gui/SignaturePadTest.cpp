/* Ultimate Teacher Notepad, based on Xournal++ GPLv2+. */
#include <cstdint>
#include <gtest/gtest.h>
#include <gdk/gdk.h>
#include "gui/toolbarMenubar/SignaturePad.h"

TEST(SignaturePad, TransparentImageSurvivesPngRoundTrip) {
    utn::SignaturePad pad;
    pad.strokes = {{{10, 20}, {80, 20}}, {{50, 40}}};
    auto* surface = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, 100, 60);
    auto* cr = cairo_create(surface);
    utn::paintSignature(cr, pad);
    auto* image = gdk_pixbuf_get_from_surface(surface, 0, 0, 100, 60);
    ASSERT_NE(image, nullptr);
    gchar* bytes = nullptr;
    gsize length = 0;
    ASSERT_TRUE(gdk_pixbuf_save_to_buffer(image, &bytes, &length, "png", nullptr, nullptr));
    auto* loader = gdk_pixbuf_loader_new_with_type("png", nullptr);
    ASSERT_NE(loader, nullptr);
    ASSERT_TRUE(gdk_pixbuf_loader_write(loader, reinterpret_cast<const guchar*>(bytes), length, nullptr));
    ASSERT_TRUE(gdk_pixbuf_loader_close(loader, nullptr));
    auto* loaded = gdk_pixbuf_loader_get_pixbuf(loader);
    ASSERT_NE(loaded, nullptr);
    ASSERT_TRUE(gdk_pixbuf_get_has_alpha(loaded));
    EXPECT_EQ(gdk_pixbuf_get_width(loaded), 100);
    EXPECT_EQ(gdk_pixbuf_get_height(loaded), 60);
    const auto* pixels = gdk_pixbuf_read_pixels(loaded);
    const int stride = gdk_pixbuf_get_rowstride(loaded);
    EXPECT_EQ(pixels[3], 0);  // The page remains visible around the signature.
    EXPECT_GT(pixels[20 * stride + 40 * 4 + 3], 200);  // Continuous stroke.
    EXPECT_GT(pixels[40 * stride + 50 * 4 + 3], 100);  // A single tap is retained.
    g_object_unref(loader);
    g_free(bytes);
    g_object_unref(image);
    cairo_destroy(cr);
    cairo_surface_destroy(surface);
}

TEST(SignaturePad, ClearedPadLeavesNoInk) {
    utn::SignaturePad pad;
    pad.strokes = {{{10, 20}, {80, 20}}};
    pad.strokes.clear();
    auto* surface = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, 100, 60);
    auto* cr = cairo_create(surface);
    utn::paintSignature(cr, pad);
    cairo_surface_flush(surface);
    const auto* pixels = cairo_image_surface_get_data(surface);
    const int size = cairo_image_surface_get_stride(surface) * 60;
    for (int i = 0; i < size; ++i) ASSERT_EQ(pixels[i], 0);
    cairo_destroy(cr);
    cairo_surface_destroy(surface);
}
