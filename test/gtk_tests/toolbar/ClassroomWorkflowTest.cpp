#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cmath>
#include <fstream>
#include <functional>
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
#include "control/jobs/XournalScheduler.h"
#include "control/layer/LayerController.h"
#include "control/settings/Settings.h"
#include "control/tools/TextEditor.h"
#include "control/tools/EditSelection.h"
#include "control/xojfile/LoadHandler.h"
#include "control/xojfile/SaveHandler.h"
#include "gui/GladeSearchpath.h"
#include "gui/MainWindow.h"
#include "gui/ResponsiveLayout.h"
#include "gui/menus/menubar/Menubar.h"
#include "gui/dialog/ExportDialog.h"
#include "gui/FloatingToolbox.h"
#include "gui/PdfFloatingToolbox.h"
#include "gui/PageView.h"
#include "gui/UtnToast.h"
#include "gui/XournalView.h"
#include "gui/inputdevices/PositionInputData.h"
#include "gui/toolbarMenubar/ToolStudentView.h"
#include "gui/toolbarMenubar/ToolUtnContextBar.h"
#include "model/Document.h"
#include "model/DocumentLockAudit.h"
#include "model/Layer.h"
#include "model/PageType.h"
#include "model/Text.h"
#include "model/Stroke.h"
#include "model/XojPage.h"
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

class ClassroomWorkflow: public ::testing::Test {
protected:
    void SetUp() override {
        if (!gtk_init_check(nullptr, nullptr)) {
            GTEST_SKIP() << "No display available";
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
            control->getDocument()->clearDocument(true);
        }
        control->setGhostHiddenAnswers(false);
        window->setTeacherWorkspace(0);
        control->selectTool(TOOL_PEN);
        for (int i = 0; i < 2; ++i) {
            auto page = std::make_shared<XojPage>(600, 800);
            page->setBackgroundType(PageType(PageTypeFormat::Plain));
            control->getDocument()->addPage(page);
        }
        control->fireDocumentChanged(DOCUMENT_CHANGE_COMPLETE);
        control->DocumentHandler::firePageSelected(0);
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

// Hidden answers shown faintly on the teacher canvas are a leak when that window is screen-shared or recorded, so the
// teacher has to ask for them. Off by default; the choice is remembered; with one display it never applies.

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

// A frozen frame is a picture of the moment it was taken. If the teacher then hides answers (or makes a layer private or
// invisible, or covers the page), the picture must not stay on the projector: the screen goes blank at once. Each case
// checks the real Student View pixels, not just the state flag.

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
