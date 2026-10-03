/*
 * Ultimate Teacher Notepad
 *
 * Shell appearance selector
 *
 * Based on Xournal++ GPLv2+
 */

#include "ToolAppearance.h"

#include <array>
#include <utility>

#include "control/Control.h"
#include "control/settings/Settings.h"
#include "control/settings/SettingsEnums.h"
#include "gui/MainWindow.h"
#include "util/gtk4_helper.h"
#include "util/i18n.h"

namespace {
struct AppearanceEntry {
    const char* label;
    ThemeVariant variant;
};

constexpr std::array<AppearanceEntry, 4> APPEARANCES{{
        {"System", THEME_VARIANT_USE_SYSTEM},
        {"Light", THEME_VARIANT_FORCE_LIGHT},
        {"Dark", THEME_VARIANT_FORCE_DARK},
        {"High Contrast", THEME_VARIANT_HIGH_CONTRAST},
}};
}  // namespace

ToolAppearance::ToolAppearance(std::string id, Control* control):
        AbstractToolItem(std::move(id), Category::MISC), control(control) {}

auto ToolAppearance::createItem(bool horizontal) -> xoj::util::WidgetSPtr {
    GtkPopover* popover = GTK_POPOVER(gtk_popover_new());
    gtk_widget_add_css_class(GTK_WIDGET(popover), "toolbar");

    GtkBox* panel = GTK_BOX(gtk_box_new(GTK_ORIENTATION_VERTICAL, 2));
    gtk_widget_set_margin_start(GTK_WIDGET(panel), 6);
    gtk_widget_set_margin_end(GTK_WIDGET(panel), 6);
    gtk_widget_set_margin_top(GTK_WIDGET(panel), 6);
    gtk_widget_set_margin_bottom(GTK_WIDGET(panel), 6);
    gtk_popover_set_child(popover, GTK_WIDGET(panel));

    for (const auto& entry: APPEARANCES) {
        GtkWidget* button = gtk_button_new_with_label(_(entry.label));
        g_object_set_data(G_OBJECT(button), "utn-control", control);
        g_object_set_data(G_OBJECT(button), "utn-theme", GINT_TO_POINTER(static_cast<int>(entry.variant)));

        g_signal_connect(
                button,
                "clicked",
                G_CALLBACK(+[](GtkButton* button, gpointer data) {
                    auto* ctrl = static_cast<Control*>(g_object_get_data(G_OBJECT(button), "utn-control"));
                    auto variant = static_cast<ThemeVariant>(
                            GPOINTER_TO_INT(g_object_get_data(G_OBJECT(button), "utn-theme")));

                    ctrl->getSettings()->setThemeVariant(variant);
                    ctrl->getWindow()->updateColorscheme();
                    gtk_popover_popdown(GTK_POPOVER(data));
                }),
                popover);

        gtk_box_append(panel, button);
    }

    GtkMenuButton* menuButton = GTK_MENU_BUTTON(gtk_menu_button_new());
    gtk_widget_set_can_focus(GTK_WIDGET(menuButton), false);
    gtk_widget_set_tooltip_text(GTK_WIDGET(menuButton), getToolDisplayName().c_str());
    gtk_button_set_child(GTK_BUTTON(menuButton), getNewToolIcon());
    gtk_menu_button_set_popover(menuButton, GTK_WIDGET(popover));
    gtk_menu_button_set_direction(menuButton, horizontal ? GTK_ARROW_DOWN : GTK_ARROW_RIGHT);

    gtk_widget_show_all(GTK_WIDGET(panel));
    return xoj::util::WidgetSPtr(GTK_WIDGET(menuButton), xoj::util::adopt);
}

auto ToolAppearance::getToolDisplayName() const -> std::string {
    return _("Appearance");
}

auto ToolAppearance::getNewToolIcon() const -> GtkWidget* {
    return gtk_label_new(_("Theme"));
}
