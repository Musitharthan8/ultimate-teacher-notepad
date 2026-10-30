/*
 * Ultimate Teacher Notepad
 *
 * Student View: what the class sees, and an always-visible status pill in the app bar.
 *
 * With a second monitor (projector extending the desktop) a clean window shows the current page without
 * teacher-only layers or hidden answers. With one display (projector duplicating the screen) students see the
 * teacher's own window, so the pill says "Same screen" and Blank covers the teacher's canvas instead.
 *
 * The window redraws only when the page changes; it does not poll or copy pages.
 *
 * Based on Xournal++ GPLv2+
 */

#pragma once

#include <atomic>
#include <cstdint>
#include <memory>
#include <string>
#include <thread>
#include <vector>

#include <cairo.h>
#include <gtk/gtk.h>

#include "model/DocumentListener.h"

#include "AbstractToolItem.h"

class Control;
class XojPage;
class Layer;

/// What one student frame shows: the page and which of its layers students were allowed to see. Lets a frozen frame
/// notice that students would now be allowed to see less.
struct StudentFrameContent {
    std::shared_ptr<XojPage> page;
    std::vector<const Layer*> visibleLayers;  // compared by address only, never dereferenced unless on page
    uint64_t layerRemovals = 0;  ///< XojPage::getLayerRemovalCount() when the frame was drawn
    uint64_t privateTransfers = 0;  ///< utn::privateTransferCount() when the frame was drawn
};

/// Spotlight and Curtain are different overlays: switching from one to the other is a change, not a continuation
enum class OverlayKind { None, Spotlight, Curtain };

class ToolStudentView: public AbstractToolItem, public DocumentListener {
public:
    /// What students can see right now
    enum class State { Off, SameScreen, Live, Frozen, Blank };

    ToolStudentView(std::string id, Control* control);
    ~ToolStudentView() override;

    xoj::util::WidgetSPtr createItem(bool horizontal) override;
    /// Freeze and Blank as two always-visible toggle buttons, for the app bar beside the pill. They drive the same
    /// state as the pill's menu and follow it.
    xoj::util::WidgetSPtr createQuickControls(bool horizontal);

    std::string getToolDisplayName() const override;
    GtkWidget* getNewToolIcon() const override;

    State getState() const;
    /// Pill wording for a state, e.g. "Students: Live"
    static const char* stateLabel(State state);
    /// The state word alone, as short as it can be said ("Same" for "Same screen"). The pill shows it when the app bar
    /// is tight; the accessible name keeps the whole phrase.
    static const char* stateShortWord(State state);

    // DocumentListener
    void pageSelected(size_t page) override;
    void pageChanged(size_t page) override;
    void pageSizeChanged(size_t page) override;
    void documentChanged(DocumentChangeType type) override;

private:
    void showStudentView();
    void closeStudentView();
    void setFrozen(bool frozen);
    void setBlanked(bool blanked);
    void setStudentFullscreen(bool enabled);

    /// True when the Student View window sits on a different monitor from the teacher's window
    bool isOnSeparateMonitor() const;
    static int monitorCount();

    /// Recompute ghosting, the pill and menu sensitivity after any state change
    void updateState();
    /**
     * Schedule one redraw of the student window; safe from any thread. Redraws are rate limited by their own cost,
     * because a frame renders on the UI thread that also handles the stylus. @p urgent skips the wait: use it
     * whenever what students may see has shrunk (answers hidden, Curtain on).
     */
    void requestRedraw(bool urgent = false);
    /// Render the frame students see into liveFrame
    void refreshLiveFrame();
    bool frameNeedsResize(int width, int height) const;
    /// True if which layers students can see on the current page differs from the last call. Never blocks.
    bool studentAudienceChanged();

