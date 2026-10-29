/*
 * Ultimate Teacher Notepad
 *
 * Based on Xournal++ GPLv2+
 */

#include "UtnWidgets.h"

#include "control/Control.h"
#include "gui/MainWindow.h"

#include <algorithm>
#include <cmath>
#include <string>

#include "util/Util.h"         // for argb_to_GdkRGBA
#include "util/gtk4_helper.h"  // for gtk_box_append, gtk_popover_set_child
#include "util/i18n.h"

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

void setMenuButtonLabel(GtkWidget* button, const char* label) {
    gtk_button_set_label(GTK_BUTTON(button), label);
    if (GtkWidget* text = gtk_bin_get_child(GTK_BIN(button)); GTK_IS_LABEL(text)) {
        gtk_label_set_xalign(GTK_LABEL(text), 0.0F);
    }
}

auto appendPinSettingsButton(GtkBox* panel, GtkPopover* popover, Control* control) -> GtkWidget* {
    GtkWidget* button = appendMenuButton(panel, popover, _("Pin settings panel"),
                                         _("Keep the full tool settings panel open beside the worksheet"));
    setAccessibleName(button, _("Pin settings panel"));
    g_signal_connect(button, "clicked", G_CALLBACK(+[](GtkButton*, gpointer data) {
                         auto* control = static_cast<Control*>(data);
                         if (MainWindow* window = control->getWindow()) {
                             window->setTeacherWorkspace(0);
                         }
                     }), control);
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

static void refreshToolOptionsName(GtkWidget* tool, GtkMenuButton* options) {
    gchar* name = gtk_widget_get_tooltip_text(tool);
    gchar* label = g_strdup_printf(_("%s options"), name ? name : _("Tool"));
    setAccessibleName(GTK_WIDGET(options), label);
    gtk_widget_set_tooltip_text(GTK_WIDGET(options), label);
    g_free(label);
    g_free(name);
}

void bindActiveToolOptions(GtkToggleButton* tool, GtkMenuButton* options) {
    refreshToolOptionsName(GTK_WIDGET(tool), options);
    g_signal_connect_object(tool, "notify::tooltip-text",
            G_CALLBACK(+[](GObject* widget, GParamSpec*, gpointer data) {
                refreshToolOptionsName(GTK_WIDGET(widget), GTK_MENU_BUTTON(data));
            }), options, GConnectFlags(0));
    g_signal_connect_object(tool, "button-press-event",
            G_CALLBACK(+[](GtkWidget* widget, GdkEventButton* event, gpointer data) -> gboolean {
                if (event->button != 1 || !gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(widget))) {
                    return FALSE;
                }
                gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(data), true);
                return TRUE;
            }), options, GConnectFlags(0));
    g_signal_connect_object(tool, "key-press-event",
            G_CALLBACK(+[](GtkWidget* widget, GdkEventKey* event, gpointer data) -> gboolean {
                if (!gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(widget)) ||
                    (event->state & (GDK_CONTROL_MASK | GDK_MOD1_MASK | GDK_SUPER_MASK)) ||
                    (event->keyval != GDK_KEY_Return && event->keyval != GDK_KEY_KP_Enter &&
                     event->keyval != GDK_KEY_space)) {
                    return FALSE;
                }
                gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(data), true);
                return TRUE;
            }), options, GConnectFlags(0));
    g_signal_connect_object(tool, "notify::active",
            G_CALLBACK(+[](GObject* widget, GParamSpec*, gpointer data) {
                if (!gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(widget))) {
                    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(data), false);
                }
            }), options, GConnectFlags(0));
}

void placeOptionsCorner(GtkWidget* options) {
    gtk_widget_set_halign(options, GTK_ALIGN_END);
    gtk_widget_set_valign(options, GTK_ALIGN_END);
    gtk_widget_set_margin_end(options, 3);
    gtk_widget_set_margin_bottom(options, 3);
}

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

