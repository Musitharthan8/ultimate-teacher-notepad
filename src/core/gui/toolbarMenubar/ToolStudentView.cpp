/*
 * Ultimate Teacher Notepad
 *
 * Clean student-facing preview window
 *
 * Based on Xournal++ GPLv2+
 */

#include "ToolStudentView.h"

#include <algorithm>
#include <memory>
#include <shared_mutex>
#include <string>
#include <utility>

#include "control/Control.h"
#include "gui/MainWindow.h"
#include "gui/XournalView.h"
#include "model/Document.h"
#include "model/Layer.h"
#include "model/XojPage.h"
#include "util/gtk4_helper.h"
#include "util/i18n.h"
#include "view/DocumentView.h"

namespace {
bool isTeacherOnlyLayer(const Layer* layer) {
    if (!layer || !layer->hasName()) {
        return false;
    }

    const std::string name = layer->getName();
    return name.rfind("UTN Reveal", 0) == 0 || name.rfind("UTN Teacher", 0) == 0;
}
}  // namespace

ToolStudentView::ToolStudentView(std::string id, Control* control):
        AbstractToolItem(std::move(id), Category::NAVIGATION), control(control) {}

ToolStudentView::~ToolStudentView() {
    closeStudentView();
}

auto ToolStudentView::createItem(bool horizontal) -> xoj::util::WidgetSPtr {
    GtkWidget* button = gtk_button_new_with_label("SV");
    gtk_widget_set_can_focus(button, false);
    gtk_widget_set_tooltip_text(button, getToolDisplayName().c_str());

    g_signal_connect(
            button,
            "clicked",
            G_CALLBACK(+[](GtkButton*, gpointer data) {
                static_cast<ToolStudentView*>(data)->showStudentView();
            }),
            this);

    return xoj::util::WidgetSPtr(button, xoj::util::adopt);
}

void ToolStudentView::showStudentView() {
    if (window != nullptr) {
        gtk_window_present(GTK_WINDOW(window));
        return;
    }

    GtkApplication* app = gtk_window_get_application(control->getGtkWindow());
    window = gtk_application_window_new(app);
    gtk_window_set_title(GTK_WINDOW(window), _("UTN Student View"));
    gtk_window_set_default_size(GTK_WINDOW(window), 1280, 720);

    drawingArea = gtk_drawing_area_new();
    gtk_widget_set_hexpand(drawingArea, true);
    gtk_widget_set_vexpand(drawingArea, true);
    gtk_container_add(GTK_CONTAINER(window), drawingArea);

    g_signal_connect(drawingArea, "draw", G_CALLBACK(&ToolStudentView::drawStudentView), this);
    g_signal_connect(window, "destroy", G_CALLBACK(&ToolStudentView::studentViewDestroyed), this);

    // Prototype refresh loop for live classroom annotations
    refreshTimer = g_timeout_add(80, &ToolStudentView::refreshStudentView, this);

    gtk_widget_show_all(window);
}

void ToolStudentView::closeStudentView() {
    if (refreshTimer != 0) {
        g_source_remove(refreshTimer);
        refreshTimer = 0;
    }

    if (window != nullptr) {
        GtkWidget* oldWindow = window;
        window = nullptr;
        drawingArea = nullptr;
        gtk_widget_destroy(oldWindow);
    }
}

gboolean ToolStudentView::drawStudentView(GtkWidget* widget, cairo_t* cr, gpointer data) {
    auto* self = static_cast<ToolStudentView*>(data);
    PageRef sourcePage = self->control->getCurrentPage();
    if (!sourcePage) {
        return FALSE;
    }

    auto* doc = self->control->getDocument();
    std::shared_lock lock(*doc);
    PageRef studentPage(sourcePage->clone());
    lock.unlock();

    // Teacher-only layers stay visible on the teacher screen but not here
    for (Layer* layer: studentPage->getLayers()) {
        if (isTeacherOnlyLayer(layer)) {
            layer->setVisible(false);
        }
    }

    const double pageWidth = studentPage->getWidth();
    const double pageHeight = studentPage->getHeight();
    const double width = std::max(1, gtk_widget_get_allocated_width(widget));
    const double height = std::max(1, gtk_widget_get_allocated_height(widget));

    cairo_set_source_rgb(cr, 0.16, 0.16, 0.16);
    cairo_paint(cr);

    const double scale = std::min(width / pageWidth, height / pageHeight);
    const double offsetX = 0.5 * (width - pageWidth * scale);
    const double offsetY = 0.5 * (height - pageHeight * scale);

    cairo_save(cr);
    cairo_translate(cr, offsetX, offsetY);
    cairo_scale(cr, scale, scale);

    DocumentView view;
    if (auto* mainWindow = self->control->getWindow()) {
        if (auto* xournal = mainWindow->getXournal()) {
            view.setPdfCache(xournal->getCache());
        }
    }
    view.drawPage(studentPage, cr, true);

    cairo_restore(cr);
    return FALSE;
}

gboolean ToolStudentView::refreshStudentView(gpointer data) {
    auto* self = static_cast<ToolStudentView*>(data);
    if (self->drawingArea == nullptr) {
        self->refreshTimer = 0;
        return G_SOURCE_REMOVE;
    }

    gtk_widget_queue_draw(self->drawingArea);
    return G_SOURCE_CONTINUE;
}

void ToolStudentView::studentViewDestroyed(GtkWidget*, gpointer data) {
    auto* self = static_cast<ToolStudentView*>(data);

    if (self->refreshTimer != 0) {
        g_source_remove(self->refreshTimer);
        self->refreshTimer = 0;
    }

    self->window = nullptr;
    self->drawingArea = nullptr;
}

auto ToolStudentView::getToolDisplayName() const -> std::string {
    return _("Student View");
}

auto ToolStudentView::getNewToolIcon() const -> GtkWidget* {
    return gtk_label_new("SV");
}
