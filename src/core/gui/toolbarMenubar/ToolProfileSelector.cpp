/*
 * Ultimate Teacher Notepad
 *
 * Teacher tool profile selector
 *
 * Based on Xournal++ GPLv2+
 */

#include "ToolProfileSelector.h"

#include <array>
#include <utility>

#include "control/Control.h"
#include "control/ToolHandler.h"
#include "util/Color.h"
#include "util/gtk4_helper.h"
#include "util/i18n.h"

namespace {
enum class Profile {
    Pen,
    Pencil,
    Brush,
    Marker,
    Highlighter,
};

struct ProfileEntry {
    const char* label;
    Profile profile;
};

constexpr std::array<ProfileEntry, 5> PROFILES{{
        {"Pen", Profile::Pen},
        {"Pencil", Profile::Pencil},
        {"Brush", Profile::Brush},
        {"Marker", Profile::Marker},
        {"Highlighter", Profile::Highlighter},
}};

void applyProfile(Control* control, Profile profile) {
    auto* tools = control->getToolHandler();

    switch (profile) {
        case Profile::Pen:
            tools->selectTool(TOOL_PEN);
            tools->setDrawingType(DRAWING_TYPE_DEFAULT);
            tools->setPenSize(TOOL_SIZE_MEDIUM);
            tools->setColor(Colors::xopp_royalblue, false);
            break;
        case Profile::Pencil:
            tools->selectTool(TOOL_PEN);
            tools->setDrawingType(DRAWING_TYPE_DEFAULT);
            tools->setPenSize(TOOL_SIZE_VERY_FINE);
            tools->setColor(Colors::gray, false);
            break;
        case Profile::Brush:
            tools->selectTool(TOOL_PEN);
            tools->setDrawingType(DRAWING_TYPE_DEFAULT);
            tools->setPenSize(TOOL_SIZE_THICK);
            tools->setColor(Colors::black, false);
            break;
        case Profile::Marker:
            tools->selectTool(TOOL_PEN);
            tools->setDrawingType(DRAWING_TYPE_DEFAULT);
            tools->setPenSize(TOOL_SIZE_THICK);
            tools->setColor(Colors::xopp_darkorange, false);
            break;
        case Profile::Highlighter:
            tools->selectTool(TOOL_HIGHLIGHTER);
            tools->setDrawingType(DRAWING_TYPE_DEFAULT);
            tools->setHighlighterSize(TOOL_SIZE_MEDIUM);
            tools->setColor(Colors::yellow, false);
            break;
    }

    tools->fireToolChanged();
}
}  // namespace

ToolProfileSelector::ToolProfileSelector(std::string id, Control* control):
        AbstractToolItem(std::move(id), Category::TOOLS), control(control) {}

auto ToolProfileSelector::createItem(bool horizontal) -> xoj::util::WidgetSPtr {
    GtkPopover* popover = GTK_POPOVER(gtk_popover_new());
    gtk_widget_add_css_class(GTK_WIDGET(popover), "toolbar");

    GtkBox* panel = GTK_BOX(gtk_box_new(GTK_ORIENTATION_VERTICAL, 2));
    gtk_widget_set_margin_start(GTK_WIDGET(panel), 6);
    gtk_widget_set_margin_end(GTK_WIDGET(panel), 6);
    gtk_widget_set_margin_top(GTK_WIDGET(panel), 6);
    gtk_widget_set_margin_bottom(GTK_WIDGET(panel), 6);
    gtk_popover_set_child(popover, GTK_WIDGET(panel));

    for (const auto& entry: PROFILES) {
        GtkWidget* button = gtk_button_new_with_label(entry.label);
        gtk_widget_set_focus_on_click(button, false);  // keyboard users can still Tab to it
        g_object_set_data(G_OBJECT(button), "utn-control", control);
        g_object_set_data(G_OBJECT(button), "utn-profile", GINT_TO_POINTER(static_cast<int>(entry.profile)));

        g_signal_connect(
                button,
                "clicked",
                G_CALLBACK(+[](GtkButton* button, gpointer data) {
                    auto* ctrl = static_cast<Control*>(g_object_get_data(G_OBJECT(button), "utn-control"));
                    auto profile =
                            static_cast<Profile>(GPOINTER_TO_INT(g_object_get_data(G_OBJECT(button), "utn-profile")));

                    applyProfile(ctrl, profile);
                    gtk_popover_popdown(GTK_POPOVER(data));
                }),
                popover);

        gtk_box_append(panel, button);
    }

    GtkMenuButton* menuButton = GTK_MENU_BUTTON(gtk_menu_button_new());
    gtk_widget_set_focus_on_click(GTK_WIDGET(menuButton), false);  // keyboard users can still Tab to it
    gtk_widget_set_tooltip_text(GTK_WIDGET(menuButton), getToolDisplayName().c_str());
    gtk_button_set_child(GTK_BUTTON(menuButton), getNewToolIcon());
    gtk_menu_button_set_popover(menuButton, GTK_WIDGET(popover));
    gtk_menu_button_set_direction(menuButton, horizontal ? GTK_ARROW_DOWN : GTK_ARROW_RIGHT);

    gtk_widget_show_all(GTK_WIDGET(panel));
    return xoj::util::WidgetSPtr(GTK_WIDGET(menuButton), xoj::util::adopt);
}

auto ToolProfileSelector::getToolDisplayName() const -> std::string {
    return _("Tool Profiles");
}

auto ToolProfileSelector::getNewToolIcon() const -> GtkWidget* {
    return gtk_label_new("P");
}
