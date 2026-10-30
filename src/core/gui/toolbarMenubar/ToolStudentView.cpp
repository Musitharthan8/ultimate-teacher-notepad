/*
 * Ultimate Teacher Notepad
 *
 * Student View window and status pill
 *
 * Based on Xournal++ GPLv2+
 */

#include "ToolStudentView.h"

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <memory>
#include <shared_mutex>
#include <thread>
#include <string>
#include <utility>

#include "control/Control.h"
#include "control/ToolHandler.h"
#include "control/UtnPerf.h"
#include "control/settings/Settings.h"
#include "gui/MainWindow.h"
#include "gui/PageView.h"
#include "gui/XournalView.h"
#include "model/Document.h"
#include "model/Layer.h"
#include "model/LayerAudience.h"
#include "model/StudentVisibility.h"
#include "model/XojPage.h"
#include "util/gtk4_helper.h"
#include "util/i18n.h"
#include "view/DocumentView.h"
#include "view/PresentationOverlay.h"

#include "UtnWidgets.h"

namespace {
/// Coalesce bursts of page changes (live erasing, typing) into at most one student redraw per interval
constexpr guint REDRAW_INTERVAL_MS = 40;
/// A frame renders on the UI thread, the same thread that handles the stylus. The next frame waits this many times
/// the last frame's render time, so Student View never takes more than about one seventh of the UI thread.
constexpr double REDRAW_COST_FACTOR = 6.0;
constexpr guint REDRAW_INTERVAL_MAX_MS = 1000;

constexpr const char* STATE_CLASSES[] = {"utn-sv-off", "utn-sv-same", "utn-sv-live", "utn-sv-frozen", "utn-sv-blank"};

/// A choice in the Students menu closes it, like its other entries; otherwise the next click only dismissed the menu
void closeMenu(GtkWidget* entry) {
    if (auto* popover = g_object_get_data(G_OBJECT(entry), "utn-popover")) {
        gtk_popover_popdown(GTK_POPOVER(popover));
    }
}

/// Draw the current page as students may see it, centred and scaled into width × height
void renderStudentFrame(Control* control, cairo_t* cr, double width, double height,
                        StudentFrameContent* content = nullptr) {
    cairo_set_source_rgb(cr, 0.16, 0.16, 0.16);
    cairo_paint(cr);
    if (content) {
        *content = {};
    }

    PageRef page = control->getCurrentPage();
    if (!page) {
        return;
    }
    auto* doc = control->getDocument();
    std::shared_lock lock(*doc);
    if (content) {
        // What this frame shows, recorded under the same lock that draws it, so a frozen copy can later tell whether
        // students would now be allowed to see less
        content->page = page;
        content->layerRemovals = page->getLayerRemovalCount();
        content->privateTransfers = utn::privateTransferCount();
        for (const Layer* layer: page->getLayersView()) {
            if (utn::visibleToStudents(*layer)) {
                content->visibleLayers.push_back(layer);
            }
        }
    }

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

    // Spotlight and Curtain must cover the projected page too, not only the teacher's canvas
    auto* tools = control->getToolHandler();
    if (tools->isSpotlightEnabled() || tools->isCurtainEnabled()) {
        double pointerX = 0.5 * pageWidth;
        double pointerY = 0.5 * pageHeight;
        double teacherZoom = 1.0;
        if (auto* mainWindow = control->getWindow()) {
            if (auto* xournal = mainWindow->getXournal()) {
                if (auto* pageView = xournal->getViewFor(control->getCurrentPageNo())) {
                    pageView->getPresentationPointer(pointerX, pointerY);
                    teacherZoom = pageView->getZoom();
                }
            }
        }
        xoj::view::paintPresentationOverlay(cr, tools->isSpotlightEnabled(), pageWidth, pageHeight, pointerX, pointerY,
                                            teacherZoom, xoj::view::CURTAIN_STUDENT_ALPHA);
    }
    cairo_restore(cr);
}

/// A surface for one student frame that keeps the screen's device scale, so a HiDPI projector stays sharp
cairo_surface_t* createFrameSurface(GtkWidget* widget, int width, int height) {
    const int scale = std::max(1, gtk_widget_get_scale_factor(widget));
    cairo_surface_t* surface = cairo_image_surface_create(CAIRO_FORMAT_RGB24, width * scale, height * scale);
    cairo_surface_set_device_scale(surface, scale, scale);
    return surface;
}

/// Logical size of a frame made by createFrameSurface
void frameSize(cairo_surface_t* surface, double& width, double& height) {
    double sx = 1.0, sy = 1.0;
    cairo_surface_get_device_scale(surface, &sx, &sy);
    width = cairo_image_surface_get_width(surface) / sx;
    height = cairo_image_surface_get_height(surface) / sy;
}
}  // namespace

