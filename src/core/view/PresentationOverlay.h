/*
 * Ultimate Teacher Notepad
 *
 * The Spotlight and Curtain shading, drawn identically on the teacher's canvas and on the Student View frame so that
 * what the teacher sees covered is covered for the class too.
 *
 * Based on Xournal++ GPLv2+
 */

#pragma once

#include <cairo.h>

namespace xoj::view {

/// Opacity of the Curtain on the teacher's own canvas: faint enough that the teacher still sees the page
constexpr double CURTAIN_TEACHER_ALPHA = 0.88;
/// Opacity on the Student View frame: the covered part of the page must not be legible
constexpr double CURTAIN_STUDENT_ALPHA = 1.0;
constexpr double SPOTLIGHT_SHADE_ALPHA = 0.62;
/// Spotlight radius in screen pixels at the teacher's zoom
constexpr double SPOTLIGHT_RADIUS_PX = 115.0;

/**
 * Paint the overlay in page coordinates.
 * @param spotlight Spotlight when true, Curtain when false
 * @param zoom the zoom of the teacher's view, so the spotlight keeps the same size on the page
 * @param curtainAlpha CURTAIN_TEACHER_ALPHA or CURTAIN_STUDENT_ALPHA
 */
void paintPresentationOverlay(cairo_t* cr, bool spotlight, double pageWidth, double pageHeight, double pointerX,
                              double pointerY, double zoom, double curtainAlpha);

}  // namespace xoj::view
