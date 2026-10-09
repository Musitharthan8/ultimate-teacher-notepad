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

#include "UtnWidgets.h"

namespace {
struct AppearanceEntry {
    const char* label;
    ThemeVariant variant;
};

constexpr std::array<AppearanceEntry, 4> APPEARANCES{{
        {N_("Match system"), THEME_VARIANT_USE_SYSTEM},
        {N_("Light"), THEME_VARIANT_FORCE_LIGHT},
        {N_("Dark"), THEME_VARIANT_FORCE_DARK},
        {N_("High contrast"), THEME_VARIANT_HIGH_CONTRAST},
}};
}  // namespace

ToolAppearance::ToolAppearance(std::string id, Control* control, IconNameHelper iconNameHelper):
        AbstractToolItem(std::move(id), Category::MISC),
        control(control),
        iconName(iconNameHelper.iconName("utn-appearance")) {}

auto ToolAppearance::createItem(bool horizontal) -> xoj::util::WidgetSPtr {
    auto [popover, panel] = utn::createPopoverPanel();
    utn::appendPopoverHeading(panel, _("Colours"));

    // Radio buttons show which appearance is in use; pages and PDFs always keep their own colours
    const ThemeVariant current = control->getSettings()->getThemeVariant();
    GtkWidget* group = nullptr;
    for (const auto& entry: APPEARANCES) {
        GtkWidget* button = group ? gtk_radio_button_new_with_label_from_widget(GTK_RADIO_BUTTON(group), _(entry.label)) :
                                    gtk_radio_button_new_with_label(nullptr, _(entry.label));
        group = group ? group : button;
        gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(button), entry.variant == current);
        g_object_set_data(G_OBJECT(button), "utn-theme", GINT_TO_POINTER(static_cast<int>(entry.variant)));

        g_signal_connect(button, "toggled", G_CALLBACK(+[](GtkToggleButton* button, gpointer data) {
                             if (!gtk_toggle_button_get_active(button)) {
                                 return;
                             }
                             auto* ctrl = static_cast<Control*>(data);
                             auto variant = static_cast<ThemeVariant>(
                                     GPOINTER_TO_INT(g_object_get_data(G_OBJECT(button), "utn-theme")));
                             ctrl->getSettings()->setThemeVariant(variant);
                             ctrl->getWindow()->updateColorscheme();
                         }),
                         control);
        gtk_box_append(panel, button);
    }

    gtk_box_append(panel, gtk_separator_new(GTK_ORIENTATION_HORIZONTAL));

    bool touchUi = false;
    control->getSettings()->getCustomElement("utn").getBool("touchUi", touchUi);

    utn::appendPopoverHeading(panel, _("Size"));
    GtkWidget* density = gtk_check_button_new_with_label(_("Larger buttons for touch and stylus"));
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(density), touchUi);
    gtk_widget_set_tooltip_text(density, _("Easier to tap on a touchscreen or interactive whiteboard"));

    g_signal_connect(
            density,
            "toggled",
            G_CALLBACK(+[](GtkToggleButton* button, gpointer data) {
                auto* ctrl = static_cast<Control*>(data);
                bool enabled = gtk_toggle_button_get_active(button);

                SElement& utn = ctrl->getSettings()->getCustomElement("utn");
                utn.setBool("touchUi", enabled);
                ctrl->getSettings()->customSettingsChanged();

                GtkStyleContext* context =
                        gtk_widget_get_style_context(GTK_WIDGET(ctrl->getWindow()->getWindow()));
                if (enabled) {
                    gtk_style_context_add_class(context, "utnTouch");
                } else {
                    gtk_style_context_remove_class(context, "utnTouch");
                }
            }),
            control);

    gtk_box_append(panel, density);

    GtkMenuButton* menuButton = GTK_MENU_BUTTON(gtk_menu_button_new());
    gtk_widget_set_focus_on_click(GTK_WIDGET(menuButton), false);  // keyboard users can still Tab to it
    gtk_widget_set_tooltip_text(GTK_WIDGET(menuButton), getToolDisplayName().c_str());
    auto* heading = GTK_BOX(gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 5));
    gtk_box_append(heading, getNewToolIcon());
    gtk_box_append(heading, gtk_label_new(_("Appearance")));
    gtk_widget_show_all(GTK_WIDGET(heading));
    gtk_button_set_child(GTK_BUTTON(menuButton), GTK_WIDGET(heading));
    gtk_menu_button_set_popover(menuButton, GTK_WIDGET(popover));
    gtk_menu_button_set_direction(menuButton, horizontal ? GTK_ARROW_DOWN : GTK_ARROW_RIGHT);

    gtk_widget_show_all(GTK_WIDGET(panel));
    return xoj::util::WidgetSPtr(GTK_WIDGET(menuButton), xoj::util::adopt);
}

auto ToolAppearance::getToolDisplayName() const -> std::string {
    return _("Appearance");
}

auto ToolAppearance::getNewToolIcon() const -> GtkWidget* {
    return gtk_image_new_from_icon_name(iconName.c_str(), GTK_ICON_SIZE_LARGE_TOOLBAR);
}
