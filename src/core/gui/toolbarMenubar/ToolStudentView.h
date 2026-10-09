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
#include <string>

#include <cairo.h>
#include <gtk/gtk.h>

#include "model/DocumentListener.h"

#include "AbstractToolItem.h"

class Control;
class XojPage;

class ToolStudentView: public AbstractToolItem, public DocumentListener {
public:
    /// What students can see right now
    enum class State { Off, SameScreen, Live, Frozen, Blank };

    ToolStudentView(std::string id, Control* control);
    ~ToolStudentView() override;

    xoj::util::WidgetSPtr createItem(bool horizontal) override;

    std::string getToolDisplayName() const override;
    GtkWidget* getNewToolIcon() const override;

    State getState() const;
    /// Pill wording for a state, e.g. "Students: Live"
    static const char* stateLabel(State state);

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
    /// Schedule one redraw of the student window; safe from any thread
    void requestRedraw();

    void showTeacherBlankCover(bool show);

    static gboolean drawStudentView(GtkWidget* widget, cairo_t* cr, gpointer data);
    static gboolean studentViewKeyPressed(GtkWidget* widget, GdkEventKey* event, gpointer data);
    static void studentViewDestroyed(GtkWidget* widget, gpointer data);

private:
    Control* control;
    GtkWidget* window = nullptr;
    GtkWidget* drawingArea = nullptr;
    bool fullscreen = false;
    bool frozen = false;
    bool blanked = false;

    /// Last frame shown to students, kept while frozen
    cairo_surface_t* frozenFrame = nullptr;

    // App-bar pill and its menu entries (the latest toolbar instance)
    GtkWidget* pill = nullptr;
    GtkWidget* pillLabel = nullptr;
    GtkWidget* freezeToggle = nullptr;
    GtkWidget* blankToggle = nullptr;

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
