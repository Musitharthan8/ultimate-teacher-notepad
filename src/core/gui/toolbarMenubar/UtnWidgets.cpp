/*
 * Ultimate Teacher Notepad
 *
 * Based on Xournal++ GPLv2+
 */

#include "UtnWidgets.h"

#include <algorithm>
#include <cmath>
#include <string>

#include "util/Util.h"         // for argb_to_GdkRGBA
#include "util/gtk4_helper.h"  // for gtk_box_append, gtk_popover_set_child

namespace utn {

auto createPopoverPanel(int spacing) -> PopoverPanel {
    GtkPopover* popover = GTK_POPOVER(gtk_popover_new());
    gtk_widget_add_css_class(GTK_WIDGET(popover), "toolbar");

    GtkBox* panel = GTK_BOX(gtk_box_new(GTK_ORIENTATION_VERTICAL, spacing));
    gtk_widget_add_css_class(GTK_WIDGET(panel), "utn-menu");
    gtk_widget_set_margin_start(GTK_WIDGET(panel), 8);
    gtk_widget_set_margin_end(GTK_WIDGET(panel), 8);
    gtk_widget_set_margin_top(GTK_WIDGET(panel), 8);
    gtk_widget_set_margin_bottom(GTK_WIDGET(panel), 8);
    gtk_popover_set_child(popover, GTK_WIDGET(panel));
    return {popover, panel};
}

void appendPopoverHeading(GtkBox* panel, const char* text) {
    GtkWidget* heading = gtk_label_new(text);
    gtk_widget_set_halign(heading, GTK_ALIGN_START);
    gtk_widget_add_css_class(heading, "utn-popover-title");
    gtk_box_append(panel, heading);
}

auto appendMenuButton(GtkBox* panel, GtkPopover* popover, const char* label, const char* hint) -> GtkWidget* {
    GtkWidget* button = gtk_button_new_with_label(label);
    gtk_button_set_relief(GTK_BUTTON(button), GTK_RELIEF_NONE);
    if (GtkWidget* text = gtk_bin_get_child(GTK_BIN(button)); GTK_IS_LABEL(text)) {
        gtk_label_set_xalign(GTK_LABEL(text), 0.0F);
    }
    if (hint) {
        gtk_widget_set_tooltip_text(button, hint);
    }
    if (popover) {
        g_signal_connect_swapped(button, "clicked", G_CALLBACK(gtk_popover_popdown), popover);
    }
    gtk_box_append(panel, button);
    return button;
}

void setAccessibleName(GtkWidget* widget, const char* name) {
    if (AtkObject* accessible = gtk_widget_get_accessible(widget)) {
        atk_object_set_name(accessible, name);
    }
}

void showAcceleratorInTooltip(GtkWidget* button) {
    g_signal_connect(button, "query-tooltip",
                     G_CALLBACK(+[](GtkWidget* widget, gint, gint, gboolean, GtkTooltip* tooltip, gpointer) -> gboolean {
                         gchar* text = gtk_widget_get_tooltip_text(widget);
                         if (!text) {
                             return FALSE;
                         }
                         std::string full = text;
                         g_free(text);

                         GApplication* app = g_application_get_default();
                         const gchar* action = gtk_actionable_get_action_name(GTK_ACTIONABLE(widget));
                         if (action && GTK_IS_APPLICATION(app)) {
                             gchar* detailed = g_action_print_detailed_name(
                                     action, gtk_actionable_get_action_target_value(GTK_ACTIONABLE(widget)));
                             gchar** accels = gtk_application_get_accels_for_action(GTK_APPLICATION(app), detailed);
                             guint key = 0;
                             GdkModifierType mods{};
                             if (accels && accels[0]) {
                                 gtk_accelerator_parse(accels[0], &key, &mods);
                             }
                             if (key) {
                                 gchar* label = gtk_accelerator_get_label(key, mods);
                                 full += std::string(" (") + label + ")";
                                 g_free(label);
                             }
                             g_strfreev(accels);
                             g_free(detailed);
                         }
                         gtk_tooltip_set_text(tooltip, full.c_str());
                         return TRUE;
                     }),
                     nullptr);
}

void syncToggle(GtkToggleButton* toggle, bool active) {
    if (gtk_toggle_button_get_active(toggle) == active) {
        return;
    }
    g_object_set_data(G_OBJECT(toggle), "utn-syncing", GINT_TO_POINTER(1));
    gtk_toggle_button_set_active(toggle, active);
    g_object_set_data(G_OBJECT(toggle), "utn-syncing", nullptr);
}

auto isSyncing(GtkWidget* widget) -> bool { return g_object_get_data(G_OBJECT(widget), "utn-syncing") != nullptr; }

auto createColourSwatch(Color colour, const char* name) -> GtkWidget* {
    GtkWidget* button = gtk_button_new();
    gtk_widget_add_css_class(button, "utn-swatch");
    gtk_widget_set_tooltip_text(button, name);
    setAccessibleName(button, name);

    GtkWidget* dot = gtk_drawing_area_new();
    gtk_widget_set_size_request(dot, 18, 18);
    auto* rgba = new GdkRGBA(Util::argb_to_GdkRGBA(colour));
    rgba->alpha = 1.0;  // swatches show the hue; opacity has its own control
    g_object_set_data_full(G_OBJECT(dot), "utn-rgba", rgba, +[](gpointer p) { delete static_cast<GdkRGBA*>(p); });
    g_signal_connect(dot, "draw", G_CALLBACK(+[](GtkWidget* widget, cairo_t* cr, gpointer) -> gboolean {
                         auto* rgba = static_cast<GdkRGBA*>(g_object_get_data(G_OBJECT(widget), "utn-rgba"));
                         const double w = gtk_widget_get_allocated_width(widget);
                         const double h = gtk_widget_get_allocated_height(widget);
                         const double r = std::min(w, h) / 2.0 - 1.0;
                         cairo_arc(cr, w / 2.0, h / 2.0, r, 0, 2 * M_PI);
                         gdk_cairo_set_source_rgba(cr, rgba);
                         cairo_fill_preserve(cr);
                         // A thin outline keeps white and pale colours visible on light themes
                         cairo_set_source_rgba(cr, 0, 0, 0, 0.35);
                         cairo_set_line_width(cr, 1.0);
                         cairo_stroke(cr);
                         return false;
                     }),
                     nullptr);
    gtk_button_set_child(GTK_BUTTON(button), dot);
    return button;
}

void setSwatchSelected(GtkWidget* swatch, bool selected) {
    if (selected) {
        gtk_widget_add_css_class(swatch, "utn-swatch-selected");
        gtk_widget_set_state_flags(swatch, GTK_STATE_FLAG_CHECKED, false);
    } else {
        gtk_widget_remove_css_class(swatch, "utn-swatch-selected");
        gtk_widget_unset_state_flags(swatch, GTK_STATE_FLAG_CHECKED);
    }
}

}  // namespace utn
