/* Focused coverage for the Text settings panel that is shared with the properties bar. */
#include <memory>

#include <gtest/gtest.h>
#include <gtk/gtk.h>

#include "control/Control.h"
#include "control/settings/Settings.h"
#include "gui/GladeSearchpath.h"
#include "gui/IconNameHelper.h"
#include "gui/MainWindow.h"
#include "gui/toolbarMenubar/ToolTextMode.h"
#include "gui/toolbarMenubar/ToolUtnContextBar.h"
#include "util/PathUtil.h"

#include "config-test.h"

namespace {
GtkWidget* findNamed(GtkWidget* widget, const char* name) {
    if (AtkObject* accessible = gtk_widget_get_accessible(widget);
        accessible && g_strcmp0(atk_object_get_name(accessible), name) == 0) {
        return widget;
    }
    if (GTK_IS_CONTAINER(widget)) {
        GList* children = gtk_container_get_children(GTK_CONTAINER(widget));
        GtkWidget* found = nullptr;
        for (GList* child = children; child && !found; child = child->next) {
            found = findNamed(GTK_WIDGET(child->data), name);
        }
        g_list_free(children);
        return found;
    }
    return nullptr;
}

GtkWidget* findButton(GtkWidget* widget, const char* label) {
    if (GTK_IS_BUTTON(widget) && g_strcmp0(gtk_button_get_label(GTK_BUTTON(widget)), label) == 0) {
        return widget;
    }
    if (GTK_IS_CONTAINER(widget)) {
        GList* children = gtk_container_get_children(GTK_CONTAINER(widget));
        GtkWidget* found = nullptr;
        for (GList* child = children; child && !found; child = child->next) {
            found = findButton(GTK_WIDGET(child->data), label);
        }
        g_list_free(children);
        return found;
    }
    return nullptr;
}
}  // namespace

TEST(TextContextualSettings, exposesLiveFormattingAndModeSpecificAnswerBoxControls) {
    if (!gtk_init_check(nullptr, nullptr)) {
        GTEST_SKIP() << "No display available";
    }
    GtkApplication* app = gtk_application_new("org.utn.text.contextual.test", G_APPLICATION_NON_UNIQUE);
    GError* error = nullptr;
    ASSERT_TRUE(g_application_register(G_APPLICATION(app), nullptr, &error)) << (error ? error->message : "");
    g_clear_error(&error);

    GladeSearchpath paths;
    paths.addSearchDirectory(fs::path(GET_UI_FOLDER));
    paths.addSearchDirectory(fs::path(PROJECT_SOURCE_DIR) / "resources");
    paths.addSearchDirectory(Util::getExePath().parent_path() / "resources-templates");
    Control control(G_APPLICATION(app), &paths, true);
    control.getSettings()->setAutosaveEnabled(false);
    auto window = std::make_unique<MainWindow>(&paths, &control, app);
    control.initWindow(window.get());
    window->populate(&paths);
    control.selectTool(TOOL_PEN);
    GtkWidget* panel = ToolUtnContextBar::createTextSettingsPanel(&control);
    g_object_ref_sink(panel);

    for (const char* name: {"Font", "Text size", "Bold", "Italic", "Underline", "Strikethrough", "Alignment"}) {
        EXPECT_NE(findNamed(panel, name), nullptr) << name;
    }
    EXPECT_EQ(control.getToolHandler()->getToolType(), TOOL_PEN);  // constructing settings never selects Text

    ToolTextMode textMode("UTN_TEXT_CONTEXTUAL_TEST", &control, IconNameHelper(control.getSettings()), TextMode::Plain);
    auto toolGroup = textMode.createItem(true);
    GList* toolChildren = gtk_container_get_children(GTK_CONTAINER(toolGroup.get()));
    ASSERT_NE(toolChildren, nullptr);
    auto* toolButton = GTK_TOGGLE_BUTTON(toolChildren->data);
    auto* optionsButton = GTK_TOGGLE_BUTTON(toolChildren->next->data);
    gtk_button_clicked(GTK_BUTTON(toolButton));
    EXPECT_EQ(control.getToolHandler()->getToolType(), TOOL_TEXT);  // first activation selects the tool
    EXPECT_TRUE(gtk_toggle_button_get_active(toolButton));
    GdkEventKey key{};
    key.keyval = GDK_KEY_Return;
    gboolean handled = FALSE;
    g_signal_emit_by_name(toolButton, "key-press-event", &key, &handled);
    EXPECT_TRUE(handled);  // repeat activation opens options without deactivating Text
    EXPECT_TRUE(gtk_toggle_button_get_active(optionsButton));
    EXPECT_TRUE(gtk_toggle_button_get_active(toolButton));
    g_list_free(toolChildren);

    auto* mode = GTK_COMBO_BOX(findNamed(panel, "Text and Answer Box mode"));
    ASSERT_NE(mode, nullptr);
    gtk_combo_box_set_active(mode, 1);
    for (int i = 0; i < 20; ++i) {
        while (gtk_events_pending()) {
            gtk_main_iteration_do(false);
        }
        g_usleep(2000);
    }
    EXPECT_EQ(control.getToolHandler()->getTextMode(), TextMode::AnswerBox);
    EXPECT_NE(findButton(panel, "Delete Box"), nullptr);
    EXPECT_NE(findButton(panel, "Box style"), nullptr);

    gtk_widget_destroy(panel);
    g_object_unref(panel);
    gtk_widget_destroy(toolGroup.get());
    window.reset();
    g_object_unref(app);
}