    /**
     * A frozen frame keeps whatever it showed, including answers that were revealed at that moment. If students may now
     * see less (an answers layer hidden, a layer made private or invisible, Curtain or Spotlight switched on), the
     * frozen frame is destroyed and the students' screen goes blank at once: a blank frame is always safe, whereas
     * a replacement would have to be rendered first. The teacher is told, and "Show the page again" resumes live.
     * @return true if the frame was removed
     */
    bool dropFrozenFrameIfUnsafe();
    bool frozenFrameIsSafe();
    /// Called for every change that may reduce what students can see, wherever it was made. See model/StudentVisibility.h
    void onStudentVisibilityChanged(bool mayCheckNow);
    static OverlayKind currentOverlay(Control* control);

    void showTeacherBlankCover(bool show);

    static gboolean drawStudentView(GtkWidget* widget, cairo_t* cr, gpointer data);
    static gboolean studentViewKeyPressed(GtkWidget* widget, GdkEventKey* event, gpointer data);
    static void studentViewDestroyed(GtkWidget* widget, gpointer data);

private:
    Control* control;
    /// GTK objects may only be touched from this thread (the one that built the tool); other threads only schedule
    std::thread::id uiThread = std::this_thread::get_id();
    GtkWidget* window = nullptr;
    GtkWidget* drawingArea = nullptr;
    bool fullscreen = false;
    bool frozen = false;
    bool blanked = false;

    /// Last frame shown to students, kept while frozen
    cairo_surface_t* frozenFrame = nullptr;
    /// The current live frame. The draw handler only copies it; refreshLiveFrame() is the only place that renders.
    cairo_surface_t* liveFrame = nullptr;
    StudentFrameContent liveContent;    ///< what liveFrame shows
    StudentFrameContent frozenContent;  ///< what frozenFrame shows
    OverlayKind frozenOverlay = OverlayKind::None;  ///< Curtain or Spotlight that was part of the frozen picture
    std::atomic_bool hasFrozenFrame{false};         ///< mirror of frozenFrame != nullptr, readable from any thread
    size_t visibilityListenerId = 0;
    std::shared_ptr<std::atomic_bool> alive = std::make_shared<std::atomic_bool>(true);
    std::atomic_bool visibilityCheckQueued{false};
    std::atomic_bool frameDirty{true};
    std::atomic<guint> nextRedrawDelayMs{40};
    gint64 lastFrameEnd = 0;
    uint64_t lastAudienceSignature = 0;

    // App-bar pill and its menu entries (the latest toolbar instance)
    GtkWidget* pill = nullptr;
    GtkWidget* pillLabel = nullptr;
    GtkWidget* pillPrefix = nullptr;
    GtkWidget* pillShort = nullptr;  ///< short state word, shown instead of pillLabel while the app bar is tight
    GtkWidget* freezeToggle = nullptr;
    GtkWidget* blankToggle = nullptr;
    // The same two actions as app-bar buttons (the latest toolbar instance)
    GtkWidget* freezeQuick = nullptr;
    GtkWidget* blankQuick = nullptr;
    GtkWidget* ghostToggle = nullptr;
    GtkWidget* openButton = nullptr;
    GtkWidget* fullButton = nullptr;
    GtkWidget* closeButton = nullptr;

    GtkWidget* teacherCover = nullptr;
    /// Coloured frame round the teacher's canvas while students see a frozen or blank screen
    GtkWidget* teacherOutline = nullptr;
    void updateTeacherOutline(State state);

    size_t canvasListenerId = 0;
    std::atomic_bool redrawPending{false};
    std::atomic_uint redrawSource{0};
    gulong monitorAddedHandler = 0;
    gulong monitorRemovedHandler = 0;
};

/// App-bar item holding the Freeze and Blank buttons of a ToolStudentView. A separate item, so a toolbar layout can
/// place them anywhere, and so ToolStudentView::createItem keeps returning only the status pill.
class ToolStudentQuick: public AbstractToolItem {
public:
    ToolStudentQuick(std::string id, ToolStudentView& students);
    ~ToolStudentQuick() override = default;

    xoj::util::WidgetSPtr createItem(bool horizontal) override;

    std::string getToolDisplayName() const override;
    GtkWidget* getNewToolIcon() const override;

private:
    ToolStudentView& students;
};
