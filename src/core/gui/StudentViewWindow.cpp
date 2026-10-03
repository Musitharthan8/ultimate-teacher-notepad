/*
 * Ultimate Teacher Notepad
 *
 * Student-facing projector window
 *
 * Based on Xournal++ GPLv2+
 */

#include "StudentViewWindow.h"

#include <algorithm>
#include <shared_mutex>
#include <string>

#include "control/Control.h"
#include "gui/MainWindow.h"
#include "gui/XournalView.h"
#include "model/Document.h"
#include "model/Layer.h"
#include "model/XojPage.h"
#include "view/LayerView.h"
#include "view/View.h"
#include "view/background/BackgroundFlags.h"

namespace {
constexpr const char* TEACHER_LAYER_PREFIX = "UTN Teacher Notes";
}

StudentViewWindow::StudentViewWindow(Control* control): control(control) {
#if GTK_MAJOR_VERSION == 3
    window = GTK_WINDOW(gtk_window_new(GTK_WINDOW_TOPLEVEL));
#else
    window = GTK_WINDOW(gtk_window_new());
#endif

    gtk_window_set_title(window, "Ultimate Teacher Notepad - Student View");
    gtk_window_set_default_size(window, 1280, 720);

    drawingArea = gtk_drawing_area_new();
    gtk_widget_set_hexpand(drawingArea, true);
    gtk_widget_set_vexpand(drawingArea, true);

#if GTK_MAJOR_VERSION == 3
    gtk_container_add(GTK_CONTAINER(window), drawingArea);

    g_signal_connect(
            drawingArea,
            "draw",
            G_CALLBACK(+[](GtkWidget* widget, cairo_t* cr, gpointer data) -> gboolean {
                auto* self = static_cast<StudentViewWindow*>(data);
                self->draw(cr, gtk_widget_get_allocated_width(widget), gtk_widget_get_allocated_height(widget));
                return FALSE;
            }),
            this);

    g_signal_connect(
            window,
            "delete-event",
            G_CALLBACK(+[](GtkWidget* widget, GdkEvent*, gpointer) -> gboolean {
                gtk_widget_hide(widget);
                return TRUE;
            }),
            nullptr);
#else
    gtk_window_set_child(window, drawingArea);

    gtk_drawing_area_set_draw_func(
            GTK_DRAWING_AREA(drawingArea),
            +[](GtkDrawingArea*, cairo_t* cr, int width, int height, gpointer data) {
                static_cast<StudentViewWindow*>(data)->draw(cr, width, height);
            },
            this, nullptr);

    g_signal_connect(
            window,
            "close-request",
            G_CALLBACK(+[](GtkWindow* window, gpointer) -> gboolean {
                gtk_widget_hide(GTK_WIDGET(window));
                return TRUE;
            }),
            nullptr);
#endif

    refreshTimer = g_timeout_add(
            33,
            +[](gpointer data) -> gboolean {
                auto* self = static_cast<StudentViewWindow*>(data);
                if (self->isVisible()) {
                    self->refresh();
                }
                return G_SOURCE_CONTINUE;
            },
            this);
}

StudentViewWindow::~StudentViewWindow() {
    if (refreshTimer != 0) {
        g_source_remove(refreshTimer);
        refreshTimer = 0;
    }

    if (window) {
        gtk_window_destroy(window);
        window = nullptr;
        drawingArea = nullptr;
    }
}

void StudentViewWindow::show() {
#if GTK_MAJOR_VERSION == 3
    gtk_widget_show_all(GTK_WIDGET(window));
#else
    gtk_widget_show(GTK_WIDGET(window));
#endif
    gtk_window_present(window);
    refresh();
}

void StudentViewWindow::hide() {
    if (window) {
        gtk_widget_hide(GTK_WIDGET(window));
    }
}

void StudentViewWindow::setFullscreen(bool enabled) {
    if (!window) {
        return;
    }

    if (enabled) {
        gtk_window_fullscreen(window);
    } else {
        gtk_window_unfullscreen(window);
    }
}

void StudentViewWindow::refresh() {
    if (drawingArea) {
        gtk_widget_queue_draw(drawingArea);
    }
}

bool StudentViewWindow::isVisible() const {
    return window && gtk_widget_get_visible(GTK_WIDGET(window));
}

bool StudentViewWindow::isTeacherOnlyLayerName(const std::string& name) {
    return name.rfind(TEACHER_LAYER_PREFIX, 0) == 0;
}

void StudentViewWindow::draw(cairo_t* cr, int width, int height) {
    if (!control || !control->getWindow() || width <= 0 || height <= 0) {
        return;
    }

    Document* doc = control->getDocument();
    if (!doc || doc->getPageCount() == 0) {
        return;
    }

    size_t pageNo = std::min(control->getCurrentPageNo(), doc->getPageCount() - 1);

    std::shared_lock lock(*doc);
    ConstPageRef page = doc->getPage(pageNo);
    if (!page) {
        return;
    }

    cairo_save(cr);
    cairo_set_source_rgb(cr, 0.08, 0.08, 0.08);
    cairo_paint(cr);

    const double pageWidth = page->getWidth();
    const double pageHeight = page->getHeight();
    if (pageWidth <= 0.0 || pageHeight <= 0.0) {
        cairo_restore(cr);
        return;
    }

    const double scale = std::min(static_cast<double>(width) / pageWidth, static_cast<double>(height) / pageHeight);
    const double offsetX = (static_cast<double>(width) - pageWidth * scale) / 2.0;
    const double offsetY = (static_cast<double>(height) - pageHeight * scale) / 2.0;

    cairo_translate(cr, offsetX, offsetY);
    cairo_scale(cr, scale, scale);

    documentView.setPdfCache(control->getWindow()->getXournal()->getCache());
    documentView.initDrawing(page, cr, false);
    documentView.drawBackground(xoj::view::BACKGROUND_SHOW_ALL);

    xoj::view::Context context{cr, xoj::view::NORMAL_NON_AUDIO, xoj::view::SHOW_CURRENT_EDITING,
                               xoj::view::NORMAL_COLOR};

    for (const Layer* layer: page->getLayersView()) {
        if (!layer->isVisible()) {
            continue;
        }

        if (layer->hasName() && isTeacherOnlyLayerName(layer->getName())) {
            continue;
        }

        xoj::view::LayerView(layer).draw(context);
    }

    documentView.finializeDrawing();
    cairo_restore(cr);
}
