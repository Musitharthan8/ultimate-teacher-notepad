/*
 * Ultimate Teacher Notepad
 *
 * Shared teacher-interface widgets: keeping a button in step with the active tool must never count as a click,
 * and icon-only controls must carry names for screen readers.
 *
 * Based on Xournal++ GPLv2+
 */

#include <gtest/gtest.h>
#include <gtk/gtk.h>

#include "gui/toolbarMenubar/UtnWidgets.h"

namespace {
class UtnWidgets: public ::testing::Test {
protected:
    void SetUp() override {
        if (!gtk_init_check(nullptr, nullptr)) {
            GTEST_SKIP() << "No display available";
        }
    }
};

int userClicks = 0;
void countUserClick(GtkButton* button, gpointer) {
    if (!utn::isSyncing(GTK_WIDGET(button))) {
        ++userClicks;
    }
}
}  // namespace

TEST_F(UtnWidgets, syncingAToggleIsNotAUserClick) {
    GtkWidget* toggle = gtk_toggle_button_new();
    g_object_ref_sink(toggle);
    g_signal_connect(toggle, "clicked", G_CALLBACK(countUserClick), nullptr);
    userClicks = 0;

    // GTK3 reports these as clicks; the handler must be able to tell them apart
    utn::syncToggle(GTK_TOGGLE_BUTTON(toggle), true);
    utn::syncToggle(GTK_TOGGLE_BUTTON(toggle), false);
    EXPECT_EQ(userClicks, 0);
    EXPECT_FALSE(gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(toggle)));

    gtk_button_clicked(GTK_BUTTON(toggle));
    EXPECT_EQ(userClicks, 1);
    EXPECT_FALSE(utn::isSyncing(toggle));
    g_object_unref(toggle);
}

TEST_F(UtnWidgets, swatchesAreNamedAndShowSelection) {
    GtkWidget* swatch = utn::createColourSwatch(Color{0xD3U, 0x2FU, 0x2FU}, "Red");
    g_object_ref_sink(swatch);
    EXPECT_STREQ(atk_object_get_name(gtk_widget_get_accessible(swatch)), "Red");
    EXPECT_FALSE(gtk_widget_get_state_flags(swatch) & GTK_STATE_FLAG_CHECKED);

    utn::setSwatchSelected(swatch, true);
    EXPECT_TRUE(gtk_widget_get_state_flags(swatch) & GTK_STATE_FLAG_CHECKED);
    EXPECT_TRUE(gtk_style_context_has_class(gtk_widget_get_style_context(swatch), "utn-swatch-selected"));

    utn::setSwatchSelected(swatch, false);
    EXPECT_FALSE(gtk_widget_get_state_flags(swatch) & GTK_STATE_FLAG_CHECKED);
    g_object_unref(swatch);
}

TEST_F(UtnWidgets, menuEntriesCloseTheirMenu) {
    auto [popover, panel] = utn::createPopoverPanel();
    g_object_ref_sink(popover);
    GtkWidget* entry = utn::appendMenuButton(panel, popover, "Hide answers", "Hide the answers layer");
    EXPECT_EQ(gtk_widget_get_parent(entry), GTK_WIDGET(panel));
    gchar* tooltip = gtk_widget_get_tooltip_text(entry);
    EXPECT_STREQ(tooltip, "Hide the answers layer");
    g_free(tooltip);
    // Clicking an entry pops the menu down (no crash when it is not shown)
    gtk_button_clicked(GTK_BUTTON(entry));
    EXPECT_FALSE(gtk_widget_get_visible(GTK_WIDGET(popover)));
    gtk_widget_destroy(GTK_WIDGET(popover));
    g_object_unref(popover);
}
