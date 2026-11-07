/*
 * Ultimate Teacher Notepad
 *
 * TB-01 (live toolbar audit, 2026-11-07): the Eraser side panel clipped its first mode label at the panel edge because
 * the mode names asked for their one-line width while the panel never scrolls sideways. The mode buttons must wrap, so
 * the panel can be as narrow as its minimum content width.
 *
 * Based on Xournal++ GPLv2+
 */

#include <algorithm>
#include <memory>
#include <string>
#include <vector>

#include <gtest/gtest.h>
#include <gtk/gtk.h>

#include "control/Control.h"
#include "control/settings/Settings.h"
#include "gui/GladeSearchpath.h"
#include "gui/MainWindow.h"
#include "gui/toolbarMenubar/EraserModes.h"
#include "gui/toolbarMenubar/ToolUtnContextBar.h"
#include "util/PathUtil.h"

#include "config-test.h"

namespace {
GtkWidget* findByName(GtkWidget* widget, const char* name) {
    if (g_strcmp0(gtk_widget_get_name(widget), name) == 0) {
        return widget;
    }
    if (GTK_IS_CONTAINER(widget)) {
        GList* children = gtk_container_get_children(GTK_CONTAINER(widget));
        GtkWidget* found = nullptr;
        for (GList* child = children; child && !found; child = child->next) {
            found = findByName(GTK_WIDGET(child->data), name);
        }
        g_list_free(children);
        return found;
    }
    return nullptr;
}

void collectModeLabels(GtkWidget* widget, std::vector<GtkLabel*>& labels) {
    if (GTK_IS_RADIO_BUTTON(widget)) {
        if (GtkWidget* child = gtk_bin_get_child(GTK_BIN(widget)); child && GTK_IS_LABEL(child)) {
            labels.push_back(GTK_LABEL(child));
        }
        return;
    }
    if (GTK_IS_CONTAINER(widget)) {
        GList* children = gtk_container_get_children(GTK_CONTAINER(widget));
        for (GList* child = children; child; child = child->next) {
            collectModeLabels(GTK_WIDGET(child->data), labels);
        }
        g_list_free(children);
    }
}
}  // namespace

TEST(EraserPanelLayout, modeNamesWrapInsteadOfWideningThePanel) {
    if (!gtk_init_check(nullptr, nullptr)) {
        GTEST_SKIP() << "No display available";
    }
    GtkApplication* app = gtk_application_new("org.utn.eraser.layout.test", G_APPLICATION_NON_UNIQUE);
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
    control.selectTool(TOOL_ERASER);

    ToolUtnContextBar bar("UTN_ERASER_LAYOUT_TEST", &control, false);
    auto side = bar.createItem(false);  // the vertical panel beside the rail
    GtkWidget* content = findByName(side.get(), "utnContextBar");
    ASSERT_NE(content, nullptr);

    std::vector<GtkLabel*> labels;
    collectModeLabels(content, labels);
    ASSERT_EQ(labels.size(), utn::ERASER_MODES.size());

    int longestOneLine = 0;
    for (GtkLabel* label: labels) {
        EXPECT_TRUE(gtk_label_get_line_wrap(label)) << gtk_label_get_text(label) << " must wrap, not clip";
        EXPECT_EQ(gtk_widget_get_halign(GTK_WIDGET(label)), GTK_ALIGN_START) << "text starts at the panel's left edge";
        PangoLayout* layout = gtk_widget_create_pango_layout(GTK_WIDGET(label), gtk_label_get_text(label));
        int width = 0;
        pango_layout_get_pixel_size(layout, &width, nullptr);
        g_object_unref(layout);
        longestOneLine = std::max(longestOneLine, width);
    }

    int minimum = 0;
    gtk_widget_get_preferred_width(content, &minimum, nullptr);
    EXPECT_LT(minimum, longestOneLine) << "the panel must be able to be narrower than the longest mode name on one line";

    // Tear down in construction order reversed: panel, then the bar that built it, then the window and the app.
    gtk_widget_destroy(side.get());
    side.reset();
    while (gtk_events_pending()) {
        gtk_main_iteration_do(false);
    }
    window.reset();
    g_object_unref(app);
}
