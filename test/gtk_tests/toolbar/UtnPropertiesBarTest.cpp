/*
 * Ultimate Teacher Notepad
 *
 * The teacher properties bar must receive the toolbar row's width and show its controls, rather than collapsing to a
 * clipped badge. Exercises the real AbstractToolItem wrapping and properties viewport inside a real GtkToolbar.
 *
 * Based on Xournal++ GPLv2+
 */

#include <gtest/gtest.h>
#include <gtk/gtk.h>

#include "gui/toolbarMenubar/AbstractToolItem.h"
#include "gui/toolbarMenubar/ToolUtnContextBar.h"

namespace {

constexpr int CONTROL_WIDTH = 60;

class PropertiesProbe: public AbstractToolItem {
public:
    explicit PropertiesProbe(int controls): AbstractToolItem("UTN_CONTEXT_PROBE", Category::TOOLS), controls(controls) {}

    xoj::util::WidgetSPtr createItem(bool) override {
        content = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
        GtkWidget* badge = gtk_label_new("Highlight");
        gtk_widget_set_size_request(badge, 90, -1);
        gtk_container_add(GTK_CONTAINER(content), badge);
        for (int i = 0; i < controls; ++i) {
            last = gtk_button_new_with_label("x");
            gtk_widget_set_size_request(last, CONTROL_WIDTH, 28);
            gtk_container_add(GTK_CONTAINER(content), last);
        }
        viewport = ToolUtnContextBar::createPropertiesViewport(content);
        return xoj::util::WidgetSPtr(viewport, xoj::util::adopt);
    }
    bool expandsInToolbar() const override { return true; }
    std::string getToolDisplayName() const override { return "probe"; }
    GtkWidget* getNewToolIcon() const override { return gtk_image_new(); }

    int controls;
    GtkWidget* content = nullptr;
    GtkWidget* viewport = nullptr;
    GtkWidget* last = nullptr;
};

struct ToolbarWindow {
    GtkWidget* window = nullptr;
    GtkWidget* toolbar = nullptr;
    xoj::util::WidgetSPtr item;

    ToolbarWindow(PropertiesProbe& probe, int width) {
        window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
        gtk_window_set_default_size(GTK_WINDOW(window), width, 120);
        GtkWidget* column = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
        toolbar = gtk_toolbar_new();
        gtk_widget_set_hexpand(toolbar, true);
        item = probe.createToolItem(true);
        gtk_toolbar_insert(GTK_TOOLBAR(toolbar), GTK_TOOL_ITEM(item.get()), -1);
        gtk_container_add(GTK_CONTAINER(column), toolbar);
        gtk_container_add(GTK_CONTAINER(column), gtk_drawing_area_new());
        gtk_container_add(GTK_CONTAINER(window), column);
        gtk_widget_show_all(window);
        settle();
    }
    ~ToolbarWindow() { gtk_widget_destroy(window); }

    void resize(int width) {
        gtk_window_resize(GTK_WINDOW(window), width, 120);
        settle();
    }

    static void settle() {
        for (int i = 0; i < 50; ++i) {
            while (gtk_events_pending()) {
                gtk_main_iteration_do(false);
            }
            g_usleep(2000);
        }
    }
};

int allocatedWidth(GtkWidget* widget) {
    GtkAllocation a;
    gtk_widget_get_allocation(widget, &a);
    return a.width;
}

int naturalWidth(GtkWidget* widget) {
    int min = 0;
    int nat = 0;
    gtk_widget_get_preferred_width(widget, &min, &nat);
    return nat;
}

class UtnPropertiesBar: public ::testing::Test {
protected:
    void SetUp() override {
        if (!gtk_init_check(nullptr, nullptr)) {
            GTEST_SKIP() << "No display available";
        }
    }
};

}  // namespace

TEST_F(UtnPropertiesBar, propertiesItemExpandsInToolbar) {
    PropertiesProbe probe(4);
    auto item = probe.createToolItem(true);
    ASSERT_TRUE(GTK_IS_TOOL_ITEM(item.get()));
    EXPECT_TRUE(gtk_tool_item_get_expand(GTK_TOOL_ITEM(item.get())));

    // Vertical toolbars and ordinary items keep upstream sizing
    PropertiesProbe vertical(4);
    auto verticalItem = vertical.createToolItem(false);
    EXPECT_FALSE(gtk_tool_item_get_expand(GTK_TOOL_ITEM(verticalItem.get())));
}

TEST_F(UtnPropertiesBar, viewportRequestsItsControlsNaturalWidth) {
    PropertiesProbe probe(10);
    auto item = probe.createToolItem(true);
    gtk_widget_show_all(item.get());
    EXPECT_GE(naturalWidth(probe.viewport), naturalWidth(probe.content));
}

TEST_F(UtnPropertiesBar, wideWindowShowsEveryControl) {
    PropertiesProbe probe(10);
    ToolbarWindow win(probe, 1400);

    const int viewport = allocatedWidth(probe.viewport);
    // The bar fills the row instead of collapsing to the badge
    EXPECT_GE(viewport, allocatedWidth(win.toolbar) * 9 / 10);
    // and has room for its last control without scrolling
    EXPECT_GE(viewport, naturalWidth(probe.content));
    GtkAllocation last;
    gtk_widget_get_allocation(probe.last, &last);
    EXPECT_GT(last.width, 0);
    EXPECT_LE(last.x + last.width, allocatedWidth(probe.content));
}

TEST_F(UtnPropertiesBar, narrowWindowKeepsBarUsableWithoutWideningWindow) {
    PropertiesProbe probe(20);  // wider than the window
    ToolbarWindow win(probe, 1400);
    win.resize(500);

    int windowMin = 0;
    int windowNat = 0;
    gtk_widget_get_preferred_width(win.window, &windowMin, &windowNat);
    // Long property rows do not set the lesson window's minimum width
    EXPECT_LT(windowMin, naturalWidth(probe.content));
    // The bar still gets most of the row and scrolls for the rest
    EXPECT_GE(allocatedWidth(probe.viewport), 300);
    auto* adjustment = gtk_scrolled_window_get_hadjustment(GTK_SCROLLED_WINDOW(probe.viewport));
    EXPECT_GT(gtk_adjustment_get_upper(adjustment), gtk_adjustment_get_page_size(adjustment));
}