ToolStudentView::ToolStudentView(std::string id, Control* control):
        AbstractToolItem(std::move(id), Category::NAVIGATION), control(control) {
    registerListener(control);
    // A null page means the audience changed (answers hidden or revealed): show it without waiting
    canvasListenerId = control->addCanvasChangedListener([this](const XojPage* page) { requestRedraw(page == nullptr); });
    // Layers, audiences and overlays announce here whatever user interface path changed them
    visibilityListenerId =
            utn::addStudentVisibilityListener([this](bool mayCheckNow) { onStudentVisibilityChanged(mayCheckNow); });

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
    utn::removeStudentVisibilityListener(visibilityListenerId);  // returns only when no call is running
    *alive = false;                                              // a queued check must not touch this object
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
    // These overlays belong to the teacher window, which can outlive this tool (for example on toolbar reload).
    // Their draw/destroy callbacks hold this pointer, so remove them before the tool is released.
    for (GtkWidget** overlay: {&teacherOutline, &teacherCover}) {
        if (*overlay) {
            GtkWidget* widget = std::exchange(*overlay, nullptr);
            g_signal_handlers_disconnect_by_data(widget, this);
            gtk_widget_destroy(widget);
        }
    }
    if (frozenFrame) {
        cairo_surface_destroy(frozenFrame);
    }
    if (liveFrame) {
        cairo_surface_destroy(liveFrame);
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

auto ToolStudentView::stateShortWord(State state) -> const char* {
    switch (state) {
        case State::Off:
            return _("Off");
        case State::SameScreen:
            return _("Same");
        case State::Live:
            return _("Live");
        case State::Frozen:
            return _("Frozen");
        case State::Blank:
            return _("Blank");
    }
    return "";
}

void ToolStudentView::updateState() {
    // Hidden answers may be shown faintly to the teacher only if the teacher asked for it AND students watch another
    // screen. It is off by default: a teacher window that is screen-shared or recorded must never show hidden answers.
    bool ghostWanted = false;
    control->getSettings()->getCustomElement("utn").getBool("ghostHiddenAnswers", ghostWanted);
    control->setGhostHiddenAnswers(ghostWanted && isOnSeparateMonitor());
    if (ghostToggle) {
        utn::syncToggle(GTK_TOGGLE_BUTTON(ghostToggle), ghostWanted);
    }

    const State state = getState();
    // Revealing everything asks for a second press while the class may be looking
    control->setStudentsMayWatch(state == State::Live || state == State::SameScreen);
    if (pill) {
        for (const char* cls: STATE_CLASSES) {
            gtk_widget_remove_css_class(pill, cls);
        }
        gtk_widget_add_css_class(pill, STATE_CLASSES[static_cast<int>(state)]);
        // "Students: Live" is shown as a prefix plus the state, so a narrow window can drop only the prefix.
        // The accessible name and tooltip keep the whole phrase.
        const std::string full = stateLabel(state);
        const size_t split = full.find(": ");
        gtk_label_set_text(GTK_LABEL(pillPrefix), split == std::string::npos ? "" : full.substr(0, split + 1).c_str());
        gtk_label_set_text(GTK_LABEL(pillLabel), split == std::string::npos ? full.c_str() : full.c_str() + split + 2);
        gtk_label_set_text(GTK_LABEL(pillShort), stateShortWord(state));
        utn::setAccessibleName(pill, full.c_str());
    }
    const bool open = window != nullptr;
    if (freezeToggle) {
        gtk_widget_set_sensitive(freezeToggle, open);
        // The button says what it will do next, so a frozen class always has an obvious way back to live
        gtk_button_set_label(GTK_BUTTON(freezeToggle), frozen ? _("Resume live view") : _("Freeze this page"));
        gtk_widget_set_tooltip_text(freezeToggle,
                                    !open  ? _("Open Student View on the projector first") :
                                    frozen ? _("Show students the page you are on now") :
                                             _("Keep showing this page while you prepare the next step"));
        utn::syncToggle(GTK_TOGGLE_BUTTON(freezeToggle), frozen);
    }
    if (blankToggle) {
        gtk_button_set_label(GTK_BUTTON(blankToggle), blanked ? _("Show the page again") : _("Blank screen"));
        utn::syncToggle(GTK_TOGGLE_BUTTON(blankToggle), blanked);
    }
    if (freezeQuick) {
        gtk_button_set_label(GTK_BUTTON(freezeQuick), frozen ? _("Resume live") : _("Freeze"));
        utn::syncToggle(GTK_TOGGLE_BUTTON(freezeQuick), frozen);
        gtk_widget_set_tooltip_text(freezeQuick,
                                    !open  ? _("Freeze needs Student View. Open it on the projector first, then students "
                                               "keep seeing this page while you prepare the next step.") :
                                    frozen ? _("Students are seeing a frozen page. Choose to show them the page you are "
                                               "on now.") :
                                             _("Keep showing students this page while you prepare the next step"));
        utn::setAccessibleName(freezeQuick, frozen ? _("Resume live view for students") : _("Freeze students' screen"));
    }
    if (blankQuick) {
        gtk_button_set_label(GTK_BUTTON(blankQuick), blanked ? _("Show page") : _("Blank"));
        utn::syncToggle(GTK_TOGGLE_BUTTON(blankQuick), blanked);
        gtk_widget_set_tooltip_text(blankQuick, blanked ? _("Students see a blank screen. Choose to show the page again.") :
                                                          _("Hide the page from students until you turn this off"));
        utn::setAccessibleName(blankQuick, blanked ? _("Show the page to students again") : _("Blank students' screen"));
    }
    if (pill) {
        // Teacher View and Student View are different screens; say which one the class is looking at
        const char* meaning =
                state == State::Off        ? _("Students see nothing yet. Open Student View on the projector to show them "
                                               "the page without your tools.") :
                state == State::SameScreen ? _("The projector shows this screen, so students see your tools and private "
                                               "notes. Open Student View on a second screen to keep them private.") :
                state == State::Live       ? _("Students see the page only: no tools, private notes or hidden answers.") :
                state == State::Frozen     ? _("Students see a frozen picture of the page. Your screen can move on.") :
                                             _("Students see a blank screen.");
        // A tight app bar shows one word, so the tooltip leads with the whole phrase
        std::string tip = std::string(stateLabel(state)) + "\n" + meaning + "\n" + _("Click for Student View controls.");
        gtk_widget_set_tooltip_text(pill, tip.c_str());
        // A new word is a new width, and the app bar is fitted to what it measures
        if (MainWindow* mainWindow = control->getWindow()) {
            mainWindow->refitAppBar();
        }
    }
    if (openButton) {
        gtk_button_set_label(GTK_BUTTON(openButton), open ? _("Show Student View window") : _("Open on the projector"));
    }
    if (fullButton) {
        gtk_widget_set_sensitive(fullButton, open);
    }
    if (closeButton) {
        gtk_widget_set_sensitive(closeButton, open);
    }

    updateTeacherOutline(state);

    // Move the list aside first: a listener may react by changing something that comes back here
    auto listeners = std::move(stateListeners);
    stateListeners.clear();
    std::vector<std::function<bool()>> kept;
    for (auto& listener: listeners) {
        if (listener()) {
            kept.push_back(std::move(listener));
        }
    }
    stateListeners.insert(stateListeners.begin(), std::make_move_iterator(kept.begin()),
                          std::make_move_iterator(kept.end()));
}

void ToolStudentView::addStateListener(std::function<bool()> listener) {
    stateListeners.push_back(std::move(listener));
}

void ToolStudentView::restoreNormal() {
    if (blanked) {
        setBlanked(false);
    }
    if (frozen) {
        setFrozen(false);
    }
}

bool ToolStudentView::isTeacherCoverShown() const {
    return teacherCover && gtk_widget_get_visible(teacherCover);
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
        // Overlay pass-through only skips the overlay's own child window. A drawing area normally adds a GdkWindow
        // of its own, which covered the whole teacher window (toolbars included) and swallowed every click while
        // students were frozen, including clicks on the Students pill. Without a window it only paints.
        gtk_widget_set_has_window(teacherOutline, false);
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

bool ToolStudentView::frozenFrameIsSafe() {
    if (!frozenFrame) {
        return true;
    }
    // Any overlay that is not the one already in the frozen picture. Switching on Curtain or Spotlight after the freeze
    // is a change; so is switching from one to the other, because the new one can be the stricter (Spotlight to Curtain)
    // and a frozen picture of the old one would then show what the teacher just covered. Switching an overlay off
    // keeps the frozen picture, which was already covered at least as much.
    const OverlayKind overlay = currentOverlay(control);
    if (overlay != OverlayKind::None && overlay != frozenOverlay) {
        return false;
    }
    if (!frozenContent.page) {
        return true;  // frozen before there was a page: nothing private in it
    }
    // Visible content moved into a layer students cannot see since the frame was drawn: no layer setting shows that,
    // and it needs no Document lock to know.
    if (utn::privateTransferCount() != frozenContent.privateTransfers) {
        return false;
    }
    // Never block (this runs inside document notifications), but do not guess either: try a few times, then treat
    // "cannot tell" as unsafe. A blank screen is always the safe answer.
    auto* doc = control->getDocument();
    bool locked = false;
    for (int attempt = 0; attempt < 8 && !(locked = doc->try_lock_shared()); ++attempt) {
        g_usleep(500);
    }
    if (!locked) {
        return false;
    }
    // A layer removed since the frame was drawn: its address may be reused, so do not trust the comparison below
    bool safe = frozenContent.page->getLayerRemovalCount() == frozenContent.layerRemovals;
    for (const Layer* shown: frozenContent.visibleLayers) {
        if (!safe) {
            break;
        }
        bool stillVisible = false;
        for (const Layer* layer: frozenContent.page->getLayersView()) {
            if (layer == shown) {
                stillVisible = utn::visibleToStudents(*layer);
                break;
            }
        }
        if (!stillVisible) {  // hidden, made private, or gone
            safe = false;
            break;
        }
    }
    doc->unlock_shared();
    return safe;
}

bool ToolStudentView::dropFrozenFrameIfUnsafe() {
    if (!frozenFrame || frozenFrameIsSafe()) {
        return false;
    }
    cairo_surface_destroy(frozenFrame);
    frozenFrame = nullptr;
    hasFrozenFrame = false;
    frozenContent = {};
    frozen = false;
    setBlanked(true);  // blank at once; also covers the teacher's canvas when students watch the same screen
    if (drawingArea) {
        gtk_widget_queue_draw(drawingArea);
    }
    if (auto* win = control->getWindow()) {
        win->showToast(_("Answers were hidden while the screen was frozen. Students now see a blank screen. Choose "
                         "\"Show the page again\" to continue."));
    }
    return true;
}

OverlayKind ToolStudentView::currentOverlay(Control* control) {
    auto* tools = control->getToolHandler();
    if (tools->isCurtainEnabled()) {
        return OverlayKind::Curtain;
    }
    return tools->isSpotlightEnabled() ? OverlayKind::Spotlight : OverlayKind::None;
}

void ToolStudentView::onStudentVisibilityChanged(bool mayCheckNow) {
    if (mayCheckNow && std::this_thread::get_id() == uiThread) {
        // From the layer controller or the tool handler: the Document is not locked, so check now. A frozen frame that
        // students may no longer see goes blank before this returns; a live one is redrawn without waiting.
        dropFrozenFrameIfUnsafe();
        requestRedraw(studentAudienceChanged());
        return;
    }
    // From a model setter: possibly under the Document lock or on another thread. Check as soon as the main loop is free,
    // and only if there is a frozen frame to protect.
    if (!hasFrozenFrame.load() || visibilityCheckQueued.exchange(true)) {
        return;
    }
    using Token = std::pair<std::weak_ptr<std::atomic_bool>, ToolStudentView*>;
    g_idle_add_full(G_PRIORITY_HIGH, +[](gpointer data) -> gboolean {
        std::unique_ptr<Token> token(static_cast<Token*>(data));
        if (auto alive = token->first.lock(); alive && *alive) {
            ToolStudentView* self = token->second;
            self->visibilityCheckQueued = false;
            self->dropFrozenFrameIfUnsafe();
        }
        return G_SOURCE_REMOVE;
    }, new Token(alive, this), nullptr);
}

void ToolStudentView::requestRedraw(bool urgent) {
    if (frozenFrame && std::this_thread::get_id() == uiThread) {
        dropFrozenFrameIfUnsafe();
    }
    utn::perf::record(utn::perf::Metric::StudentRedrawRequests, 1);
    if (urgent) {
        utn::perf::record(utn::perf::Metric::StudentRedrawUrgent, 1);
    }
    frameDirty = true;
    if (redrawPending.exchange(true)) {
        if (!urgent) {
            return;
        }
        // A slower redraw is already waiting; replace it with an immediate one
        if (guint pending = redrawSource.exchange(0)) {
            g_source_remove(pending);
        }
    }
    // Leading edge: after a quiet spell the first change is shown at once. Only the follow-ups of a burst are held
    // back. Changes of who may see what (hiding answers) are always urgent, so students never keep seeing them.
    const gint64 sinceLastFrameMs = (g_get_monotonic_time() - lastFrameEnd) / 1000;
    const gint64 wait = urgent ? 1 : std::max<gint64>(1, static_cast<gint64>(nextRedrawDelayMs) - sinceLastFrameMs);
    redrawSource = g_timeout_add(static_cast<guint>(wait), +[](gpointer data) -> gboolean {
        auto* self = static_cast<ToolStudentView*>(data);
        self->redrawSource = 0;
        self->redrawPending = false;
        if (self->drawingArea && !self->frozen && !self->blanked) {
            // Render here, once, and let the draw handler only copy the result. Window exposure, moves and resizes
            // then no longer re-render the whole page.
            self->refreshLiveFrame();
            gtk_widget_queue_draw(self->drawingArea);
        }
        return G_SOURCE_REMOVE;
    }, this);
}

bool ToolStudentView::frameNeedsResize(int width, int height) const {
    if (!liveFrame || !drawingArea) {
        return true;
    }
    const int scale = std::max(1, gtk_widget_get_scale_factor(drawingArea));
    return cairo_image_surface_get_width(liveFrame) != width * scale ||
           cairo_image_surface_get_height(liveFrame) != height * scale;
}

void ToolStudentView::refreshLiveFrame() {
    if (!drawingArea) {
        return;
    }
    const int width = std::max(1, gtk_widget_get_allocated_width(drawingArea));
    const int height = std::max(1, gtk_widget_get_allocated_height(drawingArea));

    const auto start = std::chrono::steady_clock::now();
    if (liveFrame && frameNeedsResize(width, height)) {
        cairo_surface_destroy(liveFrame);
        liveFrame = nullptr;
    }
    if (!liveFrame) {
        liveFrame = createFrameSurface(drawingArea, width, height);
    }
    cairo_t* cr = cairo_create(liveFrame);
    renderStudentFrame(control, cr, width, height, &liveContent);
    cairo_destroy(cr);
    frameDirty = false;

    const double ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count();
    lastFrameEnd = g_get_monotonic_time();
    utn::perf::record(utn::perf::Metric::StudentFrame, ms);
    // The slower a frame is, the longer the stylus gets to itself before the next one
    nextRedrawDelayMs = static_cast<guint>(std::clamp(ms * REDRAW_COST_FACTOR, static_cast<double>(REDRAW_INTERVAL_MS),
                                                      static_cast<double>(REDRAW_INTERVAL_MAX_MS)));
}

bool ToolStudentView::studentAudienceChanged() {
    // Never block here: this runs from document notifications, some of which are sent while the Document is locked
    auto* doc = control->getDocument();
    if (!doc->try_lock_shared()) {
        // Cannot tell right now. Not urgent: while the teacher writes, the lock is often busy, and every busy moment
        // used to bypass the redraw throttle. Operations that shrink the audience notify after releasing the lock.
        return false;
    }
    uint64_t signature = 1469598103934665603ULL;
    signature = (signature ^ (static_cast<uint64_t>(currentOverlay(control)) + 7U)) * 1099511628211ULL;
    // doc->getPage, not Control::getCurrentPage(): that takes the Document lock again, and the lock is not recursive
    if (PageRef page = doc->getPage(control->getCurrentPageNo())) {
        size_t index = 0;
        for (const Layer* layer: page->getLayersView()) {
            signature = (signature ^ (utn::visibleToStudents(*layer) ? 2U : 1U)) * 1099511628211ULL;
            signature = (signature ^ ++index) * 1099511628211ULL;
        }
        // Not the page itself: moving to another page is announced like any change (shown at once after a quiet
        // spell, rate limited in a burst), only a change in who may see what skips the wait.
    }
    doc->unlock_shared();
    const bool changed = signature != lastAudienceSignature;
    lastAudienceSignature = signature;
    return changed;
}

void ToolStudentView::pageSelected(size_t) { requestRedraw(studentAudienceChanged()); }
void ToolStudentView::pageChanged(size_t) { requestRedraw(studentAudienceChanged()); }
void ToolStudentView::pageSizeChanged(size_t) { requestRedraw(); }
void ToolStudentView::documentChanged(DocumentChangeType) { requestRedraw(studentAudienceChanged()); }

auto ToolStudentView::createItem(bool horizontal) -> xoj::util::WidgetSPtr {
    auto [popover, panel] = utn::createPopoverPanel(4);
    utn::appendPopoverHeading(panel, _("Student View"));

    openButton = utn::appendMenuButton(panel, popover, _("Open on the projector"),
                                       _("Show the lesson on the second screen without your tools"));
    g_signal_connect(openButton, "clicked", G_CALLBACK(+[](GtkButton*, gpointer data) {
                         static_cast<ToolStudentView*>(data)->showStudentView();
                     }),
                     this);

    freezeToggle = gtk_toggle_button_new_with_label(_("Freeze this page"));
    g_signal_connect(freezeToggle, "toggled", G_CALLBACK(+[](GtkToggleButton* button, gpointer data) {
                         if (!utn::isSyncing(GTK_WIDGET(button))) {
                             closeMenu(GTK_WIDGET(button));
                             static_cast<ToolStudentView*>(data)->setFrozen(gtk_toggle_button_get_active(button));
                         }
                     }),
                     this);
    g_object_set_data(G_OBJECT(freezeToggle), "utn-popover", popover);
    gtk_box_append(panel, freezeToggle);

    blankToggle = gtk_toggle_button_new_with_label(_("Blank screen"));
    gtk_widget_set_tooltip_text(blankToggle, _("Hide the page from students until you turn this off"));
    gtk_widget_add_css_class(blankToggle, "utn-danger");
    g_signal_connect(blankToggle, "toggled", G_CALLBACK(+[](GtkToggleButton* button, gpointer data) {
                         if (!utn::isSyncing(GTK_WIDGET(button))) {
                             closeMenu(GTK_WIDGET(button));
                             static_cast<ToolStudentView*>(data)->setBlanked(gtk_toggle_button_get_active(button));
                         }
                     }),
                     this);
    g_object_set_data(G_OBJECT(blankToggle), "utn-popover", popover);
    gtk_box_append(panel, blankToggle);

    ghostToggle = gtk_toggle_button_new_with_label(_("Show hidden answers faintly to me"));
    gtk_widget_set_tooltip_text(ghostToggle, _("Only on your screen, and only while the class watches a second screen. "
                                               "Turn this off before you share your screen or record."));
    g_signal_connect(ghostToggle, "toggled", G_CALLBACK(+[](GtkToggleButton* button, gpointer data) {
                         if (utn::isSyncing(GTK_WIDGET(button))) {
                             return;
                         }
                         auto* self = static_cast<ToolStudentView*>(data);
                         self->control->getSettings()->getCustomElement("utn").setBool(
                                 "ghostHiddenAnswers", gtk_toggle_button_get_active(button));
                         self->control->getSettings()->customSettingsChanged();
                         self->updateState();
                     }),
                     this);
    gtk_box_append(panel, ghostToggle);

    fullButton = utn::appendMenuButton(panel, popover, _("Full screen"),
                                       _("Fill the projector with the student window. F11 or Esc in that window "
                                         "leaves full screen."));
    g_signal_connect(fullButton, "clicked", G_CALLBACK(+[](GtkButton*, gpointer data) {
                         static_cast<ToolStudentView*>(data)->setStudentFullscreen(true);
                     }),
                     this);
    closeButton = utn::appendMenuButton(panel, popover, _("Close Student View"), _("Close the student window"));
    g_signal_connect(closeButton, "clicked", G_CALLBACK(+[](GtkButton*, gpointer data) {
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
    pillPrefix = gtk_label_new(nullptr);
    gtk_widget_add_css_class(pillPrefix, "utn-narrow-hidden");  // dropped in compact windows; the state word stays
    gtk_box_append(GTK_BOX(content), pillPrefix);
    pillLabel = gtk_label_new(nullptr);
    gtk_widget_add_css_class(pillLabel, "utn-tight-hidden");  // swapped for the short word when the app bar is tight
    gtk_box_append(GTK_BOX(content), pillLabel);
    pillShort = gtk_label_new(nullptr);
    gtk_widget_add_css_class(pillShort, "utn-tight-only");
    gtk_widget_set_no_show_all(pillShort, true);  // show_all on the toolbar item must not reveal it
    gtk_box_append(GTK_BOX(content), pillShort);
    gtk_widget_show_all(content);
    gtk_widget_hide(pillShort);
    gtk_button_set_child(GTK_BUTTON(menuButton), content);
    gtk_menu_button_set_popover(menuButton, GTK_WIDGET(popover));
    gtk_menu_button_set_direction(menuButton, horizontal ? GTK_ARROW_DOWN : GTK_ARROW_RIGHT);

    pill = GTK_WIDGET(menuButton);
    g_signal_connect(pill, "destroy", G_CALLBACK(+[](GtkWidget* widget, gpointer data) {
                         auto* self = static_cast<ToolStudentView*>(data);
                         if (self->pill == widget) {
                             self->pill = nullptr;
                             self->pillLabel = nullptr;
                             self->pillPrefix = nullptr;
                             self->pillShort = nullptr;
                             self->freezeToggle = nullptr;
                             self->blankToggle = nullptr;
                             self->ghostToggle = nullptr;
                             self->openButton = nullptr;
                             self->fullButton = nullptr;
                             self->closeButton = nullptr;
                         }
                     }),
                     this);

    updateState();
    return xoj::util::WidgetSPtr(pill, xoj::util::adopt);
}

auto ToolStudentView::createQuickControls(bool horizontal) -> xoj::util::WidgetSPtr {
    GtkWidget* box = gtk_box_new(horizontal ? GTK_ORIENTATION_HORIZONTAL : GTK_ORIENTATION_VERTICAL, 4);
    gtk_widget_add_css_class(box, "utn-sv-quick");

    freezeQuick = gtk_toggle_button_new_with_label(_("Freeze"));
    gtk_widget_add_css_class(freezeQuick, "utn-quick-freeze");
    gtk_widget_set_focus_on_click(freezeQuick, false);  // keyboard focus stays where the teacher was working
    g_signal_connect(freezeQuick, "toggled", G_CALLBACK(+[](GtkToggleButton* button, gpointer data) {
                         if (utn::isSyncing(GTK_WIDGET(button))) {
                             return;
                         }
                         auto* self = static_cast<ToolStudentView*>(data);
                         if (!self->window) {
                             // Nothing to freeze yet: say why instead of doing nothing, and leave the button off
                             if (auto* win = self->control->getWindow()) {
                                 win->showToast(_("Open Student View on the projector first. Then Freeze keeps its "
                                                  "screen still while you prepare the next step."));
                             }
                             self->updateState();
                             return;
                         }
                         self->setFrozen(gtk_toggle_button_get_active(button));
                     }),
                     this);
    gtk_box_append(GTK_BOX(box), freezeQuick);

    blankQuick = gtk_toggle_button_new_with_label(_("Blank"));
    gtk_widget_add_css_class(blankQuick, "utn-quick-blank");
    gtk_widget_set_focus_on_click(blankQuick, false);
    g_signal_connect(blankQuick, "toggled", G_CALLBACK(+[](GtkToggleButton* button, gpointer data) {
                         if (!utn::isSyncing(GTK_WIDGET(button))) {
                             static_cast<ToolStudentView*>(data)->setBlanked(gtk_toggle_button_get_active(button));
                         }
                     }),
                     this);
    gtk_box_append(GTK_BOX(box), blankQuick);

    g_signal_connect(box, "destroy", G_CALLBACK(+[](GtkWidget*, gpointer data) {
                         auto* self = static_cast<ToolStudentView*>(data);
                         self->freezeQuick = nullptr;
                         self->blankQuick = nullptr;
                     }),
                     this);
    gtk_widget_show_all(box);
    updateState();
    return xoj::util::WidgetSPtr(box, xoj::util::adopt);
}

void ToolStudentView::showStudentView() {
    if (window != nullptr) {
        gtk_window_present(GTK_WINDOW(window));
        return;
    }
    showTeacherBlankCover(false);

    // A plain window: an application window would show the editor's menubar (File, Edit… Quit) to the class and
    // would answer the application's keyboard shortcuts. The teacher window owns it and closes it on exit.
    window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title(GTK_WINDOW(window), _("UTN Student View"));
    gtk_window_set_icon_name(GTK_WINDOW(window), gtk_window_get_icon_name(control->getGtkWindow()));
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
        // Keep the frame students are seeing now: the cached one, brought up to date if a change is still pending
        const int width = std::max(1, gtk_widget_get_allocated_width(drawingArea));
        const int height = std::max(1, gtk_widget_get_allocated_height(drawingArea));
        if (frozenFrame) {
            cairo_surface_destroy(frozenFrame);
            frozenFrame = nullptr;
        }
        if (frameDirty || frameNeedsResize(width, height)) {
            refreshLiveFrame();
        }
        if (liveFrame) {
            // A copy, so refreshing the live frame after "Resume" can never alter what is frozen
            frozenFrame = createFrameSurface(drawingArea, width, height);
            cairo_t* cr = cairo_create(frozenFrame);
            cairo_set_source_surface(cr, liveFrame, 0, 0);
            cairo_paint(cr);
            cairo_destroy(cr);
            frozenContent = liveContent;
            frozenOverlay = currentOverlay(control);
        }
        frozen = frozenFrame != nullptr;
        hasFrozenFrame = frozen;
    } else if (!enabled) {
        frozen = false;
        if (frozenFrame) {
            cairo_surface_destroy(frozenFrame);
            frozenFrame = nullptr;
        }
        hasFrozenFrame = false;
        // The page may have changed while students saw the frozen frame; never show a stale live frame on resume
        refreshLiveFrame();
    }
    if (drawingArea) {
        gtk_widget_queue_draw(drawingArea);
    }
    updateState();
}

void ToolStudentView::setBlanked(bool enabled) {
    blanked = enabled;
    if (!enabled && drawingArea && !frozen) {
        refreshLiveFrame();  // never show a stale frame when the screen comes back
    }
    if (!window && monitorCount() < 2) {
        // Students see this screen, so cover the teacher's canvas instead
        showTeacherBlankCover(enabled);
    } else if (enabled && !window) {
        showStudentView();
    }
    if (!enabled) {
        // The cover may have been raised before Student View opened; turning Blank off must always take it down,
        // or the teacher is left looking at a cover that hides the only controls that could remove it
        showTeacherBlankCover(false);
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
        gtk_widget_add_css_class(showButton, "utn-blank-show");
        gtk_box_append(GTK_BOX(content), showButton);
        g_object_set_data(G_OBJECT(teacherCover), "utn-show-button", showButton);
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
            // The cover hides every control that could undo it: put keyboard focus on the way back (Enter or Space)
            if (auto* showButton = GTK_WIDGET(g_object_get_data(G_OBJECT(teacherCover), "utn-show-button"))) {
                gtk_widget_grab_focus(showButton);
            }
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
        // Last line of defence: whatever delayed redraw or exposure got us here, a frozen picture that students may no
        // longer see is never painted. Painting black is safe inside a draw; the state change itself follows at once.
        if (!self->frozenFrameIsSafe()) {
            cairo_set_source_rgb(cr, 0.0, 0.0, 0.0);
            cairo_paint(cr);
            using Token = std::pair<std::weak_ptr<std::atomic_bool>, ToolStudentView*>;
            g_idle_add_full(G_PRIORITY_HIGH, +[](gpointer data) -> gboolean {
                std::unique_ptr<Token> token(static_cast<Token*>(data));
                if (auto alive = token->first.lock(); alive && *alive) {
                    token->second->dropFrozenFrameIfUnsafe();
                }
                return G_SOURCE_REMOVE;
            }, new Token(self->alive, self), nullptr);
            return FALSE;
        }
        cairo_set_source_rgb(cr, 0.16, 0.16, 0.16);
        cairo_paint(cr);
        double frameWidth = 0, frameHeight = 0;
        frameSize(self->frozenFrame, frameWidth, frameHeight);
        const double scale = std::min(width / frameWidth, height / frameHeight);
        cairo_translate(cr, 0.5 * (width - frameWidth * scale), 0.5 * (height - frameHeight * scale));
        cairo_scale(cr, scale, scale);
        cairo_set_source_surface(cr, self->frozenFrame, 0, 0);
        cairo_paint(cr);
        return FALSE;
    }
    // Normally the throttled timer has produced the frame. Render here only when there is none yet or the window
    // changed size.
    if (self->liveFrame == nullptr || self->frameNeedsResize(static_cast<int>(width), static_cast<int>(height))) {
        self->refreshLiveFrame();
    }
    if (self->liveFrame) {
        cairo_set_source_surface(cr, self->liveFrame, 0, 0);
        cairo_paint(cr);
    }
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
    if (self->liveFrame) {
        cairo_surface_destroy(self->liveFrame);
        self->liveFrame = nullptr;
    }
    self->updateState();
}

auto ToolStudentView::getToolDisplayName() const -> std::string { return _("Student View"); }

auto ToolStudentView::getNewToolIcon() const -> GtkWidget* { return gtk_label_new("SV"); }

ToolStudentQuick::ToolStudentQuick(std::string id, ToolStudentView& students):
        AbstractToolItem(std::move(id), Category::NAVIGATION), students(students) {}

auto ToolStudentQuick::createItem(bool horizontal) -> xoj::util::WidgetSPtr {
    return students.createQuickControls(horizontal);
}

auto ToolStudentQuick::getToolDisplayName() const -> std::string { return _("Freeze and Blank"); }

auto ToolStudentQuick::getNewToolIcon() const -> GtkWidget* { return gtk_label_new("FB"); }
