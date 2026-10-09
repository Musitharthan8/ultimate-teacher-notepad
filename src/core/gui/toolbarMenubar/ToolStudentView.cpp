/*
 * Ultimate Teacher Notepad
 *
 * Student View window and status pill
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
#include "model/LayerAudience.h"
#include "model/XojPage.h"
#include "util/gtk4_helper.h"
#include "util/i18n.h"
#include "view/DocumentView.h"

#include "UtnWidgets.h"

namespace {
/// Coalesce bursts of page changes (live erasing, typing) into at most one student redraw per interval
constexpr guint REDRAW_INTERVAL_MS = 40;

constexpr const char* STATE_CLASSES[] = {"utn-sv-off", "utn-sv-same", "utn-sv-live", "utn-sv-frozen", "utn-sv-blank"};

/// Draw the current page as students may see it, centred and scaled into width × height
void renderStudentFrame(Control* control, cairo_t* cr, double width, double height) {
    cairo_set_source_rgb(cr, 0.16, 0.16, 0.16);
    cairo_paint(cr);

    PageRef page = control->getCurrentPage();
    if (!page) {
        return;
    }
    auto* doc = control->getDocument();
    std::shared_lock lock(*doc);

    const double pageWidth = page->getWidth();
    const double pageHeight = page->getHeight();
    if (pageWidth <= 0 || pageHeight <= 0) {
        return;
    }
    const double scale = std::min(width / pageWidth, height / pageHeight);

    cairo_save(cr);
    cairo_translate(cr, 0.5 * (width - pageWidth * scale), 0.5 * (height - pageHeight * scale));
    cairo_scale(cr, scale, scale);

    DocumentView view;
    if (auto* mainWindow = control->getWindow()) {
        if (auto* xournal = mainWindow->getXournal()) {
            view.setPdfCache(xournal->getCache());
        }
    }
    // Teacher-only layers and unrevealed answers never reach the projector
    view.setLayerFilter([](const Layer& layer) { return utn::visibleToStudents(layer); });
    view.drawPage(page, cr, true);
    cairo_restore(cr);
}
}  // namespace

ToolStudentView::ToolStudentView(std::string id, Control* control):
        AbstractToolItem(std::move(id), Category::NAVIGATION), control(control) {
    registerListener(control);
    canvasListenerId = control->addCanvasChangedListener([this](const XojPage*) { requestRedraw(); });

    if (GdkDisplay* display = gdk_display_get_default()) {
        auto onMonitorsChanged = +[](GdkDisplay*, GdkMonitor*, gpointer data) {
            auto* self = static_cast<ToolStudentView*>(data);
            self->updateState();
            if (auto* win = self->control->getWindow()) {
                win->showToast(monitorCount() > 1 ?
                                       _("Second screen found. Student View can show the lesson there.") :
                                       _("The projector now shows this screen. Hidden answers stay fully hidden."));
            }
        };
        monitorAddedHandler = g_signal_connect(display, "monitor-added", G_CALLBACK(onMonitorsChanged), this);
        monitorRemovedHandler = g_signal_connect(display, "monitor-removed", G_CALLBACK(onMonitorsChanged), this);
    }
}

ToolStudentView::~ToolStudentView() {
    control->removeCanvasChangedListener(canvasListenerId);
    if (guint source = redrawSource.exchange(0)) {
        g_source_remove(source);
    }
    if (GdkDisplay* display = gdk_display_get_default()) {
        if (monitorAddedHandler) {
            g_signal_handler_disconnect(display, monitorAddedHandler);
        }
        if (monitorRemovedHandler) {
            g_signal_handler_disconnect(display, monitorRemovedHandler);
        }
    }
    if (window != nullptr) {
        GtkWidget* oldWindow = std::exchange(window, nullptr);
        g_signal_handlers_disconnect_by_data(oldWindow, this);
        gtk_widget_destroy(oldWindow);
    }
    if (frozenFrame) {
        cairo_surface_destroy(frozenFrame);
    }
}

auto ToolStudentView::monitorCount() -> int {
    GdkDisplay* display = gdk_display_get_default();
    return display ? gdk_display_get_n_monitors(display) : 1;
}

auto ToolStudentView::isOnSeparateMonitor() const -> bool {
    GdkDisplay* display = gdk_display_get_default();
    GtkWindow* teacher = control->getGtkWindow();
    if (!display || !window || !teacher || monitorCount() < 2) {
        return false;
    }
    GdkWindow* studentWindow = gtk_widget_get_window(window);
    GdkWindow* teacherWindow = gtk_widget_get_window(GTK_WIDGET(teacher));
    if (!studentWindow || !teacherWindow) {
        return false;
    }
    return gdk_display_get_monitor_at_window(display, studentWindow) !=
           gdk_display_get_monitor_at_window(display, teacherWindow);
}

auto ToolStudentView::getState() const -> State {
    if (blanked) {
        return State::Blank;
    }
    if (window && frozen) {
        return State::Frozen;
    }
    if (window) {
        return State::Live;
    }
    return monitorCount() < 2 ? State::SameScreen : State::Off;
}

auto ToolStudentView::stateLabel(State state) -> const char* {
    switch (state) {
        case State::Off:
            return _("Students: Off");
        case State::SameScreen:
            return _("Students: Same screen");
        case State::Live:
            return _("Students: Live");
        case State::Frozen:
            return _("Students: Frozen");
        case State::Blank:
            return _("Students: Blank");
    }
    return "";
}

void ToolStudentView::updateState() {
    // Hidden answers may be shown faintly to the teacher only while students watch another screen
    control->setGhostHiddenAnswers(isOnSeparateMonitor());

    const State state = getState();
    if (pill) {
        for (const char* cls: STATE_CLASSES) {
            gtk_widget_remove_css_class(pill, cls);
        }
        gtk_widget_add_css_class(pill, STATE_CLASSES[static_cast<int>(state)]);
        gtk_label_set_text(GTK_LABEL(pillLabel), stateLabel(state));
        utn::setAccessibleName(pill, stateLabel(state));
    }
    if (freezeToggle) {
        const bool canFreeze = window != nullptr;
        gtk_widget_set_sensitive(freezeToggle, canFreeze);
        gtk_widget_set_tooltip_text(freezeToggle,
                                    canFreeze ? _("Keep showing this page while you prepare the next step") :
                                                _("Open Student View on the projector first"));
        utn::syncToggle(GTK_TOGGLE_BUTTON(freezeToggle), frozen);
    }
    if (blankToggle) {
        utn::syncToggle(GTK_TOGGLE_BUTTON(blankToggle), blanked);
    }

    updateTeacherOutline(state);
}

void ToolStudentView::updateTeacherOutline(State state) {
    // Remind the teacher on their own canvas that students are not seeing it live
    const bool show = state == State::Frozen || (state == State::Blank && window != nullptr);
    auto* win = control->getWindow();
    if (!win || (!show && !teacherOutline)) {
        return;
    }
    if (!teacherOutline) {
        teacherOutline = gtk_drawing_area_new();
        gtk_widget_set_halign(teacherOutline, GTK_ALIGN_FILL);
        gtk_widget_set_valign(teacherOutline, GTK_ALIGN_FILL);
        g_signal_connect(teacherOutline, "draw", G_CALLBACK(+[](GtkWidget* widget, cairo_t* cr, gpointer data) -> gboolean {
                             auto* self = static_cast<ToolStudentView*>(data);
                             const bool frozenState = self->getState() == State::Frozen;
                             // Amber for frozen, red for blank; 6 px so it is seen at a glance
                             cairo_set_source_rgb(cr, frozenState ? 0.88 : 0.70, frozenState ? 0.63 : 0.15,
                                                  frozenState ? 0.0 : 0.12);
                             cairo_set_line_width(cr, 6);
                             cairo_rectangle(cr, 3, 3, gtk_widget_get_allocated_width(widget) - 6,
                                             gtk_widget_get_allocated_height(widget) - 6);
                             cairo_stroke(cr);
                             return FALSE;
                         }),
                         this);
        g_signal_connect(teacherOutline, "destroy", G_CALLBACK(+[](GtkWidget*, gpointer data) {
                             static_cast<ToolStudentView*>(data)->teacherOutline = nullptr;
                         }),
                         this);
        GtkOverlay* overlay = GTK_OVERLAY(win->get("mainOverlay"));
        gtk_overlay_add_overlay(overlay, teacherOutline);
        gtk_overlay_set_overlay_pass_through(overlay, teacherOutline, true);  // never blocks drawing
    }
    gtk_widget_set_visible(teacherOutline, show);
    gtk_widget_queue_draw(teacherOutline);
}

void ToolStudentView::requestRedraw() {
    if (redrawPending.exchange(true)) {
        return;
    }
    redrawSource = g_timeout_add(REDRAW_INTERVAL_MS, +[](gpointer data) -> gboolean {
        auto* self = static_cast<ToolStudentView*>(data);
        self->redrawSource = 0;
        self->redrawPending = false;
        if (self->drawingArea && !self->frozen && !self->blanked) {
            gtk_widget_queue_draw(self->drawingArea);
        }
        return G_SOURCE_REMOVE;
    }, this);
}

void ToolStudentView::pageSelected(size_t) { requestRedraw(); }
void ToolStudentView::pageChanged(size_t) { requestRedraw(); }
void ToolStudentView::pageSizeChanged(size_t) { requestRedraw(); }
void ToolStudentView::documentChanged(DocumentChangeType) { requestRedraw(); }

auto ToolStudentView::createItem(bool horizontal) -> xoj::util::WidgetSPtr {
    auto [popover, panel] = utn::createPopoverPanel(4);
    utn::appendPopoverHeading(panel, _("Student View"));

    GtkWidget* open = utn::appendMenuButton(panel, popover, _("Open on the projector"),
                                            _("Show the lesson on the second screen without your tools"));
    g_signal_connect(open, "clicked", G_CALLBACK(+[](GtkButton*, gpointer data) {
                         static_cast<ToolStudentView*>(data)->showStudentView();
                     }),
                     this);

    freezeToggle = gtk_toggle_button_new_with_label(_("Freeze"));
    g_signal_connect(freezeToggle, "toggled", G_CALLBACK(+[](GtkToggleButton* button, gpointer data) {
                         if (!utn::isSyncing(GTK_WIDGET(button))) {
                             static_cast<ToolStudentView*>(data)->setFrozen(gtk_toggle_button_get_active(button));
                         }
                     }),
                     this);
    gtk_box_append(panel, freezeToggle);

    blankToggle = gtk_toggle_button_new_with_label(_("Blank screen"));
    gtk_widget_set_tooltip_text(blankToggle, _("Hide the page from students until you turn this off"));
    gtk_widget_add_css_class(blankToggle, "utn-danger");
    g_signal_connect(blankToggle, "toggled", G_CALLBACK(+[](GtkToggleButton* button, gpointer data) {
                         if (!utn::isSyncing(GTK_WIDGET(button))) {
                             static_cast<ToolStudentView*>(data)->setBlanked(gtk_toggle_button_get_active(button));
                         }
                     }),
                     this);
    gtk_box_append(panel, blankToggle);

    GtkWidget* full = utn::appendMenuButton(panel, popover, _("Full screen"),
                                            _("Fill the projector with the student window"));
    g_signal_connect(full, "clicked", G_CALLBACK(+[](GtkButton*, gpointer data) {
                         static_cast<ToolStudentView*>(data)->setStudentFullscreen(true);
                     }),
                     this);
    GtkWidget* close = utn::appendMenuButton(panel, popover, _("Close"), _("Close the student window"));
    g_signal_connect(close, "clicked", G_CALLBACK(+[](GtkButton*, gpointer data) {
                         static_cast<ToolStudentView*>(data)->closeStudentView();
                     }),
                     this);
    gtk_widget_show_all(GTK_WIDGET(panel));

    // The pill: a coloured dot and the state in words, always visible in the app bar
    GtkMenuButton* menuButton = GTK_MENU_BUTTON(gtk_menu_button_new());
    gtk_widget_set_focus_on_click(GTK_WIDGET(menuButton), false);
    gtk_widget_add_css_class(GTK_WIDGET(menuButton), "utn-sv-pill");
    gtk_widget_set_tooltip_text(GTK_WIDGET(menuButton), _("What students can see. Click for Student View controls."));
    GtkWidget* content = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
    GtkWidget* dot = gtk_label_new("●");
    gtk_widget_add_css_class(dot, "utn-sv-dot");
    gtk_box_append(GTK_BOX(content), dot);
    pillLabel = gtk_label_new(nullptr);
    gtk_box_append(GTK_BOX(content), pillLabel);
    gtk_widget_show_all(content);
    gtk_button_set_child(GTK_BUTTON(menuButton), content);
    gtk_menu_button_set_popover(menuButton, GTK_WIDGET(popover));
    gtk_menu_button_set_direction(menuButton, horizontal ? GTK_ARROW_DOWN : GTK_ARROW_RIGHT);

    pill = GTK_WIDGET(menuButton);
    g_signal_connect(pill, "destroy", G_CALLBACK(+[](GtkWidget* widget, gpointer data) {
                         auto* self = static_cast<ToolStudentView*>(data);
                         if (self->pill == widget) {
                             self->pill = nullptr;
                             self->pillLabel = nullptr;
                             self->freezeToggle = nullptr;
                             self->blankToggle = nullptr;
                         }
                     }),
                     this);

    updateState();
    return xoj::util::WidgetSPtr(pill, xoj::util::adopt);
}

void ToolStudentView::showStudentView() {
    if (window != nullptr) {
        gtk_window_present(GTK_WINDOW(window));
        return;
    }
    showTeacherBlankCover(false);

    GtkApplication* app = gtk_window_get_application(control->getGtkWindow());
    window = gtk_application_window_new(app);
    gtk_window_set_title(GTK_WINDOW(window), _("UTN Student View"));
    gtk_window_set_default_size(GTK_WINDOW(window), 1280, 720);

    drawingArea = gtk_drawing_area_new();
    gtk_widget_set_hexpand(drawingArea, true);
    gtk_widget_set_vexpand(drawingArea, true);
    gtk_container_add(GTK_CONTAINER(window), drawingArea);

    g_signal_connect(drawingArea, "draw", G_CALLBACK(&ToolStudentView::drawStudentView), this);
    g_signal_connect(window, "key-press-event", G_CALLBACK(&ToolStudentView::studentViewKeyPressed), this);
    g_signal_connect(window, "destroy", G_CALLBACK(&ToolStudentView::studentViewDestroyed), this);
    // Moving the window between monitors changes whether hidden answers may be ghosted for the teacher
    g_signal_connect(window, "configure-event", G_CALLBACK(+[](GtkWidget*, GdkEvent*, gpointer data) -> gboolean {
                         static_cast<ToolStudentView*>(data)->updateState();
                         return FALSE;
                     }),
                     this);

    gtk_widget_show_all(window);

    // With a second monitor, go straight to full screen on the monitor the teacher is not using
    GdkDisplay* display = gdk_display_get_default();
    GdkWindow* teacherWindow = gtk_widget_get_window(GTK_WIDGET(control->getGtkWindow()));
    if (display && teacherWindow && monitorCount() > 1) {
        GdkMonitor* teacherMonitor = gdk_display_get_monitor_at_window(display, teacherWindow);
        for (int i = 0; i < monitorCount(); ++i) {
            if (gdk_display_get_monitor(display, i) != teacherMonitor) {
                gtk_window_fullscreen_on_monitor(GTK_WINDOW(window), gtk_widget_get_screen(window), i);
                fullscreen = true;
                break;
            }
        }
    }
    updateState();
}

void ToolStudentView::setFrozen(bool enabled) {
    if (!window) {
        frozen = false;
        updateState();
        return;
    }
    if (enabled && !frozen && drawingArea) {
        // Keep the exact frame students are seeing now
        const int width = std::max(1, gtk_widget_get_allocated_width(drawingArea));
        const int height = std::max(1, gtk_widget_get_allocated_height(drawingArea));
        if (frozenFrame) {
            cairo_surface_destroy(frozenFrame);
        }
        frozenFrame = cairo_image_surface_create(CAIRO_FORMAT_RGB24, width, height);
        cairo_t* cr = cairo_create(frozenFrame);
        renderStudentFrame(control, cr, width, height);
        cairo_destroy(cr);
        frozen = true;
    } else if (!enabled) {
        frozen = false;
        if (frozenFrame) {
            cairo_surface_destroy(frozenFrame);
            frozenFrame = nullptr;
        }
    }
    if (drawingArea) {
        gtk_widget_queue_draw(drawingArea);
    }
    updateState();
}

void ToolStudentView::setBlanked(bool enabled) {
    blanked = enabled;
    if (!window && monitorCount() < 2) {
        // Students see this screen, so cover the teacher's canvas instead
        showTeacherBlankCover(enabled);
    } else if (enabled && !window) {
        showStudentView();
    }
    if (drawingArea) {
        gtk_widget_queue_draw(drawingArea);
    }
    updateState();
}

void ToolStudentView::showTeacherBlankCover(bool show) {
    auto* win = control->getWindow();
    if (!win) {
        return;
    }
    if (!teacherCover && show) {
        teacherCover = gtk_event_box_new();
        gtk_widget_add_css_class(teacherCover, "utn-blank-cover");
        gtk_widget_set_halign(teacherCover, GTK_ALIGN_FILL);
        gtk_widget_set_valign(teacherCover, GTK_ALIGN_FILL);
        GtkWidget* content = gtk_box_new(GTK_ORIENTATION_VERTICAL, 12);
        gtk_widget_set_halign(content, GTK_ALIGN_CENTER);
        gtk_widget_set_valign(content, GTK_ALIGN_CENTER);
        GtkWidget* label = gtk_label_new(_("Screen hidden"));
        gtk_widget_add_css_class(label, "utn-blank-title");
        gtk_box_append(GTK_BOX(content), label);
        GtkWidget* showButton = gtk_button_new_with_label(_("Show page"));
        g_signal_connect(showButton, "clicked", G_CALLBACK(+[](GtkButton*, gpointer data) {
                             static_cast<ToolStudentView*>(data)->setBlanked(false);
                         }),
                         this);
        gtk_box_append(GTK_BOX(content), showButton);
        gtk_container_add(GTK_CONTAINER(teacherCover), content);
        gtk_overlay_add_overlay(GTK_OVERLAY(win->get("mainOverlay")), teacherCover);
        g_signal_connect(teacherCover, "destroy", G_CALLBACK(+[](GtkWidget*, gpointer data) {
                             static_cast<ToolStudentView*>(data)->teacherCover = nullptr;
                         }),
                         this);
    }
    if (teacherCover) {
        gtk_widget_set_visible(teacherCover, show);
        if (show) {
            gtk_widget_show_all(teacherCover);
        }
    }
}

void ToolStudentView::setStudentFullscreen(bool enabled) {
    showStudentView();
    fullscreen = enabled;
    if (enabled) {
        gtk_window_fullscreen(GTK_WINDOW(window));
    } else {
        gtk_window_unfullscreen(GTK_WINDOW(window));
    }
}

void ToolStudentView::closeStudentView() {
    if (window != nullptr) {
        GtkWidget* oldWindow = window;
        gtk_widget_destroy(oldWindow);  // studentViewDestroyed resets the state
    }
}

gboolean ToolStudentView::drawStudentView(GtkWidget* widget, cairo_t* cr, gpointer data) {
    auto* self = static_cast<ToolStudentView*>(data);
    const double width = std::max(1, gtk_widget_get_allocated_width(widget));
    const double height = std::max(1, gtk_widget_get_allocated_height(widget));

    if (self->blanked) {
        cairo_set_source_rgb(cr, 0.0, 0.0, 0.0);
        cairo_paint(cr);
        return FALSE;
    }
    if (self->frozen && self->frozenFrame) {
        cairo_set_source_rgb(cr, 0.16, 0.16, 0.16);
        cairo_paint(cr);
        const double frameWidth = cairo_image_surface_get_width(self->frozenFrame);
        const double frameHeight = cairo_image_surface_get_height(self->frozenFrame);
        const double scale = std::min(width / frameWidth, height / frameHeight);
        cairo_translate(cr, 0.5 * (width - frameWidth * scale), 0.5 * (height - frameHeight * scale));
        cairo_scale(cr, scale, scale);
        cairo_set_source_surface(cr, self->frozenFrame, 0, 0);
        cairo_paint(cr);
        return FALSE;
    }
    renderStudentFrame(self->control, cr, width, height);
    return FALSE;
}

gboolean ToolStudentView::studentViewKeyPressed(GtkWidget*, GdkEventKey* event, gpointer data) {
    auto* self = static_cast<ToolStudentView*>(data);
    if (event->keyval == GDK_KEY_F11) {
        self->setStudentFullscreen(!self->fullscreen);
        return TRUE;
    }
    if (event->keyval == GDK_KEY_Escape && self->fullscreen) {
        self->fullscreen = false;
        gtk_window_unfullscreen(GTK_WINDOW(self->window));
        return TRUE;
    }
    return FALSE;
}

void ToolStudentView::studentViewDestroyed(GtkWidget*, gpointer data) {
    auto* self = static_cast<ToolStudentView*>(data);
    self->window = nullptr;
    self->drawingArea = nullptr;
    self->fullscreen = false;
    self->frozen = false;
    self->blanked = false;
    if (self->frozenFrame) {
        cairo_surface_destroy(self->frozenFrame);
        self->frozenFrame = nullptr;
    }
    self->updateState();
}

auto ToolStudentView::getToolDisplayName() const -> std::string { return _("Student View"); }

auto ToolStudentView::getNewToolIcon() const -> GtkWidget* { return gtk_label_new("SV"); }
