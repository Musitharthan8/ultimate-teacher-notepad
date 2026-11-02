#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cmath>
#include <fstream>
#include <functional>
#include <map>
#include <memory>
#include <set>
#include <string>
#include <thread>
#include <vector>

#include <gtest/gtest.h>
#include <gtk/gtk.h>

#include "control/Control.h"
#include "control/ToolHandler.h"
#include "control/UtnPerf.h"
#include "control/actions/ActionDatabase.h"
#include "control/jobs/AutosaveJob.h"
#include "control/jobs/CustomExportJob.h"
#include "control/jobs/PdfExportJob.h"
#include "control/jobs/SaveJob.h"
#include "control/jobs/XournalScheduler.h"
#include "control/layer/LayerController.h"
#include "control/settings/Settings.h"
#include "control/tools/TextEditor.h"
#include "control/tools/EditSelection.h"
#include "control/xojfile/LoadHandler.h"
#include "control/zoom/ZoomControl.h"
#include "control/xojfile/SaveHandler.h"
#include "gui/GladeSearchpath.h"
#include "gui/MainWindow.h"
#include "gui/toolbarMenubar/model/ToolbarData.h"
#include "gui/ResponsiveLayout.h"
#include "gui/menus/menubar/Menubar.h"
#include "gui/dialog/ExportDialog.h"
#include "gui/FloatingToolbox.h"
#include "gui/PdfFloatingToolbox.h"
#include "gui/Layout.h"
#include "gui/PageView.h"
#include "gui/SelectionToolbar.h"
#include "gui/UtnToast.h"
#include "gui/XournalView.h"
#include "gui/inputdevices/PositionInputData.h"
#include "gui/toolbarMenubar/DockCustomizeDialog.h"
#include "gui/toolbarMenubar/ToolMenuHandler.h"
#include "gui/toolbarMenubar/PenPresetsMenu.h"
#include "gui/toolbarMenubar/RevealActions.h"
#include "gui/CommandPalette.h"
#include "gui/sidebar/Sidebar.h"
#include "gui/toolbarMenubar/ToolLessonNavigator.h"
#include "gui/toolbarMenubar/ToolPresentationKit.h"
#include "gui/toolbarMenubar/ToolStudentView.h"
#include "gui/toolbarMenubar/ToolUtnContextBar.h"
#include "gui/toolbarMenubar/UtnWidgets.h"
#include "model/Document.h"
#include "model/DocumentLockAudit.h"
#include "model/Layer.h"
#include "model/PageType.h"
#include "model/Text.h"
#include "model/Stroke.h"
#include "model/XojPage.h"
#include "undo/InsertUndoAction.h"
#include "undo/UndoRedoHandler.h"
#include "model/eraser/MaskedStrokeCodec.h"
#include "util/PathUtil.h"
#include "util/PopupWindowWrapper.h"
#include "util/XojMsgBox.h"
#include "config-test.h"

namespace {
GtkWidget* buttonNamed(GtkWidget* widget, const char* label) {
    if (GTK_IS_BUTTON(widget) && g_strcmp0(gtk_button_get_label(GTK_BUTTON(widget)), label) == 0) {
        return widget;
    }
    if (GTK_IS_CONTAINER(widget)) {
        GList* children = gtk_container_get_children(GTK_CONTAINER(widget));
        GtkWidget* found = nullptr;
        for (GList* child = children; child && !found; child = child->next) {
            found = buttonNamed(GTK_WIDGET(child->data), label);
        }
        g_list_free(children);
        return found;
    }
    return nullptr;
}

GtkWidget* widgetNamed(GtkWidget* widget, const char* name) {
    if (g_strcmp0(gtk_widget_get_name(widget), name) == 0) {
        return widget;
    }
    if (GTK_IS_CONTAINER(widget)) {
        GList* children = gtk_container_get_children(GTK_CONTAINER(widget));
        GtkWidget* found = nullptr;
        for (GList* child = children; child && !found; child = child->next) {
            found = widgetNamed(GTK_WIDGET(child->data), name);
        }
        g_list_free(children);
        return found;
    }
    return nullptr;
}

GtkWidget* buttonAccessibleNamed(GtkWidget* widget, const char* name) {
    if (GTK_IS_BUTTON(widget)) {
        AtkObject* accessible = gtk_widget_get_accessible(widget);
        if (accessible && g_strcmp0(atk_object_get_name(accessible), name) == 0) {
            return widget;
        }
    }
    if (GTK_IS_CONTAINER(widget)) {
        GList* children = gtk_container_get_children(GTK_CONTAINER(widget));
        GtkWidget* found = nullptr;
        for (GList* child = children; child && !found; child = child->next) {
            found = buttonAccessibleNamed(GTK_WIDGET(child->data), name);
        }
        g_list_free(children);
        return found;
    }
    return nullptr;
}

GtkWidget* menuButtonAccessibleNamed(GtkWidget* widget, const char* name) {
    if (GTK_IS_MENU_BUTTON(widget)) {
        if (AtkObject* accessible = gtk_widget_get_accessible(widget);
            accessible && g_strcmp0(atk_object_get_name(accessible), name) == 0) {
            return widget;
        }
    }
    if (GTK_IS_CONTAINER(widget)) {
        GList* children = gtk_container_get_children(GTK_CONTAINER(widget));
        GtkWidget* found = nullptr;
        for (GList* child = children; child && !found; child = child->next) {
            found = menuButtonAccessibleNamed(GTK_WIDGET(child->data), name);
        }
        g_list_free(children);
        return found;
    }
    return nullptr;
}

GtkWidget* studentWindow() {
    GList* windows = gtk_window_list_toplevels();
    GtkWidget* found = nullptr;
    for (GList* entry = windows; entry; entry = entry->next) {
        if (g_strcmp0(gtk_window_get_title(GTK_WINDOW(entry->data)), "UTN Student View") == 0) {
            found = GTK_WIDGET(entry->data);
        }
    }
    g_list_free(windows);
    return found;
}

GtkWidget* railLabel(GtkWidget* widget, const char* text) {
    if (GTK_IS_LABEL(widget) && g_strcmp0(gtk_widget_get_name(widget), "utnRailLabel") == 0 &&
        g_strcmp0(gtk_label_get_text(GTK_LABEL(widget)), text) == 0) {
        return widget;
    }
    if (GTK_IS_CONTAINER(widget)) {
        GList* children = gtk_container_get_children(GTK_CONTAINER(widget));
        GtkWidget* found = nullptr;
        for (GList* child = children; child && !found; child = child->next) {
            found = railLabel(GTK_WIDGET(child->data), text);
        }
        g_list_free(children);
        return found;
    }
    return nullptr;
}

void settle() {
    for (int i = 0; i < 30; ++i) {
        for (int event = 0; event < 20 && gtk_events_pending(); ++event) {
            gtk_main_iteration_do(false);
        }
        g_usleep(2000);
    }
}

/// A GTK3 popover fades out after popdown: `visible` and `mapped` stay true until the transition (about 150 to 200 ms,
/// driven by the frame clock) has finished. Wait for that end state instead of sampling the transition.
bool waitUntilHidden(GtkWidget* popover, gint64 timeoutUs = 3 * G_USEC_PER_SEC) {
    const gint64 deadline = g_get_monotonic_time() + timeoutUs;
    while (gtk_widget_get_visible(popover) || gtk_widget_get_mapped(popover)) {
        if (g_get_monotonic_time() > deadline) {
            return false;
        }
        for (int event = 0; event < 20 && gtk_events_pending(); ++event) {
            gtk_main_iteration_do(false);
        }
        g_usleep(2000);
    }
    return true;
}

void captureWindow(GtkWidget* window, const char* path) {
    GdkWindow* gdkWindow = gtk_widget_get_window(window);
    if (!gdkWindow) {
        return;
    }
    GdkPixbuf* image = gdk_pixbuf_get_from_window(gdkWindow, 0, 0, gtk_widget_get_allocated_width(window),
                                                 gtk_widget_get_allocated_height(window));
    if (!image) {
        return;
    }
    GError* error = nullptr;
    gdk_pixbuf_save(image, path, "png", &error, nullptr);
    g_clear_error(&error);
    g_object_unref(image);
}

unsigned studentPixel(GtkWidget* student, double pageX, double pageY) {
    auto* area = gtk_bin_get_child(GTK_BIN(student));
    const int width = gtk_widget_get_allocated_width(area);
    const int height = gtk_widget_get_allocated_height(area);
    auto* surface = cairo_image_surface_create(CAIRO_FORMAT_RGB24, width, height);
    auto* cr = cairo_create(surface);
    gboolean handled = false;
    g_signal_emit_by_name(area, "draw", cr, &handled);
    cairo_destroy(cr);
    cairo_surface_flush(surface);
    const double scale = std::min(width / 600.0, height / 800.0);
    const int x = static_cast<int>((width - 600 * scale) / 2 + pageX * scale);
    const int y = static_cast<int>((height - 800 * scale) / 2 + pageY * scale);
    auto* row = cairo_image_surface_get_data(surface) + y * cairo_image_surface_get_stride(surface);
    const unsigned pixel = reinterpret_cast<unsigned*>(row)[x] & 0xFFFFFF;
    cairo_surface_destroy(surface);
    return pixel;
}

/**
 * The fixture builds Control and MainWindow without starting the job thread (XournalMain does that in the real
 * application), so queued render and autosave jobs never ran in these tests. The soak tests start it, once: Scheduler
 * cannot be restarted and its destructor stops it.
 */
void ensureSchedulerRunning(Control* control) {
    static bool started = false;
    if (!started) {
        control->getScheduler()->start();
        started = true;
    }
}

/**
 * Fails the test, loudly, when the temporary folder cannot be written and read back. Set TMP / TEMP / TMPDIR to a
 * folder you own (for example `set TMP=C:\Users\you\utn-tmp`) before running when the default, under C:\msys64\tmp,
 * is not writable for your account. Nothing is skipped: a soak that cannot autosave proves nothing.
 */
void requireWorkingTempFolder() {
    const auto folder = Util::getTmpDirSubfolder();
    const auto probe = folder / "utn-tmp-probe.txt";
    {
        std::ofstream out(probe, std::ios::binary);
        out << "probe";
    }
    std::ifstream in(probe, std::ios::binary);
    std::string text;
    in >> text;
    in.close();
    std::error_code ec;
    fs::remove(probe, ec);
    ASSERT_EQ(text, "probe") << "the temporary folder " << folder.string() << " cannot be written or read back";
}

/// Writes one metric to the XML report with the population of every number in its name: all_* covers every sample
/// since the reset, win_* the last win_n samples (the same set unless more than 65536 were recorded).
void recordPerfProperties(utn::perf::Metric metric) {
    const auto stats = utn::perf::stats(metric);
    const std::string base = std::string("perf_") + utn::perf::name(metric);
    ::testing::Test::RecordProperty(base + "_all_n", std::to_string(stats.count));
    ::testing::Test::RecordProperty(base + "_all_mean", std::to_string(stats.mean()));
    ::testing::Test::RecordProperty(base + "_all_max", std::to_string(stats.max));
    ::testing::Test::RecordProperty(base + "_win_n", std::to_string(stats.windowCount));
    ::testing::Test::RecordProperty(base + "_win_mean", std::to_string(stats.windowMean));
    ::testing::Test::RecordProperty(base + "_win_p95", std::to_string(stats.p95));
    ::testing::Test::RecordProperty(base + "_win_max", std::to_string(stats.windowMax));
}

void addMarker(Layer* layer, double y) {
    auto ink = std::make_unique<Stroke>();
    ink->setWidth(12);
    ink->setColor(Color(0U));
    ink->addPoint(Point(80, y));
    ink->addPoint(Point(120, y));
    layer->addElement(std::move(ink));
}

/// A failed save or export shows a message dialog; close them so they do not stay on screen for the next test
void closeMessageDialogs() {
    GList* windows = gtk_window_list_toplevels();
    for (GList* w = windows; w; w = w->next) {
        if (GTK_IS_MESSAGE_DIALOG(w->data)) {
            gtk_widget_destroy(GTK_WIDGET(w->data));
        }
    }
    g_list_free(windows);
    settle();
}

class ClassroomWorkflow: public ::testing::Test {
protected:
    void SetUp() override {
        if (!gtk_init_check(nullptr, nullptr)) {
            GTEST_SKIP() << "No display available";
        }
        // A test that ran a failing save, export or autosave directly leaves a modal message box (which takes every key)
        // and, if it never unblocked, every action disabled. Neither may reach the next test.
        closeMessageDialogs();
        if (control) {
            while (control->isBlocked()) {
                control->unblock();  // a test that blocked more than once and never released leaves a depth
            }
        }
        if (!control) {
            app = gtk_application_new("org.utn.classroom.qa", G_APPLICATION_NON_UNIQUE);
            GError* error = nullptr;
            ASSERT_TRUE(g_application_register(G_APPLICATION(app), nullptr, &error)) << (error ? error->message : "");
            g_clear_error(&error);
            paths.addSearchDirectory(fs::path(GET_UI_FOLDER));
            paths.addSearchDirectory(fs::path(PROJECT_SOURCE_DIR) / "resources");
            paths.addSearchDirectory(Util::getExePath().parent_path() / "resources-templates");
            control = std::make_unique<Control>(G_APPLICATION(app), &paths, true);
            control->getSettings()->setAutosaveEnabled(false);
            window = std::make_unique<MainWindow>(&paths, control.get(), app);
            control->initWindow(window.get());
            window->populate(&paths);
        } else {
            control->clearSelectionEndText();
            control->getUndoRedoHandler()->clearContents();
            control->undoRedoChanged();  // clearing the history does not tell the Undo and Redo actions
            control->getDocument()->clearDocument(true);
        }
        if (!control->getSettings()->isSidebarVisible()) {
            control->setShowSidebar(true);  // a test that hid the sidebar must not decide what the next one starts with
        }
        control->setGhostHiddenAnswers(false);
        window->setTeacherWorkspace(0);
        control->selectTool(TOOL_SELECT_RECT);  // the Select button remembers the last selection tool a test used
        control->selectTool(TOOL_PEN);
        for (int i = 0; i < 2; ++i) {
            auto page = std::make_shared<XojPage>(600, 800);
            page->setBackgroundType(PageType(PageTypeFormat::Plain));
            control->getDocument()->addPage(page);
        }
        control->fireDocumentChanged(DOCUMENT_CHANGE_COMPLETE);
        control->DocumentHandler::firePageSelected(0);
        // The keyboard must not start on a widget an earlier test hid (a sidebar list): it would decide what the next one sees
        if (GtkWidget* canvas = window->getXournal()->getWidget(); gtk_widget_is_drawable(canvas)) {
            gtk_window_set_focus(GTK_WINDOW(window->getWindow()), canvas);
        } else {
            gtk_window_set_focus(GTK_WINDOW(window->getWindow()), nullptr);
        }
        settle();
    }

    void TearDown() override {
        if (!window) {
            return;
        }
        // A test that failed half way must not leave the palette or the Blank cover up for the next one
        if (auto* palette = window->getCommandPalette()) {
            palette->close();
        }
        if (auto* handler = window->getToolMenuHandler()) {
            if (auto* students = handler->getStudentView(); students && students->isTeacherCoverShown()) {
                students->showTeacherBlankCover(false);
            }
        }
        settle();
    }

    static void TearDownTestSuite() {
        if (window) {
            control->clearSelectionEndText();
            // Match application ownership: toolbar tools must go away before Control.
            unsigned criticals = 0;
            const auto handler = g_log_set_handler("GLib-GObject", G_LOG_LEVEL_CRITICAL,
                    +[](const gchar*, GLogLevelFlags, const gchar*, gpointer data) {
                        ++*static_cast<unsigned*>(data);
                    }, &criticals);
            window.reset();
            g_log_remove_handler("GLib-GObject", handler);
            EXPECT_EQ(criticals, 0U) << "Closing the classroom window must release GTK objects safely";
            control.reset();
            XojMsgBox::setDefaultWindow(nullptr);
        }
        if (app) {
            g_object_unref(app);
        }
    }

    TextEditor* editAt(double x, double y) {
        auto* view = window->getXournal()->getViewFor(control->getCurrentPageNo());
        if (!view) {
            return nullptr;
        }
        const double zoom = view->getZoom();
        PositionInputData pos{};
        pos.x = x * zoom;
        pos.y = y * zoom;
        pos.pressure = 1;
        if (!view->onButtonPressEvent(pos)) {
            return nullptr;
        }
        view->onButtonReleaseEvent(pos);
        return control->getTextEditor();
    }

    inline static GladeSearchpath paths;
    inline static GtkApplication* app = nullptr;
    inline static std::unique_ptr<Control> control;
    inline static std::unique_ptr<MainWindow> window;
};
}  // namespace

namespace {
const std::vector<const char*> RAIL_CAPTIONS = {"Select",     "Area",   "Pen",        "Shapes",   "Highlight",
                                                "Colours",    "Eraser", "Text",       "Answer Box", "Feedback",
                                                "Classroom",  "Reveal", "Move"};

/// Every rail caption must show exactly when `expectVisible` says so
void expectCaptions(GtkWidget* rail, bool expectVisible, const std::string& context) {
    for (const char* text: RAIL_CAPTIONS) {
        auto* label = railLabel(rail, text);
        ASSERT_NE(label, nullptr) << text << " " << context;
        EXPECT_EQ(gtk_widget_get_visible(label), expectVisible) << text << " " << context;
    }
}

/// Run the event loop until the toplevel allocation stops changing, and return it. A desktop smaller than the
/// request (the 1024 px GitHub Windows runner) clamps the window, so the caller must read what was really allocated.
int allocatedWidthAfterResize(GtkWidget* window, int requested) {
    gtk_window_resize(GTK_WINDOW(window), requested, 720);
    int previous = -1;
    int current = gtk_widget_get_allocated_width(window);
    for (int attempt = 0; attempt < 30 && current != previous; ++attempt) {
        previous = current;
        settle();
        current = gtk_widget_get_allocated_width(window);
    }
    return current;
}
}  // namespace

TEST_F(ClassroomWorkflow, teacherRailCaptionsSitBeneathIconsWithAccessibleNames) {
    window->toolbarSelected("UTN Teacher");
    gtk_widget_show(window->getWindow());
    settle();
    window->setToolLabelMode(1);  // captions shown, so the structure is checked at any width
    auto* rail = window->get("tbLeft1");
    expectCaptions(rail, true, "Show");
    for (const char* text: RAIL_CAPTIONS) {
        auto* label = railLabel(rail, text);
        ASSERT_NE(label, nullptr) << text;
        auto* box = gtk_widget_get_parent(label);
        ASSERT_TRUE(GTK_IS_BOX(box)) << text;
        EXPECT_EQ(gtk_orientable_get_orientation(GTK_ORIENTABLE(box)), GTK_ORIENTATION_VERTICAL) << text;
        auto* button = gtk_widget_get_parent(box);
        ASSERT_TRUE(GTK_IS_BUTTON(button)) << text;
        EXPECT_NE(atk_object_get_name(gtk_widget_get_accessible(button)), nullptr) << text;
    }
    window->setToolLabelMode(0);
}

// Widget level test at the width the desktop really gives the window: the monitor decides what is allocated, so the
// expectation is derived from the allocation and the documented thresholds, never from the requested size.
TEST_F(ClassroomWorkflow, teacherRailCaptionsFollowTheRealAllocatedWidth) {
    window->toolbarSelected("UTN Teacher");
    control->selectTool(TOOL_ERASER);
    gtk_widget_show(window->getWindow());
    settle();
    gtk_window_unmaximize(GTK_WINDOW(window->getWindow()));
    settle();
    window->setToolLabelMode(0);
    auto* rail = window->get("tbLeft1");
    for (int requested: {1280, 1024, 1500, 900, 1280}) {
        const int actual = allocatedWidthAfterResize(window->getWindow(), requested);
        const std::string context = "requested " + std::to_string(requested) + ", allocated " + std::to_string(actual);
        SCOPED_TRACE(context);
        RecordProperty("allocated_width_for_request_" + std::to_string(requested), actual);
        ASSERT_GT(actual, utn::layout::MIN_MEANINGFUL_WIDTH);
        // Outside the hysteresis band the layout is fully determined by the width
        if (actual < utn::layout::COMPACT_ENTER_BELOW) {
            EXPECT_TRUE(window->isCompactLayout());
        } else if (actual > utn::layout::COMPACT_LEAVE_ABOVE) {
            EXPECT_FALSE(window->isCompactLayout());
        }
        for (int mode: {0, 1, 2}) {
            window->setToolLabelMode(mode);
            expectCaptions(rail, utn::layout::railLabelsVisible(mode, window->isCompactLayout()),
                           context + ", mode " + std::to_string(mode));
        }
        window->setToolLabelMode(0);
        EXPECT_EQ(control->getToolHandler()->getToolType(), TOOL_ERASER);
    }
}

// The same widgets at fixed logical widths, independent of the monitor. updateCompactLayout() is what the
// size-allocate handler calls; nothing iterates the main loop between the call and the check, so the real allocation
// cannot overwrite the width under test.
TEST_F(ClassroomWorkflow, teacherRailCaptionsAtFixedLogicalWidths) {
    window->toolbarSelected("UTN Teacher");
    control->selectTool(TOOL_PEN);
    gtk_widget_show(window->getWindow());
    settle();
    const int realWidth = gtk_widget_get_allocated_width(window->getWindow());
    auto* rail = window->get("tbLeft1");
    auto atWidth = [&](int width, int mode, bool expectCompact) {
        window->setToolLabelMode(mode);
        window->updateCompactLayout(width);
        const std::string context = "width " + std::to_string(width) + ", mode " + std::to_string(mode);
        EXPECT_EQ(window->isCompactLayout(), expectCompact) << context;
        expectCaptions(rail, utn::layout::railLabelsVisible(mode, expectCompact), context);
    };

    // Automatic: wide -> compact -> wide -> compact
    atWidth(1280, 0, false);
    atWidth(1008, 0, true);   // what the 1024 px CI desktop allocates
    atWidth(1280, 0, false);  // the wide behaviour is exercised even where no monitor can show it
    atWidth(1024, 0, true);
    atWidth(1920, 0, false);
    // Hysteresis band: the state before the band is kept
    atWidth(1100, 0, false);
    atWidth(1079, 0, true);
    atWidth(1100, 0, true);
    atWidth(1121, 0, false);
    // Show and Hide ignore the width
    for (int width: {1008, 1280, 1920}) {
        for (int mode: {1, 2}) {
            atWidth(width, mode, utn::layout::nextCompactLayout(window->isCompactLayout(), width));
        }
    }
    atWidth(1008, 1, true);
    expectCaptions(rail, true, "Show at 1008");
    atWidth(1280, 2, false);
    expectCaptions(rail, false, "Hide at 1280");

    // Tool state and the inspector workspace survive a layout change
    window->setToolLabelMode(0);
    control->selectTool(TOOL_ERASER);
    window->updateCompactLayout(1008);
    window->setTeacherWorkspace(1);
    window->updateCompactLayout(1280);
    window->setTeacherWorkspace(0);
    window->updateCompactLayout(1008);
    EXPECT_EQ(control->getToolHandler()->getToolType(), TOOL_ERASER);
    EXPECT_EQ(window->getTeacherWorkspace(), 0);
    EXPECT_NE(railLabel(rail, "Pen"), nullptr);

    window->updateCompactLayout(realWidth);
    window->setToolLabelMode(0);
}

namespace {
GtkWidget* labelWithText(GtkWidget* widget, const char* text) {
    if (GTK_IS_LABEL(widget) && g_strcmp0(gtk_label_get_text(GTK_LABEL(widget)), text) == 0) {
        return widget;
    }
    if (GTK_IS_CONTAINER(widget)) {
        GList* children = gtk_container_get_children(GTK_CONTAINER(widget));
        GtkWidget* found = nullptr;
        for (GList* child = children; child && !found; child = child->next) {
            found = labelWithText(GTK_WIDGET(child->data), text);
        }
        g_list_free(children);
        return found;
    }
    return nullptr;
}
}  // namespace

TEST_F(ClassroomWorkflow, appBarReducesSecondaryCaptionsBeforeItsCompactBreakpoint) {
    window->toolbarSelected("UTN Teacher");
    gtk_widget_show(window->getWindow());
    settle();
    const int realWidth = gtk_widget_get_allocated_width(window->getWindow());
    auto* appBar = window->get("tbTop1");
    auto* brand = labelWithText(appBar, "UTN");
    ASSERT_NE(brand, nullptr);
    auto* freeze = buttonNamed(appBar, "Freeze");
    auto* blank = buttonNamed(appBar, "Blank");
    ASSERT_NE(freeze, nullptr) << "the default Teacher layout carries Freeze";
    ASSERT_NE(blank, nullptr) << "the default Teacher layout carries Blank";

    window->updateCompactLayout(1280);
    EXPECT_TRUE(gtk_widget_get_visible(brand)) << "the UTN identity remains visible at the primary desktop width";
    window->updateCompactLayout(1008);
    EXPECT_FALSE(gtk_widget_get_visible(brand)) << "compact: the title bar already names the app";
    EXPECT_TRUE(gtk_widget_get_visible(freeze)) << "Freeze is never hidden";
    EXPECT_TRUE(gtk_widget_get_visible(blank)) << "Blank is never hidden";
    window->updateCompactLayout(1400);
    EXPECT_TRUE(gtk_widget_get_visible(brand)) << "the UTN identity returns once the wider compact band has room";
    window->updateCompactLayout(1401);
    EXPECT_TRUE(gtk_widget_get_visible(brand));
    window->updateCompactLayout(realWidth);
    window->toolbarSelected("UTN Present");
    EXPECT_TRUE(gtk_style_context_has_class(gtk_widget_get_style_context(window->getWindow()), "utnPresent"));
    window->toolbarSelected("UTN Teacher");
    EXPECT_FALSE(gtk_style_context_has_class(gtk_widget_get_style_context(window->getWindow()), "utnPresent"));
}

TEST_F(ClassroomWorkflow, appBarFitsAtTheSmallestSupportedWindowSoNoMenuIsPushedOut) {
    // GtkToolbar builds no overflow menu for wrapped widgets such as Appearance and Pen settings, so one that does
    // not fit is silently unreachable. Measure rather than read allocations: this harness has no frame clock, so a
    // reflow after the compact layout applies only shows up in the real app (checked there at 1280 -> 800).
    window->toolbarSelected("UTN Present");
    window->toolbarSelected("UTN Teacher");
    gtk_widget_show(window->getWindow());
    const int actual = allocatedWidthAfterResize(window->getWindow(), 800);
    ASSERT_LT(actual, utn::layout::COMPACT_ENTER_BELOW) << "the window must really be narrow for this check";
    EXPECT_TRUE(window->isCompactLayout());
    auto* appBar = window->get("tbTop1");
    ASSERT_TRUE(GTK_IS_TOOLBAR(appBar));
    int minWidth = 0;
    int natWidth = 0;
    gtk_widget_get_preferred_width(appBar, &minWidth, &natWidth);
    EXPECT_LE(natWidth, actual) << "the app bar needs " << natWidth << " px but the window is " << actual << " px";
    // The brand and the "Students:" prefix are what the compact bar gives up; the state word stays visible
    auto* brand = labelWithText(appBar, "UTN");
    ASSERT_NE(brand, nullptr);
    EXPECT_FALSE(gtk_widget_get_visible(brand));
    auto* prefix = labelWithText(appBar, "Students:");
    ASSERT_NE(prefix, nullptr);
    EXPECT_FALSE(gtk_widget_get_visible(prefix));
    auto* pill = gtk_widget_get_ancestor(prefix, GTK_TYPE_MENU_BUTTON);
    ASSERT_NE(pill, nullptr);
    EXPECT_EQ(std::string(atk_object_get_name(gtk_widget_get_accessible(pill))).rfind("Students: ", 0), 0u)
            << "the accessible name keeps the whole phrase";
}

namespace {
/// The app-bar buttons that may move into the "More" menu, in the order they are meant to go
std::vector<GtkToolItem*> movableAppBarItems(GtkWidget* appBar) {
    std::vector<std::pair<int, GtkToolItem*>> ranked;
    auto* bar = GTK_TOOLBAR(appBar);
    for (int i = 0; i < gtk_toolbar_get_n_items(bar); ++i) {
        GtkToolItem* item = gtk_toolbar_get_nth_item(bar, i);
        const int rank = GPOINTER_TO_INT(g_object_get_data(G_OBJECT(item), utn::OVERFLOW_RANK_KEY));
        if (rank > 0) {
            ranked.emplace_back(rank, item);
        }
    }
    std::sort(ranked.begin(), ranked.end(), [](const auto& a, const auto& b) { return a.first < b.first; });
    std::vector<GtkToolItem*> items;
    for (const auto& entry: ranked) {
        items.push_back(entry.second);
    }
    return items;
}

/// The button inside a tool item (or popover) that runs an action
GtkWidget* actionableIn(GtkWidget* widget) {
    if (GTK_IS_ACTIONABLE(widget) && gtk_actionable_get_action_name(GTK_ACTIONABLE(widget))) {
        return widget;
    }
    GtkWidget* found = nullptr;
    if (GTK_IS_CONTAINER(widget)) {
        GList* children = gtk_container_get_children(GTK_CONTAINER(widget));
        for (GList* child = children; child && !found; child = child->next) {
            found = actionableIn(GTK_WIDGET(child->data));
        }
        g_list_free(children);
    }
    return found;
}

GtkWidget* moreButtonOf(MainWindow* window) {
    return menuButtonAccessibleNamed(window->get("tbTop1"), "More actions");
}

/// The entries of a menu button's popover, in order
std::vector<GtkWidget*> moreEntries(GtkWidget* more) {
    std::vector<GtkWidget*> entries;
    std::function<void(GtkWidget*)> walk = [&](GtkWidget* widget) {
        if (GTK_IS_BUTTON(widget)) {
            entries.push_back(widget);
        } else if (GTK_IS_CONTAINER(widget)) {
            GList* children = gtk_container_get_children(GTK_CONTAINER(widget));
            for (GList* child = children; child; child = child->next) {
                walk(GTK_WIDGET(child->data));
            }
            g_list_free(children);
        }
    };
    if (auto* popover = gtk_menu_button_get_popover(GTK_MENU_BUTTON(more))) {
        walk(GTK_WIDGET(popover));
    }
    return entries;
}

/// The text of every label inside `widget` that is showing, except the state dot
std::vector<std::string> shownWords(GtkWidget* widget) {
    std::vector<std::string> words;
    std::function<void(GtkWidget*)> walk = [&](GtkWidget* w) {
        if (GTK_IS_LABEL(w) && gtk_widget_get_visible(w) && *gtk_label_get_text(GTK_LABEL(w)) &&
            g_strcmp0(gtk_label_get_text(GTK_LABEL(w)), "\xE2\x97\x8F") != 0) {
            words.emplace_back(gtk_label_get_text(GTK_LABEL(w)));
        }
        if (GTK_IS_CONTAINER(w)) {
            GList* children = gtk_container_get_children(GTK_CONTAINER(w));
            for (GList* child = children; child; child = child->next) {
                walk(GTK_WIDGET(child->data));
            }
            g_list_free(children);
        }
    };
    walk(widget);
    return words;
}

GtkWidget* studentsPill(GtkWidget* appBar) {
    auto* prefix = labelWithText(appBar, "Students:");
    return prefix ? gtk_widget_get_ancestor(prefix, GTK_TYPE_MENU_BUTTON) : nullptr;
}

/// Every classroom control must be on the bar or reachable through "More", whatever the width is
void expectNothingLost(MainWindow* window, const std::string& context) {
    SCOPED_TRACE(context);
    auto* appBar = window->get("tbTop1");
    for (const char* label: {"Freeze", "Blank", "Clear"}) {
        auto* button = buttonNamed(appBar, label);
        ASSERT_NE(button, nullptr) << label;
        EXPECT_TRUE(gtk_widget_get_visible(button)) << label << " is a classroom control and must stay on the bar";
    }
    auto* pill = studentsPill(appBar);
    ASSERT_NE(pill, nullptr);
    EXPECT_TRUE(gtk_widget_get_visible(pill)) << "the Students pill must stay on the bar";
    // The menu lists the moved buttons in the order they have on the bar
    const auto moved = [&] {
        std::vector<GtkToolItem*> hidden;
        for (GtkToolItem* item: movableAppBarItems(appBar)) {
            if (!gtk_widget_get_visible(GTK_WIDGET(item))) {
                hidden.push_back(item);
            }
        }
        std::sort(hidden.begin(), hidden.end(), [&](GtkToolItem* a, GtkToolItem* b) {
            return gtk_toolbar_get_item_index(GTK_TOOLBAR(appBar), a) < gtk_toolbar_get_item_index(GTK_TOOLBAR(appBar), b);
        });
        return hidden;
    }();
    auto* more = moreButtonOf(window);
    if (moved.empty()) {
        EXPECT_TRUE(more == nullptr || !gtk_widget_is_visible(more)) << "no \"More\" button when nothing moved";
        return;
    }
    ASSERT_NE(more, nullptr) << "buttons moved but there is no \"More\" menu";
    EXPECT_TRUE(gtk_widget_is_visible(more));
    const auto entries = moreEntries(more);
    ASSERT_EQ(entries.size(), moved.size()) << "every moved button has exactly one entry";
    for (size_t i = 0; i < moved.size(); ++i) {
        auto* source = actionableIn(GTK_WIDGET(moved[i]));
        ASSERT_NE(source, nullptr);
        EXPECT_STREQ(gtk_actionable_get_action_name(GTK_ACTIONABLE(entries[i])),
                     gtk_actionable_get_action_name(GTK_ACTIONABLE(source)))
                << "the entry runs the same action as the button it stands in for";
    }
}
}  // namespace

namespace {
/// A document with nothing pending: saved, no export, no recovery file, nothing needing attention
void resetDocumentStatus(Control* control, bool dirty = false, bool neverSaved = false) {
    control->updateDocumentStatus([&](utn::DocumentStatusModel& status) {
        status.saveSucceeded();
        status.dismissExport();
        status.clearRecovery();
        status.clearAttention();
        status.setDirty(dirty, neverSaved);
    });
}

/// PdfExportJob with the path setter made reachable, so the real job can be run without the file chooser
struct TestPdfExport: PdfExportJob {
    explicit TestPdfExport(Control* control): PdfExportJob(control) {}
    using PdfExportJob::testAndSetFilepath;
};

utn::StatusSummary documentSummary(Control* control) { return utn::summarise(control->getDocumentStatus().get()); }
}  // namespace

TEST_F(ClassroomWorkflow, aFailedSaveKeepsTheDocumentUnsavedAndSaysSoUntilASaveSucceeds) {
    window->toolbarSelected("UTN Teacher");
    window->show(nullptr);
    settle();
    window->updateCompactLayout(1450);
    auto* status = widgetNamed(window->getWindow(), "utnDocumentSaveStatus");
    ASSERT_TRUE(GTK_IS_LABEL(status));

    const fs::path folder = Util::getTmpDirSubfolder() / "status-save";
    fs::remove_all(folder);
    fs::create_directories(folder);
    Document* doc = control->getDocument();
    doc->lock();
    doc->setFilepath(folder / "lesson.xopp");
    doc->unlock();
    resetDocumentStatus(control.get());

    // An edit the undo history knows about
    auto page = doc->getPage(0);
    auto* layer = page->getSelectedLayer();
    addMarker(layer, 100);
    control->getUndoRedoHandler()->addUndoAction(
            std::make_unique<InsertUndoAction>(page, layer, layer->getElements().back().get()));
    settle();
    EXPECT_EQ(documentSummary(control.get()).state, utn::StatusState::Modified);
    EXPECT_STREQ(gtk_label_get_text(GTK_LABEL(status)), "● Unsaved changes");

    // The save fails at its last step. The way Control starts a save, then the real job
    SaveHandler::setReplaceStepForTesting(+[](const fs::path&, const fs::path&, std::string& error) {
        error = "disk full";
        return false;
    });
    control->updateDocumentStatus([](utn::DocumentStatusModel& model) { model.beginSave(); });
    EXPECT_STREQ(gtk_label_get_text(GTK_LABEL(status)), "↻ Saving…");
    auto* failing = new SaveJob(control.get());
    failing->run();
    settle();
    failing->unref();
    control->unblock();  // a job run directly never goes through BlockingJob::execute, which is what unblocks
    SaveHandler::setReplaceStepForTesting(nullptr);
    closeMessageDialogs();
    EXPECT_STREQ(gtk_label_get_text(GTK_LABEL(status)), "⚠ Save failed");
    EXPECT_TRUE(control->getUndoRedoHandler()->isChanged()) << "the edits are still unsaved";
    EXPECT_TRUE(control->getDocumentStatus().get().dirty);
    EXPECT_NE(std::string(gtk_widget_get_tooltip_text(status)).find("disk full"), std::string::npos)
            << "the teacher is told why";
    EXPECT_TRUE(documentSummary(control.get()).offersSave);

    // Opening and closing nothing changes it; only a save that works does
    control->updateDocumentStatus([](utn::DocumentStatusModel& model) { model.beginSave(); });
    auto* working = new SaveJob(control.get());
    working->run();
    settle();
    working->unref();
    control->unblock();
    EXPECT_STREQ(gtk_label_get_text(GTK_LABEL(status)), "✓");
    EXPECT_FALSE(control->getUndoRedoHandler()->isChanged());
    EXPECT_TRUE(fs::exists(folder / "lesson.xopp")) << "the file really exists";
    fs::remove_all(folder);
}

TEST_F(ClassroomWorkflow, aPdfExportIsReportedSeparatelyAndNeverChangesTheSaveState) {
    window->toolbarSelected("UTN Teacher");
    window->show(nullptr);
    settle();
    window->updateCompactLayout(1450);
    const fs::path folder = Util::getTmpDirSubfolder() / "status-export";
    fs::remove_all(folder);
    fs::create_directories(folder);
    resetDocumentStatus(control.get(), true);  // unsaved edits waiting

    auto* job = new TestPdfExport(control.get());
    ASSERT_TRUE(job->testAndSetFilepath(folder / "worksheet.pdf"));
    control->updateDocumentStatus([](utn::DocumentStatusModel& model) { model.beginExport(); });
    job->run();
    settle();
    job->unref();
    control->unblock();
    closeMessageDialogs();

    const auto& facts = control->getDocumentStatus().get();
    EXPECT_EQ(facts.exportStatus, utn::ExportStatus::Completed);
    EXPECT_EQ(facts.exportDetail, "worksheet.pdf");
    EXPECT_TRUE(facts.dirty) << "exporting did not save the document";
    EXPECT_EQ(documentSummary(control.get()).state, utn::StatusState::Modified)
            << "so the header still says there are unsaved changes";
    EXPECT_TRUE(fs::exists(folder / "worksheet.pdf")) << "and the PDF really exists";
    const auto& details = documentSummary(control.get()).details;
    EXPECT_TRUE(std::any_of(details.begin(), details.end(),
                            [](const std::string& line) { return line.find("worksheet.pdf") != std::string::npos; }));

    resetDocumentStatus(control.get());
    EXPECT_EQ(documentSummary(control.get()).state, utn::StatusState::Saved);
    control->updateDocumentStatus([](utn::DocumentStatusModel& model) {
        model.exportSucceeded("worksheet.pdf");
    });
    EXPECT_EQ(documentSummary(control.get()).state, utn::StatusState::Exported);
    fs::remove_all(folder);
}

TEST_F(ClassroomWorkflow, headerShowsEveryStateAsAShapeAWordAndASeverityClassWithTheDetailsOnHover) {
    window->toolbarSelected("UTN Teacher");
    window->show(nullptr);
    settle();
    window->updateCompactLayout(1450);
    auto* status = widgetNamed(window->getWindow(), "utnDocumentSaveStatus");
    ASSERT_TRUE(GTK_IS_LABEL(status));
    const auto hasClass = [&](const char* name) {
        return gtk_style_context_has_class(gtk_widget_get_style_context(status), name);
    };

    resetDocumentStatus(control.get());
    EXPECT_TRUE(hasClass("utn-status-ok"));

    control->updateDocumentStatus([](utn::DocumentStatusModel& model) { model.setRecovery("lesson.autosave.xopp"); });
    EXPECT_STREQ(gtk_label_get_text(GTK_LABEL(status)), "↺ Recovery available");
    EXPECT_TRUE(hasClass("utn-status-warning"));
    EXPECT_FALSE(hasClass("utn-status-ok")) << "one severity at a time";
    EXPECT_NE(std::string(gtk_widget_get_tooltip_text(status)).find("lesson.autosave.xopp"), std::string::npos);
    EXPECT_STREQ(atk_object_get_name(gtk_widget_get_accessible(status)), "Document status: a recovery file is available");

    resetDocumentStatus(control.get());
    control->updateDocumentStatus([](utn::DocumentStatusModel& model) {
        model.addAttention("The PDF this worksheet was made from is missing");
    });
    EXPECT_STREQ(gtk_label_get_text(GTK_LABEL(status)), "! Needs attention");
    EXPECT_NE(std::string(gtk_widget_get_tooltip_text(status)).find("missing"), std::string::npos);

    resetDocumentStatus(control.get());
    control->updateDocumentStatus([](utn::DocumentStatusModel& model) { model.exportFailed("the folder is read-only"); });
    EXPECT_STREQ(gtk_label_get_text(GTK_LABEL(status)), "⚠ Export failed");
    EXPECT_TRUE(hasClass("utn-status-error"));
    resetDocumentStatus(control.get());
}

namespace {
std::vector<GtkWidget*> buttonsIn(GtkWidget* widget) {
    std::vector<GtkWidget*> buttons;
    std::function<void(GtkWidget*)> walk = [&](GtkWidget* w) {
        if (GTK_IS_BUTTON(w)) {
            buttons.push_back(w);
        }
        if (GTK_IS_CONTAINER(w)) {
            GList* children = gtk_container_get_children(GTK_CONTAINER(w));
            for (GList* child = children; child; child = child->next) {
                walk(GTK_WIDGET(child->data));
            }
            g_list_free(children);
        }
    };
    walk(widget);
    return buttons;
}

uint32_t toolRgb(Control* control) { return static_cast<uint32_t>(control->getToolHandler()->getColor()) & 0xFFFFFFu; }
}  // namespace

TEST_F(ClassroomWorkflow, applyingAPresetSetsTheRealToolAndOnlyThenIsItSelected) {
    auto* tools = control->getToolHandler();
    control->updatePenPresets([](utn::PenPresetStore& store) { store.restoreDefaults(); });

    // A pen in a state that matches no preset
    control->selectTool(TOOL_PEN);
    tools->setColor(Color(0xFF123456U), false);
    tools->setPenSize(TOOL_SIZE_VERY_FINE);
    EXPECT_EQ(control->activePenPresetId(), "");
    for (const auto& entry: utn::presetMenuEntries(control.get())) {
        EXPECT_FALSE(entry.active) << entry.name << " must not look selected before it is applied";
    }

    ASSERT_TRUE(control->applyPenPreset(utn::PenPresetStore::RED_MARKING_ID));
    EXPECT_EQ(tools->getToolType(), TOOL_PEN);
    EXPECT_EQ(toolRgb(control.get()), 0xD93025u);
    EXPECT_EQ(tools->getColor().alpha, 255);
    EXPECT_EQ(tools->getPenSize(), TOOL_SIZE_MEDIUM);
    EXPECT_EQ(control->activePenPresetId(), utn::PenPresetStore::RED_MARKING_ID);
    for (const auto& entry: utn::presetMenuEntries(control.get())) {
        EXPECT_EQ(entry.active, entry.id == utn::PenPresetStore::RED_MARKING_ID) << entry.name;
    }

    // A highlighter preset switches the tool and carries its opacity
    ASSERT_TRUE(control->applyPenPreset(utn::PenPresetStore::YELLOW_HIGHLIGHTER_ID));
    EXPECT_EQ(tools->getToolType(), TOOL_HIGHLIGHTER);
    EXPECT_EQ(toolRgb(control.get()), 0xFFE94Au);
    EXPECT_EQ(tools->getColor().alpha, 128);
    EXPECT_EQ(tools->getHighlighterSize(), TOOL_SIZE_THICK);
    EXPECT_EQ(control->activePenPresetId(), utn::PenPresetStore::YELLOW_HIGHLIGHTER_ID);

    // Changing the tool by hand drops the mark: a preset never stays selected after it stopped being true
    tools->setHighlighterSize(TOOL_SIZE_FINE);
    EXPECT_EQ(control->activePenPresetId(), "");
    ASSERT_TRUE(control->applyPenPreset(utn::PenPresetStore::BLUE_EXPLANATION_ID));
    tools->setColor(Color(0xFF00AA00U), false);
    EXPECT_EQ(control->activePenPresetId(), "") << "another colour";

    EXPECT_FALSE(control->applyPenPreset("no-such-preset")) << "nothing is applied for an unknown preset";
    control->selectTool(TOOL_ERASER);
    EXPECT_EQ(control->activePenPresetId(), "") << "no preset is active while another tool is";
    EXPECT_FALSE(control->currentPenPresetValues().has_value());
}

TEST_F(ClassroomWorkflow, savingAPresetKeepsTheCurrentToolValuesAndSurvivesInTheSettings) {
    auto* tools = control->getToolHandler();
    control->updatePenPresets([](utn::PenPresetStore& store) { store.restoreDefaults(); });
    control->selectTool(TOOL_PEN);
    tools->setColor(Color(0xFF1B7F5CU), false);
    tools->setPenSize(TOOL_SIZE_THICK);

    auto values = control->currentPenPresetValues();
    ASSERT_TRUE(values.has_value());
    values->name = "Green ticks";
    std::string id;
    control->updatePenPresets([&](utn::PenPresetStore& store) { id = store.add(*values); });
    ASSERT_FALSE(id.empty());
    EXPECT_EQ(control->activePenPresetId(), id) << "the tool is in exactly the state it was saved from";

    // What a restart would read back is the saved string in the settings
    std::string saved;
    ASSERT_TRUE(control->getSettings()->getCustomElement("utn").getString("penPresets", saved));
    const utn::PenPresetStore reloaded = utn::PenPresetStore::parse(saved);
    ASSERT_NE(reloaded.find(id), nullptr);
    EXPECT_EQ(reloaded.find(id)->name, "Green ticks");
    EXPECT_EQ(reloaded.find(id)->rgb, 0x1B7F5Cu);
    EXPECT_EQ(reloaded.find(id)->size, static_cast<int>(TOOL_SIZE_THICK));
    EXPECT_EQ(reloaded.all().size(), control->getPenPresets().all().size());

    // Applying it later brings the same setup back
    tools->setColor(Color(0xFFFF0000U), false);
    ASSERT_TRUE(control->applyPenPreset(id));
    EXPECT_EQ(toolRgb(control.get()), 0x1B7F5Cu);
    EXPECT_EQ(tools->getPenSize(), TOOL_SIZE_THICK);

    control->updatePenPresets([&](utn::PenPresetStore& store) { store.remove(id); });
    control->updatePenPresets([](utn::PenPresetStore& store) { store.restoreDefaults(); });
}

TEST_F(ClassroomWorkflow, thePresetsMenuIsInThePenAndHighlighterSettingsAndAppliesWhenAnEntryIsActivated) {
    window->toolbarSelected("UTN Teacher");
    window->show(nullptr);
    // The settings panel folds away in a narrow window; this test needs it, so it does not depend on an earlier test
    allocatedWidthAfterResize(window->getWindow(), 1280);
    ASSERT_GE(gtk_widget_get_allocated_width(window->getWindow()), utn::layout::INSPECTOR_RETURN_ABOVE);
    settle();
    control->updatePenPresets([](utn::PenPresetStore& store) { store.restoreDefaults(); });
    control->selectTool(TOOL_PEN);
    settle();

    for (const ToolType tool: {TOOL_PEN, TOOL_HIGHLIGHTER}) {
        control->selectTool(tool);
        settle();
        auto* menu = menuButtonAccessibleNamed(window->getWindow(), "Presets");
        ASSERT_NE(menu, nullptr) << "no Presets menu for tool " << tool;
        EXPECT_TRUE(gtk_widget_get_can_focus(menu)) << "keyboard users reach it with Tab";
    }
    control->selectTool(TOOL_ERASER);
    settle();
    EXPECT_EQ(menuButtonAccessibleNamed(window->getWindow(), "Presets"), nullptr) << "presets are for pens only";

    control->selectTool(TOOL_PEN);
    settle();
    auto* menu = menuButtonAccessibleNamed(window->getWindow(), "Presets");
    ASSERT_NE(menu, nullptr);
    auto* popover = gtk_menu_button_get_popover(GTK_MENU_BUTTON(menu));
    ASSERT_NE(popover, nullptr);
    // The entries are named buttons a keyboard can reach, one per preset, then save and manage
    const auto buttons = buttonsIn(GTK_WIDGET(popover));
    ASSERT_EQ(buttons.size(), control->getPenPresets().all().size() + 2);
    for (GtkWidget* button: buttons) {
        EXPECT_TRUE(gtk_widget_get_can_focus(button));
        EXPECT_NE(std::string(atk_object_get_name(gtk_widget_get_accessible(button))), "");
    }
    // Activating the Yellow highlighter entry applies it for real
    GtkWidget* yellow = nullptr;
    for (GtkWidget* button: buttons) {
        const char* name = atk_object_get_name(gtk_widget_get_accessible(button));
        if (name && std::string(name).find("Yellow highlighter") != std::string::npos) {
            yellow = button;
        }
    }
    ASSERT_NE(yellow, nullptr);
    gtk_button_clicked(GTK_BUTTON(yellow));
    settle();
    EXPECT_EQ(control->getToolHandler()->getToolType(), TOOL_HIGHLIGHTER);
    EXPECT_EQ(control->activePenPresetId(), utn::PenPresetStore::YELLOW_HIGHLIGHTER_ID);
}

namespace {
bool contextHintShown(GtkWidget* widget) {
    if (GTK_IS_LABEL(widget) && gtk_widget_get_visible(widget) &&
        gtk_style_context_has_class(gtk_widget_get_style_context(widget), "utn-context-hint")) {
        return true;
    }
    bool shown = false;
    if (GTK_IS_CONTAINER(widget)) {
        GList* children = gtk_container_get_children(GTK_CONTAINER(widget));
        for (GList* child = children; child && !shown; child = child->next) {
            shown = contextHintShown(GTK_WIDGET(child->data));
        }
        g_list_free(children);
    }
    return shown;
}
}  // namespace

TEST_F(ClassroomWorkflow, interfaceModesChangeOnlyTheGuidanceAndNeverTheToolsOrTheTeachersOwnChoices) {
    using utn::layout::InterfaceMode;
    window->toolbarSelected("UTN Teacher");
    window->show(nullptr);
    allocatedWidthAfterResize(window->getWindow(), 1280);
    settle();
    auto* rail = window->get("tbLeft1");
    control->selectTool(TOOL_HAND);  // a tool whose settings carry an explanation
    settle();

    window->setInterfaceMode(InterfaceMode::Beginner);
    settle();
    EXPECT_EQ(window->getInterfaceMode(), InterfaceMode::Beginner);
    EXPECT_EQ(window->getToolLabelMode(), utn::layout::LABELS_SHOW);
    expectCaptions(rail, true, "beginner");
    EXPECT_TRUE(contextHintShown(window->getWindow())) << "Beginner explains the tool";
    const size_t beginnerTools = buttonsIn(rail).size();

    window->setInterfaceMode(InterfaceMode::Expert);
    settle();
    EXPECT_EQ(window->getInterfaceMode(), InterfaceMode::Expert);
    EXPECT_EQ(window->getToolLabelMode(), utn::layout::LABELS_HIDE);
    expectCaptions(rail, false, "expert");
    EXPECT_FALSE(contextHintShown(window->getWindow())) << "Expert drops the explanation";
    EXPECT_EQ(buttonsIn(rail).size(), beginnerTools) << "the same tools are on the rail in both modes";
    EXPECT_EQ(control->getToolHandler()->getToolType(), TOOL_HAND) << "choosing a mode never changes the active tool";

    // Teach, Mark and Present never touch the mode or the labels
    for (int workspace: {1, 2, 0}) {
        window->setTeacherWorkspace(workspace);
        settle();
        EXPECT_EQ(window->getInterfaceMode(), InterfaceMode::Expert) << "workspace " << workspace;
        EXPECT_EQ(window->getToolLabelMode(), utn::layout::LABELS_HIDE) << "workspace " << workspace;
    }

    // What a restart would read
    std::string saved;
    ASSERT_TRUE(control->getSettings()->getCustomElement("utn").getString("interfaceMode", saved));
    EXPECT_EQ(saved, "expert");

    // A label choice made by hand afterwards is kept; the mode steps aside instead of overriding it
    window->setToolLabelMode(utn::layout::LABELS_SHOW);
    EXPECT_EQ(window->getToolLabelMode(), utn::layout::LABELS_SHOW) << "the teacher's choice stands";
    EXPECT_EQ(window->getInterfaceMode(), InterfaceMode::Custom);
    ASSERT_TRUE(control->getSettings()->getCustomElement("utn").getString("interfaceMode", saved));
    EXPECT_EQ(saved, "custom");

    // And a choice that agrees with a mode keeps that mode
    window->setInterfaceMode(InterfaceMode::Expert);
    window->setToolLabelMode(utn::layout::LABELS_HIDE);
    EXPECT_EQ(window->getInterfaceMode(), InterfaceMode::Expert);

    window->setInterfaceMode(InterfaceMode::Custom);
    window->setToolLabelMode(2);  // the shipped default, for the tests after this one
    control->selectTool(TOOL_PEN);
}

namespace {
/// The rail as the teacher sees it, top to bottom: each tool's name from its tooltip, and "|" for a separator
std::vector<std::string> railToolNames(MainWindow* window) {
    std::vector<std::string> names;
    auto* rail = GTK_TOOLBAR(window->get("tbLeft1"));
    for (int i = 0; i < gtk_toolbar_get_n_items(rail); ++i) {
        GtkWidget* item = GTK_WIDGET(gtk_toolbar_get_nth_item(rail, i));
        if (GTK_IS_SEPARATOR_TOOL_ITEM(item)) {
            names.emplace_back("|");
            continue;
        }
        const char* markup = gtk_widget_get_tooltip_markup(item);
        std::string text = markup ? markup : (gtk_widget_get_tooltip_text(item) ? gtk_widget_get_tooltip_text(item) : "");
        const size_t open = text.find("<b>");
        const size_t close = text.find("</b>");
        names.push_back(open != std::string::npos && close != std::string::npos ? text.substr(open + 3, close - open - 3) :
                                                                                  text.substr(0, text.find('\n')));
    }
    return names;
}

bool hasName(const std::vector<std::string>& names, const std::string& name) {
    return std::find(names.begin(), names.end(), name) != names.end();
}
}  // namespace

TEST_F(ClassroomWorkflow, theDockArrangementIsAppliedToTheRealRailAndResetBringsTheShippedRailBack) {
    window->toolbarSelected("UTN Teacher");
    window->show(nullptr);
    settle();
    control->updateDockLayout([](utn::DockLayout& layout) { layout.reset(); });
    settle();
    const auto shipped = railToolNames(window.get());
    ASSERT_GT(shipped.size(), 8u);

    const auto defaults = window->getToolMenuHandler()->railItemIds(window->getSelectedToolbar());
    const std::string move = window->getToolMenuHandler()->toolDisplayName("HAND");
    const std::string text = window->getToolMenuHandler()->toolDisplayName("UTN_TEXT");
    ASSERT_TRUE(hasName(shipped, move)) << move;
    ASSERT_TRUE(hasName(shipped, text)) << text;

    // Pin: the tool moves to a favourites group of its own at the top, and is not also left in its old place
    control->updateDockLayout([](utn::DockLayout& layout) { layout.pin("HAND"); });
    settle();
    auto names = railToolNames(window.get());
    EXPECT_EQ(names.front(), move);
    EXPECT_EQ(names[1], "|");
    EXPECT_EQ(std::count(names.begin(), names.end(), move), 1);
    EXPECT_EQ(names.size(), shipped.size() + 1) << "one more separator, no tool lost";

    // Hide: the tool leaves the rail; an essential tool cannot
    control->updateDockLayout([](utn::DockLayout& layout) { layout.hide("UTN_TEXT"); });
    settle();
    names = railToolNames(window.get());
    EXPECT_FALSE(hasName(names, text)) << "hidden from the dock";
    const std::string pen = window->getToolMenuHandler()->toolDisplayName("PEN");
    control->updateDockLayout([](utn::DockLayout& layout) { EXPECT_FALSE(layout.hide("PEN")); });
    settle();
    EXPECT_TRUE(hasName(railToolNames(window.get()), pen)) << "the pen is always on the dock";

    // Reorder inside the group
    const size_t penBefore = static_cast<size_t>(std::find(names.begin(), names.end(), pen) - names.begin());
    control->updateDockLayout([&](utn::DockLayout& layout) { EXPECT_TRUE(layout.moveDown("PEN", defaults)); });
    settle();
    names = railToolNames(window.get());
    EXPECT_GT(static_cast<size_t>(std::find(names.begin(), names.end(), pen) - names.begin()), penBefore);

    // What a restart reads back gives the very same rail
    std::string saved;
    ASSERT_TRUE(control->getSettings()->getCustomElement("utn").getString("dockLayout", saved));
    EXPECT_EQ(utn::DockLayout::parse(saved).arrange(defaults), control->getDockLayout().arrange(defaults));

    control->updateDockLayout([](utn::DockLayout& layout) { layout.reset(); });
    settle();
    EXPECT_EQ(railToolNames(window.get()), shipped) << "reset gives the shipped rail back, separator and all";
}

TEST_F(ClassroomWorkflow, theCustomiseDockDialogWorksByActivatingNamedButtonsNotOnlyByDragging) {
    window->toolbarSelected("UTN Teacher");
    window->show(nullptr);
    settle();
    control->updateDockLayout([](utn::DockLayout& layout) { layout.reset(); });
    settle();
    const auto shipped = railToolNames(window.get());
    const std::string move = window->getToolMenuHandler()->toolDisplayName("HAND");

    const auto rows = utn::dockRows(control.get());
    ASSERT_FALSE(rows.empty());
    EXPECT_FALSE(rows.front().canMoveUp) << "the first tool of a group cannot go further up";
    for (const auto& row: rows) {
        EXPECT_EQ(row.essential, utn::DockLayout::isEssential(row.id));
        EXPECT_FALSE(row.name.empty());
    }

    utn::showDockCustomizeDialog(control.get());
    settle();
    GtkWidget* dialog = nullptr;
    GList* windows = gtk_window_list_toplevels();
    for (GList* w = windows; w; w = w->next) {
        if (GTK_IS_DIALOG(w->data) && gtk_widget_get_visible(GTK_WIDGET(w->data)) &&
            std::string(gtk_window_get_title(GTK_WINDOW(w->data))) == "Customise tool dock") {
            dialog = GTK_WIDGET(w->data);
        }
    }
    g_list_free(windows);
    ASSERT_NE(dialog, nullptr);

    const auto namedButton = [&](const std::string& name) -> GtkWidget* {
        for (GtkWidget* button: buttonsIn(dialog)) {
            const char* accessible = atk_object_get_name(gtk_widget_get_accessible(button));
            if (accessible && name == accessible) {
                return button;
            }
        }
        return nullptr;
    };
    for (GtkWidget* button: buttonsIn(dialog)) {
        EXPECT_TRUE(gtk_widget_get_can_focus(button)) << "a keyboard reaches every button";
        EXPECT_NE(std::string(atk_object_get_name(gtk_widget_get_accessible(button))), "") << "and every button has a name";
    }

    // Activating "Pin to the top" for a tool does what dragging would, and the rail follows
    GtkWidget* pin = namedButton("Pin to the top: " + move);
    ASSERT_NE(pin, nullptr);
    gtk_button_clicked(GTK_BUTTON(pin));
    settle();
    EXPECT_EQ(railToolNames(window.get()).front(), move);
    EXPECT_TRUE(control->getDockLayout().isPinned("HAND"));
    EXPECT_NE(namedButton("Unpin: " + move), nullptr) << "the row now offers the way back";
    // The list is rebuilt after every change, and the keyboard focus stays on this tool's row, not on the dialog
    GtkWidget* focused = gtk_window_get_focus(GTK_WINDOW(dialog));
    ASSERT_NE(focused, nullptr) << "focus was lost when the list was rebuilt";
    EXPECT_STREQ(atk_object_get_name(gtk_widget_get_accessible(focused)), ("Unpin: " + move).c_str());

    // An essential tool's Hide button is off, and says why
    const std::string pen = window->getToolMenuHandler()->toolDisplayName("PEN");
    GtkWidget* hidePen = namedButton("Hide: " + pen + ", not available, always on the dock");
    ASSERT_NE(hidePen, nullptr) << "the name starts with the visible word Hide and says why it is off";
    EXPECT_FALSE(gtk_widget_get_sensitive(hidePen));

    // Reset restores the shipped rail
    gtk_dialog_response(GTK_DIALOG(dialog), GTK_RESPONSE_REJECT);
    settle();
    EXPECT_EQ(railToolNames(window.get()), shipped);
    EXPECT_TRUE(control->getDockLayout().isDefault());
    gtk_dialog_response(GTK_DIALOG(dialog), GTK_RESPONSE_CLOSE);
    settle();
}

namespace {
GtkWidget* visibleDialogTitled(const char* title) {
    GtkWidget* found = nullptr;
    GList* windows = gtk_window_list_toplevels();
    for (GList* w = windows; w; w = w->next) {
        if (GTK_IS_DIALOG(w->data) && gtk_widget_get_visible(GTK_WIDGET(w->data)) &&
            std::string(gtk_window_get_title(GTK_WINDOW(w->data))) == title) {
            found = GTK_WIDGET(w->data);
        }
    }
    g_list_free(windows);
    return found;
}

GtkWidget* buttonNamedIn(GtkWidget* root, const std::string& name) {
    for (GtkWidget* button: buttonsIn(root)) {
        const char* accessible = atk_object_get_name(gtk_widget_get_accessible(button));
        if (accessible && name == accessible) {
            return button;
        }
    }
    return nullptr;
}

GtkWidget* entryNamedIn(GtkWidget* widget, const std::string& name) {
    if (GTK_IS_ENTRY(widget)) {
        const char* accessible = atk_object_get_name(gtk_widget_get_accessible(widget));
        if (accessible && name == accessible) {
            return widget;
        }
    }
    GtkWidget* found = nullptr;
    if (GTK_IS_CONTAINER(widget)) {
        GList* children = gtk_container_get_children(GTK_CONTAINER(widget));
        for (GList* child = children; child && !found; child = child->next) {
            found = entryNamedIn(GTK_WIDGET(child->data), name);
        }
        g_list_free(children);
    }
    return found;
}
}  // namespace

TEST_F(ClassroomWorkflow, managingPresetsRenamesWhenTheFieldIsLeftAndDeleteKeepsTheKeyboardWhereItWas) {
    window->toolbarSelected("UTN Teacher");
    window->show(nullptr);
    allocatedWidthAfterResize(window->getWindow(), 1280);
    settle();
    control->updatePenPresets([](utn::PenPresetStore& store) { store.restoreDefaults(); });
    control->selectTool(TOOL_PEN);
    settle();
    auto* menu = menuButtonAccessibleNamed(window->getWindow(), "Presets");
    ASSERT_NE(menu, nullptr);
    GtkWidget* manage = buttonNamedIn(GTK_WIDGET(gtk_menu_button_get_popover(GTK_MENU_BUTTON(menu))), "Manage presets…");
    ASSERT_NE(manage, nullptr);
    gtk_button_clicked(GTK_BUTTON(manage));
    settle();
    GtkWidget* dialog = visibleDialogTitled("Manage presets");
    ASSERT_NE(dialog, nullptr);

    // Each rename field has its own name, so a screen reader can tell them apart
    GtkWidget* red = entryNamedIn(dialog, "Rename preset: Red marking pen");
    GtkWidget* blue = entryNamedIn(dialog, "Rename preset: Blue explanation pen");
    ASSERT_NE(red, nullptr);
    ASSERT_NE(blue, nullptr);

    // Typing a name and leaving the field keeps it, without needing Enter
    gtk_entry_set_text(GTK_ENTRY(red), "Red marker");
    GdkEvent* leave = gdk_event_new(GDK_FOCUS_CHANGE);
    leave->focus_change.in = FALSE;
    gboolean handled = FALSE;
    g_signal_emit_by_name(red, "focus-out-event", leave, &handled);
    gdk_event_free(leave);
    ASSERT_NE(control->getPenPresets().find(utn::PenPresetStore::RED_MARKING_ID), nullptr);
    EXPECT_EQ(control->getPenPresets().find(utn::PenPresetStore::RED_MARKING_ID)->name, "Red marker");
    EXPECT_NE(entryNamedIn(dialog, "Rename preset: Red marker"), nullptr) << "and its accessible name follows";

    // Delete: the row goes, the dialog stays, and the keyboard lands on a Delete button, not nowhere
    const size_t before = control->getPenPresets().all().size();
    GtkWidget* remove = buttonNamedIn(dialog, "Delete preset: Blue explanation pen");
    ASSERT_NE(remove, nullptr);
    gtk_button_clicked(GTK_BUTTON(remove));
    settle();
    EXPECT_EQ(control->getPenPresets().all().size(), before - 1);
    GtkWidget* focused = gtk_window_get_focus(GTK_WINDOW(dialog));
    ASSERT_NE(focused, nullptr) << "focus was lost when the list was rebuilt";
    EXPECT_EQ(std::string(atk_object_get_name(gtk_widget_get_accessible(focused))).rfind("Delete preset: ", 0), 0u);

    gtk_dialog_response(GTK_DIALOG(dialog), GTK_RESPONSE_REJECT);  // restore the defaults, for the tests after this one
    settle();
    EXPECT_NE(control->getPenPresets().find(utn::PenPresetStore::BLUE_EXPLANATION_ID), nullptr);
    EXPECT_EQ(control->getPenPresets().find(utn::PenPresetStore::RED_MARKING_ID)->name, "Red marking pen");
    gtk_dialog_response(GTK_DIALOG(dialog), GTK_RESPONSE_CLOSE);
    settle();
}

TEST_F(ClassroomWorkflow, theDocumentStatusIsAButtonWhoseDetailsAndSaveActionAreNotTooltipOnly) {
    window->toolbarSelected("UTN Teacher");
    window->show(nullptr);
    allocatedWidthAfterResize(window->getWindow(), 1280);
    settle();
    resetDocumentStatus(control.get(), true);
    control->updateDocumentStatus([](utn::DocumentStatusModel& model) { model.saveFailed("disk full"); });
    settle();

    auto* chip = menuButtonAccessibleNamed(window->getWindow(), "Document status: saving failed, your changes are not saved");
    ASSERT_NE(chip, nullptr) << "the status is something a keyboard can reach and a screen reader can name";
    EXPECT_TRUE(gtk_widget_get_can_focus(chip));
    auto* popover = gtk_menu_button_get_popover(GTK_MENU_BUTTON(chip));
    ASSERT_NE(popover, nullptr);
    gtk_widget_show(GTK_WIDGET(popover));  // builds the details when it opens
    settle();
    std::string shown;
    for (const auto& word: shownWords(GTK_WIDGET(popover))) {
        shown += word + "\n";
    }
    EXPECT_NE(shown.find("disk full"), std::string::npos) << "the reason is in the popover, not only in a tooltip";
    GtkWidget* save = buttonNamedIn(GTK_WIDGET(popover), "Save now");
    ASSERT_NE(save, nullptr) << "and so is the way forward";
    EXPECT_STREQ(gtk_actionable_get_action_name(GTK_ACTIONABLE(save)), "win.save");
    gtk_widget_hide(GTK_WIDGET(popover));
    resetDocumentStatus(control.get());
}

// A popover whose content is built but never shown opens as an empty bubble: the widgets exist, so tests that only look
// them up pass, while the teacher sees nothing. Check that what a teacher would click is actually visible.
TEST_F(ClassroomWorkflow, theWorkspaceMenuOpensWithItsEntriesVisibleNotAsAnEmptyBubble) {
    window->toolbarSelected("UTN Teacher");
    window->show(nullptr);
    allocatedWidthAfterResize(window->getWindow(), 1280);
    settle();
    auto* menu = menuButtonAccessibleNamed(window->getWindow(), "Workspace layout");
    ASSERT_NE(menu, nullptr);
    auto* popover = gtk_menu_button_get_popover(GTK_MENU_BUTTON(menu));
    ASSERT_NE(popover, nullptr);
    GtkWidget* panel = gtk_bin_get_child(GTK_BIN(popover));
    ASSERT_NE(panel, nullptr);
    EXPECT_TRUE(gtk_widget_get_visible(panel)) << "the panel itself must be shown, or the popover is blank";
    const auto buttons = buttonsIn(panel);
    ASSERT_GE(buttons.size(), 8u) << "workspace modes, tool labels, Beginner, Expert and Customise tool dock";
    for (GtkWidget* button: buttons) {
        const char* name = atk_object_get_name(gtk_widget_get_accessible(button));
        EXPECT_TRUE(gtk_widget_get_visible(button)) << (name ? name : "unnamed button") << " would be invisible";
    }
    EXPECT_NE(buttonNamedIn(panel, "Customise tool dock"), nullptr);
    bool beginner = false;
    for (GtkWidget* button: buttons) {
        const char* name = atk_object_get_name(gtk_widget_get_accessible(button));
        beginner = beginner || (name && std::string(name).find("Beginner") != std::string::npos);
    }
    EXPECT_TRUE(beginner);
}

// The app bar is measured against its window, so it holds at 96 and 120 DPI alike. The widths are real allocations, as
// in the app: a width the window cannot reach is clamped, and the assertion reads what was really given.
TEST_F(ClassroomWorkflow, appBarFitsOrOffersMoreAtEveryRealWindowWidth) {
    window->toolbarSelected("UTN Teacher");
    gtk_widget_show(window->getWindow());
    auto* appBar = window->get("tbTop1");
    for (int requested: {1280, 1024, 900, 800, 1280}) {
        const int actual = allocatedWidthAfterResize(window->getWindow(), requested);
        const std::string context = "requested " + std::to_string(requested) + ", allocated " + std::to_string(actual);
        SCOPED_TRACE(context);
        int minWidth = 0;
        int natWidth = 0;
        gtk_widget_get_preferred_width(appBar, &minWidth, &natWidth);
        EXPECT_LE(natWidth, actual) << "the app bar must fit without scrolling or clipping";
        expectNothingLost(window.get(), context);
    }
}

// Forced with a width no display fits, so the overflow path is exercised whatever the scale is.
TEST_F(ClassroomWorkflow, buttonsThatCannotFitMoveIntoAMoreMenuThatRunsTheSameActions) {
    window->toolbarSelected("UTN Teacher");
    gtk_widget_show(window->getWindow());
    settle();
    const int realWidth = gtk_widget_get_allocated_width(window->getWindow());
    auto* appBar = window->get("tbTop1");
    const auto movable = movableAppBarItems(appBar);
    ASSERT_EQ(movable.size(), 5u) << "Export, Open, Save, Undo and Redo are the buttons that may move";

    window->updateCompactLayout(500);
    EXPECT_TRUE(window->isAppBarOverflowing());
    expectNothingLost(window.get(), "synthetic 500 px");
    auto* more = moreButtonOf(window.get());
    ASSERT_NE(more, nullptr);
    EXPECT_STREQ(gtk_widget_get_tooltip_text(more), "More actions");
    EXPECT_TRUE(gtk_widget_get_can_focus(more)) << "keyboard users reach the menu with Tab";
    for (GtkWidget* entry: moreEntries(more)) {
        EXPECT_TRUE(gtk_widget_get_can_focus(entry)) << "keyboard users reach every entry";
        EXPECT_NE(std::string(gtk_button_get_label(GTK_BUTTON(entry))), "") << "every entry is named";
    }

    // States follow the action: nothing to undo yet, so the Undo and Redo entries are off
    GtkWidget* undoEntry = nullptr;
    GtkWidget* redoEntry = nullptr;
    // The menu may be built again when an action changes: look the entries up again whenever they are used
    const auto findEntries = [&] {
        undoEntry = redoEntry = nullptr;
        window->updateCompactLayout(500);  // the window's real width may have laid the bar out again after the action changed
        if (auto* current = moreButtonOf(window.get())) {
            for (GtkWidget* entry: moreEntries(current)) {
                const std::string action = gtk_actionable_get_action_name(GTK_ACTIONABLE(entry));
                if (action == "win.undo") {
                    undoEntry = entry;
                } else if (action == "win.redo") {
                    redoEntry = entry;
                }
            }
        }
    };
    findEntries();
    ASSERT_NE(undoEntry, nullptr);
    ASSERT_NE(redoEntry, nullptr);
    EXPECT_FALSE(gtk_widget_get_sensitive(undoEntry));
    EXPECT_FALSE(gtk_widget_get_sensitive(redoEntry));

    // Something to undo: the Undo entry comes alive, and running it undoes
    auto page = control->getDocument()->getPage(0);
    auto* layer = page->getSelectedLayer();
    addMarker(layer, 100);
    const Element* ink = layer->getElements().back().get();
    control->getUndoRedoHandler()->addUndoAction(std::make_unique<InsertUndoAction>(page, layer, ink));
    settle();
    const auto before = layer->getElements().size();
    findEntries();
    ASSERT_NE(undoEntry, nullptr);
    EXPECT_TRUE(gtk_widget_get_sensitive(undoEntry));
    gtk_button_clicked(GTK_BUTTON(undoEntry));
    settle();
    EXPECT_EQ(layer->getElements().size(), before - 1) << "the \"More\" entry undid the stroke";
    findEntries();
    ASSERT_NE(redoEntry, nullptr);
    EXPECT_TRUE(gtk_widget_get_sensitive(redoEntry)) << "and Redo is now available from the menu too";
    gtk_button_clicked(GTK_BUTTON(redoEntry));
    settle();
    EXPECT_EQ(layer->getElements().size(), before);

    window->updateCompactLayout(realWidth);
}

TEST_F(ClassroomWorkflow, studentsPillShowsOneWordWhenTightAndKeepsItsWholeName) {
    window->toolbarSelected("UTN Teacher");
    gtk_widget_show(window->getWindow());
    settle();
    const int realWidth = gtk_widget_get_allocated_width(window->getWindow());
    auto* appBar = window->get("tbTop1");
    auto* pill = studentsPill(appBar);
    ASSERT_NE(pill, nullptr);

    std::set<std::string> shortWords;
    std::set<std::string> fullWords;
    using State = ToolStudentView::State;
    for (State state: {State::Off, State::SameScreen, State::Live, State::Frozen, State::Blank}) {
        shortWords.insert(ToolStudentView::stateShortWord(state));
        const std::string full = ToolStudentView::stateLabel(state);
        fullWords.insert(full.substr(full.find(": ") + 2));
    }

    window->updateCompactLayout(1920);
    EXPECT_FALSE(window->isAppBarTight());
    auto words = shownWords(pill);
    ASSERT_FALSE(words.empty());
    EXPECT_TRUE(fullWords.count(words.back())) << words.back() << " is the whole state word, not the short one";

    window->updateCompactLayout(500);
    EXPECT_TRUE(window->isAppBarTight());
    words = shownWords(pill);
    ASSERT_EQ(words.size(), 1u) << "one word, never both forms";
    EXPECT_TRUE(shortWords.count(words[0])) << words[0] << " is not a short state word";
    EXPECT_EQ(std::string(atk_object_get_name(gtk_widget_get_accessible(pill))).rfind("Students: ", 0), 0u)
            << "the accessible name keeps the whole phrase";
    EXPECT_EQ(std::string(gtk_widget_get_tooltip_text(pill)).rfind("Students: ", 0), 0u)
            << "and so does the tooltip";

    window->updateCompactLayout(realWidth);
}

TEST_F(ClassroomWorkflow, wideAppBarIsUntouchedAndResizingBackAndForthRestoresEverything) {
    window->toolbarSelected("UTN Teacher");
    gtk_widget_show(window->getWindow());
    settle();
    const int realWidth = gtk_widget_get_allocated_width(window->getWindow());
    auto* appBar = window->get("tbTop1");

    for (int round = 0; round < 2; ++round) {
        SCOPED_TRACE("round " + std::to_string(round));
        window->updateCompactLayout(1920);
        EXPECT_FALSE(window->isAppBarTight());
        EXPECT_FALSE(window->isAppBarOverflowing());
        for (GtkToolItem* item: movableAppBarItems(appBar)) {
            EXPECT_TRUE(gtk_widget_get_visible(GTK_WIDGET(item))) << "the wide bar keeps every button";
        }
        auto* more = moreButtonOf(window.get());
        EXPECT_TRUE(more == nullptr || !gtk_widget_is_visible(more));
        expectNothingLost(window.get(), "wide");

        window->updateCompactLayout(500);
        EXPECT_TRUE(window->isAppBarOverflowing());
        expectNothingLost(window.get(), "narrow");
    }
    window->updateCompactLayout(realWidth);
}

namespace {
/// Collects every button in `widget` that a screen reader or a hover could not name
void collectUnnamedButtons(GtkWidget* widget, std::vector<std::string>& offenders) {
    if (GTK_IS_BUTTON(widget) && gtk_widget_get_visible(widget)) {
        const char* label = gtk_button_get_label(GTK_BUTTON(widget));
        const char* accessible = atk_object_get_name(gtk_widget_get_accessible(widget));
        const char* tooltip = gtk_widget_get_tooltip_text(widget);
        const bool named = (accessible && *accessible) || (label && *label);
        const bool hinted = (tooltip && *tooltip) || (label && *label);
        if (!named || !hinted) {
            offenders.push_back(std::string(G_OBJECT_TYPE_NAME(widget)) + " name=" + gtk_widget_get_name(widget) +
                                (named ? "" : " [no accessible name]") + (hinted ? "" : " [no tooltip]") + " tip='" +
                                (tooltip ? tooltip : "") + "' in " + G_OBJECT_TYPE_NAME(gtk_widget_get_parent(widget)));
        }
    }
    if (GTK_IS_CONTAINER(widget)) {
        GList* children = gtk_container_get_children(GTK_CONTAINER(widget));
        for (GList* child = children; child; child = child->next) {
            collectUnnamedButtons(GTK_WIDGET(child->data), offenders);
        }
        g_list_free(children);
    }
}
}  // namespace

TEST_F(ClassroomWorkflow, everyRailAndAppBarButtonHasAnAccessibleNameAndATooltip) {
    window->toolbarSelected("UTN Teacher");
    gtk_widget_show(window->getWindow());
    settle();
    std::vector<std::string> offenders;
    for (const char* bar: {"tbLeft1", "tbTop1"}) {
        collectUnnamedButtons(window->get(bar), offenders);
    }
    std::string list;
    for (const auto& offender: offenders) {
        list += "\n  " + offender;
    }
    EXPECT_TRUE(offenders.empty()) << offenders.size() << " unnamed or unexplained buttons:" << list;
}

// The real Workspace menu buttons, which is how a teacher changes the mode
TEST_F(ClassroomWorkflow, workspaceMenuLabelChoicesDriveTheRailCaptions) {
    window->toolbarSelected("UTN Teacher");
    gtk_widget_show(window->getWindow());
    settle();
    auto* rail = window->get("tbLeft1");
    auto* workspace = buttonNamed(window->getWindow(), "Workspace");
    ASSERT_TRUE(GTK_IS_MENU_BUTTON(workspace));
    auto* options = GTK_WIDGET(gtk_menu_button_get_popover(GTK_MENU_BUTTON(workspace)));
    auto chooseLabels = [&](const char* text) {
        auto* choice = buttonNamed(options, text);
        ASSERT_NE(choice, nullptr) << text;
        gtk_button_clicked(GTK_BUTTON(choice));
    };
    window->updateCompactLayout(1008);
    chooseLabels("Hide");
    EXPECT_EQ(window->getToolLabelMode(), 2);
    expectCaptions(rail, false, "Hide");
    chooseLabels("Show");
    EXPECT_EQ(window->getToolLabelMode(), 1);
    expectCaptions(rail, true, "Show while compact");
    chooseLabels("Automatic");
    EXPECT_EQ(window->getToolLabelMode(), 0);
    expectCaptions(rail, false, "Automatic while compact");
    window->updateCompactLayout(1280);
    expectCaptions(rail, true, "Automatic while wide");
    window->updateCompactLayout(gtk_widget_get_allocated_width(window->getWindow()));
}

TEST_F(ClassroomWorkflow, shapesAndFeedbackCanPinInspectorWithoutReselectingTools) {
    window->toolbarSelected("UTN Teacher");
    window->show(nullptr);
    settle();
    control->selectTool(TOOL_PEN);
    auto* rail = window->get("tbLeft1");

    for (const char* name : {"Shapes", "Feedback"}) {
        window->setTeacherWorkspace(1);
        ASSERT_EQ(window->getTeacherWorkspace(), 1);

        GtkWidget* menu = menuButtonAccessibleNamed(rail, name);
        ASSERT_TRUE(GTK_IS_MENU_BUTTON(menu)) << name;
        auto* popover = gtk_menu_button_get_popover(GTK_MENU_BUTTON(menu));
        ASSERT_TRUE(GTK_IS_POPOVER(popover)) << name;
        auto* pin = buttonNamed(GTK_WIDGET(popover), "Pin settings panel");
        ASSERT_TRUE(GTK_IS_BUTTON(pin)) << name;

        gtk_button_clicked(GTK_BUTTON(menu));
        settle();
        gtk_button_clicked(GTK_BUTTON(pin));
        settle();

        EXPECT_EQ(window->getTeacherWorkspace(), 0) << name;
        EXPECT_EQ(control->getToolHandler()->getToolType(), TOOL_PEN) << name;
        EXPECT_EQ(gtk_widget_get_parent(window->get("mainContentContainer")),
                  window->get("utnInspectorPaned")) << name;
        int storedMode = -1;
        ASSERT_TRUE(control->getSettings()->getCustomElement("utn").getInt("workspaceTools", storedMode));
        EXPECT_EQ(storedMode, 0) << name;
    }
}

// The Pin settings entry must be a plain action: it may not touch the pen, the colour, the drawing mode or the width
// the teacher chose for the inspector, it must close its popover and it must work from the keyboard.
TEST_F(ClassroomWorkflow, pinSettingsKeepsToolColourModeAndInspectorWidthAndClosesThePopover) {
    window->toolbarSelected("UTN Teacher");
    window->show(nullptr);
    settle();
    auto* tools = control->getToolHandler();
    control->selectTool(TOOL_HIGHLIGHTER);
    tools->setDrawingType(DRAWING_TYPE_DEFAULT);
    const Color colour = tools->getColor();
    const int width = window->getInspectorWidth();
    auto* rail = window->get("tbLeft1");

    for (const char* name : {"Shapes", "Feedback"}) {
        window->setTeacherWorkspace(1);
        GtkWidget* menu = menuButtonAccessibleNamed(rail, name);
        ASSERT_TRUE(GTK_IS_MENU_BUTTON(menu)) << name;
        auto* popover = GTK_WIDGET(gtk_menu_button_get_popover(GTK_MENU_BUTTON(menu)));
        auto* pin = buttonNamed(popover, "Pin settings panel");
        ASSERT_TRUE(GTK_IS_BUTTON(pin)) << name;

        AtkObject* accessible = gtk_widget_get_accessible(pin);
        ASSERT_NE(accessible, nullptr) << name;
        EXPECT_STREQ(atk_object_get_name(accessible), "Pin settings panel") << name;
        EXPECT_TRUE(gtk_widget_get_can_focus(pin)) << name << ": reachable with Tab";
        EXPECT_TRUE(gtk_widget_get_sensitive(pin)) << name;

        gtk_button_clicked(GTK_BUTTON(menu));
        settle();
        ASSERT_TRUE(gtk_widget_get_visible(popover)) << name << ": the popover opens";
        gtk_widget_grab_focus(pin);
        gtk_button_clicked(GTK_BUTTON(pin));  // a focused GtkButton turns Enter and Space into this same signal
        settle();

        EXPECT_TRUE(waitUntilHidden(popover)) << name << ": the popover finishes closing (hidden and unmapped)";
        EXPECT_EQ(window->getTeacherWorkspace(), 0) << name;
        EXPECT_EQ(tools->getToolType(), TOOL_HIGHLIGHTER) << name;
        EXPECT_EQ(tools->getDrawingType(), DRAWING_TYPE_DEFAULT) << name;
        EXPECT_EQ(tools->getColor(), colour) << name;
        EXPECT_EQ(window->getInspectorWidth(), width) << name << ": saved width is kept";
    }
}

// Pinning when the panel is already pinned must stay harmless, and layouts without these tools must not grow the entry.
TEST_F(ClassroomWorkflow, pinSettingsIsHarmlessWhenAlreadyPinnedAndAbsentFromOtherLayouts) {
    window->toolbarSelected("UTN Teacher");
    window->show(nullptr);
    settle();
    window->setTeacherWorkspace(0);
    settle();
    auto* rail = window->get("tbLeft1");
    GtkWidget* menu = menuButtonAccessibleNamed(rail, "Shapes");
    ASSERT_TRUE(GTK_IS_MENU_BUTTON(menu));
    auto* pin = buttonNamed(GTK_WIDGET(gtk_menu_button_get_popover(GTK_MENU_BUTTON(menu))), "Pin settings panel");
    ASSERT_TRUE(GTK_IS_BUTTON(pin));
    gtk_button_clicked(GTK_BUTTON(pin));
    gtk_button_clicked(GTK_BUTTON(pin));
    settle();
    EXPECT_EQ(window->getTeacherWorkspace(), 0);
    EXPECT_EQ(gtk_widget_get_parent(window->get("mainContentContainer")), window->get("utnInspectorPaned"));

    window->toolbarSelected("Xournal++");
    settle();
    EXPECT_EQ(menuButtonAccessibleNamed(window->get("mainContainerBox"), "Shapes"), nullptr);
    EXPECT_EQ(menuButtonAccessibleNamed(window->get("mainContainerBox"), "Feedback"), nullptr);
    window->toolbarSelected("UTN Teacher");
    settle();
}

TEST_F(ClassroomWorkflow, repeatedAnswersActivationCreatesDistinctPrivateLayers) {
    auto page = control->getCurrentPage();
    std::set<std::string> names;
    for (int i = 0; i < 10; ++i) {
        control->startAnswersLayer();
        ASSERT_EQ(page->getLayerCount(), i + 2);
        EXPECT_EQ(page->getSelectedLayer()->getAudience(), LayerAudience::Answers);
        EXPECT_TRUE(names.insert(page->getSelectedLayer()->getName()).second);
    }
    EXPECT_EQ(control->setAnswersRevealed(false, false), 10);
    EXPECT_EQ(page->getSelectedLayerId(), 1);
    EXPECT_EQ(control->countHiddenAnswersOnCurrentPage(), 10);
    EXPECT_EQ(control->setAnswersRevealed(true, false), 10);
    EXPECT_EQ(control->setAnswersRevealed(false, false), 10);
}

TEST_F(ClassroomWorkflow, exportDialogDefaultsToStudentCopyAndRequiresTeacherOptIn) {
    bool confirmed = false;
    ExportAudience audience = ExportAudience::Student;
    xoj::popup::PopupWindowWrapper<xoj::popup::ExportDialog> popup(&paths, EXPORT_GRAPHICS_PDF, 1, 2, false,
                                   [&](const xoj::popup::ExportDialog& selected) {
                                       confirmed = selected.isConfirmed();
                                       audience = selected.getExportAudience();
                                   });
    auto* dialog = popup.getPopup();
    EXPECT_EQ(dialog->getExportAudience(), ExportAudience::Student);
    auto* dialogWindow = GTK_WIDGET(dialog->getWindow());
    popup.show(GTK_WINDOW(window->getWindow()));
    settle();
    auto* teacher = buttonNamed(dialogWindow, "Teacher copy (private content permitted)");
    ASSERT_TRUE(GTK_IS_CHECK_BUTTON(teacher));
    EXPECT_FALSE(gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(teacher)));
    gtk_button_clicked(GTK_BUTTON(teacher));
    EXPECT_TRUE(gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(teacher)));
    auto* ok = buttonNamed(dialogWindow, "Ok");
    ASSERT_NE(ok, nullptr);
    gtk_button_clicked(GTK_BUTTON(ok));
    settle();
    EXPECT_TRUE(confirmed);
    EXPECT_EQ(audience, ExportAudience::Teacher);
}

TEST_F(ClassroomWorkflow, activeInkToolOpensOptionsWithoutReselectingAndKeepsEraserMode) {
    struct ActivationCounter {
        GObject* action;
        gulong handler;
        int count = 0;
        explicit ActivationCounter(GObject* action): action(action) {
            handler = g_signal_connect(action, "change-state", G_CALLBACK(+[](GSimpleAction*, GVariant*, gpointer data) {
                ++static_cast<ActivationCounter*>(data)->count;
            }), this);
        }
        ~ActivationCounter() { g_signal_handler_disconnect(action, handler); }
    } activations(G_OBJECT(control->getActionDatabase()->getAction(Action::SELECT_TOOL).get()));
    window->toolbarSelected("UTN Teacher");
    window->show(nullptr);
    settle();
    auto* rail = window->get("tbLeft1");
    auto* tools = control->getToolHandler();
    auto button = [&](const char* caption) {
        auto* label = railLabel(rail, caption);
        return label ? gtk_widget_get_parent(gtk_widget_get_parent(label)) : nullptr;
    };
    auto menuFor = [](GtkWidget* primary) -> GtkMenuButton* {
        GList* children = gtk_container_get_children(GTK_CONTAINER(gtk_widget_get_parent(primary)));
        GtkMenuButton* menu = nullptr;
        for (GList* child = children; child && !menu; child = child->next) {
            if (GTK_IS_MENU_BUTTON(child->data)) menu = GTK_MENU_BUTTON(child->data);
        }
        g_list_free(children);
        return menu;
    };
    control->selectTool(TOOL_TEXT);
    for (const auto& [caption, tool]: {std::pair{"Area", TOOL_SELECT_RECT}, {"Pen", TOOL_PEN}, {"Eraser", TOOL_ERASER},
                                     {"Highlight", TOOL_HIGHLIGHTER}}) {
        auto* primary = button(caption);
        ASSERT_TRUE(GTK_IS_TOGGLE_BUTTON(primary)) << caption;
        auto* menu = menuFor(primary);
        ASSERT_NE(menu, nullptr) << caption;
        auto* popover = GTK_WIDGET(gtk_menu_button_get_popover(menu));
        EXPECT_NE(atk_object_get_name(gtk_widget_get_accessible(GTK_WIDGET(menu))), nullptr);
        gchar* oldTooltip = gtk_widget_get_tooltip_text(primary);
        gtk_widget_set_tooltip_text(primary, "Changed subtype");
        EXPECT_STREQ(atk_object_get_name(gtk_widget_get_accessible(GTK_WIDGET(menu))), "Changed subtype options");
        gtk_widget_set_tooltip_text(primary, oldTooltip);
        g_free(oldTooltip);
        const int beforeSelect = activations.count;
        gtk_button_clicked(GTK_BUTTON(primary));
        settle();
        if (tool != TOOL_HIGHLIGHTER) {
            EXPECT_GT(activations.count, beforeSelect);
        }
        EXPECT_EQ(tools->getToolType(), tool);
        EXPECT_FALSE(gtk_widget_get_visible(popover));
        GdkEvent* press = gdk_event_new(GDK_BUTTON_PRESS);
        press->button.window = GDK_WINDOW(g_object_ref(gtk_widget_get_window(primary)));
        gdk_event_set_device(press, gdk_seat_get_pointer(gdk_display_get_default_seat(gdk_display_get_default())));
        press->button.button = 1;
        gboolean handled = FALSE;
        const int beforeOptions = activations.count;
        g_signal_emit_by_name(primary, "button-press-event", press, &handled);
        GdkEvent* release = gdk_event_copy(press);
        release->type = GDK_BUTTON_RELEASE;
        gtk_widget_event(primary, release);
        gdk_event_free(release);
        gdk_event_free(press);
        settle();
        EXPECT_TRUE(handled) << caption;
        EXPECT_TRUE(gtk_widget_get_visible(popover)) << caption;
        EXPECT_TRUE(gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(primary)));
        EXPECT_EQ(tools->getToolType(), tool);
        EXPECT_EQ(activations.count, beforeOptions) << "active press/release must not activate SELECT_TOOL";
        gtk_popover_popdown(GTK_POPOVER(popover));
        settle();
        GdkEvent* key = gdk_event_new(GDK_KEY_PRESS);
        key->key.window = GDK_WINDOW(g_object_ref(gtk_widget_get_window(primary)));
        gdk_event_set_device(key, gdk_seat_get_keyboard(gdk_display_get_default_seat(gdk_display_get_default())));
        key->key.keyval = GDK_KEY_Return;
        handled = FALSE;
        g_signal_emit_by_name(primary, "key-press-event", key, &handled);
        gdk_event_free(key);
        settle();
        EXPECT_TRUE(handled) << caption;
        EXPECT_TRUE(gtk_widget_get_visible(popover)) << caption;
        control->selectTool(TOOL_TEXT);
        for (int attempt = 0; attempt < 8 && gtk_widget_get_visible(popover); ++attempt) {
            settle();
        }
        EXPECT_FALSE(gtk_widget_get_visible(popover)) << caption;
    }
    auto* eraser = button("Eraser");
    auto* options = GTK_WIDGET(gtk_menu_button_get_popover(menuFor(eraser)));
    auto* wholeStroke = buttonNamed(options, "delete stroke");
    ASSERT_NE(wholeStroke, nullptr);
    gtk_button_clicked(GTK_BUTTON(eraser));
    gtk_button_clicked(GTK_BUTTON(wholeStroke));
    EXPECT_EQ(tools->getEraserType(), ERASER_TYPE_DELETE_STROKE);
    EXPECT_FALSE(gtk_widget_get_visible(options));
    gtk_button_clicked(GTK_BUTTON(button("Pen")));
    gtk_button_clicked(GTK_BUTTON(eraser));
    EXPECT_EQ(tools->getEraserType(), ERASER_TYPE_DELETE_STROKE);
}

TEST_F(ClassroomWorkflow, penPropertiesPopoverAndInkDockUseLivePenSettings) {
    window->toolbarSelected("UTN Teacher");
    window->setTeacherWorkspace(1);
    gtk_window_resize(GTK_WINDOW(window->getWindow()), 1280, 720);
    window->show(nullptr);
    settle();

    const char* captureDir = g_getenv("UTN_VISUAL_CAPTURE_DIR");
    auto capture = [&](const char* name) {
        if (!captureDir) return;
        g_mkdir_with_parents(captureDir, 0755);
        gchar* path = g_build_filename(captureDir, name, nullptr);
        captureWindow(window->getWindow(), path);
        g_free(path);
    };
    capture("ACTUAL-GTK-BUILD-paper-ink-light-workspace.png");

    const std::string previousToolbar = window->getSelectedToolbar()->getId();
    const int previousWorkspace = window->getTeacherWorkspace();
    window->toolbarSelected("UTN Teacher");
    window->setTeacherWorkspace(1);
    auto* tools = control->getToolHandler();
    const Color previousColour = tools->getTool(TOOL_PEN).getColor();
    const ToolSize previousSize = tools->getPenSize();
    auto& utnSettings = control->getSettings()->getCustomElement("utn");
    bool previousDockEnabled = false;
    utnSettings.getBool("inkDockEnabled", previousDockEnabled);
    control->selectTool(TOOL_PEN);

    GtkWidget* trigger = widgetNamed(window->getWindow(), "utnPenSettingsButton");
    ASSERT_TRUE(GTK_IS_MENU_BUTTON(trigger));
    GtkWidget* popover = GTK_WIDGET(gtk_menu_button_get_popover(GTK_MENU_BUTTON(trigger)));
    ASSERT_TRUE(GTK_IS_POPOVER(popover));
    GtkWidget* dock = widgetNamed(window->get("mainOverlay"), "utnInkDock");
    ASSERT_NE(dock, nullptr);
    auto* dockToggle = buttonNamed(popover, "Show ink dock");
    ASSERT_NE(dockToggle, nullptr);
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(dockToggle), false);
    settle();
    EXPECT_FALSE(gtk_widget_get_visible(dock));

    gtk_popover_popup(GTK_POPOVER(popover));
    settle();
    EXPECT_TRUE(gtk_widget_get_visible(popover));
    {
        // Regression: the card was never shown, so the real popover was a small blank bubble.
        auto* pin = buttonNamed(popover, "Pin to side");
        ASSERT_NE(pin, nullptr);
        EXPECT_TRUE(gtk_widget_get_visible(pin));
        EXPECT_GT(gtk_widget_get_allocated_height(popover), 250);
        EXPECT_GT(gtk_widget_get_allocated_width(popover), 300);
        auto* width = widgetNamed(popover, "utnPenWidthScale");
        ASSERT_TRUE(GTK_IS_RANGE(width));
        auto* preview = widgetNamed(popover, "utnPenStrokePreview");
        EXPECT_TRUE(GTK_IS_DRAWING_AREA(preview));
        auto* advanced = widgetNamed(popover, "utnPenAdvancedOptions");
        EXPECT_TRUE(GTK_IS_EXPANDER(advanced));
    }
    capture("ACTUAL-GTK-BUILD-paper-ink-workspace-popover-open-main-window.png");
    if (captureDir) {
        gchar* path = g_build_filename(captureDir, "ACTUAL-GTK-BUILD-paper-ink-pen-properties-popover.png", nullptr);
        captureWindow(popover, path);
        g_free(path);
    }
    auto* blue = buttonAccessibleNamed(popover, "Blue");
    ASSERT_NE(blue, nullptr);
    gtk_button_clicked(GTK_BUTTON(blue));
    const Color selectedColour = tools->getTool(TOOL_PEN).getColor();
    EXPECT_EQ(selectedColour.red, 0x1eU);
    EXPECT_EQ(selectedColour.green, 0x5bU);
    EXPECT_EQ(selectedColour.blue, 0xd8U);

    // The compact width control maps to the existing five discrete model presets.
    auto* width = widgetNamed(popover, "utnPenWidthScale");
    gtk_range_set_value(GTK_RANGE(width), static_cast<double>(TOOL_SIZE_VERY_THICK));
    EXPECT_EQ(tools->getPenSize(), TOOL_SIZE_VERY_THICK);

    gtk_button_clicked(GTK_BUTTON(dockToggle));
    settle();
    EXPECT_TRUE(gtk_widget_get_visible(dock));
    bool persistedDockEnabled = false;
    EXPECT_TRUE(utnSettings.getBool("inkDockEnabled", persistedDockEnabled));
    EXPECT_TRUE(persistedDockEnabled);
    capture("ACTUAL-GTK-BUILD-paper-ink-ink-dock.png");

    auto* green = buttonAccessibleNamed(dock, "Green");
    ASSERT_NE(green, nullptr);
    gtk_button_clicked(GTK_BUTTON(green));
    EXPECT_EQ(tools->getTool(TOOL_PEN).getColor().green, 0x7dU);
    auto* thick = buttonNamed(dock, "L");
    ASSERT_NE(thick, nullptr);
    gtk_button_clicked(GTK_BUTTON(thick));
    EXPECT_EQ(tools->getPenSize(), TOOL_SIZE_THICK);

    const ThemeVariant originalTheme = control->getSettings()->getThemeVariant();
    control->getSettings()->setThemeVariant(THEME_VARIANT_FORCE_DARK);
    window->updateColorscheme();
    settle();
    capture("ACTUAL-GTK-BUILD-paper-ink-dark-workspace-dock.png");
    control->getSettings()->setThemeVariant(originalTheme);
    window->updateColorscheme();
    settle();
    gtk_popover_popdown(GTK_POPOVER(popover));
    ASSERT_TRUE(waitUntilHidden(popover));
    gtk_window_resize(GTK_WINDOW(window->getWindow()), 900, 720);
    settle();
    capture("ACTUAL-GTK-BUILD-paper-ink-compact-900.png");
    gtk_window_resize(GTK_WINDOW(window->getWindow()), 1280, 720);
    settle();

    gtk_popover_popdown(GTK_POPOVER(popover));
    ASSERT_TRUE(waitUntilHidden(popover));
    auto* dockSettings = buttonNamed(dock, "Pen settings");
    ASSERT_NE(dockSettings, nullptr);
    gtk_button_clicked(GTK_BUTTON(dockSettings));
    settle();
    EXPECT_TRUE(gtk_widget_get_visible(popover));
    EXPECT_EQ(gtk_popover_get_relative_to(GTK_POPOVER(popover)), dockSettings);
    gtk_popover_popdown(GTK_POPOVER(popover));
    ASSERT_TRUE(waitUntilHidden(popover));
    EXPECT_EQ(gtk_popover_get_relative_to(GTK_POPOVER(popover)), trigger);
    EXPECT_EQ(gtk_window_get_focus(GTK_WINDOW(window->getWindow())), dockSettings);

    window->setTeacherWorkspace(1);
    gtk_popover_popup(GTK_POPOVER(popover));
    settle();
    auto* pin = buttonNamed(popover, "Pin to side");
    ASSERT_NE(pin, nullptr);
    gtk_button_clicked(GTK_BUTTON(pin));
    EXPECT_EQ(window->getTeacherWorkspace(), 0);
    EXPECT_TRUE(waitUntilHidden(popover));
    EXPECT_EQ(gtk_window_get_focus(GTK_WINDOW(window->getWindow())), trigger);
    capture("ACTUAL-GTK-BUILD-paper-ink-properties-pinned.png");

    auto* close = buttonAccessibleNamed(dock, "Hide ink dock");
    ASSERT_NE(close, nullptr);
    gtk_button_clicked(GTK_BUTTON(close));
    EXPECT_FALSE(gtk_widget_get_visible(dock));
    EXPECT_FALSE(gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(dockToggle)));
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(dockToggle), previousDockEnabled);

    tools->setColor(previousColour, true);
    tools->setPenSize(previousSize);
    window->setTeacherWorkspace(previousWorkspace);
    window->toolbarSelected(previousToolbar);
}

TEST_F(ClassroomWorkflow, documentHeaderShowsTheNameAndTheRealStatusAndOnlyTheNameHidesWhenCompact) {
    window->toolbarSelected("UTN Teacher");
    window->show(nullptr);
    settle();
    auto* title = widgetNamed(window->getWindow(), "utnDocumentTitle");
    auto* status = widgetNamed(window->getWindow(), "utnDocumentSaveStatus");
    ASSERT_TRUE(GTK_IS_LABEL(title));
    ASSERT_TRUE(GTK_IS_LABEL(status));
    window->updateCompactLayout(1450);

    // The name follows the window title, the status follows the document's real state
    gtk_window_set_title(GTK_WINDOW(window->getWindow()), "lesson.xopp - Ultimate Teacher Notepad");
    EXPECT_STREQ(gtk_label_get_text(GTK_LABEL(title)), "lesson.xopp");
    resetDocumentStatus(control.get());
    EXPECT_STREQ(gtk_label_get_text(GTK_LABEL(status)), "✓");
    EXPECT_TRUE(gtk_widget_get_visible(status));
    gtk_window_set_title(GTK_WINDOW(window->getWindow()), "*lesson.xopp - Ultimate Teacher Notepad");
    EXPECT_STREQ(gtk_label_get_text(GTK_LABEL(status)), "✓") << "a title with a star is not evidence of unsaved edits";
    resetDocumentStatus(control.get(), true);
    EXPECT_STREQ(gtk_label_get_text(GTK_LABEL(status)), "● Unsaved changes");

    window->updateCompactLayout(900);
    EXPECT_FALSE(gtk_widget_get_visible(title)) << "the name gives way to a narrow window";
    EXPECT_TRUE(gtk_widget_get_visible(status)) << "the status never does";
    resetDocumentStatus(control.get());
    gtk_window_resize(GTK_WINDOW(window->getWindow()), 900, 720);
    settle();
    int railWidth = -1;
    auto* railScroller = widgetNamed(window->getWindow(), "tbLeft1Scroller");
    auto* railHost = widgetNamed(window->getWindow(), "utnRailHost");
    auto* expandSettings = widgetNamed(window->getWindow(), "utnExpandInspector");
    ASSERT_TRUE(GTK_IS_BUTTON(expandSettings));
    EXPECT_STREQ(gtk_button_get_label(GTK_BUTTON(expandSettings)), "");
    EXPECT_NE(gtk_button_get_image(GTK_BUTTON(expandSettings)), nullptr);
    EXPECT_STREQ(gtk_widget_get_tooltip_text(expandSettings), "Show tool settings");
    gtk_widget_get_size_request(railScroller, &railWidth, nullptr);
    EXPECT_EQ(railWidth, utn::layout::TEACHER_RAIL_WIDTH);
    EXPECT_EQ(gtk_scrolled_window_get_max_content_width(GTK_SCROLLED_WINDOW(railScroller)),
              utn::layout::TEACHER_RAIL_WIDTH);
    EXPECT_FALSE(gtk_scrolled_window_get_propagate_natural_width(GTK_SCROLLED_WINDOW(railScroller)));
    EXPECT_LE(gtk_widget_get_allocated_width(railHost), 100);
    window->updateCompactLayout(1450);
    EXPECT_TRUE(gtk_widget_get_visible(title));
    EXPECT_STREQ(gtk_button_get_label(GTK_BUTTON(expandSettings)), "");
    EXPECT_STREQ(atk_object_get_name(gtk_widget_get_accessible(expandSettings)), "Show tool settings");
    gtk_widget_get_size_request(widgetNamed(window->getWindow(), "tbLeft1Scroller"), &railWidth, nullptr);
    EXPECT_EQ(railWidth, utn::layout::TEACHER_RAIL_WIDTH);
    EXPECT_EQ(gtk_scrolled_window_get_max_content_width(GTK_SCROLLED_WINDOW(railScroller)),
              utn::layout::TEACHER_RAIL_WIDTH);
    const int previousLabelMode = window->getToolLabelMode();
    window->setToolLabelMode(2);
    gtk_window_resize(GTK_WINDOW(window->getWindow()), 1280, 800);
    settle();
    EXPECT_LE(gtk_widget_get_allocated_width(railHost), utn::layout::TEACHER_RAIL_WIDTH + 4);
    window->setToolLabelMode(previousLabelMode);
}

TEST_F(ClassroomWorkflow, paperInkControlsStayHiddenInClassicLayouts) {
    const std::string previousToolbar = window->getSelectedToolbar()->getId();
    const int previousWorkspace = window->getTeacherWorkspace();
    window->toolbarSelected("UTN Teacher");
    window->setTeacherWorkspace(1);
    control->selectTool(TOOL_PEN);
    window->show(nullptr);
    settle();

    GtkWidget* trigger = widgetNamed(window->getWindow(), "utnPenSettingsButton");
    ASSERT_TRUE(GTK_IS_MENU_BUTTON(trigger));
    GtkWidget* popover = GTK_WIDGET(gtk_menu_button_get_popover(GTK_MENU_BUTTON(trigger)));
    ASSERT_TRUE(GTK_IS_POPOVER(popover));
    GtkWidget* dock = widgetNamed(window->get("mainOverlay"), "utnInkDock");
    ASSERT_NE(dock, nullptr);

    bool previousDockEnabled = false;
    auto& utnSettings = control->getSettings()->getCustomElement("utn");
    utnSettings.getBool("inkDockEnabled", previousDockEnabled);
    gtk_popover_popup(GTK_POPOVER(popover));
    settle();
    auto* dockToggle = buttonNamed(popover, "Show ink dock");
    ASSERT_NE(dockToggle, nullptr);
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(dockToggle), true);
    settle();
    EXPECT_TRUE(gtk_widget_get_visible(dock));

    window->toolbarSelected("Xournal++");
    settle();
    trigger = widgetNamed(window->getWindow(), "utnPenSettingsButton");
    ASSERT_NE(trigger, nullptr);
    EXPECT_FALSE(gtk_widget_get_visible(trigger));
    EXPECT_FALSE(gtk_widget_get_visible(dock));

    window->toolbarSelected("UTN Teacher");
    settle();
    trigger = widgetNamed(window->getWindow(), "utnPenSettingsButton");
    ASSERT_TRUE(GTK_IS_MENU_BUTTON(trigger));
    dock = widgetNamed(window->get("mainOverlay"), "utnInkDock");
    ASSERT_NE(dock, nullptr);
    EXPECT_TRUE(gtk_widget_get_visible(trigger));
    EXPECT_TRUE(gtk_widget_get_visible(dock));

    auto* close = buttonAccessibleNamed(dock, "Hide ink dock");
    ASSERT_NE(close, nullptr);
    gtk_button_clicked(GTK_BUTTON(close));
    window->setTeacherWorkspace(previousWorkspace);
    window->toolbarSelected(previousToolbar);
    if (previousDockEnabled) {
        GtkWidget* currentPopover = GTK_WIDGET(
                gtk_menu_button_get_popover(GTK_MENU_BUTTON(widgetNamed(window->getWindow(), "utnPenSettingsButton"))));
        auto* currentToggle = buttonNamed(currentPopover, "Show ink dock");
        ASSERT_NE(currentToggle, nullptr);
        gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(currentToggle), true);
    }
}

TEST_F(ClassroomWorkflow, classroomMenuUsesSharedPresentationActions) {
    auto* model = window->getMenubar()->getModel();
    bool found = false;
    std::set<std::string> menuActions;
    for (int i = 0; i < g_menu_model_get_n_items(model); ++i) {
        gchar* label = nullptr;
        g_menu_model_get_item_attribute(model, i, G_MENU_ATTRIBUTE_LABEL, "s", &label);
        if (g_strcmp0(label, "_Classroom") == 0) {
            found = true;
            auto* submenu = g_menu_model_get_item_link(model, i, G_MENU_LINK_SUBMENU);
            if (submenu) {
                for (int section = 0; section < g_menu_model_get_n_items(submenu); ++section) {
                    auto* items = g_menu_model_get_item_link(submenu, section, G_MENU_LINK_SECTION);
                    if (!items) {
                        continue;
                    }
                    for (int item = 0; item < g_menu_model_get_n_items(items); ++item) {
                        gchar* action = nullptr;
                        if (g_menu_model_get_item_attribute(items, item, G_MENU_ATTRIBUTE_ACTION, "s", &action)) {
                            menuActions.insert(action);
                        }
                        g_free(action);
                    }
                    g_object_unref(items);
                }
                g_object_unref(submenu);
            }
        }
        g_free(label);
    }
    ASSERT_TRUE(found);
    auto* actions = G_ACTION_GROUP(window->getWindow());
    for (const auto& action: menuActions) {
        ASSERT_EQ(action.substr(0, 4), "win.");
        EXPECT_TRUE(g_action_group_has_action(actions, action.c_str() + 4)) << action;
    }
    EXPECT_EQ(menuActions.size(), 8);
    for (const char* name: {"classroom-spotlight", "classroom-curtain", "classroom-exit-overlay"}) {
        ASSERT_TRUE(g_action_group_has_action(actions, name)) << name;
        EXPECT_TRUE(g_action_group_get_action_enabled(actions, name));
    }
    auto* tools = control->getToolHandler();
    g_action_group_activate_action(actions, "classroom-spotlight", nullptr);
    EXPECT_TRUE(tools->isSpotlightEnabled());
    g_action_group_activate_action(actions, "classroom-spotlight", nullptr);
    EXPECT_FALSE(tools->isSpotlightEnabled());
    g_action_group_activate_action(actions, "classroom-curtain", nullptr);
    EXPECT_TRUE(tools->isCurtainEnabled());
    EXPECT_FALSE(tools->isSpotlightEnabled());
    g_action_group_activate_action(actions, "classroom-exit-overlay", nullptr);
    EXPECT_FALSE(tools->isCurtainEnabled());
}

TEST_F(ClassroomWorkflow, freezeAndBlankAreOneClickInTheAppBarAndStayInStepWithTheStudentsMenu) {
    auto students = std::make_unique<ToolStudentView>("CLASSROOM_STUDENTS", control.get());
    ToolStudentQuick quickTool("STUDENT_QUICK", *students);
    auto pill = students->createItem(true);
    auto quick = quickTool.createItem(true);
    auto* menu = GTK_WIDGET(gtk_menu_button_get_popover(GTK_MENU_BUTTON(pill.get())));
    auto menuClick = [&](const char* label) {
        auto* button = buttonNamed(menu, label);
        ASSERT_NE(button, nullptr) << label;
        gtk_button_clicked(GTK_BUTTON(button));
        settle();
    };

    auto* freeze = buttonNamed(quick.get(), "Freeze");
    auto* blank = buttonNamed(quick.get(), "Blank");
    ASSERT_NE(freeze, nullptr) << "Freeze must be visible in the app bar, not only inside the Students menu";
    ASSERT_NE(blank, nullptr) << "Blank must be visible in the app bar, not only inside the Students menu";
    EXPECT_FALSE(gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(blank)));

    // Blank works at once, with no projector window: on one display it covers the teacher's canvas
    gtk_button_clicked(GTK_BUTTON(blank));
    settle();
    EXPECT_EQ(students->getState(), ToolStudentView::State::Blank);
    EXPECT_TRUE(gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(blank)));
    EXPECT_STREQ(gtk_button_get_label(GTK_BUTTON(blank)), "Show page") << "the button says how to get back";
    // The same state is reachable and visible from the menu
    auto* menuBlank = buttonNamed(menu, "Show the page again");
    ASSERT_NE(menuBlank, nullptr);
    gtk_button_clicked(GTK_BUTTON(blank));
    settle();
    EXPECT_NE(students->getState(), ToolStudentView::State::Blank);
    EXPECT_FALSE(gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(blank)));
    EXPECT_STREQ(gtk_button_get_label(GTK_BUTTON(blank)), "Blank");

    // A change made in the menu is mirrored on the bar
    menuClick("Blank screen");
    EXPECT_TRUE(gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(blank)));
    menuClick("Show the page again");
    EXPECT_FALSE(gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(blank)));

    // Freeze needs the Student View window. Without it the click explains instead of silently doing nothing.
    if (studentWindow()) {  // on extended displays Blank opens the projector window by itself
        menuClick("Close Student View");
    }
    gtk_button_clicked(GTK_BUTTON(freeze));
    settle();
    EXPECT_NE(students->getState(), ToolStudentView::State::Frozen);
    EXPECT_FALSE(gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(freeze)));

    menuClick("Open on the projector");
    ASSERT_NE(studentWindow(), nullptr);
    gtk_button_clicked(GTK_BUTTON(freeze));
    settle();
    EXPECT_EQ(students->getState(), ToolStudentView::State::Frozen);
    EXPECT_TRUE(gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(freeze)));
    EXPECT_STREQ(gtk_button_get_label(GTK_BUTTON(freeze)), "Resume live");
    gtk_button_clicked(GTK_BUTTON(freeze));
    settle();
    EXPECT_EQ(students->getState(), ToolStudentView::State::Live);
    EXPECT_STREQ(gtk_button_get_label(GTK_BUTTON(freeze)), "Freeze");

    // Both buttons name themselves for screen readers and explain themselves in tooltips
    for (auto* button: {freeze, blank}) {
        AtkObject* accessible = gtk_widget_get_accessible(button);
        ASSERT_NE(accessible, nullptr);
        EXPECT_NE(atk_object_get_name(accessible), nullptr);
        EXPECT_NE(gtk_widget_get_tooltip_text(button), nullptr);
    }

    menuClick("Close Student View");
    EXPECT_EQ(studentWindow(), nullptr);
    EXPECT_FALSE(gtk_widget_get_sensitive(freeze) && gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(freeze)));
    gtk_widget_destroy(quick.get());
    quick.reset();
    gtk_widget_destroy(pill.get());
    pill.reset();
    students.reset();
    settle();
}

TEST_F(ClassroomWorkflow, presentMenuShowsWhichOverlayIsOnAndOnlyOffersExitWhenThereIsOne) {
    ToolPresentationKit kit("PRESENTATION_KIT", control.get(), IconNameHelper(control->getSettings()));
    auto item = kit.createItem(true);
    auto* menu = GTK_WIDGET(gtk_menu_button_get_popover(GTK_MENU_BUTTON(item.get())));
    auto* exit = buttonNamed(menu, "Show the whole page");
    ASSERT_NE(exit, nullptr) << "the way back must be named by what it does";
    auto* spotlight = buttonNamed(menu, "Spotlight");
    auto* curtain = buttonNamed(menu, "Curtain");
    ASSERT_NE(spotlight, nullptr);
    ASSERT_NE(curtain, nullptr);
    EXPECT_FALSE(gtk_widget_get_sensitive(exit)) << "nothing to exit yet";
    EXPECT_FALSE(gtk_style_context_has_class(gtk_widget_get_style_context(item.get()), "utn-present-active"));

    ToolPresentationKit::runClassroomAction(control.get(), Action::CLASSROOM_SPOTLIGHT);
    settle();
    EXPECT_TRUE(gtk_widget_get_sensitive(exit));
    EXPECT_STREQ(gtk_button_get_label(GTK_BUTTON(spotlight)), "Spotlight is on: choose to turn off");
    EXPECT_STREQ(gtk_button_get_label(GTK_BUTTON(curtain)), "Curtain");
    EXPECT_TRUE(gtk_style_context_has_class(gtk_widget_get_style_context(item.get()), "utn-present-active"))
            << "the app-bar button itself shows that the page is covered";

    ToolPresentationKit::runClassroomAction(control.get(), Action::CLASSROOM_CURTAIN);
    settle();
    EXPECT_STREQ(gtk_button_get_label(GTK_BUTTON(spotlight)), "Spotlight");
    EXPECT_STREQ(gtk_button_get_label(GTK_BUTTON(curtain)), "Curtain is on: choose to turn off");
    // Relabelling rebuilds GTK3's label child; entries must stay left-aligned like the rest of the menu
    for (auto* entry: {spotlight, curtain}) {
        auto* text = gtk_bin_get_child(GTK_BIN(entry));
        ASSERT_TRUE(GTK_IS_LABEL(text));
        EXPECT_FLOAT_EQ(gtk_label_get_xalign(GTK_LABEL(text)), 0.0F) << gtk_button_get_label(GTK_BUTTON(entry));
    }

    gtk_button_clicked(GTK_BUTTON(exit));
    settle();
    EXPECT_FALSE(control->getToolHandler()->isCurtainEnabled());
    EXPECT_FALSE(gtk_widget_get_sensitive(exit));
    EXPECT_FALSE(gtk_style_context_has_class(gtk_widget_get_style_context(item.get()), "utn-present-active"));
    EXPECT_STREQ(gtk_button_get_label(GTK_BUTTON(curtain)), "Curtain");

    // A tool switch that removes an overlay must not leave a stale state in a menu that was built earlier
    ToolPresentationKit::runClassroomAction(control.get(), Action::CLASSROOM_CURTAIN);
    settle();
    control->selectTool(TOOL_PEN);
    settle();
    EXPECT_FALSE(control->getToolHandler()->isCurtainEnabled());
    EXPECT_FALSE(gtk_widget_get_sensitive(exit));

    gtk_widget_destroy(item.get());
    item.reset();
    // A later tool change must not reach the destroyed menu
    control->getToolHandler()->fireToolChanged();
    settle();
}

namespace {
/// A key press as the window's key handler receives it. The widget owner keeps the event and frees it.
struct FakeKey {
    explicit FakeKey(GtkWidget* widget, guint keyval, guint state) {
        event = gdk_event_new(GDK_KEY_PRESS);
        event->key.window = gtk_widget_get_window(widget);
        if (event->key.window) {
            g_object_ref(event->key.window);
        }
        event->key.keyval = keyval;
        event->key.state = static_cast<GdkModifierType>(state);
    }
    ~FakeKey() { gdk_event_free(event); }
    GdkEvent* event;
};
}  // namespace

TEST_F(ClassroomWorkflow, workspacesKeepTheToolTheDockTheModeAndThePageAndPresentLeavesBackWhereTheTeacherWas) {
    using utn::layout::InterfaceMode;
    auto* tools = control->getToolHandler();
    window->toolbarSelected("UTN Teacher");
    window->show(nullptr);
    allocatedWidthAfterResize(window->getWindow(), 1280);
    settle();
    control->updateDockLayout([](utn::DockLayout& layout) {
        layout.reset();
        layout.pin("HAND");
    });
    window->setInterfaceMode(InterfaceMode::Expert);
    ASSERT_TRUE(control->applyPenPreset(utn::PenPresetStore::RED_MARKING_ID));
    settle();
    const auto dockBefore = railToolNames(window.get());
    const auto colour = toolRgb(control.get());
    const size_t pages = control->getDocument()->getPageCount();
    const double zoom = control->getZoomControl()->getZoom();

    auto expectUnchanged = [&](const char* where) {
        EXPECT_EQ(tools->getToolType(), TOOL_PEN) << where << ": the active tool is the teacher's choice";
        EXPECT_EQ(toolRgb(control.get()), colour) << where;
        EXPECT_EQ(control->activePenPresetId(), utn::PenPresetStore::RED_MARKING_ID) << where;
        EXPECT_EQ(window->getInterfaceMode(), InterfaceMode::Expert) << where << ": Beginner/Expert is its own setting";
        EXPECT_EQ(control->getDocument()->getPageCount(), pages) << where << ": switching changes no document";
        EXPECT_DOUBLE_EQ(control->getZoomControl()->getZoom(), zoom) << where;
    };

    window->toolbarSelected("UTN Marking");
    settle();
    expectUnchanged("Mark");
    window->toolbarSelected("UTN Present");
    settle();
    expectUnchanged("Present");

    // Leaving Present returns to the workspace the teacher came from, not to a fixed one
    window->leavePresent();
    settle();
    EXPECT_EQ(window->getSelectedToolbar()->getId(), "UTN Marking");
    window->toolbarSelected("UTN Teacher");
    window->toolbarSelected("UTN Present");
    window->leavePresent();
    EXPECT_EQ(window->getSelectedToolbar()->getId(), "UTN Teacher");
    settle();
    expectUnchanged("Teach again");
    EXPECT_EQ(railToolNames(window.get()), dockBefore) << "the customised dock comes back exactly";

    control->updateDockLayout([](utn::DockLayout& layout) { layout.reset(); });
    window->setInterfaceMode(InterfaceMode::Custom);
    window->setToolLabelMode(2);
    control->selectTool(TOOL_PEN);
}

TEST_F(ClassroomWorkflow, presentHidesTheToolDockUntilRevealedAndLeavingPresentBringsTheDockBack) {
    window->toolbarSelected("UTN Teacher");
    window->show(nullptr);
    allocatedWidthAfterResize(window->getWindow(), 1280);
    settle();
    auto* dock = window->get("tbLeft1Scroller");
    ASSERT_NE(dock, nullptr);
    EXPECT_TRUE(gtk_widget_get_visible(dock)) << "Teach shows the dock";

    window->toolbarSelected("UTN Present");
    settle();
    EXPECT_FALSE(gtk_widget_get_visible(dock)) << "Present gives the page the room";
    EXPECT_FALSE(window->isPresentToolsRevealed());

    window->setPresentToolsRevealed(true);  // the teacher asks for the tools back, for now
    settle();
    EXPECT_TRUE(gtk_widget_get_visible(dock));
    window->setPresentToolsRevealed(false);
    settle();
    EXPECT_FALSE(gtk_widget_get_visible(dock));

    window->setPresentToolsRevealed(true);
    window->toolbarSelected("UTN Teacher");
    settle();
    EXPECT_TRUE(gtk_widget_get_visible(dock)) << "outside Present the dock is always there";
    EXPECT_FALSE(window->isPresentToolsRevealed()) << "a later Present starts quiet again";
    window->toolbarSelected("UTN Present");
    settle();
    EXPECT_FALSE(gtk_widget_get_visible(dock));
    window->toolbarSelected("UTN Teacher");
}

TEST_F(ClassroomWorkflow, presentOffersAVisibleWayToRevealTheToolsAndToLeaveAndOnlyInPresent) {
    window->toolbarSelected("UTN Teacher");
    window->show(nullptr);
    settle();
    {
        ToolPresentationKit kit("PRESENTATION_KIT", control.get(), IconNameHelper(control->getSettings()));
        auto item = kit.createItem(true);
        auto* menu = GTK_WIDGET(gtk_menu_button_get_popover(GTK_MENU_BUTTON(item.get())));
        EXPECT_EQ(buttonNamed(menu, "Leave Present"), nullptr) << "outside Present there is nothing to leave";
        EXPECT_EQ(buttonNamed(menu, "Show tools"), nullptr);
        gtk_widget_destroy(item.get());
    }

    window->toolbarSelected("UTN Marking");
    window->toolbarSelected("UTN Present");
    settle();
    ToolPresentationKit kit("PRESENTATION_KIT", control.get(), IconNameHelper(control->getSettings()));
    auto item = kit.createItem(true);
    auto* menu = GTK_WIDGET(gtk_menu_button_get_popover(GTK_MENU_BUTTON(item.get())));
    auto* show = buttonNamed(menu, "Show tools");
    auto* leave = buttonNamed(menu, "Leave Present");
    ASSERT_NE(show, nullptr) << "the dock is hidden in Present, so the way to bring it back must be on screen";
    ASSERT_NE(leave, nullptr) << "and so must the way out";

    gtk_button_clicked(GTK_BUTTON(show));
    settle();
    EXPECT_TRUE(window->isPresentToolsRevealed());
    EXPECT_NE(buttonNamed(menu, "Hide tools"), nullptr) << "the button says what it will do next";
    gtk_button_clicked(GTK_BUTTON(buttonNamed(menu, "Hide tools")));
    settle();
    EXPECT_FALSE(window->isPresentToolsRevealed());

    gtk_button_clicked(GTK_BUTTON(leave));
    settle();
    EXPECT_EQ(window->getSelectedToolbar()->getId(), "UTN Marking") << "back to where the teacher came from";
    gtk_widget_destroy(item.get());
    item.reset();
    window->toolbarSelected("UTN Teacher");
}

TEST_F(ClassroomWorkflow, workspaceShortcutsSwitchAndToggleWithoutHijackingOrdinaryKeys) {
    window->toolbarSelected("UTN Teacher");
    window->show(nullptr);
    settle();
    GtkWidget* top = GTK_WIDGET(window->getWindow());
    const guint ctrlShift = GDK_CONTROL_MASK | GDK_SHIFT_MASK;
    auto press = [&](guint key, guint state) {
        FakeKey fake(top, key, state);
        return window->workspaceShortcut(&fake.event->key);
    };

    EXPECT_TRUE(press(GDK_KEY_exclam, ctrlShift));  // Ctrl+Shift+1
    EXPECT_EQ(window->getSelectedToolbar()->getId(), "UTN Teacher");
    EXPECT_TRUE(press(GDK_KEY_at, ctrlShift));  // Ctrl+Shift+2
    EXPECT_EQ(window->getSelectedToolbar()->getId(), "UTN Marking");
    EXPECT_TRUE(press(GDK_KEY_numbersign, ctrlShift));  // Ctrl+Shift+3
    EXPECT_EQ(window->getSelectedToolbar()->getId(), "UTN Present");
    EXPECT_TRUE(press(GDK_KEY_T, ctrlShift)) << "Ctrl+Shift+T reveals the tools in Present";
    EXPECT_TRUE(window->isPresentToolsRevealed());
    EXPECT_TRUE(press(GDK_KEY_T, ctrlShift));
    EXPECT_FALSE(window->isPresentToolsRevealed());
    EXPECT_TRUE(press(GDK_KEY_P, ctrlShift)) << "Ctrl+Shift+P leaves Present";
    EXPECT_EQ(window->getSelectedToolbar()->getId(), "UTN Marking") << "back to where the teacher came from";
    EXPECT_TRUE(press(GDK_KEY_P, ctrlShift)) << "and enters it again";
    EXPECT_EQ(window->getSelectedToolbar()->getId(), "UTN Present");

    // Ordinary typing is left alone
    EXPECT_FALSE(press(GDK_KEY_1, 0));
    EXPECT_FALSE(press(GDK_KEY_p, GDK_CONTROL_MASK));
    EXPECT_FALSE(press(GDK_KEY_T, GDK_SHIFT_MASK));
    EXPECT_EQ(window->getSelectedToolbar()->getId(), "UTN Present");
    window->toolbarSelected("UTN Teacher");
}

TEST_F(ClassroomWorkflow, classroomControlsGatherEveryEffectSayWhichIsOnAndBackToNormalClearsThemAll) {
    auto students = std::make_unique<ToolStudentView>("CLASSROOM_STUDENTS", control.get());
    ToolPresentationKit kit("PRESENTATION_KIT", control.get(), IconNameHelper(control->getSettings()), students.get());
    auto item = kit.createItem(true);
    auto* menu = GTK_WIDGET(gtk_menu_button_get_popover(GTK_MENU_BUTTON(item.get())));
    auto* status = widgetNamed(menu, "utnClassroomStatus");
    ASSERT_NE(status, nullptr) << "the controls must say what is on";
    auto* back = buttonNamed(menu, "Back to normal");
    ASSERT_NE(back, nullptr) << "one obvious way back, in the same place as the controls";
    for (const char* label: {"Spotlight", "Curtain", "Freeze students' screen", "Blank students' screen",
                             "Open Student View on the projector"}) {
        EXPECT_NE(buttonNamed(menu, label), nullptr) << label << " belongs in the one control surface";
    }
    EXPECT_STREQ(gtk_label_get_text(GTK_LABEL(status)), "Classroom is normal");
    EXPECT_FALSE(gtk_widget_get_sensitive(back)) << "nothing to undo yet";

    gtk_button_clicked(GTK_BUTTON(buttonNamed(menu, "Blank students' screen")));
    settle();
    EXPECT_EQ(students->getState(), ToolStudentView::State::Blank);
    EXPECT_STREQ(gtk_label_get_text(GTK_LABEL(status)), "On: Blank");
    EXPECT_TRUE(gtk_widget_get_sensitive(back));
    EXPECT_NE(buttonNamed(menu, "Show the page to students again"), nullptr) << "a button says what it will do next";

    ToolPresentationKit::runClassroomAction(control.get(), Action::CLASSROOM_SPOTLIGHT);
    settle();
    EXPECT_STREQ(gtk_label_get_text(GTK_LABEL(status)), "On: Spotlight, Blank");

    gtk_button_clicked(GTK_BUTTON(back));
    settle();
    EXPECT_FALSE(control->getToolHandler()->isSpotlightEnabled());
    EXPECT_NE(students->getState(), ToolStudentView::State::Blank);
    EXPECT_STREQ(gtk_label_get_text(GTK_LABEL(status)), "Classroom is normal");
    EXPECT_FALSE(gtk_widget_get_sensitive(back));

    // A frozen screen is cleared by the same button
    if (!students->isWindowOpen()) {  // Blank already opened it on displays where GDK reports a second monitor
        gtk_button_clicked(GTK_BUTTON(buttonNamed(menu, "Open Student View on the projector")));
    }
    settle();
    ASSERT_NE(studentWindow(), nullptr);
    gtk_button_clicked(GTK_BUTTON(buttonNamed(menu, "Freeze students' screen")));
    settle();
    EXPECT_EQ(students->getState(), ToolStudentView::State::Frozen);
    EXPECT_STREQ(gtk_label_get_text(GTK_LABEL(status)), "On: Freeze");
    gtk_button_clicked(GTK_BUTTON(back));
    settle();
    EXPECT_EQ(students->getState(), ToolStudentView::State::Live);
    EXPECT_STREQ(gtk_label_get_text(GTK_LABEL(status)), "Classroom is normal");

    EXPECT_NE(buttonNamed(menu, "Close Student View"), nullptr) << "the window button says what it will do next";
    students->closeStudentView();
    settle();
    gtk_widget_destroy(item.get());
    item.reset();
    students.reset();
    settle();
}

TEST_F(ClassroomWorkflow, escapeLeavesAnOverlayOrTheTeachersBlankCoverAndOtherwiseIsLeftAlone) {
    ASSERT_FALSE(window->escapeClassroom()) << "with nothing on, Escape belongs to text editing and selections";
    auto* students = window->getToolMenuHandler()->getStudentView();
    ASSERT_NE(students, nullptr);

    ToolPresentationKit::runClassroomAction(control.get(), Action::CLASSROOM_CURTAIN);
    settle();
    EXPECT_TRUE(window->escapeClassroom());
    EXPECT_FALSE(control->getToolHandler()->isCurtainEnabled());
    EXPECT_FALSE(window->escapeClassroom()) << "a second Escape is not swallowed";

    // One display: Blank covers the teacher's own canvas, which hides every control, so Escape must lead back
    // (The display count GDK reports varies between machines, so the cover is raised directly: it is what Blank does
    // on one display.)
    students->setBlanked(true);
    students->showTeacherBlankCover(true);
    settle();
    ASSERT_TRUE(students->isTeacherCoverShown());
    EXPECT_TRUE(window->escapeClassroom());
    EXPECT_FALSE(students->isTeacherCoverShown());
    EXPECT_NE(students->getState(), ToolStudentView::State::Blank);
    EXPECT_FALSE(window->escapeClassroom());

    // Extended displays: Blank only covers the projector, the teacher's canvas stays usable, so Escape stays free
    students->setBlanked(true);
    settle();
    if (!students->isTeacherCoverShown()) {
        EXPECT_FALSE(window->escapeClassroom());
        EXPECT_EQ(students->getState(), ToolStudentView::State::Blank);
    }
    students->restoreNormal();
    if (students->isWindowOpen()) {
        students->closeStudentView();  // Blank may have opened the projector window; later tests start without one
    }
    settle();
}

TEST_F(ClassroomWorkflow, repeatedInkThroughPageInputRetainsHistoryAndRoundTrips) {
    struct Heartbeat {
        gint64 last = g_get_monotonic_time();
        gint64 longestGap = 0;
        guint source = g_timeout_add(1, +[](gpointer data) -> gboolean {
            auto& timer = *static_cast<Heartbeat*>(data);
            const auto now = g_get_monotonic_time();
            timer.longestGap = std::max(timer.longestGap, now - timer.last);
            timer.last = now;
            return G_SOURCE_CONTINUE;
        }, this);
        ~Heartbeat() { g_source_remove(source); }
    } heartbeat;
    const char* requested = g_getenv("UTN_STRESS_STROKES");
    const auto parsed = requested ? g_ascii_strtoll(requested, nullptr, 10) : 32;
    const int strokes = static_cast<int>(std::clamp<gint64>(parsed, 1, 20000));
    window->show(nullptr);
    settle();
    heartbeat.last = g_get_monotonic_time();
    heartbeat.longestGap = 0;
    auto* view = window->getXournal()->getViewFor(0);
    ASSERT_NE(view, nullptr);
    auto page = control->getCurrentPage();
    auto* layer = page->getSelectedLayer();
    for (bool highlight: {false, true}) {
        if (highlight) {
            control->selectHighlighter(false);
        } else {
            control->selectTool(TOOL_PEN);
        }
        control->getToolHandler()->setDrawingType(DRAWING_TYPE_DEFAULT);
        std::vector<gint64> latencies;
        std::vector<gint64> releaseLatencies;
        latencies.reserve(static_cast<size_t>(strokes));
        for (int stroke = 0; stroke < strokes; ++stroke) {
            PositionInputData pos{};
            pos.pressure = 0.7;
            pos.x = (80 + stroke % 20) * view->getZoom();
            pos.y = (100 + stroke % 240) * view->getZoom();
            const auto begin = g_get_monotonic_time();
            ASSERT_TRUE(view->onButtonPressEvent(pos));
            for (int point = 1; point <= 16; ++point) {
                pos.x += 3 * view->getZoom();
                pos.y += (point % 2 ? 1 : -1) * view->getZoom();
                pos.timestamp += 8;
                view->onMotionNotifyEvent(pos);
            }
            const auto releaseBegin = g_get_monotonic_time();
            view->onButtonReleaseEvent(pos);
            releaseLatencies.push_back(g_get_monotonic_time() - releaseBegin);
            for (int event = 0; event < 8 && gtk_events_pending(); ++event) {
                gtk_main_iteration_do(false);
            }
            latencies.push_back(g_get_monotonic_time() - begin);
            if (requested && ((stroke + 1) % 100 == 0 || stroke + 1 == strokes)) {
                auto sorted = latencies;
                std::sort(sorted.begin(), sorted.end());
                const auto prefix = std::string(highlight ? "highlight_" : "pen_") + std::to_string(stroke + 1);
                RecordProperty(prefix + "_p95_us", std::to_string(sorted[(sorted.size() - 1) * 95 / 100]));
                RecordProperty(prefix + "_p99_us", std::to_string(sorted[(sorted.size() - 1) * 99 / 100]));
                RecordProperty(prefix + "_max_us", std::to_string(sorted.back()));
            }
        }
        EXPECT_EQ(layer->getElements().size(), static_cast<size_t>(strokes * (highlight ? 2 : 1)));
        if (requested) {
            std::sort(releaseLatencies.begin(), releaseLatencies.end());
            const std::string prefix = highlight ? "highlight_release" : "pen_release";
            RecordProperty(prefix + "_p99_us",
                           std::to_string(releaseLatencies[(releaseLatencies.size() - 1) * 99 / 100]));
            RecordProperty(prefix + "_max_us", std::to_string(releaseLatencies.back()));
            RecordProperty(highlight ? "highlight_heartbeat_gap_us" : "pen_heartbeat_gap_us",
                           std::to_string(heartbeat.longestGap));
            heartbeat.last = g_get_monotonic_time();
            heartbeat.longestGap = 0;
        }
    }
    const auto count = layer->getElements().size();
    control->getUndoRedoHandler()->undo();
    EXPECT_EQ(layer->getElements().size(), count - 1);
    control->getUndoRedoHandler()->redo();
    EXPECT_EQ(layer->getElements().size(), count);
    const auto file = Util::getTmpDirSubfolder() / "m3-repeated-ink.xopp";
    SaveHandler saver;
    saver.prepareSave(control->getDocument(), file);
    saver.saveTo(file);
    ASSERT_TRUE(saver.getErrorMessage().empty()) << saver.getErrorMessage();
    auto restored = LoadHandler{}.loadDocument(file);
    ASSERT_NE(restored, nullptr);
    EXPECT_EQ(restored->getPage(0)->getSelectedLayer()->getElements().size(), count);
}

TEST_F(ClassroomWorkflow, highlighterRestoresFreehandAndSemanticModeAfterSwitchingTools) {
    window->toolbarSelected("UTN Teacher");
    auto* caption = railLabel(window->get("tbLeft1"), "Highlight");
    ASSERT_NE(caption, nullptr);
    auto* highlight = GTK_BUTTON(gtk_widget_get_parent(gtk_widget_get_parent(caption)));
    auto* tools = control->getToolHandler();
    control->selectHighlighter(false);
    control->selectTool(TOOL_PEN);
    gtk_button_clicked(highlight);
    EXPECT_EQ(tools->getToolType(), TOOL_HIGHLIGHTER);
    EXPECT_FALSE(tools->isSmartHighlighterEnabled());
    control->selectHighlighter(true, SmartHighlighterSnapMode::Underline);
    control->selectTool(TOOL_ERASER);
    gtk_button_clicked(highlight);
    EXPECT_TRUE(tools->isSmartHighlighterEnabled());
    EXPECT_EQ(tools->getSmartHighlighterSnapMode(), SmartHighlighterSnapMode::Underline);
    control->selectTool(TOOL_HIGHLIGHTER);
    control->selectTool(TOOL_PEN);
    gtk_button_clicked(highlight);
    EXPECT_TRUE(tools->isSmartHighlighterEnabled()) << "incidental resets must not overwrite a chosen mode";
    EXPECT_EQ(tools->getSmartHighlighterSnapMode(), SmartHighlighterSnapMode::Underline);
}

TEST_F(ClassroomWorkflow, inspectorWorkspaceModesPreserveToolStateAndPersist) {
    auto* settings = control->getSettings();
    auto* tools = control->getToolHandler();
    tools->setPenSize(TOOL_SIZE_THICK);
    const Color penColour = tools->getColor();
    GtkWidget* paned = window->get("utnInspectorPaned");
    GtkWidget* canvasContainer = window->get("mainContentContainer");
    GtkWidget* mainBox = window->get("mainContainerBox");
    GtkWidget* rail = window->get("tbLeft1Scroller");
    gtk_window_resize(GTK_WINDOW(window->getWindow()), 1280, 720);
    window->show(nullptr);
    settle();
    if (const char* captureDir = g_getenv("UTN_VISUAL_CAPTURE_DIR")) {
        g_mkdir_with_parents(captureDir, 0755);
        const bool sidebarWasVisible = settings->isSidebarVisible();
        window->setSidebarVisible(false);
        window->getFloatingToolbox()->hide();
        window->getPdfToolbox()->hide();
        settle();
        const auto originalTheme = settings->getThemeVariant();
        const std::array<std::pair<ThemeVariant, const char*>, 3> captures{{
                {THEME_VARIANT_FORCE_LIGHT, "utn-visual-polish-light.png"},
                {THEME_VARIANT_FORCE_DARK, "utn-visual-polish-dark.png"},
                {THEME_VARIANT_HIGH_CONTRAST, "utn-visual-polish-high-contrast.png"},
        }};
        for (const auto& [variant, name]: captures) {
            settings->setThemeVariant(variant);
            window->updateColorscheme();
            settle();
            gchar* path = g_build_filename(captureDir, name, nullptr);
            captureWindow(window->getWindow(), path);
            g_free(path);
        }
        settings->setThemeVariant(originalTheme);
        window->updateColorscheme();
        settle();
        g_object_set_data(G_OBJECT(window->getWindow()), "utn-capture-sidebar-visible",
                          GINT_TO_POINTER(sidebarWasVisible));
    }

    ASSERT_EQ(gtk_widget_get_parent(paned), mainBox);
    ASSERT_EQ(gtk_widget_get_parent(canvasContainer), paned);
    gtk_paned_set_position(GTK_PANED(paned), 340);
    settle();
    GdkEvent* release = gdk_event_new(GDK_BUTTON_RELEASE);
    gboolean handled = FALSE;
    g_signal_emit_by_name(paned, "button-release-event", release, &handled);
    gdk_event_free(release);
    int persistedWidth = 0;
    ASSERT_TRUE(settings->getCustomElement("utn").getInt("inspectorWidth", persistedWidth));
    EXPECT_EQ(persistedWidth, 340);
    gtk_window_resize(GTK_WINDOW(window->getWindow()), 1024, 768);
    settle();
    const int available = gtk_widget_get_allocated_width(paned);
    EXPECT_GE(gtk_paned_get_position(GTK_PANED(paned)), 150);
    EXPECT_LE(gtk_paned_get_position(GTK_PANED(paned)), std::max(150, available - 520));
    gtk_window_resize(GTK_WINDOW(window->getWindow()), 1280, 720);
    settle();
    EXPECT_EQ(gtk_paned_get_position(GTK_PANED(paned)), 340);
    window->setTeacherWorkspace(1);
    EXPECT_EQ(window->getTeacherWorkspace(), 1);
    EXPECT_EQ(gtk_widget_get_parent(canvasContainer), mainBox);
    EXPECT_FALSE(gtk_widget_get_visible(paned));
    EXPECT_TRUE(gtk_widget_get_visible(rail));
    if (const char* captureDir = g_getenv("UTN_VISUAL_CAPTURE_DIR")) {
        settle();
        gchar* path = g_build_filename(captureDir, "utn-visual-polish-rail-only.png", nullptr);
        captureWindow(window->getWindow(), path);
        g_free(path);
    }

    control->selectTool(TOOL_ERASER);
    EXPECT_EQ(window->getTeacherWorkspace(), 1);
    EXPECT_EQ(tools->getPenSize(), TOOL_SIZE_THICK);
    control->selectTool(TOOL_PEN);
    EXPECT_EQ(tools->getColor(), penColour);

    window->setTeacherWorkspace(2);
    EXPECT_FALSE(gtk_widget_get_visible(rail));
    EXPECT_EQ(gtk_widget_get_parent(canvasContainer), mainBox);
    if (const char* captureDir = g_getenv("UTN_VISUAL_CAPTURE_DIR")) {
        settle();
        gchar* path = g_build_filename(captureDir, "utn-visual-polish-canvas-only.png", nullptr);
        captureWindow(window->getWindow(), path);
        g_free(path);
    }
    window->setTeacherWorkspace(0);
    settle();
    EXPECT_EQ(gtk_widget_get_parent(canvasContainer), paned);
    EXPECT_TRUE(gtk_widget_get_visible(rail));
    EXPECT_EQ(gtk_paned_get_position(GTK_PANED(paned)), 340);

    int persistedMode = -1;
    ASSERT_TRUE(settings->getCustomElement("utn").getInt("workspaceTools", persistedMode));
    EXPECT_EQ(persistedMode, 0);
    control->selectTool(TOOL_ERASER);
    EXPECT_EQ(tools->getToolType(), TOOL_ERASER);
    if (g_getenv("UTN_VISUAL_CAPTURE_DIR")) {
        const bool sidebarWasVisible = GPOINTER_TO_INT(
                g_object_get_data(G_OBJECT(window->getWindow()), "utn-capture-sidebar-visible"));
        window->setSidebarVisible(sidebarWasVisible);
    }
}

TEST_F(ClassroomWorkflow, studentWindowPrivacyFreezeBlankNavigationAndReopen) {
    auto page = control->getCurrentPage();
    addMarker(page->getSelectedLayer(), 100);
    control->startAnswersLayer();
    addMarker(page->getSelectedLayer(), 200);
    control->getLayerController()->addNewLayer(false);
    auto* notes = page->getSelectedLayer();
    notes->setAudience(LayerAudience::TeacherOnly);
    addMarker(notes, 300);
    control->setAnswersRevealed(false, false);

    GList* initialOverlays = gtk_container_get_children(GTK_CONTAINER(window->get("mainOverlay")));
    const auto initialOverlayCount = g_list_length(initialOverlays);
    g_list_free(initialOverlays);
    auto students = std::make_unique<ToolStudentView>("CLASSROOM_STUDENTS", control.get());
    auto pill = students->createItem(true);
    auto* menu = GTK_WIDGET(gtk_menu_button_get_popover(GTK_MENU_BUTTON(pill.get())));
    auto click = [&](const char* label) {
        auto* button = buttonNamed(menu, label);
        EXPECT_NE(button, nullptr) << label;
        if (button) {
            gtk_button_clicked(GTK_BUTTON(button));
            settle();
        }
    };
    for (int cycle = 0; cycle < 3; ++cycle) {
        click("Open on the projector");
        auto* student = studentWindow();
        ASSERT_NE(student, nullptr);
        EXPECT_EQ(students->getState(), ToolStudentView::State::Live);
        EXPECT_EQ(studentPixel(student, 100, 100), 0U);
        EXPECT_EQ(studentPixel(student, 100, 200), 0xFFFFFFU);
        EXPECT_EQ(studentPixel(student, 100, 300), 0xFFFFFFU);
        control->setAnswersRevealed(true, false);
        settle();
        EXPECT_EQ(studentPixel(student, 100, 200), 0U);
        click("Freeze this page");
        EXPECT_EQ(students->getState(), ToolStudentView::State::Frozen);
        control->setAnswersRevealed(false, false);
        control->firePageSelected(1);
        settle();
        // Hiding answers while frozen must not leave the revealed frame on the projector: the screen goes blank
        EXPECT_EQ(students->getState(), ToolStudentView::State::Blank);
        EXPECT_EQ(studentPixel(student, 100, 200), 0U);
        EXPECT_EQ(studentPixel(student, 300, 700), 0U) << "blank, not the old page";
        click("Show the page again");
        EXPECT_EQ(students->getState(), ToolStudentView::State::Live) << "the frozen frame is gone, not restored";
        EXPECT_EQ(studentPixel(student, 100, 100), 0xFFFFFFU) << "Page two is empty";
        click("Blank screen");
        EXPECT_EQ(students->getState(), ToolStudentView::State::Blank);
        EXPECT_EQ(studentPixel(student, 100, 100), 0U);
        EXPECT_EQ(studentPixel(student, 100, 400), 0U);
        click("Show the page again");
        EXPECT_EQ(students->getState(), ToolStudentView::State::Live);
        EXPECT_EQ(studentPixel(student, 100, 100), 0xFFFFFFU) << "Page two is empty";
        control->firePageSelected(0);
        settle();
        EXPECT_EQ(studentPixel(student, 100, 100), 0U);
        EXPECT_EQ(studentPixel(student, 100, 200), 0xFFFFFFU);
        gtk_window_resize(GTK_WINDOW(student), 640, 480);
        settle();
        EXPECT_EQ(studentPixel(student, 100, 100), 0U);
        EXPECT_EQ(studentPixel(student, 100, 300), 0xFFFFFFU);
        click("Close Student View");
        EXPECT_EQ(studentWindow(), nullptr);
    }
    // On a single display this creates the teacher's blank cover; on extended displays it opens a blank window.
    click("Blank screen");
    EXPECT_EQ(students->getState(), ToolStudentView::State::Blank);
    gtk_widget_destroy(pill.get());
    pill.reset();
    control->notifyCanvasChanged(page.get());
    students.reset();
    settle();
    GList* overlays = gtk_container_get_children(GTK_CONTAINER(window->get("mainOverlay")));
    EXPECT_EQ(g_list_length(overlays), initialOverlayCount) << "Student overlays must not outlive their tool";
    g_list_free(overlays);
}

TEST_F(ClassroomWorkflow, hidingDuringAnswerEditKeepsContentOnPrivateLayer) {
    control->startAnswersLayer();
    auto page = control->getCurrentPage();
    auto* answers = page->getSelectedLayer();
    auto box = std::make_unique<Text>();
    box->setText("Secret answer 42");
    box->setBoxEnabled(true);
    box->move(100, 100);
    answers->addElement(std::move(box));
    control->selectTextMode(TextMode::AnswerBox);
    ASSERT_NE(editAt(101, 101), nullptr);
    ASSERT_EQ(control->getTextEditor()->getTextElement()->getText(), "Secret answer 42");
    control->getTextEditor()->setUnderline(true);
    control->setAnswersRevealed(false, false);
    control->clearSelectionEndText();
    EXPECT_FALSE(answers->isVisible());
    ASSERT_EQ(answers->getElements().size(), 1);
    EXPECT_TRUE(static_cast<Text*>(answers->getElements().front().get())->isUnderlined());
    EXPECT_TRUE(page->getLayers().front()->getElements().empty());
    control->getUndoRedoHandler()->undo();
    EXPECT_FALSE(static_cast<Text*>(answers->getElements().front().get())->isUnderlined());
    control->getUndoRedoHandler()->redo();
    EXPECT_TRUE(static_cast<Text*>(answers->getElements().front().get())->isUnderlined());
}

TEST_F(ClassroomWorkflow, hiddenAnswerEditNavigationDeleteHistoryAndReload) {
    control->startAnswersLayer();
    auto page = control->getCurrentPage();
    auto* answers = page->getSelectedLayer();
    auto box = std::make_unique<Text>();
    box->setText("Answer: \xCF\x80 = 3.14; \xE7\xAD\x94\xE6\xA1\x88");
    const auto content = box->getText();
    box->setBoxEnabled(true);
    box->move(100, 100);
    answers->addElement(std::move(box));
    control->setAnswersRevealed(false, false);
    // Simulate the teacher-only ghost editing state, without claiming monitor-placement validation.
    control->setGhostHiddenAnswers(true);
    control->getLayerController()->switchToLay(2);
    control->selectTextMode(TextMode::AnswerBox);
    ASSERT_NE(editAt(101, 101), nullptr);
    ASSERT_EQ(control->getTextEditor()->getTextElement()->getText(), content);
    control->getTextEditor()->setLineSpacing(1.5);
    control->firePageSelected(1);
    ASSERT_EQ(control->getTextEditor(), nullptr);
    ASSERT_EQ(answers->getElements().size(), 1);
    EXPECT_DOUBLE_EQ(static_cast<Text*>(answers->getElements().front().get())->getLineSpacing(), 1.5);
    EXPECT_FALSE(answers->isVisible());

    control->firePageSelected(0);
    control->getLayerController()->switchToLay(2);
    control->selectTextMode(TextMode::AnswerBox);
    ASSERT_NE(editAt(101, 101), nullptr);
    control->deleteEditedAnswerBox();
    EXPECT_TRUE(answers->getElements().empty());
    control->getUndoRedoHandler()->undo();
    ASSERT_EQ(answers->getElements().size(), 1);
    control->getUndoRedoHandler()->redo();
    EXPECT_TRUE(answers->getElements().empty());
    control->getUndoRedoHandler()->undo();
    ASSERT_EQ(answers->getElements().size(), 1);

    const auto file = Util::getTmpDirSubfolder() / "classroom-answer-edit.xopp";
    SaveHandler saver;
    saver.prepareSave(control->getDocument(), file);
    saver.saveTo(file);
    ASSERT_TRUE(saver.getErrorMessage().empty()) << saver.getErrorMessage();
    auto reopened = LoadHandler{}.loadDocument(file);
    ASSERT_NE(reopened, nullptr);
    auto* savedAnswers = reopened->getPage(0)->getLayers().at(1);
    EXPECT_EQ(savedAnswers->getAudience(), LayerAudience::Answers);
    EXPECT_FALSE(savedAnswers->isVisible());
    ASSERT_EQ(savedAnswers->getElements().size(), 1);
    auto* savedBox = dynamic_cast<Text*>(savedAnswers->getElements().front().get());
    ASSERT_NE(savedBox, nullptr);
    EXPECT_EQ(savedBox->getText(), content);
    EXPECT_TRUE(savedBox->isBoxEnabled());
    EXPECT_DOUBLE_EQ(savedBox->getLineSpacing(), 1.5);
}

TEST_F(ClassroomWorkflow, mixedLessonSelectionMoveResizeDeleteHistoryAndSave) {
    auto lesson = LoadHandler{}.loadDocument(fs::path(GET_TESTFILE(u8"utn/lesson-roundtrip.xopp")));
    ASSERT_NE(lesson, nullptr);
    control->getDocument()->lock();
    *control->getDocument() = *lesson;
    control->getDocument()->unlock();
    control->DocumentHandler::firePageSelected(0);
    auto page = control->getCurrentPage();
    auto* layer = page->getSelectedLayer();
    ASSERT_EQ(layer->getElements().size(), 6);
    std::vector<const Element*> order;
    std::vector<xoj::util::Rectangle<double>> bounds;
    for (const auto& element: layer->getElements()) {
        order.push_back(element.get());
        bounds.push_back(element->getBoundingBox());
    }
    control->selectAllOnPage();
    auto* selection = window->getXournal()->getSelection();
    ASSERT_NE(selection, nullptr);
    EXPECT_EQ(selection->getElementsView().size(), 6);
    selection->moveSelection(12, 8, true);
    control->clearSelection();
    ASSERT_EQ(layer->getElements().size(), 6);
    for (size_t i = 0; i < order.size(); ++i) {
        EXPECT_EQ(layer->getElements()[i].get(), order[i]);
        EXPECT_NEAR(order[i]->getBoundingBox().x, bounds[i].x + 12, 0.01);
        EXPECT_NEAR(order[i]->getBoundingBox().y, bounds[i].y + 8, 0.01);
    }
    control->getUndoRedoHandler()->undo();
    for (size_t i = 0; i < order.size(); ++i) {
        EXPECT_NEAR(order[i]->getBoundingBox().x, bounds[i].x, 0.01);
        EXPECT_NEAR(order[i]->getBoundingBox().y, bounds[i].y, 0.01);
    }
    control->getUndoRedoHandler()->redo();
    control->selectAllOnPage();
    selection = window->getXournal()->getSelection();
    ASSERT_NE(selection, nullptr);
    const auto rect = selection->getRect();
    const double zoom = selection->getView()->getZoom();
    selection->mouseDown(CURSOR_SELECTION_BOTTOM_RIGHT, (rect.x + rect.width) * zoom, (rect.y + rect.height) * zoom);
    selection->mouseMove((rect.x + rect.width * 1.1) * zoom, (rect.y + rect.height * 1.1) * zoom, false);
    selection->mouseUp();
    control->clearSelection();
    const auto resizedWidth = order.front()->getBoundingBox().width;
    EXPECT_GT(resizedWidth, bounds.front().width);
    control->getUndoRedoHandler()->undo();
    EXPECT_NEAR(order.front()->getBoundingBox().width, bounds.front().width, 0.01);
    control->getUndoRedoHandler()->redo();
    EXPECT_NEAR(order.front()->getBoundingBox().width, resizedWidth, 0.01);
    control->selectAllOnPage();
    control->deleteSelection();
    EXPECT_TRUE(layer->getElements().empty());
    control->getUndoRedoHandler()->undo();
    ASSERT_EQ(layer->getElements().size(), 6);
    for (size_t i = 0; i < order.size(); ++i) {
        EXPECT_EQ(layer->getElements()[i].get(), order[i]);
    }
    control->getUndoRedoHandler()->redo();
    EXPECT_TRUE(layer->getElements().empty());
    control->getUndoRedoHandler()->undo();
    control->selectTool(TOOL_PEN);
    control->selectHighlighter(false);
    control->selectTool(TOOL_SELECT_OBJECT);
    const auto file = Util::getTmpDirSubfolder() / "classroom-mixed-edit.xopp";
    SaveHandler saver;
    saver.prepareSave(control->getDocument(), file);
    saver.saveTo(file);
    ASSERT_TRUE(saver.getErrorMessage().empty()) << saver.getErrorMessage();
    auto reopened = LoadHandler{}.loadDocument(file);
    ASSERT_NE(reopened, nullptr);
    const auto& saved = reopened->getPage(0)->getLayers().front()->getElements();
    ASSERT_EQ(saved.size(), order.size());
    for (size_t i = 0; i < order.size(); ++i) {
        EXPECT_EQ(saved[i]->getType(), order[i]->getType());
        EXPECT_NEAR(saved[i]->getBoundingBox().x, order[i]->getBoundingBox().x, 0.01);
        EXPECT_NEAR(saved[i]->getBoundingBox().y, order[i]->getBoundingBox().y, 0.01);
        EXPECT_NEAR(saved[i]->getBoundingBox().width, order[i]->getBoundingBox().width, 0.01);
    }
    EXPECT_FALSE(reopened->getPage(0)->getLayers().at(1)->isVisible());
}

namespace {
bool pressEscape(MainWindow* window) {
    GdkEvent* event = gdk_event_new(GDK_KEY_PRESS);
    event->key.keyval = GDK_KEY_Escape;
    gboolean handled = FALSE;
    g_signal_emit_by_name(window->getWindow(), "key-press-event", event, &handled);
    gdk_event_free(event);
    return handled;
}

void turnOnOverlay(Control* control, bool spotlight) {
    auto* tools = control->getToolHandler();
    control->selectTool(TOOL_HAND);
    if (spotlight) {
        tools->setSpotlightEnabled(true);
    } else {
        tools->setCurtainEnabled(true);
    }
    tools->fireToolChanged();
    settle();
}
}  // namespace

// M2-01: the active Spotlight/Curtain is named on screen, and Escape, the banner button, a tool switch and the
// Present menu entry all leave a usable page.
TEST_F(ClassroomWorkflow, spotlightAndCurtainAreNamedOnScreenAndEveryExitWorks) {
    window->show(nullptr);
    auto* tools = control->getToolHandler();
    EXPECT_FALSE(window->isPresentationBannerVisible());

    for (bool spotlight: {true, false}) {
        const std::string expected = spotlight ? "Spotlight" : "Curtain";

        turnOnOverlay(control.get(), spotlight);
        ASSERT_TRUE(window->isPresentationBannerVisible());
        EXPECT_NE(window->getPresentationBannerText().find(expected), std::string::npos);

        // Escape exits, and is consumed
        EXPECT_TRUE(pressEscape(window.get()));
        EXPECT_FALSE(tools->isSpotlightEnabled());
        EXPECT_FALSE(tools->isCurtainEnabled());
        EXPECT_FALSE(window->isPresentationBannerVisible());

        // Any ordinary tool switch leaves a safe page and hides the banner
        turnOnOverlay(control.get(), spotlight);
        control->selectTool(TOOL_PEN);
        EXPECT_FALSE(tools->isSpotlightEnabled());
        EXPECT_FALSE(tools->isCurtainEnabled());
        EXPECT_FALSE(window->isPresentationBannerVisible());

        // The explicit exit function used by the banner button and the menu
        turnOnOverlay(control.get(), spotlight);
        window->exitPresentationOverlay();
        EXPECT_FALSE(tools->isSpotlightEnabled() || tools->isCurtainEnabled());
        EXPECT_FALSE(window->isPresentationBannerVisible());
    }
}

// Choosing the active Spotlight/Curtain entry in the Present menu must do what its label promises: turn it off.
TEST_F(ClassroomWorkflow, choosingTheActivePresentEntryAgainTurnsItOffAndEveryIndicatorFollows) {
    window->show(nullptr);
    ToolPresentationKit kit("PRESENTATION_KIT", control.get(), IconNameHelper(control->getSettings()));
    auto item = kit.createItem(true);
    auto* menu = GTK_WIDGET(gtk_menu_button_get_popover(GTK_MENU_BUTTON(item.get())));
    auto* spotlight = buttonNamed(menu, "Spotlight");
    auto* curtain = buttonNamed(menu, "Curtain");
    auto* exit = buttonNamed(menu, "Show the whole page");
    ASSERT_TRUE(spotlight && curtain && exit);
    auto* tools = control->getToolHandler();
    auto hasActiveClass = [&] {
        return gtk_style_context_has_class(gtk_widget_get_style_context(item.get()), "utn-present-active");
    };
    auto expectOff = [&](const char* when) {
        SCOPED_TRACE(when);
        EXPECT_FALSE(tools->isSpotlightEnabled());
        EXPECT_FALSE(tools->isCurtainEnabled());
        EXPECT_FALSE(window->isPresentationBannerVisible());
        EXPECT_FALSE(hasActiveClass());
        EXPECT_FALSE(gtk_widget_get_sensitive(exit));
        EXPECT_STREQ(gtk_button_get_label(GTK_BUTTON(spotlight)), "Spotlight");
        EXPECT_STREQ(gtk_button_get_label(GTK_BUTTON(curtain)), "Curtain");
    };
    auto click = [&](GtkWidget* w) {
        gtk_button_clicked(GTK_BUTTON(w));
        settle();
    };

    expectOff("initially");
    for (bool isSpotlight: {true, false}) {
        auto* self = isSpotlight ? spotlight : curtain;
        auto* other = isSpotlight ? curtain : spotlight;
        const std::string name = isSpotlight ? "Spotlight" : "Curtain";

        click(self);
        EXPECT_EQ(tools->isSpotlightEnabled(), isSpotlight);
        EXPECT_EQ(tools->isCurtainEnabled(), !isSpotlight);
        EXPECT_TRUE(window->isPresentationBannerVisible());
        EXPECT_NE(window->getPresentationBannerText().find(name), std::string::npos);
        EXPECT_TRUE(hasActiveClass());
        EXPECT_TRUE(gtk_widget_get_sensitive(exit));
        EXPECT_STREQ(gtk_button_get_label(GTK_BUTTON(self)), (name + " is on: choose to turn off").c_str());

        click(self);
        expectOff("same entry chosen again");

        // Switching straight from the other overlay leaves only the newly chosen one on
        click(other);
        click(self);
        EXPECT_EQ(tools->isSpotlightEnabled(), isSpotlight);
        EXPECT_EQ(tools->isCurtainEnabled(), !isSpotlight);
        EXPECT_STREQ(gtk_button_get_label(GTK_BUTTON(other)), isSpotlight ? "Curtain" : "Spotlight");
        EXPECT_TRUE(window->isPresentationBannerVisible());

        // Explicit exit and Escape still work from here
        click(exit);
        expectOff("explicit exit");
        click(self);
        EXPECT_TRUE(pressEscape(window.get()));
        settle();
        expectOff("Escape");
    }

    gtk_widget_destroy(item.get());
    item.reset();
    tools->fireToolChanged();
    settle();
}

namespace {
bool answersShowing(Control* control) {
    auto page = control->getCurrentPage();
    for (const Layer* layer: page->getLayersView()) {
        if (layer->getAudience() == LayerAudience::Answers && layer->isVisible()) {
            return true;
        }
    }
    return false;
}
}  // namespace

// Revealing everything to a class that may be watching must not be one accidental tap, and every reveal can be undone.
TEST_F(ClassroomWorkflow, revealAllNeedsASecondPressOnlyWhileTheClassMayWatchAndCanBeHiddenAgain) {
    window->show(nullptr);
    auto page = control->getCurrentPage();
    control->startAnswersLayer();
    addMarker(page->getSelectedLayer(), 200);
    control->setAnswersRevealed(false, true);
    ASSERT_FALSE(answersShowing(control.get()));

    constexpr int64_t second = 1000 * 1000;
    // Nobody watching: one press does it, and the toast offers the way back
    control->setStudentsMayWatch(false);
    utn::cancelRevealAllConfirmation();
    EXPECT_EQ(utn::revealAnswersFromUi(control.get(), true, 10 * second), utn::RevealOutcome::Revealed);
    EXPECT_TRUE(answersShowing(control.get()));
    EXPECT_NE(window->getToast()->currentMessage().find("All answers"), std::string::npos);
    utn::hideAnswersFromUi(control.get());
    EXPECT_FALSE(answersShowing(control.get()));

    // The class may be watching: the first press only arms
    control->setStudentsMayWatch(true);
    EXPECT_EQ(utn::revealAnswersFromUi(control.get(), true, 20 * second), utn::RevealOutcome::NeedsConfirmation);
    EXPECT_FALSE(answersShowing(control.get())) << "the first press must not show anything";
    EXPECT_NE(window->getToast()->currentMessage().find("again"), std::string::npos);
    // A confirmation that comes too late is just another first press
    EXPECT_EQ(utn::revealAnswersFromUi(control.get(), true, 20 * second + utn::REVEAL_ALL_CONFIRM_MICROSECONDS + 1),
              utn::RevealOutcome::NeedsConfirmation);
    EXPECT_FALSE(answersShowing(control.get()));
    // In time, it goes ahead
    EXPECT_EQ(utn::revealAnswersFromUi(control.get(), true, 20 * second + utn::REVEAL_ALL_CONFIRM_MICROSECONDS + 2 * second),
              utn::RevealOutcome::Revealed);
    EXPECT_TRUE(answersShowing(control.get()));

    // Hide again, then the single page reveal is never gated
    utn::hideAnswersFromUi(control.get());
    EXPECT_EQ(utn::revealAnswersFromUi(control.get(), false, 100 * second), utn::RevealOutcome::Revealed);
    EXPECT_TRUE(answersShowing(control.get()));

    // Any other action disarms nothing by itself, but cancelling forgets a pending first press
    utn::hideAnswersFromUi(control.get());
    EXPECT_EQ(utn::revealAnswersFromUi(control.get(), true, 200 * second), utn::RevealOutcome::NeedsConfirmation);
    utn::cancelRevealAllConfirmation();
    EXPECT_EQ(utn::revealAnswersFromUi(control.get(), true, 201 * second), utn::RevealOutcome::NeedsConfirmation);
    EXPECT_FALSE(answersShowing(control.get()));
    control->setStudentsMayWatch(false);
}

// Hidden answers shown faintly on the teacher canvas are a leak when that window is screen-shared or recorded, so the
// teacher has to ask for them. Off by default; the choice is remembered; with one display it never applies.
TEST_F(ClassroomWorkflow, ghostedHiddenAnswersAreOptInAndRememberedAndNeverShownOnASingleDisplay) {
    window->show(nullptr);
    control->getSettings()->getCustomElement("utn").setBool("ghostHiddenAnswers", false);

    auto students = std::make_unique<ToolStudentView>("CLASSROOM_STUDENTS", control.get());
    auto pill = students->createItem(true);
    auto* menu = GTK_WIDGET(gtk_menu_button_get_popover(GTK_MENU_BUTTON(pill.get())));
    auto* toggle = buttonNamed(menu, "Show hidden answers faintly to me");
    ASSERT_NE(toggle, nullptr);
    EXPECT_FALSE(gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(toggle))) << "off until the teacher asks";
    EXPECT_FALSE(control->isGhostHiddenAnswers());

    gtk_button_clicked(GTK_BUTTON(toggle));
    bool wanted = false;
    control->getSettings()->getCustomElement("utn").getBool("ghostHiddenAnswers", wanted);
    EXPECT_TRUE(wanted) << "the choice is stored";
    // The class watches the same display (or there is only one), so nothing may be ghosted, whatever was asked
    EXPECT_FALSE(control->isGhostHiddenAnswers());

    gtk_button_clicked(GTK_BUTTON(toggle));
    control->getSettings()->getCustomElement("utn").getBool("ghostHiddenAnswers", wanted);
    EXPECT_FALSE(wanted);

    gtk_widget_destroy(pill.get());
    pill.reset();
    students.reset();
    settle();
}

namespace {
/// The real compatibility export, started without the file chooser
class CompatibilityExportForTest: public CustomExportJob {
public:
    explicit CompatibilityExportForTest(Control* control): CustomExportJob(control) {}
    void configure(const fs::path& target) {
        filepath = target;
        exportTypeXoj = true;
    }
    const std::string& error() const { return lastError; }

protected:
    ~CompatibilityExportForTest() override = default;
};
}  // namespace

// The compatibility (.xoj) export used to hold the shared Document lock while it drew every erased stroke and compressed
// the file, so the next pen-up waited for all of it. Run the real job on a dense lesson while this thread edits, erases and
// autosaves, and measure how long editing waits for the lock.
TEST_F(ClassroomWorkflow, compatibilityExportDoesNotBlockEditingErasingOrAutosave) {
    requireWorkingTempFolder();
    window->show(nullptr);
    settle();
    ensureSchedulerRunning(control.get());
    auto page = control->getCurrentPage();
    Document* doc = control->getDocument();
    for (int i = 0; i < 160; ++i) {
        auto bar = std::make_unique<Stroke>();
        bar->setWidth(8);
        bar->addPoint(Point(20, 10 + (i % 75) * 9.0));
        bar->addPoint(Point(560, 10 + (i % 75) * 9.0));
        bar->setErasures({{{250, 0}, {250, 780}, 12, {}}, {{400, 0}, {400, 780}, 12, {}}});
        page->getSelectedLayer()->addElement(std::move(bar));
    }
    const auto target = Util::getTmpDirSubfolder() / "compat-export.xoj";
    fs::remove(target);

    auto* job = new CompatibilityExportForTest(control.get());
    job->configure(target);
    const size_t rasterBefore = utn::eraser::rasterizeMaskedStrokeCallCount();
#ifdef UTN_LOCK_AUDIT
    const unsigned nestedBefore = utn::lockaudit::nestedViolations();
#endif
    std::atomic<bool> done{false};
    std::thread exporting([&] {
        job->run();
        done = true;
    });
    // This thread is the "teacher": strokes, erasing through the eraser handler, autosave jobs, undo
    control->selectTool(TOOL_ERASER);
    control->getToolHandler()->setEraserType(ERASER_TYPE_DEFAULT);
    auto* view = window->getXournal()->getViewFor(0);
    ASSERT_NE(view, nullptr);
    double worstMs = 0;
    int rounds = 0;
    while (!done && rounds < 400) {
        const auto start = std::chrono::steady_clock::now();
        PositionInputData pos{};
        pos.pressure = 0.5;
        pos.x = (100 + (rounds % 50) * 5) * view->getZoom();
        pos.y = (60 + (rounds % 40) * 12) * view->getZoom();
        view->onButtonPressEvent(pos);
        for (int step = 1; step <= 6; ++step) {
            pos.x += 8 * view->getZoom();
            pos.timestamp += 8;
            view->onMotionNotifyEvent(pos);
        }
        view->onButtonReleaseEvent(pos);  // takes the exclusive lock several times
        if (rounds % 10 == 9) {
            auto* autosave = new AutosaveJob(control.get());
            control->getScheduler()->addJob(autosave, JOB_PRIORITY_NONE);
            autosave->unref();
        }
        worstMs = std::max(worstMs, std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count());
        for (int event = 0; event < 20 && gtk_events_pending(); ++event) {
            gtk_main_iteration_do(false);
        }
        ++rounds;
    }
    exporting.join();
    control->unblock();  // the job blocks the application when it is made; a job run directly never unblocks it
    settle();
    RecordProperty("export_overlap_rounds", rounds);
    RecordProperty("export_worst_edit_ms", std::to_string(worstMs));
    EXPECT_GT(rounds, 3) << "the export finished before editing overlapped it";
    EXPECT_TRUE(job->error().empty()) << job->error();
    EXPECT_GE(utn::eraser::rasterizeMaskedStrokeCallCount() - rasterBefore, 160U) << "the export drew the erased strokes";
    // 160 erased strokes take far longer than this to draw, so an edit that waited for them would exceed it
    EXPECT_LT(worstMs, 250.0) << "an edit waited for the export";
#ifdef UTN_LOCK_AUDIT
    EXPECT_EQ(utn::lockaudit::nestedViolations(), nestedBefore);
#endif
    ASSERT_TRUE(fs::exists(target)) << "the export wrote no file";
    EXPECT_FALSE(fs::exists(target.string() + ".utn-tmp"));
    auto reopened = LoadHandler{}.loadDocument(target);
    fs::remove(target);
    ASSERT_TRUE(reopened) << "the exported file does not open";
    EXPECT_GE(reopened->getPageCount(), 1U);
    job->unref();
    control->selectTool(TOOL_PEN);
    (void)doc;
}

// A frozen frame is a picture of the moment it was taken. If the teacher then hides answers (or makes a layer private or
// invisible, or covers the page), the picture must not stay on the projector: the screen goes blank at once. Each case
// checks the real Student View pixels, not just the state flag.
TEST_F(ClassroomWorkflow, frozenStudentViewNeverKeepsWhatStudentsMayNoLongerSee) {
    window->show(nullptr);
    auto page = control->getCurrentPage();
    addMarker(page->getSelectedLayer(), 100);
    control->startAnswersLayer();
    Layer* answers = page->getSelectedLayer();
    addMarker(answers, 200);

    auto students = std::make_unique<ToolStudentView>("CLASSROOM_STUDENTS", control.get());
    auto pill = students->createItem(true);
    auto* menu = GTK_WIDGET(gtk_menu_button_get_popover(GTK_MENU_BUTTON(pill.get())));
    auto click = [&](const char* label) {
        auto* button = buttonNamed(menu, label);
        ASSERT_NE(button, nullptr) << label;
        gtk_button_clicked(GTK_BUTTON(button));
        settle();
    };
    click("Open on the projector");
    auto* student = studentWindow();
    ASSERT_NE(student, nullptr);

    // Live, answers revealed, then frozen: the picture holds the answer marker (black) on a white page
    auto freezeShowingAnswers = [&]() {
        if (students->getState() == ToolStudentView::State::Blank) {
            click("Show the page again");
        }
        if (students->getState() == ToolStudentView::State::Frozen) {
            click("Resume live view");
        }
        window->exitPresentationOverlay();
        answers->setAudience(LayerAudience::Answers);
        answers->setVisible(true);
        control->firePageChanged(0);  // set-up only: shows the revealed answer on the live frame
        settle();
        click("Freeze this page");
        ASSERT_EQ(students->getState(), ToolStudentView::State::Frozen);
        ASSERT_EQ(studentPixel(student, 100, 200), 0U) << "the answer is on the frozen picture";
        ASSERT_EQ(studentPixel(student, 300, 700), 0xFFFFFFU);
    };
    auto expectBlankNow = [&](const char* what) {
        settle();
        EXPECT_EQ(students->getState(), ToolStudentView::State::Blank) << what;
        EXPECT_EQ(studentPixel(student, 300, 700), 0U) << what << ": the old picture must be gone";
        EXPECT_EQ(studentPixel(student, 100, 200), 0U) << what;
    };

    freezeShowingAnswers();
    control->setAnswersRevealed(false, false);
    expectBlankNow("hide answers on this page");

    freezeShowingAnswers();
    control->setAnswersRevealed(false, true);
    expectBlankNow("hide all answers");

    // No page-change notification is injected below: the layer UI does not send one, so neither may the test
    freezeShowingAnswers();
    answers->setAudience(LayerAudience::TeacherOnly);
    expectBlankNow("layer became teacher-only");

    freezeShowingAnswers();
    control->getLayerController()->setLayerVisible(1 + static_cast<Layer::Index>(std::find(page->getLayers().begin(), page->getLayers().end(), answers) - page->getLayers().begin()), false);
    expectBlankNow("layer hidden");

    freezeShowingAnswers();
    turnOnOverlay(control.get(), false);
    expectBlankNow("Curtain switched on");
    window->exitPresentationOverlay();
    settle();
    EXPECT_EQ(students->getState(), ToolStudentView::State::Blank) << "leaving Curtain does not bring the old frame back";

    freezeShowingAnswers();
    turnOnOverlay(control.get(), true);
    expectBlankNow("Spotlight switched on");

    // Blank on top of a frozen frame, answers hidden underneath, then "Show the page again": live and sanitised, never
    // the frozen picture with the answers in it
    freezeShowingAnswers();
    click("Blank screen");
    ASSERT_EQ(students->getState(), ToolStudentView::State::Blank);
    control->setAnswersRevealed(false, false);
    settle();
    click("Show the page again");
    EXPECT_EQ(students->getState(), ToolStudentView::State::Live);
    EXPECT_EQ(studentPixel(student, 100, 200), 0xFFFFFFU) << "the hidden answer is not on the projector";
    EXPECT_EQ(studentPixel(student, 100, 100), 0U) << "ordinary ink is";

    // Changes that give students nothing new keep the freeze: new public ink does not appear, and nothing goes blank
    freezeShowingAnswers();
    addMarker(page->getLayers().front(), 500);
    control->firePageChanged(0);
    settle();
    EXPECT_EQ(students->getState(), ToolStudentView::State::Frozen);
    EXPECT_EQ(studentPixel(student, 100, 500), 0xFFFFFFU) << "the frozen picture does not follow the page";
    EXPECT_EQ(studentPixel(student, 100, 200), 0U);
    // Resizing the projector window rescales the picture; it neither blanks nor refreshes it
    gtk_window_resize(GTK_WINDOW(student), 800, 600);
    settle();
    EXPECT_EQ(students->getState(), ToolStudentView::State::Frozen);

    gtk_widget_destroy(student);
    gtk_widget_destroy(pill.get());
    pill.reset();
    students.reset();
    settle();
}

namespace {
/// The student window rig of the frozen-frame tests: answers on their own layer, Student View open on the "projector"
struct FrozenRig {
    Control* control;
    MainWindow* window;
    PageRef page;
    Layer* answers = nullptr;
    std::unique_ptr<ToolStudentView> students;
    xoj::util::WidgetSPtr pill;
    GtkWidget* menu = nullptr;
    GtkWidget* student = nullptr;

    FrozenRig(Control* control, MainWindow* window): control(control), window(window) {
        window->show(nullptr);
        page = control->getCurrentPage();
        addMarker(page->getSelectedLayer(), 100);
        control->startAnswersLayer();
        answers = page->getSelectedLayer();
        addMarker(answers, 200);
        students = std::make_unique<ToolStudentView>("CLASSROOM_STUDENTS", control);
        pill = students->createItem(true);
        menu = GTK_WIDGET(gtk_menu_button_get_popover(GTK_MENU_BUTTON(pill.get())));
        click("Open on the projector");
        student = studentWindow();
    }
    ~FrozenRig() {
        if (student) gtk_widget_destroy(student);
        gtk_widget_destroy(pill.get());
        pill.reset();
        students.reset();
        settle();
    }
    void click(const char* label) {
        auto* button = buttonNamed(menu, label);
        ASSERT_NE(button, nullptr) << label;
        gtk_button_clicked(GTK_BUTTON(button));
        settle();
    }
    Layer::Index answersId() const {
        Layer::Index id = 1;
        for (const Layer* layer: page->getLayersView()) {
            if (layer == answers) return id;
            ++id;
        }
        return 0;
    }
    /// Live, answers showing (revealed through the real layer checkbox path), then frozen
    void freezeShowingAnswers() {
        if (students->getState() == ToolStudentView::State::Blank) click("Show the page again");
        if (students->getState() == ToolStudentView::State::Frozen) click("Resume live view");
        window->exitPresentationOverlay();
        answers->setAudience(LayerAudience::Answers);
        control->getLayerController()->setLayerVisible(answersId(), true);
        settle();
        click("Freeze this page");
        ASSERT_EQ(students->getState(), ToolStudentView::State::Frozen);
        ASSERT_EQ(studentPixel(student, 100, 200), 0U) << "the answer is on the frozen picture";
        ASSERT_EQ(studentPixel(student, 300, 700), 0xFFFFFFU);
    }
    /// Checked at once: no settle(), no main loop iteration since the action
    void expectBlankImmediately(const char* what) {
        EXPECT_EQ(students->getState(), ToolStudentView::State::Blank) << what;
        EXPECT_EQ(studentPixel(student, 300, 700), 0U) << what << ": the old picture must be gone";
        EXPECT_EQ(studentPixel(student, 100, 200), 0U) << what << ": the answer must be gone";
        EXPECT_EQ(studentPixel(student, 100, 100), 0U) << what;
    }
};
}  // namespace

// The tests above call model setters and then announce a page change by hand. The real layer checkbox, the "Hide
// answers" button and the Spotlight/Curtain menu entries do not do that, so here every action goes through the same
// function the widget calls, nothing is announced by the test, and the state is read before the main loop runs again.
TEST_F(ClassroomWorkflow, frozenStudentViewIsBlankedByTheRealLayerAndOverlayActions) {
    FrozenRig rig(control.get(), window.get());
    ASSERT_NE(rig.student, nullptr);
    auto* layers = control->getLayerController();

    // 1. the layer checkbox (ShowLayerEntry and the sidebar both call LayerController::setLayerVisible)
    rig.freezeShowingAnswers();
    layers->setLayerVisible(rig.answersId(), false);
    rig.expectBlankImmediately("layer hidden with the layer checkbox");

    // 2. "Hide all layers"
    rig.freezeShowingAnswers();
    layers->hideAllLayer();
    rig.expectBlankImmediately("all layers hidden");
    layers->showAllLayer();

    // 3. Hide answers from the toolbar, the real button handler
    rig.freezeShowingAnswers();
    utn::hideAnswersFromUi(control.get());
    rig.expectBlankImmediately("Hide answers");

    // 4. A layer made teacher-only. No layer widget does this today, so nothing but the model setter runs: the change
    //    must be noticed anyway. First by the draw itself (nothing has been processed yet), then by the state.
    rig.freezeShowingAnswers();
    rig.answers->setAudience(LayerAudience::TeacherOnly);
    EXPECT_EQ(studentPixel(rig.student, 100, 200), 0U) << "painted black before any other event was processed";
    EXPECT_EQ(studentPixel(rig.student, 300, 700), 0U);
    settle();
    EXPECT_EQ(rig.students->getState(), ToolStudentView::State::Blank);

    // 5. The same through the model hide, with a delayed redraw queued first: no timer can bring the old picture back
    rig.freezeShowingAnswers();
    control->firePageChanged(0);  // queues a throttled redraw while the frame is still frozen
    layers->setLayerVisible(rig.answersId(), false);
    rig.expectBlankImmediately("hidden with a redraw already pending");
    for (int i = 0; i < 8; ++i) {
        control->firePageChanged(0);
        settle();
        control->notifyCanvasChanged(rig.page.get());
        g_usleep(60 * 1000);
        settle();
    }
    EXPECT_EQ(rig.students->getState(), ToolStudentView::State::Blank);
    EXPECT_EQ(studentPixel(rig.student, 100, 200), 0U) << "a delayed redraw restored the unsafe frame";
    EXPECT_EQ(studentPixel(rig.student, 300, 700), 0U);

    // 6. Blank, then back to live: sanitised, never the frozen picture
    rig.click("Show the page again");
    EXPECT_EQ(rig.students->getState(), ToolStudentView::State::Live);
    EXPECT_EQ(studentPixel(rig.student, 100, 200), 0xFFFFFFU) << "the hidden answer is not projected";
    EXPECT_EQ(studentPixel(rig.student, 100, 100), 0U) << "ordinary ink is";
}

TEST_F(ClassroomWorkflow, frozenStudentViewIsBlankedWhenAnOverlayChanges) {
    FrozenRig rig(control.get(), window.get());
    ASSERT_NE(rig.student, nullptr);
    auto* tools = control->getToolHandler();
    auto overlay = [&](Action action) { ToolPresentationKit::runClassroomAction(control.get(), action); };

    // Curtain switched on while frozen (the real menu entry)
    rig.freezeShowingAnswers();
    overlay(Action::CLASSROOM_CURTAIN);
    ASSERT_TRUE(tools->isCurtainEnabled());
    rig.expectBlankImmediately("Curtain switched on");

    // Spotlight switched on while frozen
    rig.freezeShowingAnswers();
    overlay(Action::CLASSROOM_SPOTLIGHT);
    ASSERT_TRUE(tools->isSpotlightEnabled());
    rig.expectBlankImmediately("Spotlight switched on");

    // Frozen under Spotlight, then Curtain: the new overlay is stricter, the old picture must not stay
    rig.freezeShowingAnswers();
    rig.click("Resume live view");
    overlay(Action::CLASSROOM_SPOTLIGHT);  // live under Spotlight
    settle();
    rig.click("Freeze this page");
    ASSERT_EQ(rig.students->getState(), ToolStudentView::State::Frozen) << "Spotlight alone must not blank the freeze";
    overlay(Action::CLASSROOM_CURTAIN);
    ASSERT_TRUE(tools->isCurtainEnabled());
    ASSERT_FALSE(tools->isSpotlightEnabled());
    EXPECT_EQ(rig.students->getState(), ToolStudentView::State::Blank) << "Spotlight to Curtain kept the old picture";
    EXPECT_EQ(studentPixel(rig.student, 100, 200), 0U);
    EXPECT_EQ(studentPixel(rig.student, 100, 100), 0U);

    // Frozen under Curtain, then Spotlight: also a change of overlay, so also blank (the conservative choice)
    overlay(Action::CLASSROOM_EXIT_OVERLAY);
    rig.click("Show the page again");
    overlay(Action::CLASSROOM_CURTAIN);
    settle();
    rig.click("Freeze this page");
    ASSERT_EQ(rig.students->getState(), ToolStudentView::State::Frozen);
    overlay(Action::CLASSROOM_SPOTLIGHT);
    EXPECT_EQ(rig.students->getState(), ToolStudentView::State::Blank) << "Curtain to Spotlight kept the old picture";

    // Leaving the overlay that was part of the frozen picture shows nothing new: the freeze stays
    overlay(Action::CLASSROOM_EXIT_OVERLAY);
    rig.click("Show the page again");
    overlay(Action::CLASSROOM_CURTAIN);
    settle();
    rig.click("Freeze this page");
    ASSERT_EQ(rig.students->getState(), ToolStudentView::State::Frozen);
    const unsigned coveredBefore = studentPixel(rig.student, 300, 700);
    overlay(Action::CLASSROOM_EXIT_OVERLAY);
    settle();
    EXPECT_EQ(rig.students->getState(), ToolStudentView::State::Frozen);
    EXPECT_EQ(studentPixel(rig.student, 300, 700), coveredBefore) << "still the covered picture";
}

TEST_F(ClassroomWorkflow, frozenStudentViewIsBlankedWhenTheAnswersLayerIsRemoved) {
    FrozenRig rig(control.get(), window.get());
    ASSERT_NE(rig.student, nullptr);
    rig.freezeShowingAnswers();
    ASSERT_EQ(control->getLayerController()->getCurrentLayerId(), rig.answersId());
    control->getLayerController()->deleteCurrentLayer();  // the real "Delete layer" action
    rig.expectBlankImmediately("answers layer deleted");
}

// Moving ink that students could see into a teacher-only layer changes no layer's visibility and removes no layer, yet
// the frozen picture still shows that ink. The real actions: select all (Ctrl+A), move the selection up one layer (the
// keyboard shortcut's action), finish the selection, undo, redo.
TEST_F(ClassroomWorkflow, movingVisibleInkToAPrivateLayerBlanksTheFrozenStudentView) {
    window->show(nullptr);
    auto page = control->getCurrentPage();
    auto* layers = control->getLayerController();
    auto* actions = control->getActionDatabase();
    Layer* open = page->getSelectedLayer();
    addMarker(open, 100);
    layers->addNewLayer(false);
    Layer* priv = page->getSelectedLayer();
    priv->setAudience(LayerAudience::TeacherOnly);
    layers->switchToLay(1);
    ASSERT_EQ(page->getSelectedLayer(), open);

    auto students = std::make_unique<ToolStudentView>("CLASSROOM_STUDENTS", control.get());
    auto pill = students->createItem(true);
    auto* menu = GTK_WIDGET(gtk_menu_button_get_popover(GTK_MENU_BUTTON(pill.get())));
    auto click = [&](const char* label) {
        auto* button = buttonNamed(menu, label);
        ASSERT_NE(button, nullptr) << label;
        gtk_button_clicked(GTK_BUTTON(button));
        settle();
    };
    click("Open on the projector");
    auto* student = studentWindow();
    ASSERT_NE(student, nullptr);
    auto freeze = [&]() {
        if (students->getState() == ToolStudentView::State::Blank) click("Show the page again");
        if (students->getState() == ToolStudentView::State::Frozen) click("Resume live view");
        control->firePageChanged(0);  // set-up only: the live frame must show the ink before it is frozen
        settle();
        click("Freeze this page");
        ASSERT_EQ(students->getState(), ToolStudentView::State::Frozen);
        ASSERT_EQ(studentPixel(student, 100, 100), 0U) << "the visible ink is on the frozen picture";
    };
    auto expectBlankNow = [&](const char* what) {  // no settle(): before the next main-loop iteration
        EXPECT_EQ(students->getState(), ToolStudentView::State::Blank) << what;
        EXPECT_EQ(studentPixel(student, 100, 100), 0U) << what;
        EXPECT_EQ(studentPixel(student, 300, 700), 0U) << what << ": the old picture must be gone";
    };
    auto moveSelectionUpAndFinish = [&]() {
        settle();  // the page view must exist before select-all can work on it
        control->selectAllOnPage();  // what Ctrl+A runs
        ASSERT_NE(window->getXournal()->getSelection(), nullptr);
        actions->fireActivateAction(Action::MOVE_SELECTION_LAYER_UP);
        control->clearSelectionEndText();  // the selection is put into the selected (private) layer
    };

    freeze();
    moveSelectionUpAndFinish();
    ASSERT_EQ(priv->getElements().size(), 1U) << "the ink really is in the private layer now";
    expectBlankNow("ink moved to a teacher-only layer");

    // Delayed redraws cannot bring the picture back
    for (int i = 0; i < 4; ++i) {
        control->firePageChanged(0);
        settle();
        g_usleep(60 * 1000);
    }
    expectBlankNow("after delayed redraws");

    // Undo puts it back where students can see it; redo moves it into the private layer again
    click("Show the page again");
    control->getUndoRedoHandler()->undo();
    settle();
    ASSERT_EQ(open->getElements().size(), 1U);
    freeze();
    control->getUndoRedoHandler()->redo();
    ASSERT_EQ(priv->getElements().size(), 1U);
    expectBlankNow("redo moved the ink to the teacher-only layer");

    // The same through a hidden (not teacher-only) layer, and undo of the move while frozen
    click("Show the page again");
    control->getUndoRedoHandler()->undo();
    settle();
    priv->setAudience(LayerAudience::Everyone);
    layers->setLayerVisible(2, false);
    settle();
    freeze();
    control->getUndoRedoHandler()->redo();
    expectBlankNow("redo moved the ink to a hidden layer");

    // Harmless: new ink and moves inside the private layer keep a safe frozen picture
    click("Show the page again");
    priv->setAudience(LayerAudience::TeacherOnly);
    layers->setLayerVisible(2, true);
    layers->switchToLay(2);
    settle();
    click("Freeze this page");
    ASSERT_EQ(students->getState(), ToolStudentView::State::Frozen);
    addMarker(priv, 400);
    control->firePageChanged(0);
    actions->fireActivateAction(Action::SELECT_ALL);
    control->clearSelectionEndText();
    settle();
    EXPECT_EQ(students->getState(), ToolStudentView::State::Frozen) << "private work must not destroy the freeze";

    gtk_widget_destroy(student);
    gtk_widget_destroy(pill.get());
    pill.reset();
    students.reset();
    settle();
}

TEST_F(ClassroomWorkflow, ordinaryChangesKeepTheFreezeAndLiveAndBlankStillWork) {
    FrozenRig rig(control.get(), window.get());
    ASSERT_NE(rig.student, nullptr);
    auto* layers = control->getLayerController();
    auto* tools = control->getToolHandler();

    // Answers hidden before the freeze: the frozen picture has none
    utn::hideAnswersFromUi(control.get());
    settle();
    rig.click("Freeze this page");
    ASSERT_EQ(rig.students->getState(), ToolStudentView::State::Frozen);
    ASSERT_EQ(studentPixel(rig.student, 100, 200), 0xFFFFFFU);
    ASSERT_EQ(studentPixel(rig.student, 100, 100), 0U);

    // New ink, a new layer, a pen switch, hiding something that was never shown, revealing answers on the teacher's side
    addMarker(rig.page->getLayers().front(), 500);
    control->firePageChanged(0);
    control->selectTool(TOOL_PEN);
    layers->addNewLayer(false);
    layers->setLayerVisible(rig.answersId(), false);  // already hidden: shows nothing new, hides nothing students saw
    utn::revealAnswersFromUi(control.get(), false);   // students see the frozen picture, not the answers
    settle();
    EXPECT_EQ(rig.students->getState(), ToolStudentView::State::Frozen) << "an ordinary change destroyed the freeze";
    EXPECT_EQ(studentPixel(rig.student, 100, 500), 0xFFFFFFU) << "the frozen picture does not follow the page";
    EXPECT_EQ(studentPixel(rig.student, 100, 200), 0xFFFFFFU) << "revealing later does not reach a frozen frame";
    EXPECT_EQ(studentPixel(rig.student, 100, 100), 0U);
    EXPECT_FALSE(tools->isCurtainEnabled());

    // Resume: live shows the current page; Blank and back
    rig.click("Resume live view");
    EXPECT_EQ(rig.students->getState(), ToolStudentView::State::Live);
    EXPECT_EQ(studentPixel(rig.student, 100, 500), 0U) << "live follows the page again";
    rig.click("Blank screen");
    EXPECT_EQ(rig.students->getState(), ToolStudentView::State::Blank);
    EXPECT_EQ(studentPixel(rig.student, 300, 700), 0U);
    rig.click("Show the page again");
    EXPECT_EQ(rig.students->getState(), ToolStudentView::State::Live);
    EXPECT_EQ(studentPixel(rig.student, 100, 100), 0U);
}

// The M2 field test: a "Drag the divider to resize settings" tooltip covered the worksheet while annotating. The
// divider must stay draggable (and keyboard operable) without any tooltip on the panes around the worksheet.
TEST_F(ClassroomWorkflow, noPaneAroundTheWorksheetCarriesATooltip) {
    window->show(nullptr);
    settle();
    for (const char* name: {"utnInspectorPaned", "mainContentContainer", "panelMainContents"}) {
        GtkWidget* pane = window->get(name);
        ASSERT_NE(pane, nullptr) << name;
        EXPECT_FALSE(gtk_widget_get_has_tooltip(pane)) << name << " would show a tooltip over the worksheet";
        EXPECT_EQ(gtk_widget_get_tooltip_text(pane), nullptr) << name;
    }
    GtkWidget* canvas = window->getXournal()->getWidget();
    EXPECT_FALSE(gtk_widget_get_has_tooltip(canvas)) << "the drawing area must never show a tooltip";
    EXPECT_TRUE(gtk_widget_get_can_focus(window->get("utnInspectorPaned"))) << "the divider stays keyboard operable";
}

// Spotlight and Curtain were drawn on the teacher's canvas only, so a teacher who covered the page still projected it.
TEST_F(ClassroomWorkflow, spotlightAndCurtainAlsoCoverTheStudentViewFrame) {
    window->show(nullptr);
    auto page = control->getCurrentPage();
    addMarker(page->getSelectedLayer(), 100);

    auto students = std::make_unique<ToolStudentView>("CLASSROOM_STUDENTS", control.get());
    auto pill = students->createItem(true);
    auto* menu = GTK_WIDGET(gtk_menu_button_get_popover(GTK_MENU_BUTTON(pill.get())));
    ASSERT_NE(buttonNamed(menu, "Open on the projector"), nullptr);
    gtk_button_clicked(GTK_BUTTON(buttonNamed(menu, "Open on the projector")));
    settle();
    auto* student = studentWindow();
    ASSERT_NE(student, nullptr);

    EXPECT_EQ(studentPixel(student, 300, 700), 0xFFFFFFU) << "the empty lower page is white without an overlay";

    // Curtain: with no pointer position yet it covers the lower half, and the cover is opaque for the class
    turnOnOverlay(control.get(), false);
    settle();
    EXPECT_EQ(studentPixel(student, 300, 700), 0U) << "Curtain must hide the page from students";
    EXPECT_EQ(studentPixel(student, 300, 100), 0xFFFFFFU) << "the part above the curtain line stays visible";
    EXPECT_TRUE(pressEscape(window.get()));
    settle();
    EXPECT_EQ(studentPixel(student, 300, 700), 0xFFFFFFU) << "Escape shows the whole page to students again";

    // Spotlight: the corner is darkened, the middle is clear
    turnOnOverlay(control.get(), true);
    settle();
    EXPECT_NE(studentPixel(student, 20, 20), 0xFFFFFFU);
    EXPECT_EQ(studentPixel(student, 300, 400), 0xFFFFFFU);
    window->exitPresentationOverlay();
    settle();
    EXPECT_EQ(studentPixel(student, 20, 20), 0xFFFFFFU);

    gtk_widget_destroy(student);
    gtk_widget_destroy(pill.get());
    pill.reset();
    students.reset();
    settle();
}

TEST_F(ClassroomWorkflow, presentationExitPreservesSavedAnnotationsAndKeepsAnswersHidden) {
    control->startAnswersLayer();
    auto page = control->getCurrentPage();
    auto* answers = page->getSelectedLayer();
    auto answer = std::make_unique<Text>();
    answer->setText("Private answer 42");
    answers->addElement(std::move(answer));
    control->setAnswersRevealed(false, false);

    auto* lesson = page->getLayers().front();
    auto annotation = std::make_unique<Text>();
    annotation->setText("Saved lesson note");
    annotation->move(100, 100);
    lesson->addElement(std::move(annotation));
    ASSERT_FALSE(answers->isVisible());

    // Control is shared by the whole suite. An earlier test that runs an export job directly never reaches the
    // unblock() of the scheduled job, which would leave every win.* action disabled here.
    control->unblock();
    window->show(nullptr);
    auto* actions = G_ACTION_GROUP(window->getWindow());
    ASSERT_TRUE(g_action_group_get_action_enabled(actions, "classroom-spotlight"));
    g_action_group_activate_action(actions, "classroom-spotlight", nullptr);
    ASSERT_TRUE(control->getToolHandler()->isSpotlightEnabled());
    auto* exitButton = buttonNamed(window->get("mainOverlay"), "Show the whole page (Esc)");
    ASSERT_NE(exitButton, nullptr);
    gtk_button_clicked(GTK_BUTTON(exitButton));
    settle();

    EXPECT_FALSE(control->getToolHandler()->isSpotlightEnabled());
    EXPECT_EQ(lesson->getElements().size(), 1U);
    EXPECT_EQ(answers->getElements().size(), 1U);
    EXPECT_FALSE(answers->isVisible());

    // Tapping the active Curtain action a second time also exits without changing document content.
    g_action_group_activate_action(actions, "classroom-curtain", nullptr);
    ASSERT_TRUE(control->getToolHandler()->isCurtainEnabled());
    g_action_group_activate_action(actions, "classroom-curtain", nullptr);
    EXPECT_FALSE(control->getToolHandler()->isCurtainEnabled());
    EXPECT_EQ(lesson->getElements().size(), 1U);
    EXPECT_EQ(answers->getElements().size(), 1U);
    EXPECT_FALSE(answers->isVisible());

    const auto file = Util::getTmpDirSubfolder() / "presentation-exit-preserves-answers.xopp";
    SaveHandler saver;
    saver.prepareSave(control->getDocument(), file);
    saver.saveTo(file);
    ASSERT_TRUE(saver.getErrorMessage().empty()) << saver.getErrorMessage();
    auto reopened = LoadHandler{}.loadDocument(file);
    ASSERT_NE(reopened, nullptr);
    ASSERT_EQ(reopened->getPage(0)->getLayers().size(), 2U);
    EXPECT_EQ(reopened->getPage(0)->getLayers().front()->getElements().size(), 1U);
    auto* savedAnswers = reopened->getPage(0)->getLayers().at(1);
    EXPECT_EQ(savedAnswers->getAudience(), LayerAudience::Answers);
    EXPECT_FALSE(savedAnswers->isVisible());
    ASSERT_EQ(savedAnswers->getElements().size(), 1U);
    EXPECT_EQ(static_cast<Text*>(savedAnswers->getElements().front().get())->getText(), "Private answer 42");
}

// Closer to a teaching session than the single-page stress test: strokes spread over several pages, undo/redo
// churn, and the real AutosaveJob running on the scheduler while worker threads render. UTN_SOAK_STROKES raises the
// size (default is small enough for the normal suite). When built with -DUTN_LOCK_AUDIT=ON it also fails on any nested
// Document lock acquisition, the pattern that deadlocks std::shared_mutex once a writer is waiting.
TEST_F(ClassroomWorkflow, longSessionWithAutosaveAndSeveralPagesKeepsTheDocumentLockHealthy) {
    const char* requested = g_getenv("UTN_SOAK_STROKES");
    const auto parsed = requested ? g_ascii_strtoll(requested, nullptr, 10) : 90;
    const int strokes = static_cast<int>(std::clamp<gint64>(parsed, 1, 20000));
    constexpr size_t PAGES = 5;

    Document* doc = control->getDocument();
    // File backed, so the autosave lands next to a temporary file and never in the user's autosave folder
    const auto lesson = Util::getTmpDirSubfolder() / "soak-lesson.xopp";
    doc->lock();
    doc->setFilepath(lesson);
    doc->unlock();
    while (doc->getPageCount() < PAGES) {
        control->insertNewPage(doc->getPageCount(), true);
    }
    window->show(nullptr);
    settle();
    ensureSchedulerRunning(control.get());
#ifdef UTN_LOCK_AUDIT
    const unsigned nestedBefore = utn::lockaudit::nestedViolations();
#endif
    control->selectTool(TOOL_PEN);
    control->getToolHandler()->setDrawingType(DRAWING_TYPE_DEFAULT);

    std::vector<gint64> latencies;
    latencies.reserve(static_cast<size_t>(strokes));
    size_t drawn = 0;
    for (int stroke = 0; stroke < strokes; ++stroke) {
        auto* view = window->getXournal()->getViewFor(static_cast<size_t>(stroke) % PAGES);
        ASSERT_NE(view, nullptr);
        PositionInputData pos{};
        pos.pressure = 0.6;
        pos.x = (60 + (stroke * 37) % 300) * view->getZoom();
        pos.y = (60 + (stroke * 53) % 500) * view->getZoom();
        const auto begin = g_get_monotonic_time();
        ASSERT_TRUE(view->onButtonPressEvent(pos));
        for (int point = 1; point <= 24; ++point) {
            pos.x += 4 * view->getZoom();
            pos.y += (point % 3 - 1) * 2 * view->getZoom();
            pos.timestamp += 8;
            view->onMotionNotifyEvent(pos);
        }
        view->onButtonReleaseEvent(pos);
        ++drawn;
        if (stroke % 11 == 10) {
            control->getUndoRedoHandler()->undo();
            control->getUndoRedoHandler()->redo();
        }
        if (stroke % 20 == 19) {
            auto* job = new AutosaveJob(control.get());
            control->getScheduler()->addJob(job, JOB_PRIORITY_NONE);
            job->unref();
        }
        for (int event = 0; event < 40 && gtk_events_pending(); ++event) {
            gtk_main_iteration_do(false);
        }
        latencies.push_back(g_get_monotonic_time() - begin);
    }
    settle();  // let queued renders and the last autosave finish

    size_t elements = 0;
    for (size_t i = 0; i < PAGES; ++i) {
        elements += doc->getPage(i)->getSelectedLayer()->getElements().size();
    }
    EXPECT_EQ(elements, drawn) << "every stroke survives the undo/redo churn and the saves";

    std::sort(latencies.begin(), latencies.end());
    RecordProperty("soak_strokes", strokes);
    RecordProperty("soak_p95_us", std::to_string(latencies[(latencies.size() - 1) * 95 / 100]));
    RecordProperty("soak_p99_us", std::to_string(latencies[(latencies.size() - 1) * 99 / 100]));
    RecordProperty("soak_max_us", std::to_string(latencies.back()));
    // A multi-second stall on one stroke is the field symptom; this is a tripwire, not a latency benchmark
    EXPECT_LT(latencies.back(), 5 * G_USEC_PER_SEC);
#ifdef UTN_LOCK_AUDIT
    EXPECT_EQ(utn::lockaudit::nestedViolations(), nestedBefore) << "nested Document lock acquisition on one thread";
    RecordProperty("soak_longest_lock_wait_ms", std::to_string(utn::lockaudit::longestWaitMs()));
    RecordProperty("soak_slow_lock_waits", static_cast<int>(utn::lockaudit::slowAcquisitions()));
#endif
    std::error_code ec;
    fs::remove(fs::path(lesson).replace_filename(".soak-lesson.autosave.xopp"), ec);
}

// What the first soak does not do: erase, highlight, vary the pressure, keep Student View open on the dense pages and
// autosave while erased strokes exist. The failure it looks for is the field one, a session that gets slower the longer
// it runs, so besides the usual tripwires it compares the last third of the session with the first.
// UTN_SOAK_STROKES scales it (default is small enough for the normal suite).
TEST_F(ClassroomWorkflow, heavySessionWithErasingHighlightingStudentViewAndAutosaveDoesNotSlowDown) {
    const char* requested = g_getenv("UTN_SOAK_STROKES");
    const auto parsed = requested ? g_ascii_strtoll(requested, nullptr, 10) : 120;
    const int iterations = static_cast<int>(std::clamp<gint64>(parsed, 12, 20000));
    constexpr size_t PAGES = 3;

    Document* doc = control->getDocument();
    const auto lesson = Util::getTmpDirSubfolder() / "soak-heavy-lesson.xopp";
    doc->lock();
    doc->setFilepath(lesson);
    doc->unlock();
    while (doc->getPageCount() < PAGES) {
        control->insertNewPage(doc->getPageCount(), true);
    }
    // Autosave and export write temporary files. If the temporary folder cannot be used the soak would measure a failing
    // autosave, so say so instead of reporting numbers.
    requireWorkingTempFolder();
    window->show(nullptr);
    settle();
    ensureSchedulerRunning(control.get());

    utn::perf::setEnabled(true);
    utn::perf::reset();
    utn::perf::startHeartbeat();  // UI-thread responsiveness (ui_stall_ms) and the process counters
    utn::perf::sampleProcess();
#ifdef UTN_LOCK_AUDIT
    const unsigned nestedBefore = utn::lockaudit::nestedViolations();
#endif

    // The class is watching: Student View is open and redraws while the teacher writes
    auto students = std::make_unique<ToolStudentView>("CLASSROOM_STUDENTS", control.get());
    auto pill = students->createItem(true);
    auto* menu = GTK_WIDGET(gtk_menu_button_get_popover(GTK_MENU_BUTTON(pill.get())));
    ASSERT_NE(buttonNamed(menu, "Open on the projector"), nullptr);
    gtk_button_clicked(GTK_BUTTON(buttonNamed(menu, "Open on the projector")));
    settle();
    ASSERT_NE(studentWindow(), nullptr);

    auto* tools = control->getToolHandler();
    tools->setEraserType(ERASER_TYPE_DEFAULT);

    std::vector<gint64> latencies;
    latencies.reserve(static_cast<size_t>(iterations));
    size_t penStrokes = 0;
    size_t autosavesQueued = 0;
    for (int i = 0; i < iterations; ++i) {
        const size_t pageNo = static_cast<size_t>(i) % PAGES;
        auto* view = window->getXournal()->getViewFor(pageNo);
        ASSERT_NE(view, nullptr);
        const double zoom = view->getZoom();
        PositionInputData pos{};
        pos.pressure = 0.5;
        const auto begin = g_get_monotonic_time();

        if (i % 7 == 6) {
            // A curved eraser sweep through the ink the earlier strokes left
            control->selectTool(TOOL_ERASER);
            tools->setEraserType(ERASER_TYPE_DEFAULT);
            pos.x = 80 * zoom;
            pos.y = 120 * zoom;
            ASSERT_TRUE(view->onButtonPressEvent(pos));
            for (int step = 1; step <= 90; ++step) {
                const double a = step * 0.07;
                pos.x = (80 + step * 3.0) * zoom;
                pos.y = (120 + 60 * std::sin(a) + step * 2.0) * zoom;
                pos.timestamp += 8;
                view->onMotionNotifyEvent(pos);
            }
            view->onButtonReleaseEvent(pos);
        } else {
            const bool highlight = i % 5 == 4;
            control->selectTool(highlight ? TOOL_HIGHLIGHTER : TOOL_PEN);
            tools->setDrawingType(DRAWING_TYPE_DEFAULT);
            pos.x = (60 + (i * 37) % 200) * zoom;
            pos.y = (80 + (i * 53) % 300) * zoom;
            ASSERT_TRUE(view->onButtonPressEvent(pos));
            for (int point = 1; point <= 60; ++point) {
                pos.x += (highlight ? 3.0 : 2.0) * zoom;
                pos.y += (point % 5 - 2) * 1.5 * zoom;
                pos.pressure = 0.3 + 0.5 * std::abs(std::sin(point * 0.2));
                pos.timestamp += 8;
                view->onMotionNotifyEvent(pos);
            }
            view->onButtonReleaseEvent(pos);
            ++penStrokes;
        }
        if (i % 11 == 10) {
            control->getUndoRedoHandler()->undo();
            control->getUndoRedoHandler()->redo();
        }
        // The real autosave is a timer: it never queues a second job while the last is still running. Queueing one every
        // 15 strokes regardless built a backlog of minutes (and teardown waited for all of it).
        if (i % 15 == 14 && utn::perf::stats(utn::perf::Metric::Autosave).count >= autosavesQueued) {
            auto* job = new AutosaveJob(control.get());
            control->getScheduler()->addJob(job, JOB_PRIORITY_NONE);
            job->unref();
            ++autosavesQueued;
        }
        for (int event = 0; event < 40 && gtk_events_pending(); ++event) {
            gtk_main_iteration_do(false);
        }
        latencies.push_back(g_get_monotonic_time() - begin);
        if (i % 100 == 99) {
            utn::perf::sampleProcess();  // memory and handle growth over the session
        }
    }
    // Let the queued renders and the last autosave finish before the numbers are read
    for (int i = 0; i < 80; ++i) {
        settle();
    }
    for (int i = 0; i < 600 && utn::perf::stats(utn::perf::Metric::Autosave).count < autosavesQueued; ++i) {
        settle();  // an autosave on a dense lesson takes seconds
    }
    ASSERT_GE(utn::perf::stats(utn::perf::Metric::Autosave).count, autosavesQueued) << "an autosave job never finished";
    utn::perf::sampleProcess();

    // The autosave must really have written its file, and it must reopen: otherwise every autosave number is a lie
    const auto autosaveFile = fs::path(lesson).replace_filename(".soak-heavy-lesson.autosave.xopp");
    ASSERT_TRUE(fs::exists(autosaveFile)) << "autosave wrote no file: " << autosaveFile.string();
    EXPECT_GT(fs::file_size(autosaveFile), 200U);
    auto reopenedAutosave = LoadHandler{}.loadDocument(autosaveFile);
    ASSERT_TRUE(reopenedAutosave) << "the autosave file does not open";
    EXPECT_EQ(reopenedAutosave->getPageCount(), PAGES);
    EXPECT_GT(utn::perf::stats(utn::perf::Metric::Autosave).count, 0U) << "no autosave job ran";

    // Compare the end of the session with its beginning, in the order it happened
    const size_t third = latencies.size() / 3;
    auto p95 = [](std::vector<gint64> part) {
        std::sort(part.begin(), part.end());
        return part[(part.size() - 1) * 95 / 100];
    };
    const gint64 early = p95({latencies.begin(), latencies.begin() + static_cast<std::ptrdiff_t>(third)});
    const gint64 late = p95({latencies.end() - static_cast<std::ptrdiff_t>(third), latencies.end()});
    RecordProperty("heavy_iterations", iterations);
    RecordProperty("heavy_early_p95_us", std::to_string(early));
    RecordProperty("heavy_late_p95_us", std::to_string(late));
    const gint64 slowest = *std::max_element(latencies.begin(), latencies.end());
    RecordProperty("heavy_max_us", std::to_string(slowest));
    EXPECT_LT(slowest, 5 * G_USEC_PER_SEC);
    // Generous on purpose (a noisy CI machine), but a session that keeps getting slower fails it
    EXPECT_LE(late, 2 * early + 50 * 1000) << "the session got slower the longer it ran";

    for (auto metric: {utn::perf::Metric::PenUp, utn::perf::Metric::PenUpLockWait, utn::perf::Metric::RenderJob,
                       utn::perf::Metric::Autosave, utn::perf::Metric::AutosaveLockHold,
                       utn::perf::Metric::StudentFrame, utn::perf::Metric::StudentRedrawRequests,
                       utn::perf::Metric::StudentRedrawUrgent, utn::perf::Metric::EraseEvent,
                       utn::perf::Metric::ErasuresPerStroke, utn::perf::Metric::DocLockWaitExclusive}) {
        recordPerfProperties(metric);
    }
    // UI-thread responsiveness: how late the 20 ms heartbeat ran (only lateness above 8 ms is recorded)
    recordPerfProperties(utn::perf::Metric::UiStall);
    // Growth over the session. First sample is taken before the first stroke, last after everything has settled.
    for (auto metric: {utn::perf::Metric::ProcessPrivateMiB, utn::perf::Metric::GdiObjects,
                       utn::perf::Metric::UserObjects, utn::perf::Metric::ProcessHandles}) {
        const auto stats = utn::perf::stats(metric);
        const std::string base = std::string("growth_") + utn::perf::name(metric);
        RecordProperty(base + "_first", std::to_string(stats.first));
        RecordProperty(base + "_last", std::to_string(stats.last));
        RecordProperty(base + "_max", std::to_string(stats.max));
    }
#ifdef _WIN32
    // Leak tripwires, generous: a page of handles or GDI objects that keeps growing is the leak, not the working set
    const auto gdi = utn::perf::stats(utn::perf::Metric::GdiObjects);
    const auto handles = utn::perf::stats(utn::perf::Metric::ProcessHandles);
    EXPECT_LT(gdi.last - gdi.first, 500.0) << "GDI objects grew over the session";
    EXPECT_LT(handles.last - handles.first, 1000.0) << "kernel handles grew over the session";
#endif
    // Merged eraser capsules keep a stroke that was swept many times cheap to draw and still saveable with its data
    EXPECT_LE(utn::perf::stats(utn::perf::Metric::ErasuresPerStroke).max,
              static_cast<double>(utn::eraser::MASKED_STROKE_MAX_ERASURES));
    // The student window is throttled by its own cost: it may not take most of the session's UI time
    const auto frames = utn::perf::stats(utn::perf::Metric::StudentFrame);
    double sessionMs = 0;
    for (gint64 latency: latencies) {
        sessionMs += static_cast<double>(latency) / 1000.0;
    }
    RecordProperty("heavy_student_frame_share", std::to_string(frames.total / sessionMs));
    EXPECT_LT(frames.total, 0.2 * sessionMs) << "Student View took a large share of the UI thread";
    EXPECT_GT(penStrokes, 0U);
#ifdef UTN_LOCK_AUDIT
    EXPECT_EQ(utn::lockaudit::nestedViolations(), nestedBefore) << "nested Document lock acquisition on one thread";
    RecordProperty("heavy_longest_lock_wait_ms", std::to_string(utn::lockaudit::longestWaitMs()));
    RecordProperty("heavy_slow_lock_waits", static_cast<int>(utn::lockaudit::slowAcquisitions()));
#endif

    gtk_widget_destroy(studentWindow());
    gtk_widget_destroy(pill.get());
    pill.reset();
    students.reset();
    settle();
    utn::perf::setEnabled(false);
    std::error_code ec;
    fs::remove(autosaveFile, ec);
}

namespace {
/// Deliver a key press to the text editor the way the canvas does, through its key bindings
bool pressEditorKey(MainWindow* window, TextEditor* editor, guint keyval, GdkModifierType state = GdkModifierType(0)) {
    GdkEvent* event = gdk_event_new(GDK_KEY_PRESS);
    if (auto* gdkWindow = gtk_widget_get_window(window->getWindow())) {
        event->key.window = GDK_WINDOW(g_object_ref(gdkWindow));
    }
    event->key.keyval = keyval;
    event->key.state = state;
    const bool handled = editor->onKeyPressEvent(KeyEvent{keyval, state, event});
    gdk_event_free(event);
    return handled;
}

std::string editorText(TextEditor* editor) {
    return pango_layout_get_text(editor->getUpToDateLayout());
}

}  // namespace

TEST_F(ClassroomWorkflow, enterContinuesAListAndShiftEnterBreaksInsideTheItem) {
    auto page = control->getCurrentPage();
    auto box = std::make_unique<Text>();
    box->setText("\xE2\x80\xA2 one");  // "• one"
    box->move(100, 100);
    page->getSelectedLayer()->addElement(std::move(box));
    control->selectTextMode(TextMode::Plain);
    auto* editor = editAt(101, 101);
    ASSERT_NE(editor, nullptr);
    ASSERT_TRUE(pressEditorKey(window.get(), editor, GDK_KEY_End, GDK_CONTROL_MASK));

    // Enter starts the next item
    ASSERT_TRUE(pressEditorKey(window.get(), editor, GDK_KEY_Return));
    EXPECT_EQ(editorText(editor), "\xE2\x80\xA2 one\n\xE2\x80\xA2 ");
    // Enter on the empty item ends the list
    ASSERT_TRUE(pressEditorKey(window.get(), editor, GDK_KEY_Return));
    EXPECT_EQ(editorText(editor), "\xE2\x80\xA2 one\n");

    // Shift+Enter in a bullet item: a new line without a marker, lined up under the text. Repeating keeps the line up.
    control->clearSelectionEndText();
    auto second = std::make_unique<Text>();
    second->setText("\xE2\x80\xA2 two");
    second->move(100, 200);
    page->getSelectedLayer()->addElement(std::move(second));
    control->selectTextMode(TextMode::Plain);
    editor = editAt(101, 201);
    ASSERT_NE(editor, nullptr);
    ASSERT_TRUE(pressEditorKey(window.get(), editor, GDK_KEY_End, GDK_CONTROL_MASK));
    ASSERT_TRUE(pressEditorKey(window.get(), editor, GDK_KEY_Return, GDK_SHIFT_MASK));
    EXPECT_EQ(editorText(editor), "\xE2\x80\xA2 two\n  ");
    ASSERT_TRUE(pressEditorKey(window.get(), editor, GDK_KEY_KP_Enter, GDK_SHIFT_MASK));
    EXPECT_EQ(editorText(editor), "\xE2\x80\xA2 two\n  \n  ");
    control->clearSelectionEndText();
}

TEST_F(ClassroomWorkflow, enterContinuesTheListAfterASoftBreakAndExitsAnEmptyContinuation) {
    for (const auto& [source, continued, empty]: {
                 std::array<std::string, 3>{"12. answer\n    more", "12. answer\n    more\n13. ", "12. answer\n"},
                 std::array<std::string, 3>{"12. answer\n    first\n    more",
                                            "12. answer\n    first\n    more\n13. ", "12. answer\n"},
                 std::array<std::string, 3>{"12. answer\n    \n    more",
                                            "12. answer\n    \n    more\n13. ", "12. answer\n"},
                 std::array<std::string, 3>{"\xE2\x80\xA2 answer\n  more",
                                            "\xE2\x80\xA2 answer\n  more\n\xE2\x80\xA2 ",
                                            "\xE2\x80\xA2 answer\n"}}) {
        auto* layer = control->getCurrentPage()->getSelectedLayer();
        auto text = std::make_unique<Text>();
        text->setText(source);
        text->move(100, 100);
        layer->addElement(std::move(text));
        control->selectTextMode(TextMode::Plain);
        auto* editor = editAt(101, 101);
        ASSERT_NE(editor, nullptr);
        ASSERT_TRUE(pressEditorKey(window.get(), editor, GDK_KEY_End, GDK_CONTROL_MASK));
        ASSERT_TRUE(pressEditorKey(window.get(), editor, GDK_KEY_Return));
        EXPECT_EQ(editorText(editor), continued);
        control->clearSelectionEndText();
        control->getUndoRedoHandler()->clearContents();
        while (!layer->getElements().empty()) {
            layer->removeElement(layer->getElements().front().get());
        }

        text = std::make_unique<Text>();
        text->setText(source.substr(0, source.find('\n')));
        text->move(100, 100);
        layer->addElement(std::move(text));
        control->selectTextMode(TextMode::Plain);
        editor = editAt(101, 101);
        ASSERT_NE(editor, nullptr);
        ASSERT_TRUE(pressEditorKey(window.get(), editor, GDK_KEY_End, GDK_CONTROL_MASK));
        ASSERT_TRUE(pressEditorKey(window.get(), editor, GDK_KEY_Return, GDK_SHIFT_MASK));
        ASSERT_TRUE(pressEditorKey(window.get(), editor, GDK_KEY_Return));
        EXPECT_EQ(editorText(editor), empty);
        control->clearSelectionEndText();
        control->getUndoRedoHandler()->clearContents();
        while (!layer->getElements().empty()) {
            layer->removeElement(layer->getElements().front().get());
        }
    }
}

TEST_F(ClassroomWorkflow, enterDoesNotContinueAcrossAParagraphOrDifferentIndentation) {
    for (const char* source: {"12. answer\n\n    paragraph", "12. answer\n  paragraph",
                             "12. answer\nplain\n    paragraph"}) {
        auto* layer = control->getCurrentPage()->getSelectedLayer();
        auto text = std::make_unique<Text>();
        text->setText(source);
        text->move(100, 100);
        layer->addElement(std::move(text));
        control->selectTextMode(TextMode::Plain);
        auto* editor = editAt(101, 101);
        ASSERT_NE(editor, nullptr);
        ASSERT_TRUE(pressEditorKey(window.get(), editor, GDK_KEY_End, GDK_CONTROL_MASK));
        ASSERT_TRUE(pressEditorKey(window.get(), editor, GDK_KEY_Return));
        EXPECT_EQ(editorText(editor), std::string(source) + "\n");
        control->clearSelectionEndText();
        control->getUndoRedoHandler()->clearContents();
        // Finalizing replaces the original Text object.
        layer->removeElement(layer->getElements().back().get());
    }
}

TEST_F(ClassroomWorkflow, softBreakSplitsUnicodeAndReplacesTheSelectedText) {
    auto text = std::make_unique<Text>();
    text->setText("alpha \xCE\xB2 tail");
    text->move(100, 100);
    control->getCurrentPage()->getSelectedLayer()->addElement(std::move(text));
    control->selectTextMode(TextMode::Plain);
    auto* editor = editAt(101, 101);
    ASSERT_NE(editor, nullptr);
    ASSERT_TRUE(pressEditorKey(window.get(), editor, GDK_KEY_Home, GDK_CONTROL_MASK));
    for (int i = 0; i < 7; ++i) {
        ASSERT_TRUE(pressEditorKey(window.get(), editor, GDK_KEY_Right));
    }
    ASSERT_TRUE(pressEditorKey(window.get(), editor, GDK_KEY_Return, GDK_SHIFT_MASK));
    EXPECT_EQ(editorText(editor), "alpha \xCE\xB2\n tail");
    ASSERT_TRUE(pressEditorKey(window.get(), editor, GDK_KEY_a, GDK_CONTROL_MASK));
    ASSERT_TRUE(pressEditorKey(window.get(), editor, GDK_KEY_ISO_Enter, GDK_SHIFT_MASK));
    EXPECT_EQ(editorText(editor), "\n");
    const auto controlShift = static_cast<GdkModifierType>(GDK_CONTROL_MASK | GDK_SHIFT_MASK);
    EXPECT_FALSE(pressEditorKey(window.get(), editor, GDK_KEY_Return, controlShift));
    EXPECT_EQ(editorText(editor), "\n");
    control->clearSelectionEndText();
}

TEST_F(ClassroomWorkflow, softBreakHasOneTextUndoTransactionAndSurvivesSaveReopen) {
    auto* layer = control->getCurrentPage()->getSelectedLayer();
    const std::string source = "12. \xE7\xAD\x94\xE6\xA1\x88";
    const auto expected = source + "\n    \n    \n    ";
    for (bool answerBox: {false, true}) {
        auto text = std::make_unique<Text>();
        text->setText(source);
        text->setBoxEnabled(answerBox);
        const int y = answerBox ? 250 : 100;
        text->move(100, y);
        layer->addElement(std::move(text));
        control->selectTextMode(answerBox ? TextMode::AnswerBox : TextMode::Plain);
        auto* editor = editAt(101, y + 1);
        ASSERT_NE(editor, nullptr);
        auto* history = control->getUndoRedoHandler();
        history->clearContents();
        ASSERT_TRUE(pressEditorKey(window.get(), editor, GDK_KEY_End, GDK_CONTROL_MASK));
        for (guint key: {GDK_KEY_Return, GDK_KEY_KP_Enter, GDK_KEY_ISO_Enter}) {
            ASSERT_TRUE(pressEditorKey(window.get(), editor, key, GDK_SHIFT_MASK));
        }
        EXPECT_EQ(editorText(editor), expected);
        EXPECT_FALSE(history->canUndo()) << "editing is committed as one text transaction on exit";
        control->clearSelectionEndText();
        ASSERT_TRUE(history->canUndo());
        history->undo();
        EXPECT_EQ(static_cast<Text*>(layer->getElements().back().get())->getText(), source);
        EXPECT_FALSE(history->canUndo()) << "no duplicate transactions for the three soft breaks";
        history->redo();
        EXPECT_EQ(static_cast<Text*>(layer->getElements().back().get())->getText(), expected);
        EXPECT_FALSE(history->canRedo());
    }
    const auto file = Util::getTmpDirSubfolder() / "m3-soft-break-text.xopp";
    SaveHandler saver;
    saver.prepareSave(control->getDocument(), file);
    saver.saveTo(file);
    ASSERT_TRUE(saver.getErrorMessage().empty()) << saver.getErrorMessage();
    auto reopened = LoadHandler{}.loadDocument(file);
    ASSERT_NE(reopened, nullptr);
    const auto& elements = reopened->getPage(0)->getSelectedLayer()->getElements();
    ASSERT_EQ(elements.size(), 2U);
    for (size_t i = 0; i < elements.size(); ++i) {
        auto* saved = dynamic_cast<Text*>(elements[i].get());
        ASSERT_NE(saved, nullptr);
        EXPECT_EQ(saved->getText(), expected);
        EXPECT_EQ(saved->isBoxEnabled(), i == 1);
    }
}

namespace {
GtkWidget* accessibleNamed(GtkWidget* widget, const char* name) {
    if (AtkObject* accessible = gtk_widget_get_accessible(widget);
        accessible && g_strcmp0(atk_object_get_name(accessible), name) == 0) {
        return widget;
    }
    if (GTK_IS_CONTAINER(widget)) {
        GList* children = gtk_container_get_children(GTK_CONTAINER(widget));
        GtkWidget* found = nullptr;
        for (GList* child = children; child && !found; child = child->next) {
            found = accessibleNamed(GTK_WIDGET(child->data), name);
        }
        g_list_free(children);
        return found;
    }
    return nullptr;
}

GtkWidget* firstMenuButton(GtkWidget* widget) {
    if (GTK_IS_MENU_BUTTON(widget)) {
        return widget;
    }
    if (GTK_IS_CONTAINER(widget)) {
        GList* children = gtk_container_get_children(GTK_CONTAINER(widget));
        GtkWidget* found = nullptr;
        for (GList* child = children; child && !found; child = child->next) {
            found = firstMenuButton(GTK_WIDGET(child->data));
        }
        g_list_free(children);
        return found;
    }
    return nullptr;
}

GtkWidget* firstToggleButton(GtkWidget* widget) {
    if (GTK_IS_TOGGLE_BUTTON(widget)) {
        return widget;
    }
    if (GTK_IS_CONTAINER(widget)) {
        GList* children = gtk_container_get_children(GTK_CONTAINER(widget));
        GtkWidget* found = nullptr;
        for (GList* child = children; child && !found; child = child->next) {
            found = firstToggleButton(GTK_WIDGET(child->data));
        }
        g_list_free(children);
        return found;
    }
    return nullptr;
}

/// The "Text settings" popover of a rail item ("Text" or "Answer Box")
GtkWidget* textSettingsPopover(GtkWidget* rail, const char* caption, GtkWidget** menuOut = nullptr) {
    GtkWidget* found = nullptr;
    GList* items = gtk_container_get_children(GTK_CONTAINER(rail));
    for (GList* item = items; item && !found; item = item->next) {
        AtkObject* accessible = gtk_widget_get_accessible(GTK_WIDGET(item->data));
        if (accessible && g_strcmp0(atk_object_get_name(accessible), caption) == 0) {
            found = GTK_WIDGET(item->data);
        }
    }
    g_list_free(items);
    GtkWidget* menu = found ? firstMenuButton(found) : nullptr;
    if (menuOut) {
        *menuOut = menu;
    }
    return menu ? GTK_WIDGET(gtk_menu_button_get_popover(GTK_MENU_BUTTON(menu))) : nullptr;
}

/// A GTK3 popover fades out after popdown: visible and mapped stay true until the transition has finished
bool waitForHidden(GtkWidget* popover) {
    const gint64 deadline = g_get_monotonic_time() + 3 * G_USEC_PER_SEC;
    while (gtk_widget_get_visible(popover) || gtk_widget_get_mapped(popover)) {
        if (g_get_monotonic_time() > deadline) {
            return false;
        }
        for (int event = 0; event < 20 && gtk_events_pending(); ++event) {
            gtk_main_iteration_do(false);
        }
        g_usleep(2000);
    }
    return true;
}
}  // namespace

// The panels are built when the toolbar is created. Opening one later must show what is true now, not what was true then.
TEST_F(ClassroomWorkflow, textSettingsPopoversShowTheCurrentModeAndFontWhenOpened) {
    window->toolbarSelected("UTN Teacher");
    window->show(nullptr);
    settle();
    auto* rail = window->get("tbLeft1");
    GtkWidget* answerMenu = nullptr;
    GtkWidget* answerPopover = textSettingsPopover(rail, "Answer Box", &answerMenu);
    ASSERT_NE(answerPopover, nullptr);

    control->selectTextMode(TextMode::AnswerBox);  // the first tap on the Answer Box rail button
    settle();
    gtk_button_clicked(GTK_BUTTON(answerMenu));  // the repeat tap opens the settings
    settle();
    ASSERT_TRUE(gtk_widget_get_visible(answerPopover));
    auto* mode = GTK_COMBO_BOX(accessibleNamed(answerPopover, "Text and Answer Box mode"));
    ASSERT_NE(mode, nullptr);
    EXPECT_EQ(gtk_combo_box_get_active(mode), 1) << "the selector names the mode that is active";
    EXPECT_NE(buttonNamed(answerPopover, "Delete Box"), nullptr) << "Answer Box controls are offered for an Answer Box";
    gtk_popover_popdown(GTK_POPOVER(answerPopover));
    settle();

    // Same for the font: it was changed after the panel was built
    GtkWidget* textMenu = nullptr;
    GtkWidget* textPopover = textSettingsPopover(rail, "Text", &textMenu);
    ASSERT_NE(textPopover, nullptr);
    control->selectTextMode(TextMode::Plain);
    XojFont font = control->getSettings()->getFont();
    font.setSize(37);
    control->fontChanged(font);
    control->getActionDatabase()->setActionState(Action::FONT, font.asString().c_str());
    settle();
    gtk_button_clicked(GTK_BUTTON(textMenu));
    settle();
    auto* size = GTK_SPIN_BUTTON(accessibleNamed(textPopover, "Text size"));
    ASSERT_NE(size, nullptr);
    EXPECT_DOUBLE_EQ(gtk_spin_button_get_value(size), 37.0);
    mode = GTK_COMBO_BOX(accessibleNamed(textPopover, "Text and Answer Box mode"));
    ASSERT_NE(mode, nullptr);
    EXPECT_EQ(gtk_combo_box_get_active(mode), 0);
    EXPECT_EQ(buttonNamed(textPopover, "Delete Box"), nullptr);
    gtk_popover_popdown(GTK_POPOVER(textPopover));
    settle();
}

// Formatting from the floating panel must act on the box being edited without ending the edit or touching its text.
TEST_F(ClassroomWorkflow, changingTextSettingsFromThePopoverKeepsTheEditAndTheText) {
    window->toolbarSelected("UTN Teacher");
    window->show(nullptr);
    settle();
    auto page = control->getCurrentPage();
    auto box = std::make_unique<Text>();
    box->setText("hello world");
    box->move(100, 100);
    page->getSelectedLayer()->addElement(std::move(box));
    control->selectTextMode(TextMode::Plain);
    auto* editor = editAt(101, 101);
    ASSERT_NE(editor, nullptr);
    ASSERT_TRUE(pressEditorKey(window.get(), editor, GDK_KEY_a, GDK_CONTROL_MASK));  // select all
    const std::string before = editorText(editor);
    ASSERT_EQ(before, "hello world");

    GtkWidget* menu = nullptr;
    GtkWidget* popover = textSettingsPopover(window->get("tbLeft1"), "Text", &menu);
    ASSERT_NE(popover, nullptr);
    gtk_button_clicked(GTK_BUTTON(menu));
    settle();
    ASSERT_TRUE(gtk_widget_get_visible(popover));
    auto* size = GTK_SPIN_BUTTON(accessibleNamed(popover, "Text size"));
    ASSERT_NE(size, nullptr);
    gtk_spin_button_set_value(size, 31.0);
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(accessibleNamed(popover, "Bold")), true);
    settle();
    gtk_popover_popdown(GTK_POPOVER(popover));
    settle();

    ASSERT_EQ(control->getTextEditor(), editor) << "the same edit is still open";
    EXPECT_EQ(editorText(editor), before);
    EXPECT_EQ(control->getToolHandler()->getToolType(), TOOL_TEXT);
    const XojFont font = editor->getTextElement()->getFont();
    EXPECT_DOUBLE_EQ(font.getSize(), 31.0);
    auto* description = pango_font_description_from_string(font.asString().c_str());
    EXPECT_GE(pango_font_description_get_weight(description), PANGO_WEIGHT_SEMIBOLD);
    pango_font_description_free(description);
    // Editing continues in the same box after the panel closed (printable keys go through the IM context, so use a binding)
    ASSERT_TRUE(pressEditorKey(window.get(), editor, GDK_KEY_End, GDK_CONTROL_MASK));
    ASSERT_TRUE(pressEditorKey(window.get(), editor, GDK_KEY_BackSpace));
    EXPECT_EQ(editorText(editor), "hello worl");
    control->clearSelectionEndText();
    size_t texts = 0;
    for (auto& element: page->getSelectedLayer()->getElements()) {
        texts += element->getType() == ELEMENT_TEXT ? 1 : 0;
    }
    EXPECT_EQ(texts, 1U) << "formatting did not duplicate or drop the box";
}

// First tap selects, a repeat tap (pointer, Return or Space) opens the settings; none of them changes the tool.
TEST_F(ClassroomWorkflow, textSettingsOpenOnRepeatActivationOnlyAndKeyboardWorks) {
    window->toolbarSelected("UTN Teacher");
    window->show(nullptr);
    settle();
    auto* rail = window->get("tbLeft1");
    GtkWidget* menu = nullptr;
    GtkWidget* popover = textSettingsPopover(rail, "Text", &menu);
    ASSERT_NE(popover, nullptr);
    GtkWidget* group = gtk_widget_get_parent(menu);
    GList* children = gtk_container_get_children(GTK_CONTAINER(group));
    auto* toggle = GTK_TOGGLE_BUTTON(children->data);
    g_list_free(children);

    control->selectTool(TOOL_PEN);
    settle();
    gtk_button_clicked(GTK_BUTTON(toggle));  // first tap
    settle();
    EXPECT_EQ(control->getToolHandler()->getToolType(), TOOL_TEXT);
    EXPECT_FALSE(gtk_widget_get_visible(popover)) << "the first tap only selects";

    for (guint key: {GDK_KEY_Return, GDK_KEY_space, GDK_KEY_KP_Enter}) {
        GdkEventKey event{};
        event.keyval = key;
        gboolean handled = FALSE;
        g_signal_emit_by_name(toggle, "key-press-event", &event, &handled);
        settle();
        EXPECT_TRUE(handled) << key;
        EXPECT_TRUE(gtk_widget_get_visible(popover)) << "key " << key << " opens the settings";
        EXPECT_EQ(control->getToolHandler()->getToolType(), TOOL_TEXT) << key;
        EXPECT_TRUE(gtk_toggle_button_get_active(toggle)) << key << ": the tool stays selected";
        gtk_popover_popdown(GTK_POPOVER(popover));
        ASSERT_TRUE(waitForHidden(popover)) << key;
    }
    EXPECT_NE(gtk_widget_get_accessible(menu), nullptr);
}

// Delete Box from the panel removes only the box being edited, undo restores it into the same private layer, and
// neither opening the panel nor deleting makes the answers visible to students.
TEST_F(ClassroomWorkflow, deleteBoxFromThePopoverIsDeliberateUndoableAndKeepsAnswersHidden) {
    window->toolbarSelected("UTN Teacher");
    window->show(nullptr);
    settle();
    control->startAnswersLayer();
    auto page = control->getCurrentPage();
    auto* answers = page->getSelectedLayer();
    for (double y: {100.0, 300.0}) {
        auto box = std::make_unique<Text>();
        box->setText("answer");
        box->setBoxEnabled(true);
        box->move(100, y);
        answers->addElement(std::move(box));
    }
    control->setAnswersRevealed(false, false);
    control->setGhostHiddenAnswers(true);
    control->getLayerController()->switchToLay(2);
    control->selectTextMode(TextMode::AnswerBox);

    GtkWidget* menu = nullptr;
    GtkWidget* popover = textSettingsPopover(window->get("tbLeft1"), "Answer Box", &menu);
    ASSERT_NE(popover, nullptr);
    gtk_button_clicked(GTK_BUTTON(menu));
    settle();
    auto* remove = buttonNamed(popover, "Delete Box");
    ASSERT_NE(remove, nullptr);
    EXPECT_FALSE(answers->isVisible()) << "opening the panel does not reveal anything";

    // Nothing is being edited: the button must not delete anything
    ASSERT_EQ(control->getTextEditor(), nullptr);
    gtk_button_clicked(GTK_BUTTON(remove));
    settle();
    EXPECT_EQ(answers->getElements().size(), 2U);

    gtk_popover_popdown(GTK_POPOVER(popover));
    ASSERT_TRUE(waitForHidden(popover));
    ASSERT_NE(editAt(101, 101), nullptr);
    gtk_button_clicked(GTK_BUTTON(menu));
    settle();
    remove = buttonNamed(popover, "Delete Box");
    ASSERT_NE(remove, nullptr);
    gtk_button_clicked(GTK_BUTTON(remove));
    settle();
    EXPECT_EQ(answers->getElements().size(), 1U) << "only the edited box goes";
    EXPECT_FALSE(answers->isVisible());

    control->getUndoRedoHandler()->undo();
    EXPECT_EQ(answers->getElements().size(), 2U);
    EXPECT_EQ(answers->getAudience(), LayerAudience::Answers);
    EXPECT_FALSE(answers->isVisible());
}

// Switching mode several times before the main loop runs must leave one consistent panel and no stale callbacks.
TEST_F(ClassroomWorkflow, rapidModeSwitchingInThePanelLeavesOneConsistentPanel) {
    window->toolbarSelected("UTN Teacher");
    window->show(nullptr);
    settle();
    GtkWidget* menu = nullptr;
    GtkWidget* popover = textSettingsPopover(window->get("tbLeft1"), "Text", &menu);
    ASSERT_NE(popover, nullptr);
    for (int round = 0; round < 3; ++round) {
        control->selectTextMode(TextMode::Plain);
        settle();
        gtk_button_clicked(GTK_BUTTON(menu));
        settle();
        auto* mode = GTK_COMBO_BOX(accessibleNamed(popover, "Text and Answer Box mode"));
        ASSERT_NE(mode, nullptr);
        for (int flip = 0; flip < 7; ++flip) {
            gtk_combo_box_set_active(mode, flip % 2 == 0 ? 1 : 0);  // 7 flips end on Answer Box, before any idle runs
        }
        settle();
        EXPECT_EQ(control->getToolHandler()->getTextMode(), TextMode::AnswerBox) << round;
        // The queued rebuilds destroyed the old selector: exactly one is left, and it agrees with the tool handler
        int selectors = 0;
        std::function<void(GtkWidget*)> count = [&](GtkWidget* widget) {
            if (AtkObject* accessible = gtk_widget_get_accessible(widget);
                accessible && g_strcmp0(atk_object_get_name(accessible), "Text and Answer Box mode") == 0) {
                ++selectors;
            }
            if (GTK_IS_CONTAINER(widget)) {
                GList* children = gtk_container_get_children(GTK_CONTAINER(widget));
                for (GList* child = children; child; child = child->next) {
                    count(GTK_WIDGET(child->data));
                }
                g_list_free(children);
            }
        };
        count(popover);
        EXPECT_EQ(selectors, 1) << round;
        gtk_popover_popdown(GTK_POPOVER(popover));
        ASSERT_TRUE(waitForHidden(popover)) << round;
    }
    // A panel destroyed while a rebuild is queued must not be touched afterwards
    GtkWidget* orphan = ToolUtnContextBar::createTextSettingsPanel(control.get());
    g_object_ref_sink(orphan);
    auto* orphanMode = GTK_COMBO_BOX(accessibleNamed(orphan, "Text and Answer Box mode"));
    ASSERT_NE(orphanMode, nullptr);
    gtk_combo_box_set_active(orphanMode, gtk_combo_box_get_active(orphanMode) == 0 ? 1 : 0);
    gtk_widget_destroy(orphan);
    g_object_unref(orphan);
    settle();
    control->selectTextMode(TextMode::Plain);
}

// Switching mode closes the old mode's popover; keyboard focus must land outside the panel being rebuilt.
TEST_F(ClassroomWorkflow, switchingModeClosesPopoverWithoutLeavingFocusInDestroyedSettings) {
    window->toolbarSelected("UTN Teacher");
    window->show(nullptr);
    settle();
    auto* rail = window->get("tbLeft1");
    GtkWidget* menu = nullptr;
    GtkWidget* popover = textSettingsPopover(rail, "Text", &menu);
    ASSERT_NE(popover, nullptr);
    control->selectTextMode(TextMode::Plain);
    settle();
    gtk_button_clicked(GTK_BUTTON(menu));
    settle();
    ASSERT_TRUE(gtk_widget_get_visible(popover));
    auto* mode = GTK_COMBO_BOX(accessibleNamed(popover, "Text and Answer Box mode"));
    ASSERT_NE(mode, nullptr);
    gtk_widget_grab_focus(GTK_WIDGET(mode));
    GtkWidget* selectorFocus = gtk_window_get_focus(GTK_WINDOW(gtk_widget_get_toplevel(GTK_WIDGET(mode))));
    ASSERT_NE(selectorFocus, nullptr);
    ASSERT_TRUE(selectorFocus == GTK_WIDGET(mode) || gtk_widget_is_ancestor(selectorFocus, GTK_WIDGET(mode)))
            << "focus should be within the selector before switching modes";

    gtk_combo_box_set_active(mode, 1);
    settle();
    ASSERT_TRUE(waitForHidden(popover));
    GtkWidget* answerItem = accessibleNamed(rail, "Answer Box");
    ASSERT_NE(answerItem, nullptr);
    auto* answerButton = GTK_TOGGLE_BUTTON(firstToggleButton(answerItem));
    ASSERT_TRUE(GTK_IS_TOGGLE_BUTTON(answerButton));
    EXPECT_TRUE(gtk_toggle_button_get_active(answerButton));
    GtkWidget* focus = gtk_window_get_focus(GTK_WINDOW(gtk_widget_get_toplevel(GTK_WIDGET(answerButton))));
    ASSERT_NE(focus, nullptr) << "focus must return to the window when the old popover closes";
    EXPECT_TRUE(gtk_widget_get_visible(focus));
    EXPECT_FALSE(gtk_widget_is_ancestor(focus, popover)) << "focus must not remain inside the hidden settings panel";
}

// The Answer Box panel is about 615 px tall. On a small screen or a short window the popover must stay inside the
// window and scroll, otherwise Delete Box and the box style controls sit off screen with no way to reach them.
TEST_F(ClassroomWorkflow, answerBoxSettingsPopoverFitsAShortWindowAndKeepsEveryControlReachable) {
    window->toolbarSelected("UTN Teacher");
    window->show(nullptr);
    gtk_window_resize(GTK_WINDOW(window->getWindow()), 1000, 560);
    settle();
    const int windowHeight = gtk_widget_get_allocated_height(window->getWindow());
    ASSERT_LE(windowHeight, 600) << "the desktop must allow a short window for this check";
    control->selectTextMode(TextMode::AnswerBox);
    settle();
    GtkWidget* menu = nullptr;
    GtkWidget* popover = textSettingsPopover(window->get("tbLeft1"), "Answer Box", &menu);
    ASSERT_NE(popover, nullptr);
    gtk_button_clicked(GTK_BUTTON(menu));
    settle();
    ASSERT_TRUE(gtk_widget_get_visible(popover));


    // Every control stays inside a scrolled area rather than being cut off
    auto* remove = buttonNamed(popover, "Delete Box");
    ASSERT_NE(remove, nullptr);
    GtkWidget* ancestor = gtk_widget_get_ancestor(remove, GTK_TYPE_SCROLLED_WINDOW);
    ASSERT_NE(ancestor, nullptr) << "Delete Box sits inside a scrolled area";
    EXPECT_LE(gtk_widget_get_allocated_height(ancestor), windowHeight - 40)
            << "the scrolled area fits inside the window, so the popover is not clipped";
    gtk_popover_popdown(GTK_POPOVER(popover));
    ASSERT_TRUE(waitForHidden(popover));
    gtk_window_resize(GTK_WINDOW(window->getWindow()), 1280, 800);
    settle();
    control->selectTextMode(TextMode::Plain);
}

// A save or autosave whose final replace fails must leave the previous good file exactly as it was, under its own name,
// and must not leave rotation leftovers (.swap, stray temporary files) behind.
TEST_F(ClassroomWorkflow, saveAndAutosaveKeepTheLastGoodFileWhenTheReplaceFails) {
    const auto folder = Util::getTmpDirSubfolder() / "p0-save-failure";
    fs::remove_all(folder);
    fs::create_directories(folder);
    const auto lesson = folder / "lesson.xopp";
    auto bytesOf = [](const fs::path& p) {
        std::ifstream in(p, std::ios::binary);
        return std::string(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
    };
    auto autosaveFile = [&]() {
        for (const auto& entry: fs::directory_iterator(folder)) {
            const auto name = entry.path().filename().string();
            if (name.size() > 14 && name.compare(name.size() - 14, 14, ".autosave.xopp") == 0) {
                return entry.path();
            }
        }
        return fs::path{};
    };

    Document* doc = control->getDocument();
    doc->lock();
    doc->setFilepath(lesson);
    doc->unlock();
    addMarker(control->getCurrentPage()->getSelectedLayer(), 100);

    // A good save and a good autosave first
    {
        auto* job = new SaveJob(control.get());
        EXPECT_TRUE(job->save());
        job->unref();
        auto* autosave = new AutosaveJob(control.get());
        autosave->run();
        autosave->unref();
    }
    ASSERT_TRUE(fs::exists(lesson));
    const auto savedBefore = bytesOf(lesson);
    const auto autosavePath = autosaveFile();
    ASSERT_FALSE(autosavePath.empty()) << "the autosave wrote no file";
    const auto autosavedBefore = bytesOf(autosavePath);
    ASSERT_FALSE(savedBefore.empty());
    ASSERT_FALSE(autosavedBefore.empty());

    // More ink, then both writes fail at the very last step
    addMarker(control->getCurrentPage()->getSelectedLayer(), 300);
    SaveHandler::setReplaceStepForTesting(+[](const fs::path&, const fs::path&, std::string& error) {
        error = "injected failure";
        return false;
    });
    {
        auto* job = new SaveJob(control.get());
        EXPECT_FALSE(job->save());
        job->unref();
        auto* autosave = new AutosaveJob(control.get());
        autosave->run();
        autosave->unref();
    }
    SaveHandler::setReplaceStepForTesting(nullptr);
    settle();

    ASSERT_TRUE(fs::exists(lesson)) << "the real file vanished";
    EXPECT_EQ(bytesOf(lesson), savedBefore) << "the real file changed";
    ASSERT_TRUE(fs::exists(autosavePath)) << "the last autosave vanished";
    EXPECT_EQ(bytesOf(autosavePath), autosavedBefore) << "the last autosave changed";
    for (const auto& entry: fs::directory_iterator(folder)) {
        const auto name = entry.path().filename().string();
        EXPECT_TRUE(name == "lesson.xopp" || name == "lesson.xopp~" || entry.path() == autosavePath)
                << "left behind: " << name;
    }

    // And with the replace working again, the next save goes through
    {
        auto* job = new SaveJob(control.get());
        EXPECT_TRUE(job->save());
        job->unref();
    }
    EXPECT_NE(bytesOf(lesson), savedBefore);
    fs::remove_all(folder);
}

namespace {
bool sidebarShown(MainWindow* window) { return gtk_widget_get_visible(window->get("sidebar")); }

bool labelShown(GtkWidget* button) {
    bool shown = false;
    std::function<void(GtkWidget*)> walk = [&](GtkWidget* w) {
        if (GTK_IS_LABEL(w) && gtk_widget_get_visible(w)) {
            shown = true;
        }
        if (GTK_IS_CONTAINER(w)) {
            GList* children = gtk_container_get_children(GTK_CONTAINER(w));
            for (GList* c = children; c; c = c->next) {
                walk(GTK_WIDGET(c->data));
            }
            g_list_free(children);
        }
    };
    walk(button);
    return shown;
}
}  // namespace

// Canvas first: panels beside the page give way to a narrow window and come back, without touching saved settings
TEST_F(ClassroomWorkflow, panelsStepAsideInNarrowWindowsAndComeBackWithoutChangingSavedSettings) {
    window->toolbarSelected("UTN Teacher");
    gtk_widget_show(window->getWindow());
    settle();
    const int realWidth = gtk_widget_get_allocated_width(window->getWindow());
    auto* settings = control->getSettings();
    ASSERT_TRUE(settings->isSidebarVisible());
    ASSERT_EQ(window->getTeacherWorkspace(), 0);

    window->updateCompactLayout(1500);
    EXPECT_FALSE(window->isThumbnailsAutoHidden());
    EXPECT_TRUE(sidebarShown(window.get()));

    window->updateCompactLayout(1280);
    EXPECT_TRUE(window->isThumbnailsAutoHidden());
    EXPECT_FALSE(sidebarShown(window.get())) << "thumbnails give the page the room at 1280 px";
    EXPECT_TRUE(settings->isSidebarVisible()) << "the saved choice is not rewritten";
    EXPECT_FALSE(window->isInspectorAutoFolded()) << "the settings stay open at 1280 px";
    EXPECT_TRUE(gtk_widget_get_visible(window->get("utnInspectorHost")));

    window->updateCompactLayout(900);
    EXPECT_TRUE(window->isInspectorAutoFolded());
    EXPECT_EQ(window->getTeacherWorkspace(), 0) << "the saved workspace mode is not rewritten";
    EXPECT_TRUE(gtk_widget_get_visible(window->get("utnExpandInspector"))) << "the way back stays on screen";
    auto* appBar = window->get("tbTop1");
    for (const char* label: {"Freeze", "Blank", "Workspace"}) {
        auto* button = buttonNamed(appBar, label);
        ASSERT_NE(button, nullptr) << label;
        EXPECT_TRUE(gtk_widget_get_visible(button)) << label << " must stay reachable at 900 px";
    }
    auto* workspace = buttonNamed(appBar, "Workspace");
    EXPECT_FALSE(labelShown(workspace)) << "Workspace is icon only at 900 px";
    EXPECT_NE(buttonNamed(appBar, "Workspace"), nullptr) << "its name is unchanged for assistive technology";

    // Widening brings both back
    window->updateCompactLayout(1500);
    EXPECT_TRUE(sidebarShown(window.get()));
    EXPECT_FALSE(window->isInspectorAutoFolded());
    EXPECT_TRUE(labelShown(workspace));

    // A teacher's own choice wins over the width policy
    window->updateCompactLayout(1280);
    ASSERT_FALSE(sidebarShown(window.get()));
    control->setShowSidebar(true);
    EXPECT_TRUE(sidebarShown(window.get()));
    EXPECT_FALSE(window->isThumbnailsAutoHidden());
    window->updateCompactLayout(1500);
    EXPECT_TRUE(sidebarShown(window.get()));

    window->updateCompactLayout(900);
    ASSERT_TRUE(window->isInspectorAutoFolded());
    window->setTeacherWorkspace(0);  // "Show settings"
    EXPECT_FALSE(window->isInspectorAutoFolded());
    EXPECT_TRUE(gtk_widget_get_visible(window->get("utnInspectorHost")));
    window->updateCompactLayout(1500);
    EXPECT_TRUE(gtk_widget_get_visible(window->get("utnInspectorHost")));

    window->updateCompactLayout(realWidth);
}

// A width change from a real allocation is applied in an idle callback. A teacher's choice made before that callback
// runs must not be undone by it, and the next width crossing hands control back to the policy.
TEST_F(ClassroomWorkflow, manualPanelChoiceMadeWhileThePolicyIsQueuedIsNotUndone) {
    window->toolbarSelected("UTN Teacher");
    gtk_widget_show(window->getWindow());
    settle();
    const int realWidth = gtk_widget_get_allocated_width(window->getWindow());

    // Sidebar: the policy for 1280 px is queued, the teacher closes and reopens the sidebar, then the queue runs
    window->updateCompactLayout(1500);
    ASSERT_TRUE(sidebarShown(window.get()));
    window->updateCompactLayout(1280, true);
    ASSERT_TRUE(sidebarShown(window.get())) << "queued, not applied yet";
    control->setShowSidebar(false);
    control->setShowSidebar(true);
    settle();
    EXPECT_TRUE(sidebarShown(window.get())) << "the queued policy must not hide what the teacher just opened";
    EXPECT_FALSE(window->isThumbnailsAutoHidden());
    window->updateCompactLayout(1500);
    EXPECT_TRUE(sidebarShown(window.get()));
    window->updateCompactLayout(1280);  // the next crossing: the policy is in charge again
    EXPECT_FALSE(sidebarShown(window.get()));
    EXPECT_TRUE(window->isThumbnailsAutoHidden());

    // Inspector: folding at 900 px is queued, the teacher chooses "Show settings", then the queue runs
    window->updateCompactLayout(1500);
    ASSERT_TRUE(gtk_widget_get_visible(window->get("utnInspectorHost")));
    window->updateCompactLayout(900, true);
    window->setTeacherWorkspace(0);
    settle();
    EXPECT_FALSE(window->isInspectorAutoFolded()) << "the queued policy must not fold what the teacher chose";
    EXPECT_TRUE(gtk_widget_get_visible(window->get("utnInspectorHost")));
    EXPECT_FALSE(gtk_widget_get_visible(window->get("utnExpandInspector")));
    window->updateCompactLayout(1500);
    window->updateCompactLayout(900);  // the next crossing folds it again
    EXPECT_TRUE(window->isInspectorAutoFolded());

    window->updateCompactLayout(realWidth);
    settle();
}

// reloadToolbars() destroys the Pen settings button (and its popover) before toolbarSelected() tears the menu down.
// Repeated reloads must neither touch a stale popover nor leave several settings buttons, and the card must stay usable.
TEST_F(ClassroomWorkflow, toolbarReloadsKeepOnePenSettingsButtonAndAWorkingPopover) {
    window->toolbarSelected("UTN Teacher");
    window->show(nullptr);
    control->selectTool(TOOL_PEN);
    settle();
    for (int i = 0; i < 5; ++i) {
        window->reloadToolbars();
        settle();
    }
    GtkWidget* trigger = widgetNamed(window->getWindow(), "utnPenSettingsButton");
    ASSERT_TRUE(GTK_IS_MENU_BUTTON(trigger));
    GtkWidget* popover = GTK_WIDGET(gtk_menu_button_get_popover(GTK_MENU_BUTTON(trigger)));
    ASSERT_TRUE(GTK_IS_POPOVER(popover));
    gtk_popover_popup(GTK_POPOVER(popover));
    settle();
    EXPECT_TRUE(gtk_widget_get_visible(popover));
    EXPECT_GT(gtk_widget_get_allocated_height(popover), 250);
    gtk_popover_popdown(GTK_POPOVER(popover));
    settle();

    // The dock offers every pen size and shows the real one
    auto* tools = control->getToolHandler();
    const ToolSize previous = tools->getPenSize();
    for (ToolSize size: {TOOL_SIZE_VERY_FINE, TOOL_SIZE_FINE, TOOL_SIZE_MEDIUM, TOOL_SIZE_THICK, TOOL_SIZE_VERY_THICK}) {
        tools->setPenSize(size);
        tools->fireToolChanged();
        settle();
        int active = 0;
        for (const char* name: {"Very fine pen", "Fine pen", "Medium pen", "Thick pen", "Very thick pen"}) {
            auto* radio = buttonAccessibleNamed(window->get("mainOverlay"), name);
            ASSERT_NE(radio, nullptr) << name;
            active += gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(radio)) ? 1 : 0;
        }
        EXPECT_EQ(active, 1) << "exactly one size is shown as selected, size " << static_cast<int>(size);
    }
    tools->setPenSize(previous);
}

namespace {
void addPlainPages(Control* control, int count) {
    for (int i = 0; i < count; ++i) {
        auto page = std::make_shared<XojPage>(600, 800);
        page->setBackgroundType(PageType(PageTypeFormat::Plain));
        control->getDocument()->addPage(page);
    }
    control->fireDocumentChanged(DOCUMENT_CHANGE_COMPLETE);
    control->DocumentHandler::firePageSelected(0);
    settle();
}


std::vector<std::string> buttonTexts(GtkWidget* box) {
    std::vector<std::string> out;
    GList* children = gtk_container_get_children(GTK_CONTAINER(box));
    for (GList* child = children; child; child = child->next) {
        if (GTK_IS_BUTTON(child->data)) {
            out.emplace_back(gtk_button_get_label(GTK_BUTTON(child->data)));
        }
    }
    g_list_free(children);
    return out;
}

/// The Lesson Navigator built the way a toolbar builds it, with its popover contents refreshed as opening does
struct NavigatorUnderTest {
    explicit NavigatorUnderTest(Control* control): nav("LESSON_NAVIGATOR", control), item(nav.createItem(true)) {
        popover = GTK_WIDGET(gtk_menu_button_get_popover(GTK_MENU_BUTTON(firstMenuButton(item.get()))));
        refresh();
    }
    void refresh() {
        g_signal_emit_by_name(popover, "show");
        settle();
    }
    void goTo(const char* text) {
        gtk_editable_set_text(GTK_EDITABLE(widgetNamed(popover, "utnGoToPage")), text);
        g_signal_emit_by_name(widgetNamed(popover, "utnGoToPage"), "activate");
        settle();
    }
    GtkWidget* named(const char* name) { return widgetNamed(popover, name); }
    ToolLessonNavigator nav;
    xoj::util::WidgetSPtr item;
    GtkWidget* popover = nullptr;
};
}  // namespace

TEST_F(ClassroomWorkflow, lessonNavigatorGoesToATypedPageKeepsToolAndZoomAndExplainsNumbersThatAreNotPages) {
    addPlainPages(control.get(), 3);  // five pages
    NavigatorUnderTest navigator(control.get());
    ASSERT_NE(navigator.named("utnGoToPage"), nullptr) << "a page number box belongs in the navigator";
    ASSERT_NE(navigator.named("utnGoToPageError"), nullptr);
    control->selectTool(TOOL_HIGHLIGHTER);
    const double zoom = control->getZoomControl()->getZoom();

    navigator.goTo("4");
    EXPECT_EQ(control->getCurrentPageNo(), 3u);
    EXPECT_STREQ(gtk_label_get_text(GTK_LABEL(navigator.named("utnGoToPageError"))), "");
    EXPECT_EQ(control->getToolHandler()->getToolType(), TOOL_HIGHLIGHTER) << "navigating keeps the tool";
    EXPECT_DOUBLE_EQ(control->getZoomControl()->getZoom(), zoom) << "and the zoom";
    EXPECT_FALSE(control->getUndoRedoHandler()->canUndo()) << "going to a page changes nothing in the document";

    navigator.goTo("9");
    EXPECT_EQ(control->getCurrentPageNo(), 3u) << "a number that is not a page goes nowhere";
    const std::string message = gtk_label_get_text(GTK_LABEL(navigator.named("utnGoToPageError")));
    EXPECT_NE(message.find("1 to 5"), std::string::npos) << "it says which pages exist: " << message;

    navigator.goTo("abc");
    EXPECT_EQ(control->getCurrentPageNo(), 3u);
    navigator.goTo("1");
    EXPECT_EQ(control->getCurrentPageNo(), 0u);
    EXPECT_STREQ(gtk_label_get_text(GTK_LABEL(navigator.named("utnGoToPageError"))), "") << "a good number clears it";
    control->selectTool(TOOL_PEN);
}

TEST_F(ClassroomWorkflow, lessonNavigatorBookmarksThePageUndoablyAndNeverReplacesATeachersLabel) {
    addPlainPages(control.get(), 2);
    NavigatorUnderTest navigator(control.get());
    auto* bookmark = navigator.named("utnBookmarkButton");
    ASSERT_NE(bookmark, nullptr);
    auto page = control->getDocument()->getPage(0);
    EXPECT_STREQ(gtk_button_get_label(GTK_BUTTON(bookmark)), "Bookmark this page");

    gtk_button_clicked(GTK_BUTTON(bookmark));
    navigator.refresh();
    EXPECT_EQ(page->getUtnPageLabel(), "Bookmark");
    EXPECT_STREQ(gtk_button_get_label(GTK_BUTTON(bookmark)), "Remove bookmark");
    EXPECT_EQ(buttonTexts(navigator.named("utnLessonMap")), (std::vector<std::string>{"1   Bookmark"}))
            << "a bookmark is in the lesson map";

    control->getUndoRedoHandler()->undo();
    EXPECT_EQ(page->getUtnPageLabel(), "") << "undo takes it off";
    control->getUndoRedoHandler()->redo();
    EXPECT_EQ(page->getUtnPageLabel(), "Bookmark");

    gtk_button_clicked(GTK_BUTTON(bookmark));
    navigator.refresh();
    EXPECT_EQ(page->getUtnPageLabel(), "") << "pressing it again removes it";

    page->setUtnPageLabel("Answer Key");
    navigator.refresh();
    EXPECT_STREQ(gtk_button_get_label(GTK_BUTTON(bookmark)), "Page is labelled");
    EXPECT_FALSE(gtk_widget_get_sensitive(bookmark)) << "the button cannot overwrite or remove the label of the teacher";
    EXPECT_EQ(page->getUtnPageLabel(), "Answer Key");
    page->setUtnPageLabel("");
}

TEST_F(ClassroomWorkflow, lessonNavigatorMarksTheCurrentPageAndListsRecentPagesThroughInsertDeleteAndMove) {
    addPlainPages(control.get(), 3);  // five pages
    NavigatorUnderTest navigator(control.get());
    auto doc = control->getDocument();
    auto pageAt = [&](size_t i) { return doc->getPage(i); };
    PageRef first = pageAt(0), fourth = pageAt(3);

    navigator.goTo("4");
    navigator.goTo("2");
    ASSERT_EQ(control->getCurrentPageNo(), 1u);
    EXPECT_EQ(buttonTexts(navigator.named("utnRecentPages")), (std::vector<std::string>{"Page 4", "Page 1"}))
            << "recent pages, newest first, without the page being shown";

    // A new page in front moves every number up; the list follows the pages, not the numbers
    control->insertNewPage(0);
    settle();
    navigator.refresh();
    const size_t current = control->getCurrentPageNo();
    std::vector<std::string> expected;
    for (const PageRef& p: {fourth, first}) {
        const size_t index = doc->indexOf(p);
        if (index != current) {
            expected.push_back("Page " + std::to_string(index + 1));
        }
    }
    EXPECT_EQ(buttonTexts(navigator.named("utnRecentPages")), expected);
    EXPECT_EQ(doc->indexOf(first), 1u) << "the first page moved down by one";

    // Marked pages follow a page that moves
    navigator.goTo("1");
    gtk_button_clicked(GTK_BUTTON(navigator.named("utnBookmarkButton")));
    PageRef marked = pageAt(0);
    EXPECT_EQ(marked->getUtnPageLabel(), "Bookmark");
    control->getActionDatabase()->fireActivateAction(Action::MOVE_PAGE_TOWARDS_END);
    settle();
    navigator.refresh();
    const size_t movedTo = doc->indexOf(marked);
    EXPECT_GT(movedTo, 0u);
    EXPECT_EQ(buttonTexts(navigator.named("utnLessonMap")),
              (std::vector<std::string>{std::to_string(movedTo + 1) + "   Bookmark"}));
    ASSERT_NE(navigator.named("utnLessonMapCurrent"), nullptr) << "the page being shown is marked in the map";

    // Deleting a page leaves no entry that points nowhere
    control->deletePage();
    settle();
    navigator.refresh();
    for (const auto& text: buttonTexts(navigator.named("utnRecentPages"))) {
        const size_t number = static_cast<size_t>(std::stoul(text.substr(5)));
        EXPECT_GE(number, 1u);
        EXPECT_LE(number, doc->getPageCount()) << text;
    }
    marked->setUtnPageLabel("");
}

TEST_F(ClassroomWorkflow, lessonNavigatorBookmarksSurviveSaveAndReopenAndThumbnailsOpenTheSidebar) {
    addPlainPages(control.get(), 2);
    NavigatorUnderTest navigator(control.get());
    navigator.goTo("3");
    gtk_button_clicked(GTK_BUTTON(navigator.named("utnBookmarkButton")));

    const auto file = Util::getTmpDirSubfolder() / "lesson-navigator-bookmark.xopp";
    SaveHandler saver;
    saver.prepareSave(control->getDocument(), file);
    saver.saveTo(file);
    ASSERT_TRUE(saver.getErrorMessage().empty()) << saver.getErrorMessage();
    auto reopened = LoadHandler{}.loadDocument(file);
    ASSERT_NE(reopened, nullptr);
    ASSERT_EQ(reopened->getPageCount(), 4u);
    EXPECT_EQ(reopened->getPage(2)->getUtnPageLabel(), "Bookmark") << "the bookmark is saved with its page";
    EXPECT_EQ(reopened->getPage(0)->getUtnPageLabel(), "");
    control->getDocument()->getPage(2)->setUtnPageLabel("");

    control->setShowSidebar(false);
    settle();
    auto* thumbnails = buttonNamed(navigator.popover, "Show page thumbnails");
    ASSERT_NE(thumbnails, nullptr);
    gtk_button_clicked(GTK_BUTTON(thumbnails));
    settle();
    EXPECT_TRUE(control->getSettings()->isSidebarVisible());
    EXPECT_EQ(control->getSidebar()->getSelectedTab(), SIDEBAR_PAGES_TAB) << "the page thumbnails tab";
}

namespace {
/// Delivers the key through the main loop, like a real press. Whether a handler took the key is not reported by
/// gtk_main_do_event, and delivering it any other way (gtk_widget_event) would skip the modal grab the palette tests
/// depend on, so every test checks the effect of the key instead.
void sendKey(GtkWidget* widget, guint keyval, guint state) {
    FakeKey fake(widget, keyval, state);
    // Real key events name their keyboard and are delivered by the main loop (which also applies the modal grab of the
    // palette, and sets the current event the canvas key handler reads)
    gdk_event_set_device(fake.event, gdk_seat_get_keyboard(gdk_display_get_default_seat(gdk_display_get_default())));
    gtk_main_do_event(fake.event);
    settle();
}

std::string accessibleName(GtkWidget* widget) {
    AtkObject* accessible = gtk_widget_get_accessible(widget);
    const char* name = accessible ? atk_object_get_name(accessible) : nullptr;
    return name ? name : "";
}

/// What the palette lists, in order, as a screen reader would be told
std::vector<std::string> paletteRows(CommandPalette* palette) {
    std::vector<std::string> out;
    GtkWidget* list = widgetNamed(palette->getWidget(), "utnPaletteList");
    GList* children = gtk_container_get_children(GTK_CONTAINER(list));
    for (GList* child = children; child; child = child->next) {
        out.push_back(accessibleName(GTK_WIDGET(child->data)));
    }
    g_list_free(children);
    return out;
}

std::string selectedRow(CommandPalette* palette) {
    GtkWidget* list = widgetNamed(palette->getWidget(), "utnPaletteList");
    auto* row = gtk_list_box_get_selected_row(GTK_LIST_BOX(list));
    return row ? accessibleName(GTK_WIDGET(row)) : std::string{};
}

void typeInPalette(CommandPalette* palette, const char* text) {
    GtkWidget* search = widgetNamed(palette->getWidget(), "utnPaletteSearch");
    gtk_entry_set_text(GTK_ENTRY(search), text);
    // The entry says "search-changed" about 150 ms after the text changes: wait for that, so a late second signal cannot
    // rebuild the list in the middle of a test
    for (int i = 0; i < 5; ++i) {
        settle();
    }
}

bool startsWith(const std::string& text, const char* prefix) { return text.rfind(prefix, 0) == 0; }
}  // namespace

TEST_F(ClassroomWorkflow, ctrlKOpensThePaletteAndEnterRunsTheToolActionEscapeLeavesAndFocusComesBack) {
    window->toolbarSelected("UTN Teacher");
    window->show(nullptr);
    settle();
    GtkWidget* top = GTK_WIDGET(window->getWindow());
    // Whatever holds the keyboard when the palette opens must hold it again afterwards
    GtkWidget* canvas = gtk_window_get_focus(GTK_WINDOW(top));
    if (!canvas || GTK_IS_CONTAINER(canvas)) {  // a container is not where keys are typed, and grabbing it moves on to a child
        gtk_window_set_focus(GTK_WINDOW(top), window->getXournal()->getWidget());
        settle();
        canvas = gtk_window_get_focus(GTK_WINDOW(top));
    }
    ASSERT_NE(canvas, nullptr);
    control->selectTool(TOOL_HIGHLIGHTER);
    settle();

    // Keys that are not Ctrl+K leave the palette shut
    sendKey(top, GDK_KEY_k, 0);
    sendKey(top, GDK_KEY_k, GDK_CONTROL_MASK | GDK_SHIFT_MASK);
    EXPECT_EQ(window->getCommandPalette() != nullptr && window->getCommandPalette()->isOpen(), false);

    sendKey(top, GDK_KEY_k, GDK_CONTROL_MASK);
    auto* palette = window->getCommandPalette();
    ASSERT_NE(palette, nullptr);
    ASSERT_TRUE(palette->isOpen());
    EXPECT_NE(widgetNamed(palette->getWidget(), "utnPaletteSearch"), nullptr);
    EXPECT_EQ(accessibleName(widgetNamed(palette->getWidget(), "utnPaletteSearch")), "Search commands");
    EXPECT_FALSE(paletteRows(palette).empty()) << "an empty search lists the commands";

    typeInPalette(palette, "pen");
    ASSERT_FALSE(paletteRows(palette).empty());
    EXPECT_TRUE(startsWith(selectedRow(palette), "Pen, Tool")) << selectedRow(palette);

    // Up and Down move the choice (over a search with several available commands) and never leave the search box
    typeInPalette(palette, "page");
    const std::string first = selectedRow(palette);
    sendKey(palette->getWidget(), GDK_KEY_Down, 0);
    EXPECT_NE(selectedRow(palette), first);
    sendKey(palette->getWidget(), GDK_KEY_Up, 0);
    EXPECT_EQ(selectedRow(palette), first);
    typeInPalette(palette, "pen");
    EXPECT_TRUE(startsWith(selectedRow(palette), "Pen, Tool")) << selectedRow(palette);

    sendKey(palette->getWidget(), GDK_KEY_Return, 0);
    settle();
    EXPECT_FALSE(palette->isOpen());
    EXPECT_EQ(control->getToolHandler()->getToolType(), TOOL_PEN) << "Enter ran the same action as the Pen button";
    EXPECT_EQ(gtk_window_get_focus(GTK_WINDOW(top)), canvas) << "the keyboard went back to the page";

    // Escape puts it away and changes nothing
    control->selectTool(TOOL_HIGHLIGHTER);
    sendKey(top, GDK_KEY_k, GDK_CONTROL_MASK);
    ASSERT_TRUE(palette->isOpen());
    typeInPalette(palette, "eraser");
    sendKey(palette->getWidget(), GDK_KEY_Escape, 0);
    settle();
    EXPECT_FALSE(palette->isOpen());
    EXPECT_EQ(control->getToolHandler()->getToolType(), TOOL_HIGHLIGHTER);
    EXPECT_EQ(gtk_window_get_focus(GTK_WINDOW(top)), canvas);

    // Ctrl+K again closes it, too
    sendKey(top, GDK_KEY_k, GDK_CONTROL_MASK);
    ASSERT_TRUE(palette->isOpen());
    sendKey(palette->getWidget(), GDK_KEY_k, GDK_CONTROL_MASK);
    settle();
    EXPECT_FALSE(palette->isOpen());
    control->selectTool(TOOL_PEN);
}

TEST_F(ClassroomWorkflow, paletteListsUnavailableCommandsInWordsAndNeverRunsThem) {
    window->toolbarSelected("UTN Teacher");
    window->show(nullptr);
    settle();
    window->openCommandPalette();
    auto* palette = window->getCommandPalette();
    ASSERT_TRUE(palette->isOpen());

    typeInPalette(palette, "leave present");
    const auto rows = paletteRows(palette);
    ASSERT_FALSE(rows.empty());
    EXPECT_EQ(rows[0], "Leave Present, Workspace, shortcut Ctrl+Shift+P, unavailable");
    EXPECT_EQ(selectedRow(palette), "") << "an unavailable command cannot be chosen";
    sendKey(palette->getWidget(), GDK_KEY_Return, 0);
    EXPECT_TRUE(palette->isOpen()) << "Enter on nothing runs nothing and keeps the palette open";
    EXPECT_EQ(window->getSelectedToolbar()->getId(), "UTN Teacher");

    // An available command sorts above an unavailable one for the same words
    typeInPalette(palette, "present");
    const auto present = paletteRows(palette);
    ASSERT_GE(present.size(), 2u);
    EXPECT_TRUE(startsWith(present.front(), "Present workspace")) << present.front();
    EXPECT_NE(present.back().find("unavailable"), std::string::npos);
    window->getCommandPalette()->close();
    settle();
}

TEST_F(ClassroomWorkflow, paletteWorkspaceAndClassroomCommandsDoWhatTheirButtonsDo) {
    window->toolbarSelected("UTN Teacher");
    window->show(nullptr);
    settle();
    auto run = [&](const char* query) {
        window->openCommandPalette();
        auto* palette = window->getCommandPalette();
        EXPECT_TRUE(palette->isOpen());
        typeInPalette(palette, query);
        sendKey(palette->getWidget(), GDK_KEY_Return, 0);
        settle();
        EXPECT_FALSE(palette->isOpen()) << query;
    };

    ASSERT_NO_FATAL_FAILURE(run("mark workspace"));
    EXPECT_EQ(window->getSelectedToolbar()->getId(), "UTN Marking");
    ASSERT_NO_FATAL_FAILURE(run("present workspace"));
    EXPECT_EQ(window->getSelectedToolbar()->getId(), "UTN Present");
    ASSERT_NO_FATAL_FAILURE(run("show tools"));
    EXPECT_TRUE(window->isPresentToolsRevealed()) << "the same reveal as Ctrl+Shift+T";
    ASSERT_NO_FATAL_FAILURE(run("leave present"));
    EXPECT_EQ(window->getSelectedToolbar()->getId(), "UTN Marking") << "back to where the teacher came from";
    window->toolbarSelected("UTN Teacher");

    ASSERT_NO_FATAL_FAILURE(run("spotlight"));
    EXPECT_TRUE(control->getToolHandler()->isSpotlightEnabled());
    ASSERT_NO_FATAL_FAILURE(run("back to normal"));
    EXPECT_FALSE(control->getToolHandler()->isSpotlightEnabled());
    ASSERT_NO_FATAL_FAILURE(run("curtain"));
    EXPECT_TRUE(control->getToolHandler()->isCurtainEnabled());
    ASSERT_NO_FATAL_FAILURE(run("exit spotlight"));
    EXPECT_FALSE(control->getToolHandler()->isCurtainEnabled());

    // Freeze is greyed out exactly while the popover greys its own Freeze: Student View must be open first
    auto* students = window->getToolMenuHandler()->getStudentView();
    ASSERT_NE(students, nullptr);
    // Opening Student View is fullscreen and is never done in these tests: if one is open, the check below would prove
    // nothing, so say so instead of passing without having looked
    ASSERT_FALSE(students->isWindowOpen()) << "Student View is open, so the Freeze check cannot be made";
    // With one monitor the resting state is SameScreen, with two or more it is Off: either way the palette must not change it
    const auto restingState = students->getState();
    window->openCommandPalette();
    auto* palette = window->getCommandPalette();
    typeInPalette(palette, "freeze");
    const auto rows = paletteRows(palette);
    ASSERT_FALSE(rows.empty());
    EXPECT_NE(std::find(rows.begin(), rows.end(), "Freeze students' screen, Classroom, unavailable"), rows.end())
            << "Freeze is listed, and says why it cannot run";
    EXPECT_EQ(students->getState(), restingState);
    palette->close();
    settle();
}

TEST_F(ClassroomWorkflow, paletteGoesToATypedPageBookmarksAndNeverListsPageLabels) {
    addPlainPages(control.get(), 3);  // five pages
    window->toolbarSelected("UTN Teacher");
    window->show(nullptr);
    settle();
    window->openCommandPalette();
    auto* palette = window->getCommandPalette();

    typeInPalette(palette, "page 4");
    EXPECT_TRUE(startsWith(selectedRow(palette), "Go to page 4, Page")) << selectedRow(palette);
    sendKey(palette->getWidget(), GDK_KEY_Return, 0);
    settle();
    EXPECT_EQ(control->getCurrentPageNo(), 3u);

    window->openCommandPalette();
    typeInPalette(palette, "bookmark");
    EXPECT_TRUE(startsWith(selectedRow(palette), "Bookmark this page, Page")) << selectedRow(palette);
    sendKey(palette->getWidget(), GDK_KEY_Return, 0);
    settle();
    EXPECT_EQ(control->getDocument()->getPage(3)->getUtnPageLabel(), "Bookmark");
    control->getUndoRedoHandler()->undo();
    EXPECT_EQ(control->getDocument()->getPage(3)->getUtnPageLabel(), "") << "the same undoable label as the button";

    window->openCommandPalette();
    typeInPalette(palette, "page 9");
    for (const auto& row: paletteRows(palette)) {
        EXPECT_FALSE(startsWith(row, "Go to page")) << "there is no page 9: " << row;
    }
    palette->close();

    // A label is the teacher's private note: it can be on screen while the class watches, so it is never listed
    control->getDocument()->getPage(3)->setUtnPageLabel("Secret answers");
    window->openCommandPalette();
    typeInPalette(palette, "secret");
    EXPECT_TRUE(paletteRows(palette).empty());
    typeInPalette(palette, "bookmark");
    ASSERT_FALSE(paletteRows(palette).empty());
    EXPECT_NE(paletteRows(palette)[0].find("unavailable"), std::string::npos)
            << "it does not replace a label of the teacher";
    palette->close();
    settle();
    control->getDocument()->getPage(3)->setUtnPageLabel("");
}

TEST_F(ClassroomWorkflow, navigatingAThreeHundredPageDocumentRapidlyWithThumbnailsOpenStaysResponsive) {
    addPlainPages(control.get(), 298);  // 300 pages
    ASSERT_EQ(control->getDocument()->getPageCount(), 300u);
    window->toolbarSelected("UTN Teacher");
    window->show(nullptr);
    control->setShowSidebar(true);
    settle();
    NavigatorUnderTest navigator(control.get());
    ToolLessonNavigator::showThumbnails(control.get());
    settle();

    const gint64 start = g_get_monotonic_time();
    gint64 slowest = 0;
    const char* targets[] = {"300", "1", "150", "299", "2", "77", "250", "10", "280", "40"};
    for (int round = 0; round < 5; ++round) {
        for (const char* target: targets) {
            const gint64 before = g_get_monotonic_time();
            navigator.goTo(target);
            slowest = std::max(slowest, g_get_monotonic_time() - before);
        }
    }
    const double totalMs = static_cast<double>(g_get_monotonic_time() - start) / 1000.0;
    g_message("UTN navigator perf: 50 jumps over 300 pages in %.0f ms, slowest %.0f ms (each includes a 60 ms settle)",
              totalMs, static_cast<double>(slowest) / 1000.0);
    EXPECT_EQ(control->getCurrentPageNo(), 39u);
    EXPECT_LT(slowest, 2 * G_USEC_PER_SEC) << "no single jump may freeze the window for two seconds";
    EXPECT_FALSE(control->getUndoRedoHandler()->canUndo()) << "rapid navigation leaves the document untouched";
}

TEST_F(ClassroomWorkflow, paletteEnterRunsWhatWasTypedEvenBeforeTheSearchHasCaughtUp) {
    addPlainPages(control.get(), 3);  // five pages
    window->toolbarSelected("UTN Teacher");
    window->show(nullptr);
    settle();
    control->selectTool(TOOL_HIGHLIGHTER);
    window->openCommandPalette();
    auto* palette = window->getCommandPalette();
    ASSERT_TRUE(palette->isOpen());

    // The entry reports a changed search about 150 ms later. Enter pressed sooner must still run what was typed, not the
    // command that happened to be on top of the old list.
    gtk_entry_set_text(GTK_ENTRY(widgetNamed(palette->getWidget(), "utnPaletteSearch")), "last page");
    sendKey(palette->getWidget(), GDK_KEY_Return, 0);
    EXPECT_EQ(control->getCurrentPageNo(), 4u) << "the last page was typed";
    EXPECT_EQ(control->getToolHandler()->getToolType(), TOOL_HIGHLIGHTER) << "the top command of the old list must not run";
    EXPECT_FALSE(palette->isOpen());
    control->selectTool(TOOL_PEN);
}

// The Blank cover hides the page from the class on the same screen. The palette is a window of its own, so the cover does
// not hide it: command names, "Remove bookmark" and "Unavailable: This page already has a label" would be on show, and the
// class could watch commands run. It must not open over the cover, and must go away when the cover goes up.
TEST_F(ClassroomWorkflow, paletteNeverOpensOverTheBlankCoverAndClosesWhenTheCoverGoesUp) {
    window->toolbarSelected("UTN Teacher");
    window->show(nullptr);
    settle();
    GtkWidget* top = GTK_WIDGET(window->getWindow());
    auto* students = window->getToolMenuHandler()->getStudentView();
    ASSERT_NE(students, nullptr);
    const auto paletteOpen = [&] { return window->getCommandPalette() && window->getCommandPalette()->isOpen(); };

    students->showTeacherBlankCover(true);  // the cover inside the main window; never Student View itself
    settle();
    ASSERT_TRUE(students->isTeacherCoverShown());
    sendKey(top, GDK_KEY_k, GDK_CONTROL_MASK);
    EXPECT_FALSE(paletteOpen()) << "Ctrl+K opened the palette over the cover";
    window->openCommandPalette();  // what the Workspace menu button calls
    settle();
    EXPECT_FALSE(paletteOpen()) << "the Workspace menu opened the palette over the cover";

    students->showTeacherBlankCover(false);
    settle();
    sendKey(top, GDK_KEY_k, GDK_CONTROL_MASK);
    ASSERT_TRUE(paletteOpen()) << "with the cover down Ctrl+K opens it again";

    students->showTeacherBlankCover(true);
    settle();
    EXPECT_FALSE(paletteOpen()) << "an open palette was left on show when the cover went up";

    students->showTeacherBlankCover(false);
    settle();
    sendKey(top, GDK_KEY_k, GDK_CONTROL_MASK);
    EXPECT_TRUE(paletteOpen());
    window->getCommandPalette()->close();
    settle();
}

// Control::block() disables every action while a save or export runs; GAction ignores a disabled action, so the palette
// has to say the same, in words, for the commands that do not go through one (and for Ctrl+K itself).
TEST_F(ClassroomWorkflow, paletteRefusesEveryCommandWhileTheApplicationIsBusySavingOrExporting) {
    addPlainPages(control.get(), 3);  // five pages
    window->toolbarSelected("UTN Teacher");
    window->show(nullptr);
    settle();
    GtkWidget* top = GTK_WIDGET(window->getWindow());
    control->setShowSidebar(false);
    settle();
    ASSERT_FALSE(control->getSettings()->isSidebarVisible());
    const auto paletteOpen = [&] { return window->getCommandPalette() && window->getCommandPalette()->isOpen(); };

    // Open before the job starts, then block: Enter must run nothing
    window->openCommandPalette();
    auto* palette = window->getCommandPalette();
    ASSERT_TRUE(paletteOpen());
    const auto enterWhileBusy = [&](const char* query, const char* row) {
        typeInPalette(palette, query);
        ASSERT_TRUE(startsWith(selectedRow(palette), row)) << selectedRow(palette);
        control->block("Saving");
        sendKey(palette->getWidget(), GDK_KEY_Return, 0);
        const bool stillOpen = paletteOpen();
        control->unblock();
        settle();
        EXPECT_TRUE(stillOpen) << row << ": a refused command leaves the palette open";
    };
    ASSERT_NO_FATAL_FAILURE(enterWhileBusy("thumbnails", "Show page thumbnails"));
    EXPECT_FALSE(control->getSettings()->isSidebarVisible()) << "Show page thumbnails ran while the application was busy";
    ASSERT_NO_FATAL_FAILURE(enterWhileBusy("page 2", "Go to page 2"));
    EXPECT_EQ(control->getCurrentPageNo(), 0u) << "Go to page ran while the application was busy";
    ASSERT_NO_FATAL_FAILURE(enterWhileBusy("sidebar", "Show sidebar"));
    EXPECT_FALSE(control->getSettings()->isSidebarVisible()) << "the sidebar command ran while the application was busy";
    palette->close();
    settle();
    control->block("Saving");

    // The palette does not open at all, and every command says why it cannot run
    sendKey(top, GDK_KEY_k, GDK_CONTROL_MASK);
    EXPECT_FALSE(paletteOpen()) << "Ctrl+K opened the palette while the application was busy";
    window->openCommandPalette();
    EXPECT_FALSE(paletteOpen());
    const auto busyCommands = palette->describeCommands();
    ASSERT_FALSE(busyCommands.empty());
    for (const auto& command: busyCommands) {
        EXPECT_FALSE(command.enabled) << command.id;
        EXPECT_EQ(command.unavailableReason, "Busy saving or exporting") << command.id;
    }

    // The navigator's own paths are shut too
    EXPECT_EQ(ToolLessonNavigator::toggleBookmark(control.get()), utn::BookmarkChange::KeepLabel);
    EXPECT_EQ(control->getCurrentPage()->getUtnPageLabel(), "") << "a label was written while the application was busy";
    EXPECT_FALSE(control->getUndoRedoHandler()->canUndo());
    ToolLessonNavigator::showThumbnails(control.get());
    EXPECT_FALSE(control->getSettings()->isSidebarVisible());

    // Once it is free again the same commands work
    control->unblock();
    settle();
    sendKey(top, GDK_KEY_k, GDK_CONTROL_MASK);
    ASSERT_TRUE(paletteOpen()) << "with the application free again Ctrl+K opens the palette";
    typeInPalette(palette, "thumbnails");
    sendKey(palette->getWidget(), GDK_KEY_Return, 0);
    EXPECT_TRUE(control->getSettings()->isSidebarVisible());
    EXPECT_FALSE(paletteOpen());
}

// A second save or export started while one runs must not let the first one's end reopen the gates.
TEST_F(ClassroomWorkflow, nestedBlocksKeepTheApplicationBusyUntilTheLastOneIsReleased) {
    ASSERT_FALSE(control->isBlocked());
    control->block("First");
    control->block("Second");
    ASSERT_TRUE(control->isBlocked());
    control->unblock();
    EXPECT_TRUE(control->isBlocked()) << "the first job is still running";
    EXPECT_FALSE(control->getActionDatabase()->isActionEnabled(Action::SELECT_TOOL)) << "and its actions stay disabled";
    window->openCommandPalette();
    EXPECT_FALSE(window->getCommandPalette() && window->getCommandPalette()->isOpen());
    control->unblock();
    EXPECT_FALSE(control->isBlocked());
    EXPECT_TRUE(control->getActionDatabase()->isActionEnabled(Action::SELECT_TOOL));
    control->unblock();  // one too many changes nothing
    EXPECT_FALSE(control->isBlocked());
    control->block("Again");
    EXPECT_TRUE(control->isBlocked());
    control->unblock();
    EXPECT_FALSE(control->isBlocked());
}

// A palette that is already open when a save starts must show the truth the next time it lists, not what it knew at opening.
TEST_F(ClassroomWorkflow, anOpenPaletteListsCommandsAsBusyOnceASaveStartsAndAvailableAgainAfterwards) {
    window->toolbarSelected("UTN Teacher");
    window->show(nullptr);
    settle();
    control->setShowSidebar(false);
    settle();
    window->openCommandPalette();
    auto* palette = window->getCommandPalette();
    ASSERT_TRUE(palette->isOpen());
    typeInPalette(palette, "show page thumbnails");
    ASSERT_TRUE(startsWith(selectedRow(palette), "Show page thumbnails, Page")) << selectedRow(palette);

    control->block("Saving");
    typeInPalette(palette, "page thumbnails");
    auto rows = paletteRows(palette);
    ASSERT_FALSE(rows.empty());
    EXPECT_NE(rows[0].find("unavailable"), std::string::npos) << rows[0];
    EXPECT_EQ(selectedRow(palette), "") << "nothing can be chosen while busy";
    sendKey(palette->getWidget(), GDK_KEY_Return, 0);
    EXPECT_TRUE(palette->isOpen());
    EXPECT_FALSE(control->getSettings()->isSidebarVisible());

    control->unblock();
    settle();
    typeInPalette(palette, "page thumbnail");
    rows = paletteRows(palette);
    ASSERT_FALSE(rows.empty());
    EXPECT_EQ(rows[0].find("unavailable"), std::string::npos) << rows[0];
    EXPECT_TRUE(startsWith(selectedRow(palette), "Show page thumbnails, Page")) << selectedRow(palette);
    palette->close();
    settle();
}

// The toolbar button says "Page 3/10 · Bookmark". The palette, and Undo and Redo, change the label without going through
// the navigator's own buttons, so the button has to follow them.
TEST_F(ClassroomWorkflow, theNavigatorButtonFollowsABookmarkFromThePaletteAndItsUndoAndRedo) {
    addPlainPages(control.get(), 3);  // five pages
    window->toolbarSelected("UTN Teacher");
    window->show(nullptr);
    settle();
    NavigatorUnderTest navigator(control.get());
    navigator.goTo("3");
    ASSERT_EQ(control->getCurrentPageNo(), 2u);
    auto* button = GTK_BUTTON(firstMenuButton(navigator.item.get()));
    const auto buttonText = [&] { return std::string(gtk_button_get_label(button)); };
    ASSERT_EQ(buttonText(), "Page 3/5");

    window->openCommandPalette();
    auto* palette = window->getCommandPalette();
    typeInPalette(palette, "bookmark");
    ASSERT_TRUE(startsWith(selectedRow(palette), "Bookmark this page")) << selectedRow(palette);
    sendKey(palette->getWidget(), GDK_KEY_Return, 0);
    ASSERT_EQ(control->getDocument()->getPage(2)->getUtnPageLabel(), "Bookmark");
    EXPECT_NE(buttonText().find("Bookmark"), std::string::npos) << "after the palette bookmark: " << buttonText();

    control->getUndoRedoHandler()->undo();
    settle();
    EXPECT_EQ(buttonText().find("Bookmark"), std::string::npos) << "after undo: " << buttonText();
    control->getUndoRedoHandler()->redo();
    settle();
    EXPECT_NE(buttonText().find("Bookmark"), std::string::npos) << "after redo: " << buttonText();
    control->getUndoRedoHandler()->undo();
    settle();
}

// A key can arrive before the entry has said its text changed (it does so about 150 ms later). Down must then move over the
// list of what was typed, and the late "changed" must not put the choice back on the first row.
TEST_F(ClassroomWorkflow, downAfterTypingChoosesInTheNewListAndALateSearchChangedKeepsTheChoice) {
    window->toolbarSelected("UTN Teacher");
    window->show(nullptr);
    settle();
    window->openCommandPalette();
    auto* palette = window->getCommandPalette();
    ASSERT_TRUE(palette->isOpen());

    // What a settled search for "tool" looks like, and where Down goes from its first row
    typeInPalette(palette, "tool");
    const auto rows = paletteRows(palette);
    const std::string firstRow = selectedRow(palette);
    ASSERT_FALSE(firstRow.empty());
    sendKey(palette->getWidget(), GDK_KEY_Down, 0);
    const std::string secondRow = selectedRow(palette);
    ASSERT_NE(secondRow, firstRow) << "the search needs two available rows";

    typeInPalette(palette, "page");
    ASSERT_NE(paletteRows(palette), rows);
    // Same words again, but now the entry has not reported them yet when Down arrives
    GtkWidget* search = widgetNamed(palette->getWidget(), "utnPaletteSearch");
    bool lateSignalSeen = false;  // set after the palette's own handler (connected first) has run
    const gulong watcher = g_signal_connect(search, "search-changed", G_CALLBACK(+[](GtkSearchEntry*, gpointer data) {
                                                *static_cast<bool*>(data) = true;
                                            }),
                                            &lateSignalSeen);
    gtk_entry_set_text(GTK_ENTRY(search), "tool");
    sendKey(palette->getWidget(), GDK_KEY_Down, 0);
    const gint64 deadline = g_get_monotonic_time() + 5 * G_USEC_PER_SEC;
    while (!lateSignalSeen && g_get_monotonic_time() < deadline) {
        settle();
    }
    g_signal_handler_disconnect(search, watcher);
    ASSERT_TRUE(lateSignalSeen) << "the entry never reported the change, so the test proved nothing";
    EXPECT_EQ(paletteRows(palette), rows) << "the list is that of the typed words";
    EXPECT_EQ(selectedRow(palette), secondRow) << "Down moved over the new list, and the late signal kept the choice";
    palette->close();
    settle();
}

namespace {
const Action* actionBehind(const std::string& id) {
    static const std::map<std::string, Action> actions = {
            {"tool.pen", Action::SELECT_TOOL},
            {"tool.highlighter", Action::SELECT_TOOL},
            {"tool.eraser", Action::SELECT_TOOL},
            {"tool.text", Action::SELECT_TOOL},
            {"tool.select-rect", Action::SELECT_TOOL},
            {"tool.select-region", Action::SELECT_TOOL},
            {"tool.hand", Action::SELECT_TOOL},
            {"tool.laser", Action::SELECT_TOOL},
            {"edit.undo", Action::UNDO},
            {"edit.redo", Action::REDO},
            {"page.goto", Action::GOTO_PAGE},
            {"page.next", Action::GOTO_NEXT},
            {"page.previous", Action::GOTO_PREVIOUS},
            {"page.first", Action::GOTO_FIRST},
            {"page.last", Action::GOTO_LAST},
            {"page.next-annotated", Action::GOTO_NEXT_ANNOTATED_PAGE},
            {"page.previous-annotated", Action::GOTO_PREVIOUS_ANNOTATED_PAGE},
            {"page.back", Action::NAVIGATE_BACK},
            {"page.forward", Action::NAVIGATE_FORWARD},
            {"page.new-after", Action::NEW_PAGE_AFTER},
            {"page.new-before", Action::NEW_PAGE_BEFORE},
            {"page.new-end", Action::NEW_PAGE_AT_END},
            {"page.duplicate", Action::DUPLICATE_PAGE},
            {"page.move-earlier", Action::MOVE_PAGE_TOWARDS_BEGINNING},
            {"page.move-later", Action::MOVE_PAGE_TOWARDS_END},
            {"page.delete", Action::DELETE_PAGE},
            {"view.zoom-in", Action::ZOOM_IN},
            {"view.zoom-out", Action::ZOOM_OUT},
            {"view.zoom-fit", Action::ZOOM_FIT},
            {"view.zoom-100", Action::ZOOM_100},
            {"file.save", Action::SAVE},
            {"file.save-as", Action::SAVE_AS},
            {"file.export-pdf", Action::EXPORT_AS_PDF},
    };
    const auto it = actions.find(id);
    return it == actions.end() ? nullptr : &it->second;
}

/// Commands that are not one GAction: their availability is their own rule
bool isOwnRuleCommand(const std::string& id) {
    static const std::set<std::string> own = {
            "page.bookmark",          "page.thumbnails",         "workspace.teach",          "workspace.mark",
            "workspace.present",      "workspace.leave-present", "workspace.present-tools",  "classroom.spotlight",
            "classroom.curtain",      "classroom.exit-overlay",  "classroom.freeze",         "classroom.blank",
            "classroom.student-view", "classroom.back-to-normal", "view.sidebar"};
    return own.count(id) != 0;
}
}  // namespace

TEST_F(ClassroomWorkflow, everyPaletteCommandIsListedOnceAvailableAsItsActionIsAndASafeSubsetDoesWhatItSays) {
    addPlainPages(control.get(), 3);  // five pages
    window->toolbarSelected("UTN Teacher");
    window->show(nullptr);
    settle();
    ToolLessonNavigator::goToPage(control.get(), 1);
    settle();

    window->openCommandPalette();
    auto* palette = window->getCommandPalette();
    const auto commands = palette->describeCommands();  // closes the palette: the list is read for the state of now
    ASSERT_FALSE(commands.empty());
    std::set<std::string> ids;
    for (const auto& command: commands) {
        EXPECT_FALSE(command.id.empty());
        EXPECT_TRUE(ids.insert(command.id).second) << "listed twice: " << command.id;
        EXPECT_FALSE(command.title.empty()) << command.id;
        if (const Action* action = actionBehind(command.id)) {
            EXPECT_EQ(command.enabled, control->getActionDatabase()->isActionEnabled(*action))
                    << command.id << " is not available exactly when its action is enabled";
        } else {
            EXPECT_TRUE(isOwnRuleCommand(command.id)) << command.id << " is new: say which action it is backed by";
        }
    }

    const auto titleOf = [&](const char* id) {
        for (const auto& command: commands) {
            if (command.id == id) {
                return command.title;
            }
        }
        ADD_FAILURE() << "no command " << id;
        return std::string{};
    };
    const auto run = [&](const char* id) {
        const std::string title = titleOf(id);
        window->openCommandPalette();
        auto* open = window->getCommandPalette();
        ASSERT_TRUE(open->isOpen()) << id;
        typeInPalette(open, title.c_str());
        ASSERT_TRUE(startsWith(selectedRow(open), (title + ", ").c_str())) << id << ": " << selectedRow(open);
        sendKey(open->getWidget(), GDK_KEY_Return, 0);
        settle();
        EXPECT_FALSE(open->isOpen()) << id;
    };

    // Pages
    ASSERT_NO_FATAL_FAILURE(run("page.next"));
    EXPECT_EQ(control->getCurrentPageNo(), 2u);
    ASSERT_NO_FATAL_FAILURE(run("page.previous"));
    EXPECT_EQ(control->getCurrentPageNo(), 1u);
    ASSERT_NO_FATAL_FAILURE(run("page.last"));
    EXPECT_EQ(control->getCurrentPageNo(), 4u);
    ASSERT_NO_FATAL_FAILURE(run("page.first"));
    EXPECT_EQ(control->getCurrentPageNo(), 0u);

    // Tools
    const std::vector<std::pair<const char*, ToolType>> tools = {{"tool.highlighter", TOOL_HIGHLIGHTER},
                                                                {"tool.eraser", TOOL_ERASER},
                                                                {"tool.text", TOOL_TEXT},
                                                                {"tool.select-rect", TOOL_SELECT_RECT},
                                                                {"tool.select-region", TOOL_SELECT_REGION},
                                                                {"tool.hand", TOOL_HAND},
                                                                {"tool.laser", TOOL_LASER_POINTER_PEN}};
    for (const auto& [id, type]: tools) {
        ASSERT_NO_FATAL_FAILURE(run(id));
        EXPECT_EQ(control->getToolHandler()->getToolType(), type) << id;
    }
    control->selectTool(TOOL_PEN);

    // Undo and Redo, over one change the history knows about
    auto page = control->getCurrentPage();
    auto* layer = page->getSelectedLayer();
    const size_t before = layer->getElements().size();
    addMarker(layer, 100);
    control->getUndoRedoHandler()->addUndoAction(
            std::make_unique<InsertUndoAction>(page, layer, layer->getElements().back().get()));
    settle();
    ASSERT_EQ(layer->getElements().size(), before + 1);
    ASSERT_NO_FATAL_FAILURE(run("edit.undo"));
    EXPECT_EQ(layer->getElements().size(), before);
    ASSERT_NO_FATAL_FAILURE(run("edit.redo"));
    EXPECT_EQ(layer->getElements().size(), before + 1);

    // Zoom
    const double start = control->getZoomControl()->getZoom();
    struct RestoreZoom {  // the zoom must not leak into the next test, whatever fails below
        double zoom;
        ~RestoreZoom() { control->getZoomControl()->setZoom(zoom); }
    } restoreZoom{start};
    ASSERT_NO_FATAL_FAILURE(run("view.zoom-in"));
    const double larger = control->getZoomControl()->getZoom();
    EXPECT_GT(larger, start);
    ASSERT_NO_FATAL_FAILURE(run("view.zoom-out"));
    EXPECT_LT(control->getZoomControl()->getZoom(), larger);
    ASSERT_NO_FATAL_FAILURE(run("view.zoom-100"));
    EXPECT_NEAR(control->getZoomControl()->getZoom(), control->getZoomControl()->getZoom100Value(), 1e-6);
}

namespace {
/// The first button a person could Tab to, outside the page itself
GtkWidget* firstFocusableButton(GtkWidget* widget, GtkWidget* skip) {
    if (widget == skip) {
        return nullptr;
    }
    if (GTK_IS_BUTTON(widget) && gtk_widget_get_can_focus(widget) && gtk_widget_is_drawable(widget) &&
        gtk_widget_is_sensitive(widget)) {
        return widget;
    }
    if (GTK_IS_CONTAINER(widget)) {
        GList* children = gtk_container_get_children(GTK_CONTAINER(widget));
        GtkWidget* found = nullptr;
        for (GList* child = children; child && !found; child = child->next) {
            found = firstFocusableButton(GTK_WIDGET(child->data), skip);
        }
        g_list_free(children);
        return found;
    }
    return nullptr;
}

/// Keyboard focus a person can see and use: it exists, is shown, and accepts keys
::testing::AssertionResult focusIsUseful(GtkWidget* top) {
    GtkWidget* focus = gtk_window_get_focus(GTK_WINDOW(top));
    if (!focus) {
        return ::testing::AssertionFailure() << "no widget holds the keyboard";
    }
    if (!gtk_widget_get_visible(focus) || !gtk_widget_get_mapped(focus)) {
        return ::testing::AssertionFailure() << "the focus is on a hidden widget (" << G_OBJECT_TYPE_NAME(focus) << ")";
    }
    if (!gtk_widget_is_sensitive(focus)) {
        return ::testing::AssertionFailure() << "the focus is on a widget that does not accept keys";
    }
    return ::testing::AssertionSuccess();
}

std::string accessibleDescription(GtkWidget* widget) {
    AtkObject* accessible = gtk_widget_get_accessible(widget);
    const char* text = accessible ? atk_object_get_description(accessible) : nullptr;
    return text ? text : "";
}
}  // namespace

// A layout change destroys or hides the control that held the keyboard. Without a new place for it, Tab starts from
// nowhere and a keyboard user has lost their position.
TEST_F(ClassroomWorkflow, keyboardFocusLandsOnAVisibleControlAfterAWorkspaceSwitchTheDockHidingOrLeavingPresent) {
    window->toolbarSelected("UTN Teacher");
    window->show(nullptr);
    settle();
    GtkWidget* top = GTK_WIDGET(window->getWindow());
    GtkWidget* canvas = window->getXournal()->getWidget();
    const auto focusAButton = [&]() {
        GtkWidget* button = firstFocusableButton(window->get("mainBox"), canvas);
        ASSERT_NE(button, nullptr) << "the layout has no control to put the keyboard on";
        gtk_window_set_focus(GTK_WINDOW(top), button);
        settle();
        ASSERT_EQ(gtk_window_get_focus(GTK_WINDOW(top)), button);
    };

    // A workspace switch from the palette
    ASSERT_NO_FATAL_FAILURE(focusAButton());
    window->openCommandPalette();
    auto* palette = window->getCommandPalette();
    ASSERT_TRUE(palette->isOpen());
    typeInPalette(palette, "mark workspace");
    sendKey(palette->getWidget(), GDK_KEY_Return, 0);
    settle();
    ASSERT_EQ(window->getSelectedToolbar()->getId(), "UTN Marking");
    EXPECT_TRUE(focusIsUseful(top)) << "after Mark workspace from the palette";

    // Entering Present, revealing and hiding its dock, and leaving it
    ASSERT_NO_FATAL_FAILURE(focusAButton());
    window->toolbarSelected("UTN Present");
    settle();
    EXPECT_TRUE(focusIsUseful(top)) << "after entering Present";
    window->setPresentToolsRevealed(true);
    settle();
    ASSERT_NO_FATAL_FAILURE(focusAButton());
    window->setPresentToolsRevealed(false);
    settle();
    EXPECT_TRUE(focusIsUseful(top)) << "after the Present dock hides";
    ASSERT_NO_FATAL_FAILURE(focusAButton());
    window->leavePresent();
    settle();
    EXPECT_TRUE(focusIsUseful(top)) << "after leaving Present";

    // A palette command that changes the layout, run from a palette that gave the keyboard back to a control
    ASSERT_NO_FATAL_FAILURE(focusAButton());
    window->openCommandPalette();
    typeInPalette(palette, "present workspace");
    sendKey(palette->getWidget(), GDK_KEY_Return, 0);
    settle();
    ASSERT_EQ(window->getSelectedToolbar()->getId(), "UTN Present");
    EXPECT_TRUE(focusIsUseful(top)) << "after Present workspace from the palette";
    window->toolbarSelected("UTN Teacher");
    settle();
}

// An unavailable command must still be reachable with the arrow keys: a keyboard or screen-reader user otherwise never
// learns why it is greyed out.
TEST_F(ClassroomWorkflow, paletteUnavailableRowsCanBeChosenWithDownAndSayWhyAndEnterOnThemRunsNothing) {
    window->toolbarSelected("UTN Teacher");
    window->show(nullptr);
    settle();
    window->openCommandPalette();
    auto* palette = window->getCommandPalette();
    ASSERT_TRUE(palette->isOpen());
    GtkWidget* status = widgetNamed(palette->getWidget(), "utnPaletteStatus");
    ASSERT_NE(status, nullptr);

    typeInPalette(palette, "leave present");
    ASSERT_EQ(selectedRow(palette), "") << "nothing is available, so nothing starts out chosen";
    sendKey(palette->getWidget(), GDK_KEY_Down, 0);
    EXPECT_TRUE(startsWith(selectedRow(palette), "Leave Present, Workspace")) << selectedRow(palette);
    EXPECT_STREQ(gtk_label_get_text(GTK_LABEL(status)), "Not in Present") << "the reason is in the status line";
    auto* row = gtk_list_box_get_selected_row(GTK_LIST_BOX(widgetNamed(palette->getWidget(), "utnPaletteList")));
    ASSERT_NE(row, nullptr);
    EXPECT_EQ(accessibleDescription(GTK_WIDGET(row)), "Not in Present") << "and a screen reader gets it as a description";

    sendKey(palette->getWidget(), GDK_KEY_Return, 0);
    EXPECT_TRUE(palette->isOpen()) << "Enter on an unavailable command keeps the palette open";
    EXPECT_EQ(window->getSelectedToolbar()->getId(), "UTN Teacher") << "and runs nothing";
    EXPECT_STREQ(gtk_label_get_text(GTK_LABEL(status)), "Not in Present") << "the reason stays visible";

    // Moving on to an available command puts the usual hint back
    typeInPalette(palette, "pen");
    EXPECT_TRUE(startsWith(selectedRow(palette), "Pen, Tool")) << selectedRow(palette);
    EXPECT_NE(std::string(gtk_label_get_text(GTK_LABEL(status))).find("Enter runs"), std::string::npos);
    EXPECT_EQ(accessibleDescription(GTK_WIDGET(gtk_list_box_get_selected_row(
                      GTK_LIST_BOX(widgetNamed(palette->getWidget(), "utnPaletteList"))))),
              "");
    palette->close();
    settle();
}

// While the Blank cover is up the toolbar and tools behind it are hidden but would still take Tab, so a keyboard user
// could press a button they cannot see.
TEST_F(ClassroomWorkflow, theBlankCoverHoldsTheKeyboardWhileUpAndFocusAndSensitivityComeBackWhenItGoesDown) {
    window->toolbarSelected("UTN Teacher");
    window->show(nullptr);
    settle();
    GtkWidget* top = GTK_WIDGET(window->getWindow());
    GtkWidget* canvas = window->getXournal()->getWidget();
    auto* students = window->getToolMenuHandler()->getStudentView();
    ASSERT_NE(students, nullptr);
    GtkWidget* button = firstFocusableButton(window->get("mainBox"), canvas);
    ASSERT_NE(button, nullptr);
    const bool buttonSensitive = gtk_widget_get_sensitive(button);
    const bool buttonCanFocus = gtk_widget_get_can_focus(button);
    const bool boxSensitive = gtk_widget_get_sensitive(window->get("mainBox"));
    gtk_window_set_focus(GTK_WINDOW(top), button);
    settle();

    students->showTeacherBlankCover(true);
    settle();
    ASSERT_TRUE(students->isTeacherCoverShown());
    GtkWidget* show = buttonNamed(window->get("mainOverlay"), "Show page");
    ASSERT_NE(show, nullptr);
    EXPECT_EQ(gtk_window_get_focus(GTK_WINDOW(top)), show) << "the cover's own control holds the keyboard";
    for (int i = 0; i < 6; ++i) {
        gtk_widget_child_focus(top, i % 2 ? GTK_DIR_TAB_BACKWARD : GTK_DIR_TAB_FORWARD);
        settle();
        EXPECT_EQ(gtk_window_get_focus(GTK_WINDOW(top)), show) << "Tab " << i << " left the cover";
    }

    students->showTeacherBlankCover(false);
    settle();
    EXPECT_EQ(gtk_window_get_focus(GTK_WINDOW(top)), button) << "the keyboard goes back to where it was";
    EXPECT_EQ(gtk_widget_get_sensitive(button), buttonSensitive);
    EXPECT_EQ(gtk_widget_get_can_focus(button), buttonCanFocus);
    EXPECT_EQ(gtk_widget_get_sensitive(window->get("mainBox")), boxSensitive);

    // Nothing to go back to: the page gets the keyboard
    gtk_window_set_focus(GTK_WINDOW(top), nullptr);
    students->showTeacherBlankCover(true);
    settle();
    students->showTeacherBlankCover(false);
    settle();
    EXPECT_EQ(gtk_window_get_focus(GTK_WINDOW(top)), canvas);
    EXPECT_TRUE(focusIsUseful(top));
}

// Clicking a page in the open popover moves the "current page" marker and the Recent list at once; they used to stay as
// they were until the popover was closed and opened again.
TEST_F(ClassroomWorkflow, lessonMapAndRecentListFollowAPageClickedInsideTheOpenPopover) {
    addPlainPages(control.get(), 3);  // five pages
    auto doc = control->getDocument();
    doc->getPage(0)->setUtnPageLabel("CW");
    doc->getPage(2)->setUtnPageLabel("SW");
    NavigatorUnderTest navigator(control.get());
    navigator.goTo("1");
    navigator.refresh();
    ASSERT_EQ(control->getCurrentPageNo(), 0u);
    auto* current = navigator.named("utnLessonMapCurrent");
    ASSERT_NE(current, nullptr);
    ASSERT_STREQ(gtk_button_get_label(GTK_BUTTON(current)), "1   CW");

    gtk_button_clicked(GTK_BUTTON(buttonNamed(navigator.named("utnLessonMap"), "3   SW")));
    settle();
    ASSERT_EQ(control->getCurrentPageNo(), 2u);
    current = navigator.named("utnLessonMapCurrent");
    ASSERT_NE(current, nullptr);
    EXPECT_STREQ(gtk_button_get_label(GTK_BUTTON(current)), "3   SW") << "the marker follows the click";
    EXPECT_EQ(accessibleName(current), "3 SW, current page");
    const auto recent = buttonTexts(navigator.named("utnRecentPages"));
    EXPECT_NE(std::find(recent.begin(), recent.end(), "1   CW"), recent.end()) << "the page just left is recent";
    EXPECT_EQ(std::find(recent.begin(), recent.end(), "3   SW"), recent.end()) << "the page being shown is not";

    // A click in the Recent list does the same
    GtkWidget* back = buttonNamed(navigator.named("utnRecentPages"), "1   CW");
    ASSERT_NE(back, nullptr);
    gtk_button_clicked(GTK_BUTTON(back));
    settle();
    EXPECT_EQ(control->getCurrentPageNo(), 0u);
    EXPECT_STREQ(gtk_button_get_label(GTK_BUTTON(navigator.named("utnLessonMapCurrent"))), "1   CW");
    doc->getPage(0)->setUtnPageLabel("");
    doc->getPage(2)->setUtnPageLabel("");
}

namespace {
/// Label in name (WCAG 2.5.3): a person who says the words on the button must hit it
void expectNameHasLabel(GtkWidget* button, const char* where) {
    const std::string label = gtk_button_get_label(GTK_BUTTON(button));
    EXPECT_NE(accessibleName(button).find(label), std::string::npos)
            << where << ": the name '" << accessibleName(button) << "' lacks the visible label '" << label << "'";
}

bool oneDisplay() {
    GdkDisplay* display = gdk_display_get_default();
    return display && gdk_display_get_n_monitors(display) < 2;
}
}  // namespace

TEST_F(ClassroomWorkflow, freezeAndBlankAccessibleNamesAlwaysContainTheVisibleLabel) {
    ToolStudentView students("CLASSROOM_STUDENTS", control.get());
    auto quick = students.createQuickControls(true);
    auto pill = students.createItem(true);
    auto* menu = GTK_WIDGET(gtk_menu_button_get_popover(GTK_MENU_BUTTON(pill.get())));

    GtkWidget* freeze = buttonNamed(quick.get(), "Freeze");
    GtkWidget* blank = buttonNamed(quick.get(), "Blank");
    GtkWidget* freezeInMenu = buttonNamed(menu, "Freeze this page");
    GtkWidget* blankInMenu = buttonNamed(menu, "Blank screen");
    for (GtkWidget* button: {freeze, blank, freezeInMenu, blankInMenu}) {
        ASSERT_NE(button, nullptr);
    }
    const auto checkAll = [&](const char* state) {
        expectNameHasLabel(freeze, state);
        expectNameHasLabel(blank, state);
        expectNameHasLabel(freezeInMenu, state);
        expectNameHasLabel(blankInMenu, state);
    };
    checkAll("resting");

    // Blank flips every label at once. With a second display it would open the projector window full screen, which this
    // test does not do; Frozen needs that window as well, so it is not reachable here either.
    if (oneDisplay()) {
        students.setBlanked(true);
        settle();
        EXPECT_TRUE(gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(blank)));
        EXPECT_STREQ(gtk_button_get_label(GTK_BUTTON(blank)), "Show page") << "the label flipped, so the name must follow";
        checkAll("blanked");
        students.restoreNormal();
        settle();
        checkAll("back to normal");
    } else {
        RecordProperty("blank_state_flip_skipped", "two displays: Blank would open the projector window");
    }
}

// Only a description that differs from the tooltip proves it was set: the kit's Freeze button shows another tooltip
TEST_F(ClassroomWorkflow, theKitFreezeAndBlankButtonsNameTheirLabelAndFreezeSaysWhyItIsGreyedOut) {
    auto students = std::make_unique<ToolStudentView>("CLASSROOM_STUDENTS", control.get());
    ASSERT_FALSE(students->isWindowOpen());
    ToolPresentationKit kit("PRESENTATION_KIT", control.get(), IconNameHelper(control->getSettings()), students.get());
    auto item = kit.createItem(true);
    auto* kitMenu = GTK_WIDGET(gtk_menu_button_get_popover(GTK_MENU_BUTTON(item.get())));
    GtkWidget* freezeInKit = buttonNamed(kitMenu, "Freeze students' screen");
    ASSERT_NE(freezeInKit, nullptr);
    EXPECT_FALSE(gtk_widget_get_sensitive(freezeInKit));
    EXPECT_NE(std::string(gtk_widget_get_tooltip_text(freezeInKit)), "Open Student View on the projector first");
    EXPECT_EQ(accessibleDescription(freezeInKit), "Open Student View on the projector first");
    GtkWidget* blankInKit = buttonNamed(kitMenu, "Blank students' screen");
    ASSERT_NE(blankInKit, nullptr);
    expectNameHasLabel(freezeInKit, "kit, resting");
    expectNameHasLabel(blankInKit, "kit, resting");  // their labels flip with Blank/Freeze (two displays: not reachable here)
    gtk_widget_destroy(item.get());
    item.reset();
    students.reset();
    settle();
}

// Hiding the sidebar while one of its entries holds the keyboard must not leave the keyboard on a widget nobody sees
TEST_F(ClassroomWorkflow, hidingTheSidebarWhileItHoldsTheKeyboardMovesTheKeyboardToSomethingVisible) {
    window->toolbarSelected("UTN Teacher");
    window->show(nullptr);
    control->setShowSidebar(true);
    settle();
    GtkWidget* top = GTK_WIDGET(window->getWindow());
    GtkWidget* tree = window->get("buttonCloseSidebar");  // a control inside the sidebar
    ASSERT_NE(tree, nullptr);
    ASSERT_TRUE(gtk_widget_is_drawable(tree)) << "the sidebar is not showing";
    gtk_window_set_focus(GTK_WINDOW(top), tree);
    settle();
    ASSERT_EQ(gtk_window_get_focus(GTK_WINDOW(top)), tree);

    control->setShowSidebar(false);
    settle();
    EXPECT_TRUE(focusIsUseful(top));

    // Palette opened while the keyboard sits on a widget that is hidden, then put away: the keyboard still lands well
    gtk_window_set_focus(GTK_WINDOW(top), tree);
    window->openCommandPalette();
    auto* palette = window->getCommandPalette();
    ASSERT_TRUE(palette->isOpen());
    sendKey(palette->getWidget(), GDK_KEY_Escape, 0);
    settle();
    EXPECT_FALSE(palette->isOpen());
    EXPECT_TRUE(focusIsUseful(top)) << "after Escape from a palette opened over a hidden focus";
    control->setShowSidebar(true);
    settle();
}

// The keyboard follows a click in the open popover instead of being lost with the button that was rebuilt
TEST_F(ClassroomWorkflow, aPageClickedInTheOpenLessonPopoverKeepsTheKeyboardInsideIt) {
    addPlainPages(control.get(), 3);
    control->getDocument()->getPage(0)->setUtnPageLabel("CW");
    control->getDocument()->getPage(2)->setUtnPageLabel("SW");
    NavigatorUnderTest navigator(control.get());
    GtkWidget* host = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_container_add(GTK_CONTAINER(host), navigator.item.get());
    gtk_widget_show_all(host);
    settle();
    gtk_popover_popup(GTK_POPOVER(navigator.popover));
    settle();
    if (!gtk_widget_get_mapped(navigator.popover)) {
        gtk_widget_destroy(host);
        control->getDocument()->getPage(0)->setUtnPageLabel("");
        control->getDocument()->getPage(2)->setUtnPageLabel("");
        GTEST_SKIP() << "this display does not map the popover";
    }
    gtk_button_clicked(GTK_BUTTON(buttonNamed(navigator.named("utnLessonMap"), "3   SW")));
    settle();
    GtkWidget* focus = gtk_window_get_focus(GTK_WINDOW(host));
    ASSERT_NE(focus, nullptr);
    EXPECT_STREQ(gtk_widget_get_name(focus), "utnLessonMapCurrent") << "the keyboard stays on the page just chosen";
    gtk_widget_destroy(host);
    control->getDocument()->getPage(0)->setUtnPageLabel("");
    control->getDocument()->getPage(2)->setUtnPageLabel("");
}

namespace {
/// The floating selection toolbar's button with this visible label, or nullptr
GtkWidget* selectionToolbarButton(MainWindow* window, const char* label) {
    return buttonNamed(window->getSelectionToolbar()->getWidget(), label);
}

bool selectionToolbarShown(MainWindow* window) {
    GtkWidget* toolbar = window->getSelectionToolbar()->getWidget();
    return gtk_widget_get_visible(toolbar) && gtk_widget_get_mapped(toolbar);
}

/// Teach workspace, one black pen stroke on the current page's selected layer, selected with the rectangle tool
void selectOneStroke(Control* control, MainWindow* window, Layer* layer, double y = 200) {
    window->toolbarSelected("UTN Teacher");
    window->show(nullptr);
    addMarker(layer, y);
    control->selectTool(TOOL_SELECT_RECT);
    control->selectAllOnPage();
    settle();
}
}  // namespace

// The toolbar appears beside a fresh selection, leaves the keyboard on the page, names every button as it reads, and
// sits next to the selection, not on it.
TEST_F(ClassroomWorkflow, selectionToolbarAppearsBesideTheSelectionWithoutTakingTheKeyboard) {
    window->getSelectionToolbar()->setEnabled(true);  // Control loads the real settings, where it may be off
    auto page = control->getCurrentPage();
    GtkWidget* canvas = window->getXournal()->getWidget();
    window->toolbarSelected("UTN Teacher");
    window->show(nullptr);
    settle();
    gtk_widget_grab_focus(canvas);
    selectOneStroke(control.get(), window.get(), page->getSelectedLayer());
    ASSERT_NE(window->getXournal()->getSelection(), nullptr);
    ASSERT_TRUE(selectionToolbarShown(window.get()));
    EXPECT_EQ(gtk_window_get_focus(GTK_WINDOW(window->getWindow())), canvas) << "appearing never takes the keyboard";

    for (const char* label: {"Delete", "Duplicate", "Colour", "Bring to front", "Send to back"}) {
        GtkWidget* button = selectionToolbarButton(window.get(), label);
        ASSERT_NE(button, nullptr) << label;
        EXPECT_TRUE(gtk_widget_get_visible(button)) << label;
        EXPECT_TRUE(gtk_widget_get_can_focus(button)) << label << " must be reachable by keyboard";
        EXPECT_NE(accessibleName(button).find(label), std::string::npos) << label << ": the name contains the label";
        gchar* tip = gtk_widget_get_tooltip_text(button);
        EXPECT_TRUE(tip && *tip) << label << " explains itself";
        g_free(tip);
    }
    EXPECT_TRUE(gtk_widget_is_sensitive(selectionToolbarButton(window.get(), "Duplicate")));

    // Beside the selection, not over it, and inside the visible canvas
    GtkWidget* toolbar = window->getSelectionToolbar()->getWidget();
    GtkWidget* scroller = gtk_widget_get_ancestor(canvas, GTK_TYPE_SCROLLED_WINDOW);
    ASSERT_NE(scroller, nullptr);
    const auto box = window->getXournal()->getSelection()->getBoundingBoxInView();
    const auto visible = window->getXournal()->getLayout()->getVisibleRect();
    int tx = 0, ty = 0;
    ASSERT_TRUE(gtk_widget_translate_coordinates(toolbar, scroller, 0, 0, &tx, &ty));
    const int tw = gtk_widget_get_allocated_width(toolbar);
    const int th = gtk_widget_get_allocated_height(toolbar);
    const double sx = box.x - visible.x, sy = box.y - visible.y;
    const bool overlaps = tx < sx + box.width && sx < tx + tw && ty < sy + box.height && sy < ty + th;
    EXPECT_FALSE(overlaps) << "toolbar " << tx << "," << ty << " " << tw << "x" << th << " selection " << sx << ","
                           << sy << " " << box.width << "x" << box.height;
    EXPECT_LE(ty + th, sy) << "there is room above, so it goes above";
    EXPECT_GE(tx, 0);
    EXPECT_GE(ty, 0);
    EXPECT_LE(tx + tw, gtk_widget_get_allocated_width(scroller));

    control->clearSelection();
    settle();
    EXPECT_FALSE(gtk_widget_get_visible(toolbar)) << "no selection, no toolbar";
}

// Escape in the toolbar puts it away for this selection and gives the keyboard back to the page; changing the tool, the
// page or the workspace hides it too.
TEST_F(ClassroomWorkflow, selectionToolbarHidesOnEscapeToolChangePageChangeAndOutsideTheTeacherWorkspace) {
    window->getSelectionToolbar()->setEnabled(true);  // Control loads the real settings, where it may be off
    auto page = control->getCurrentPage();
    GtkWidget* canvas = window->getXournal()->getWidget();
    selectOneStroke(control.get(), window.get(), page->getSelectedLayer());
    ASSERT_TRUE(selectionToolbarShown(window.get()));

    GtkWidget* front = selectionToolbarButton(window.get(), "Bring to front");
    gtk_widget_grab_focus(front);
    ASSERT_EQ(gtk_window_get_focus(GTK_WINDOW(window->getWindow())), front);
    sendKey(window->getWindow(), GDK_KEY_Escape, 0);
    EXPECT_FALSE(selectionToolbarShown(window.get())) << "Escape dismisses the toolbar";
    EXPECT_EQ(gtk_window_get_focus(GTK_WINDOW(window->getWindow())), canvas) << "and returns the keyboard to the page";
    EXPECT_NE(window->getXournal()->getSelection(), nullptr) << "the selection itself stays";
    window->updateSelectionToolbar();
    settle();
    EXPECT_FALSE(selectionToolbarShown(window.get())) << "dismissed until the selection changes";

    control->clearSelection();
    control->selectAllOnPage();
    settle();
    EXPECT_TRUE(selectionToolbarShown(window.get())) << "a new selection brings it back";

    control->firePageSelected(1);
    settle();
    EXPECT_FALSE(selectionToolbarShown(window.get())) << "another page is current";
    control->firePageSelected(0);
    settle();

    control->clearSelection();
    control->selectAllOnPage();
    settle();
    ASSERT_TRUE(selectionToolbarShown(window.get()));
    control->selectTool(TOOL_PEN);
    settle();
    EXPECT_FALSE(selectionToolbarShown(window.get())) << "a drawing tool has no selection toolbar";

    control->selectTool(TOOL_SELECT_RECT);
    control->selectAllOnPage();
    settle();
    ASSERT_TRUE(selectionToolbarShown(window.get()));
    window->toolbarSelected("Xournal++");
    settle();
    EXPECT_FALSE(selectionToolbarShown(window.get())) << "classic layouts keep the original interface";
    window->toolbarSelected("UTN Teacher");
    control->clearSelection();
    settle();
}

// Never over the Blank cover that hides the teacher's canvas, and never while a job holds the application.
TEST_F(ClassroomWorkflow, selectionToolbarNeverShowsOverTheBlankCoverOrWhileTheApplicationIsBusy) {
    window->getSelectionToolbar()->setEnabled(true);  // Control loads the real settings, where it may be off
    auto page = control->getCurrentPage();
    selectOneStroke(control.get(), window.get(), page->getSelectedLayer());
    ASSERT_TRUE(selectionToolbarShown(window.get()));
    auto* students = window->getToolMenuHandler()->getStudentView();
    ASSERT_NE(students, nullptr);

    students->showTeacherBlankCover(true);
    settle();
    ASSERT_TRUE(students->isTeacherCoverShown());
    EXPECT_FALSE(gtk_widget_get_visible(window->getSelectionToolbar()->getWidget())) << "hidden behind the cover";
    window->updateSelectionToolbar();
    settle();
    EXPECT_FALSE(gtk_widget_get_visible(window->getSelectionToolbar()->getWidget())) << "and stays hidden";
    students->showTeacherBlankCover(false);
    settle();
    EXPECT_TRUE(selectionToolbarShown(window.get())) << "back once the cover is down";

    control->block("test");
    settle();
    EXPECT_FALSE(gtk_widget_get_visible(window->getSelectionToolbar()->getWidget())) << "busy: nothing to click";
    control->unblock();
    settle();
    EXPECT_TRUE(selectionToolbarShown(window.get()));
    control->clearSelection();
    settle();
}

// Delete runs the real delete: the stroke goes, Undo brings it back, Redo removes it again.
TEST_F(ClassroomWorkflow, selectionToolbarDeleteIsTheRealDeleteWithUndoAndRedo) {
    window->getSelectionToolbar()->setEnabled(true);  // Control loads the real settings, where it may be off
    auto page = control->getCurrentPage();
    Layer* layer = page->getSelectedLayer();
    selectOneStroke(control.get(), window.get(), layer);
    ASSERT_TRUE(selectionToolbarShown(window.get()));

    gtk_button_clicked(GTK_BUTTON(selectionToolbarButton(window.get(), "Delete")));
    settle();
    EXPECT_EQ(window->getXournal()->getSelection(), nullptr);
    EXPECT_TRUE(layer->getElements().empty());
    EXPECT_FALSE(gtk_widget_get_visible(window->getSelectionToolbar()->getWidget()));

    control->getActionDatabase()->fireActivateAction(Action::UNDO);
    settle();
    control->clearSelection();
    EXPECT_EQ(layer->getElements().size(), 1U) << "Undo restores the stroke";
    control->getActionDatabase()->fireActivateAction(Action::REDO);
    settle();
    control->clearSelection();
    EXPECT_TRUE(layer->getElements().empty()) << "Redo deletes it again";
}

// Duplicate copies into the layer the objects came from, here a teacher-only layer below the top one, never the
// clipboard; the copy is offset, selected, and one Undo removes it.
TEST_F(ClassroomWorkflow, selectionToolbarDuplicateCopiesIntoTheSourceLayerAsOneUndoStep) {
    window->getSelectionToolbar()->setEnabled(true);  // Control loads the real settings, where it may be off
    auto page = control->getCurrentPage();
    auto* layers = control->getLayerController();
    Layer* open = page->getSelectedLayer();
    addMarker(open, 400);
    layers->addNewLayer(false);
    Layer* priv = page->getSelectedLayer();
    priv->setAudience(LayerAudience::TeacherOnly);
    layers->addNewLayer(false);
    Layer* top = page->getSelectedLayer();
    layers->switchToLay(2);
    ASSERT_EQ(page->getSelectedLayer(), priv);
    control->getUndoRedoHandler()->clearContents();  // adding layers is undoable; only the duplicate counts here
    selectOneStroke(control.get(), window.get(), priv);
    ASSERT_TRUE(selectionToolbarShown(window.get()));
    const double originalLeft = window->getXournal()->getSelection()->getRect().x;

    GtkWidget* duplicate = selectionToolbarButton(window.get(), "Duplicate");
    ASSERT_TRUE(gtk_widget_is_sensitive(duplicate));
    gtk_button_clicked(GTK_BUTTON(duplicate));
    settle();
    auto* copy = window->getXournal()->getSelection();
    ASSERT_NE(copy, nullptr) << "the copy is selected";
    EXPECT_EQ(copy->getSourceLayer(), priv);
    EXPECT_GT(copy->getRect().x, originalLeft) << "offset, so both can be seen";
    EXPECT_TRUE(selectionToolbarShown(window.get()));

    control->clearSelection();
    EXPECT_EQ(priv->getElements().size(), 2U) << "original and copy in the teacher-only layer";
    EXPECT_EQ(open->getElements().size(), 1U) << "the layer students see is untouched";
    EXPECT_TRUE(top->getElements().empty()) << "nothing pasted into another layer";

    control->getActionDatabase()->fireActivateAction(Action::UNDO);
    settle();
    control->clearSelection();
    EXPECT_EQ(priv->getElements().size(), 1U) << "one Undo removes the copy";
    EXPECT_FALSE(control->getUndoRedoHandler()->canUndo()) << "the duplicate was one step";
    control->getActionDatabase()->fireActivateAction(Action::REDO);
    settle();
    control->clearSelection();
    EXPECT_EQ(priv->getElements().size(), 2U);
    layers->switchToLay(1);
}

// A Duplicate that cannot keep the copy in its source layer is greyed with the reason and does nothing when activated;
// Colour is offered only while the selection has a colour, and recolouring is undoable.
TEST_F(ClassroomWorkflow, selectionToolbarButtonsOnlyRunWhatTheSelectionAllowsAndColourIsUndoable) {
    window->getSelectionToolbar()->setEnabled(true);  // Control loads the real settings, where it may be off
    auto page = control->getCurrentPage();
    auto* layers = control->getLayerController();
    Layer* first = page->getSelectedLayer();
    layers->addNewLayer(false);
    Layer* second = page->getSelectedLayer();
    layers->switchToLay(1);
    control->getUndoRedoHandler()->clearContents();
    selectOneStroke(control.get(), window.get(), first);
    ASSERT_TRUE(selectionToolbarShown(window.get()));

    // Another layer selected under the selection (not through the layer controls, which clear the selection)
    page->setSelectedLayerId(2);
    window->updateSelectionToolbar();
    GtkWidget* duplicate = selectionToolbarButton(window.get(), "Duplicate");
    EXPECT_FALSE(gtk_widget_is_sensitive(duplicate));
    gchar* reason = gtk_widget_get_tooltip_text(duplicate);
    ASSERT_NE(reason, nullptr);
    EXPECT_EQ(std::string(reason).rfind("Unavailable", 0), 0U) << reason;
    g_free(reason);
    gtk_button_clicked(GTK_BUTTON(duplicate));  // even if a click reaches it
    settle();
    EXPECT_FALSE(control->getUndoRedoHandler()->canUndo()) << "nothing ran";
    EXPECT_TRUE(second->getElements().empty());
    page->setSelectedLayerId(1);
    window->updateSelectionToolbar();
    EXPECT_TRUE(gtk_widget_is_sensitive(duplicate));

    // Colour follows what the selection can take
    GtkWidget* colour = selectionToolbarButton(window.get(), "Colour");
    ASSERT_TRUE(GTK_IS_MENU_BUTTON(colour));
    EXPECT_TRUE(gtk_widget_get_visible(colour));
    control->getToolHandler()->setSelectionEditTools(false, false, false, false);  // as for a picture
    settle();
    EXPECT_FALSE(gtk_widget_get_visible(colour)) << "no colour to change, no Colour button";
    control->clearSelection();
    control->selectAllOnPage();
    settle();
    ASSERT_TRUE(gtk_widget_get_visible(colour));

    GtkWidget* popover = GTK_WIDGET(gtk_menu_button_get_popover(GTK_MENU_BUTTON(colour)));
    GtkWidget* red = buttonAccessibleNamed(popover, "Red");
    ASSERT_NE(red, nullptr);
    gtk_button_clicked(GTK_BUTTON(red));
    settle();
    control->clearSelection();
    ASSERT_EQ(first->getElements().size(), 1U);
    const Color recoloured = first->getElements().front()->getColor();
    EXPECT_EQ(recoloured.red, 0xD3U);
    EXPECT_EQ(recoloured.green, 0x2FU);
    EXPECT_EQ(recoloured.blue, 0x2FU);
    control->getActionDatabase()->fireActivateAction(Action::UNDO);
    settle();
    control->clearSelection();
    const Color restored = first->getElements().front()->getColor();
    EXPECT_EQ(restored.red, 0U);
    EXPECT_EQ(restored.green, 0U);
    EXPECT_EQ(restored.blue, 0U);
    control->selectTool(TOOL_PEN);
}

// The Workspace menu turns the toolbar off and on again, and remembers the choice in the (test) settings.
TEST_F(ClassroomWorkflow, selectionToolbarSettingOffHidesItAndOnBringsItBack) {
    // Control loads the real settings, where the teacher may have turned it off: start from on and put back whatever
    // was there, even when an assertion ends the test early
    struct RestoreSetting {
        SelectionToolbar* toolbar;
        bool enabled;
        ~RestoreSetting() { toolbar->setEnabled(enabled); }
    } restore{window->getSelectionToolbar(), window->getSelectionToolbar()->isEnabled()};
    window->getSelectionToolbar()->setEnabled(true);
    auto page = control->getCurrentPage();
    selectOneStroke(control.get(), window.get(), page->getSelectedLayer());
    ASSERT_TRUE(selectionToolbarShown(window.get()));
    auto& utnSettings = control->getSettings()->getCustomElement("utn");

    auto* menu = menuButtonAccessibleNamed(window->getWindow(), "Workspace layout");
    ASSERT_NE(menu, nullptr);
    GtkWidget* toggle = buttonNamed(GTK_WIDGET(gtk_menu_button_get_popover(GTK_MENU_BUTTON(menu))),
                                    "Show selection toolbar");
    ASSERT_TRUE(GTK_IS_CHECK_BUTTON(toggle));
    // The menu was built from the setting as it was then; bring the check box in step with the forced start
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(toggle), true);
    settle();
    ASSERT_TRUE(selectionToolbarShown(window.get()));

    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(toggle), false);
    settle();
    EXPECT_FALSE(gtk_widget_get_visible(window->getSelectionToolbar()->getWidget()));
    bool saved = true;
    EXPECT_TRUE(utnSettings.getBool(SelectionToolbar::SETTING, saved));
    EXPECT_FALSE(saved);

    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(toggle), true);
    settle();
    EXPECT_TRUE(selectionToolbarShown(window.get()));
    EXPECT_TRUE(utnSettings.getBool(SelectionToolbar::SETTING, saved));
    EXPECT_TRUE(saved);
    control->clearSelection();
    settle();
}

// Present is the quiet projector layout: its tools stay out of the class's sight until the teacher reveals them, and
// the selection toolbar (a red Delete among them) is one of those tools.
TEST_F(ClassroomWorkflow, selectionToolbarStaysHiddenInPresentUntilTheToolsAreRevealed) {
    window->getSelectionToolbar()->setEnabled(true);  // Control loads the real settings, where it may be off
    auto page = control->getCurrentPage();
    selectOneStroke(control.get(), window.get(), page->getSelectedLayer());
    ASSERT_TRUE(selectionToolbarShown(window.get()));

    window->toolbarSelected("UTN Present");
    settle();
    ASSERT_EQ(window->getSelectedToolbar()->getId(), "UTN Present");
    ASSERT_FALSE(window->isPresentToolsRevealed());
    if (!window->getXournal()->getSelection()) {
        control->selectTool(TOOL_SELECT_RECT);
        control->selectAllOnPage();
        settle();
    }
    ASSERT_NE(window->getXournal()->getSelection(), nullptr);
    EXPECT_FALSE(gtk_widget_get_visible(window->getSelectionToolbar()->getWidget())) << "Present keeps its tools hidden";
    window->updateSelectionToolbar();
    settle();
    EXPECT_FALSE(gtk_widget_get_visible(window->getSelectionToolbar()->getWidget())) << "and stays hidden";

    window->setPresentToolsRevealed(true);
    settle();
    EXPECT_TRUE(selectionToolbarShown(window.get())) << "the teacher asked for the tools";
    window->setPresentToolsRevealed(false);
    settle();
    EXPECT_FALSE(gtk_widget_get_visible(window->getSelectionToolbar()->getWidget())) << "tools put away again";

    window->toolbarSelected("UTN Teacher");
    settle();
    if (!window->getXournal()->getSelection()) {
        control->selectTool(TOOL_SELECT_RECT);
        control->selectAllOnPage();
        settle();
    }
    EXPECT_TRUE(selectionToolbarShown(window.get())) << "Teach shows it as before";
    control->clearSelection();
    settle();
}

// Undo straight after Duplicate, while the copy is still selected: the copy goes, the original stays where it was
// (the same object), and Redo brings back the very same copy.
TEST_F(ClassroomWorkflow, selectionToolbarDuplicateThenImmediateUndoRemovesOnlyTheCopyAndRedoRestoresIt) {
    window->getSelectionToolbar()->setEnabled(true);  // Control loads the real settings, where it may be off
    auto page = control->getCurrentPage();
    Layer* layer = page->getSelectedLayer();
    selectOneStroke(control.get(), window.get(), layer);
    control->getUndoRedoHandler()->clearContents();
    ASSERT_TRUE(selectionToolbarShown(window.get()));
    const Element* original = window->getXournal()->getSelection()->getElementsView().front();
    const auto originalBox = original->getBoundingBox();

    gtk_button_clicked(GTK_BUTTON(selectionToolbarButton(window.get(), "Duplicate")));
    settle();
    auto* selection = window->getXournal()->getSelection();
    ASSERT_NE(selection, nullptr);
    ASSERT_EQ(selection->getElementsView().size(), 1U);
    const Element* copy = selection->getElementsView().front();
    ASSERT_NE(copy, original) << "a new object, not the original moved";
    ASSERT_EQ(layer->getElements().size(), 1U) << "the selected copy is out of the layer while selected";
    EXPECT_EQ(layer->getElements().front().get(), original);

    control->getActionDatabase()->fireActivateAction(Action::UNDO);  // no clearSelection first
    settle();
    control->clearSelection();
    settle();
    ASSERT_EQ(layer->getElements().size(), 1U) << "Undo removes the copy";
    EXPECT_EQ(layer->getElements().front().get(), original) << "the original is untouched";
    const auto box = original->getBoundingBox();
    EXPECT_DOUBLE_EQ(box.x, originalBox.x);
    EXPECT_DOUBLE_EQ(box.y, originalBox.y);
    EXPECT_FALSE(control->getUndoRedoHandler()->canUndo()) << "Duplicate was one step";

    control->getActionDatabase()->fireActivateAction(Action::REDO);
    settle();
    control->clearSelection();
    settle();
    ASSERT_EQ(layer->getElements().size(), 2U) << "Redo brings the copy back";
    EXPECT_EQ(layer->getElements()[0].get(), original);
    EXPECT_EQ(layer->getElements()[1].get(), copy) << "the same copy, not a new one";
}

// A copy of something at the right or bottom edge of the page is offset the other way, so it stays on the page.
TEST_F(ClassroomWorkflow, selectionToolbarDuplicateAtThePageEdgeKeepsTheCopyOnThePage) {
    window->getSelectionToolbar()->setEnabled(true);  // Control loads the real settings, where it may be off
    auto page = control->getCurrentPage();
    Layer* layer = page->getSelectedLayer();
    window->toolbarSelected("UTN Teacher");
    window->show(nullptr);
    auto ink = std::make_unique<Stroke>();
    ink->setWidth(12);
    ink->setColor(Color(0U));
    ink->addPoint(Point(page->getWidth() - 40, page->getHeight() - 10));
    ink->addPoint(Point(page->getWidth() - 10, page->getHeight() - 10));
    const Element* original = ink.get();
    layer->addElement(std::move(ink));
    control->selectTool(TOOL_SELECT_RECT);
    control->selectAllOnPage();
    settle();
    ASSERT_NE(window->getXournal()->getSelection(), nullptr);
    const auto originalBox = original->getBoundingBox();
    ASSERT_LE(originalBox.x + originalBox.width, page->getWidth());
    ASSERT_LE(originalBox.y + originalBox.height, page->getHeight());

    // The bottom of the page may be scrolled out of the window, and the toolbar with it: run what its button runs
    ASSERT_TRUE(control->canDuplicateSelection());
    control->duplicateSelection();
    settle();
    auto* selection = window->getXournal()->getSelection();
    ASSERT_NE(selection, nullptr);
    const Element* copy = selection->getElementsView().front();
    control->clearSelection();
    settle();
    ASSERT_EQ(layer->getElements().size(), 2U);
    const auto box = copy->getBoundingBox();
    EXPECT_GE(box.x, 0);
    EXPECT_GE(box.y, 0);
    EXPECT_LE(box.x + box.width, page->getWidth()) << "the copy stays inside the page";
    EXPECT_LE(box.y + box.height, page->getHeight()) << "the copy stays inside the page";
    EXPECT_TRUE(box.x != originalBox.x || box.y != originalBox.y) << "still offset, so both can be seen";
}

// With the vertical scrollbar on the left the canvas starts a scrollbar width to the right of the scrolled window;
// the toolbar is placed against the canvas itself.
TEST_F(ClassroomWorkflow, selectionToolbarIsPlacedAgainstTheCanvasWhenTheScrollbarIsOnTheLeft) {
    window->getSelectionToolbar()->setEnabled(true);  // Control loads the real settings, where it may be off
    GtkWidget* canvas = window->getXournal()->getWidget();
    GtkWidget* scroller = gtk_widget_get_ancestor(canvas, GTK_TYPE_SCROLLED_WINDOW);
    ASSERT_NE(scroller, nullptr);
    auto* scrolled = GTK_SCROLLED_WINDOW(scroller);
    GtkPolicyType hPolicy{}, vPolicy{};
    gtk_scrolled_window_get_policy(scrolled, &hPolicy, &vPolicy);
    struct RestoreScroller {
        GtkScrolledWindow* scrolled;
        GtkCornerType placement;
        bool overlay;
        GtkPolicyType h, v;
        ~RestoreScroller() {
            gtk_scrolled_window_set_placement(scrolled, placement);
            gtk_scrolled_window_set_overlay_scrolling(scrolled, overlay);
            gtk_scrolled_window_set_policy(scrolled, h, v);
            settle();
        }
    } restore{scrolled, gtk_scrolled_window_get_placement(scrolled),
              static_cast<bool>(gtk_scrolled_window_get_overlay_scrolling(scrolled)), hPolicy, vPolicy};
    // Contents to the right of the scrollbar: a classic, always-shown scrollbar on the left
    gtk_scrolled_window_set_placement(scrolled, GTK_CORNER_TOP_RIGHT);
    gtk_scrolled_window_set_overlay_scrolling(scrolled, false);
    gtk_scrolled_window_set_policy(scrolled, hPolicy, GTK_POLICY_ALWAYS);

    auto page = control->getCurrentPage();
    window->toolbarSelected("UTN Teacher");
    window->show(nullptr);
    auto ink = std::make_unique<Stroke>();
    ink->setWidth(12);
    ink->setColor(Color(0U));
    ink->addPoint(Point(280, 300));
    ink->addPoint(Point(320, 300));
    page->getSelectedLayer()->addElement(std::move(ink));
    control->selectTool(TOOL_SELECT_RECT);
    control->selectAllOnPage();
    settle();
    ASSERT_TRUE(selectionToolbarShown(window.get()));
    int canvasX = 0, canvasY = 0;
    ASSERT_TRUE(gtk_widget_translate_coordinates(canvas, scroller, 0, 0, &canvasX, &canvasY));
    if (canvasX <= 0) {
        control->clearSelection();
        GTEST_SKIP() << "this theme draws no scrollbar beside the canvas";
    }

    GtkWidget* toolbar = window->getSelectionToolbar()->getWidget();
    int tx = 0, ty = 0;
    ASSERT_TRUE(gtk_widget_translate_coordinates(toolbar, canvas, 0, 0, &tx, &ty));
    const int tw = gtk_widget_get_allocated_width(toolbar);
    const auto box = window->getXournal()->getSelection()->getBoundingBoxInView();
    const auto visible = window->getXournal()->getLayout()->getVisibleRect();
    const double centred = box.x - visible.x + box.width / 2.0 - tw / 2.0;
    const double maxX = std::max(0.0, static_cast<double>(gtk_widget_get_allocated_width(canvas) - tw));
    EXPECT_NEAR(tx, std::clamp(centred, 0.0, maxX), 1.0) << "scrollbar width " << canvasX;
    EXPECT_GE(tx, 0) << "never over the scrollbar";
    control->clearSelection();
    settle();
}

// The ink dock and the Spotlight banner (with its Exit button) stay above the selection toolbar in the overlay, so the
// toolbar can never cover them.
TEST_F(ClassroomWorkflow, selectionToolbarLiesBelowTheInkDockAndTheBannerInTheOverlay) {
    GtkWidget* toolbar = window->getSelectionToolbar()->getWidget();
    GtkWidget* overlay = gtk_widget_get_parent(toolbar);
    ASSERT_TRUE(GTK_IS_OVERLAY(overlay));
    GList* children = gtk_container_get_children(GTK_CONTAINER(overlay));
    int toolbarAt = -1, dockAt = -1, bannerAt = -1, index = 0;
    for (GList* c = children; c; c = c->next, ++index) {
        auto* child = GTK_WIDGET(c->data);
        if (child == toolbar) {
            toolbarAt = index;
        } else if (std::string(gtk_widget_get_name(child)) == "utnInkDock") {
            dockAt = index;
        } else if (gtk_style_context_has_class(gtk_widget_get_style_context(child), "utn-presentation-banner")) {
            bannerAt = index;
        }
    }
    g_list_free(children);
    ASSERT_GE(toolbarAt, 0);
    ASSERT_GE(dockAt, 0);
    ASSERT_GE(bannerAt, 0);
    EXPECT_LT(toolbarAt, dockAt) << "drawn first, so the dock is on top";
    EXPECT_LT(toolbarAt, bannerAt) << "drawn first, so the banner is on top";
}
