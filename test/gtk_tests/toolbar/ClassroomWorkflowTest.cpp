#include <algorithm>
#include <memory>
#include <set>
#include <string>
#include <vector>

#include <gtest/gtest.h>
#include <gtk/gtk.h>

#include "control/Control.h"
#include "control/ToolHandler.h"
#include "control/layer/LayerController.h"
#include "control/settings/Settings.h"
#include "control/tools/TextEditor.h"
#include "control/tools/EditSelection.h"
#include "control/xojfile/LoadHandler.h"
#include "control/xojfile/SaveHandler.h"
#include "gui/GladeSearchpath.h"
#include "gui/MainWindow.h"
#include "gui/FloatingToolbox.h"
#include "gui/PdfFloatingToolbox.h"
#include "gui/PageView.h"
#include "gui/XournalView.h"
#include "gui/inputdevices/PositionInputData.h"
#include "gui/toolbarMenubar/ToolStudentView.h"
#include "model/Document.h"
#include "model/Layer.h"
#include "model/PageType.h"
#include "model/Text.h"
#include "model/Stroke.h"
#include "model/XojPage.h"
#include "util/PathUtil.h"
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

void settle() {
    for (int i = 0; i < 30; ++i) {
        for (int event = 0; event < 20 && gtk_events_pending(); ++event) {
            gtk_main_iteration_do(false);
        }
        g_usleep(2000);
    }
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
        EXPECT_EQ(studentPixel(student, 100, 200), 0U) << "Freeze retains the revealed frame";
        click("Blank screen");
        EXPECT_EQ(students->getState(), ToolStudentView::State::Blank);
        EXPECT_EQ(studentPixel(student, 100, 100), 0U);
        EXPECT_EQ(studentPixel(student, 100, 400), 0U);
        click("Show the page again");
        EXPECT_EQ(students->getState(), ToolStudentView::State::Frozen);
        click("Resume live view");
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