auto createGlyph(Glyph glyph, double weight) -> GtkWidget* {
    GtkWidget* area = gtk_drawing_area_new();
    const bool isWeight = glyph == Glyph::Weight;
    gtk_widget_set_size_request(area, isWeight ? 30 : 26, isWeight ? 14 : 18);
    gtk_widget_set_halign(area, GTK_ALIGN_CENTER);
    g_object_set_data(G_OBJECT(area), "utn-glyph", GINT_TO_POINTER(static_cast<int>(glyph)));
    g_object_set_data(G_OBJECT(area), "utn-glyph-weight", GINT_TO_POINTER(static_cast<int>(weight * 100.0)));
    g_signal_connect(
            area, "draw", G_CALLBACK(+[](GtkWidget* widget, cairo_t* cr, gpointer) -> gboolean {
                // Colour comes from the enclosing button, so hover, checked and High Contrast states carry over
                GtkWidget* owner = widget;
                for (GtkWidget* it = widget; it; it = gtk_widget_get_parent(it)) {
                    if (GTK_IS_BUTTON(it)) {
                        owner = it;
                        break;
                    }
                }
                GtkStyleContext* style = gtk_widget_get_style_context(owner);
                GdkRGBA colour;
                gtk_style_context_get_color(style, gtk_style_context_get_state(style), &colour);
                gdk_cairo_set_source_rgba(cr, &colour);

                const double w = gtk_widget_get_allocated_width(widget);
                const double h = gtk_widget_get_allocated_height(widget);
                const auto kind = static_cast<Glyph>(GPOINTER_TO_INT(g_object_get_data(G_OBJECT(widget), "utn-glyph")));
                const double weightPx = GPOINTER_TO_INT(g_object_get_data(G_OBJECT(widget), "utn-glyph-weight")) / 100.0;
                cairo_set_line_cap(cr, CAIRO_LINE_CAP_ROUND);
                cairo_set_line_join(cr, CAIRO_LINE_JOIN_ROUND);
                cairo_set_line_width(cr, 1.6);
                const double pad = 3.0;
                switch (kind) {
                    case Glyph::Freehand:
                        cairo_move_to(cr, pad, h * 0.7);
                        cairo_curve_to(cr, w * 0.3, h * 0.0, w * 0.45, h * 1.1, w * 0.62, h * 0.45);
                        cairo_curve_to(cr, w * 0.72, h * 0.15, w * 0.85, h * 0.35, w - pad, h * 0.3);
                        cairo_stroke(cr);
                        break;
                    case Glyph::Rectangle:
                        cairo_rectangle(cr, pad, pad + 1, w - 2 * pad, h - 2 * pad - 2);
                        cairo_stroke(cr);
                        break;
                    case Glyph::Ellipse:
                        cairo_save(cr);
                        cairo_translate(cr, w / 2.0, h / 2.0);
                        cairo_scale(cr, (w - 2 * pad) / 2.0, (h - 2 * pad + 2) / 2.0);
                        cairo_arc(cr, 0, 0, 1.0, 0, 2 * M_PI);
                        cairo_restore(cr);
                        cairo_stroke(cr);
                        break;
                    case Glyph::Line:
                        cairo_move_to(cr, pad, h - pad);
                        cairo_line_to(cr, w - pad, pad);
                        cairo_stroke(cr);
                        break;
                    case Glyph::Arrow: {
                        const double x1 = w - pad;
                        const double y1 = pad + 1;
                        cairo_move_to(cr, pad, h - pad - 1);
                        cairo_line_to(cr, x1, y1);
                        cairo_stroke(cr);
                        cairo_move_to(cr, x1 - 7, y1);
                        cairo_line_to(cr, x1, y1);
                        cairo_line_to(cr, x1, y1 + 7);
                        cairo_stroke(cr);
                        break;
                    }
                    case Glyph::SmartShape: {
                        // A rough hand-drawn loop, the kind of shape Smart Shape tidies
                        double dashes[2];  // no braced list: its comma would split the G_CALLBACK macro argument
                        dashes[0] = dashes[1] = 2.5;
                        cairo_set_dash(cr, dashes, 2, 0);
                        cairo_save(cr);
                        cairo_translate(cr, w / 2.0, h / 2.0);
                        cairo_scale(cr, (w - 2 * pad) / 2.0, (h - 2 * pad + 2) / 2.0);
                        cairo_arc(cr, 0, 0, 1.0, 0, 2 * M_PI);
                        cairo_restore(cr);
                        cairo_stroke(cr);
                        break;
                    }
                    case Glyph::Weight:
                        cairo_set_line_width(cr, weightPx > 0 ? weightPx : 1.6);
                        cairo_move_to(cr, pad + weightPx / 2, h / 2.0);
                        cairo_line_to(cr, w - pad - weightPx / 2, h / 2.0);
                        cairo_stroke(cr);
                        break;
                }
                return FALSE;
            }),
            nullptr);
    return area;
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
