/*
 * Ultimate Teacher Notepad
 *
 * Answer box toolbar toggle
 *
 * Based on Xournal++ GPLv2+
 */

#include "ToolAnswerBox.h"

#include <array>
#include <utility>

#include "control/Control.h"
#include "control/ToolHandler.h"
#include "util/Color.h"
#include "util/gtk4_helper.h"
#include "util/i18n.h"

ToolAnswerBox::ToolAnswerBox(std::string id, Control* control, IconNameHelper iconNameHelper):
        AbstractToolItem(std::move(id), Category::TOOLS),
        control(control),
        iconName(iconNameHelper.iconName("tool-text")) {}

auto ToolAnswerBox::createItem(bool horizontal) -> xoj::util::WidgetSPtr {
    auto* tools = control->getToolHandler();

    GtkWidget* toggle = gtk_toggle_button_new();
    gtk_button_set_child(GTK_BUTTON(toggle), getNewToolIcon());
    gtk_widget_set_tooltip_text(toggle, getToolDisplayName().c_str());
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(toggle), tools->isAnswerBoxEnabled());

    // Enable answer box mode and switch directly to text
    g_signal_connect(
            toggle,
            "toggled",
            G_CALLBACK(+[](GtkToggleButton* button, gpointer data) {
                auto* ctrl = static_cast<Control*>(data);
                bool enabled = gtk_toggle_button_get_active(button);

                auto* tools = ctrl->getToolHandler();
                tools->setAnswerBoxEnabled(enabled);

                if (enabled) {
                    tools->selectTool(TOOL_TEXT);
                    tools->fireToolChanged();
                }
            }),
            control);

    GtkPopover* popover = GTK_POPOVER(gtk_popover_new());
    gtk_widget_add_css_class(GTK_WIDGET(popover), "toolbar");

    GtkBox* panel = GTK_BOX(gtk_box_new(GTK_ORIENTATION_VERTICAL, 6));
    gtk_widget_set_margin_start(GTK_WIDGET(panel), 8);
    gtk_widget_set_margin_end(GTK_WIDGET(panel), 8);
    gtk_widget_set_margin_top(GTK_WIDGET(panel), 8);
    gtk_widget_set_margin_bottom(GTK_WIDGET(panel), 8);
    gtk_popover_set_child(popover, GTK_WIDGET(panel));

    auto appendRow = [panel](const char* label, GtkWidget* widget) {
        GtkBox* row = GTK_BOX(gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8));
        GtkWidget* text = gtk_label_new(label);
        gtk_widget_set_halign(text, GTK_ALIGN_START);
        gtk_widget_set_hexpand(text, true);
        gtk_box_append(row, text);
        gtk_box_append(row, widget);
        gtk_box_append(panel, GTK_WIDGET(row));
    };

    GtkWidget* presetTitle = gtk_label_new(_("Presets"));
    gtk_widget_set_halign(presetTitle, GTK_ALIGN_START);
    gtk_box_append(panel, presetTitle);

    GtkBox* presetBox = GTK_BOX(gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 4));
    struct Preset {
        const char* label;
        Color background;
        Color border;
    };

    const std::array<Preset, 4> presets{{
            {_("Model Answer"), Color{255U, 248U, 214U, 230U}, Color{180U, 140U, 20U, 255U}},
            {_("Definition"), Color{224U, 240U, 255U, 230U}, Color{50U, 110U, 180U, 255U}},
            {_("Warning"), Color{255U, 228U, 232U, 235U}, Color{190U, 55U, 70U, 255U}},
            {_("Note"), Color{232U, 247U, 232U, 230U}, Color{60U, 135U, 75U, 255U}},
    }};

    for (const auto& preset: presets) {
        GtkWidget* button = gtk_button_new_with_label(preset.label);
        g_object_set_data(G_OBJECT(button), "utn-control", control);

        auto* backgroundData = new Color(preset.background);
        auto* borderData = new Color(preset.border);
        g_object_set_data_full(G_OBJECT(button), "utn-background", backgroundData,
                               +[](gpointer data) { delete static_cast<Color*>(data); });
        g_object_set_data_full(G_OBJECT(button), "utn-border", borderData,
                               +[](gpointer data) { delete static_cast<Color*>(data); });

        g_signal_connect(
                button,
                "clicked",
                G_CALLBACK(+[](GtkButton* button, gpointer) {
                    auto* ctrl = static_cast<Control*>(g_object_get_data(G_OBJECT(button), "utn-control"));
                    auto* background = static_cast<Color*>(g_object_get_data(G_OBJECT(button), "utn-background"));
                    auto* border = static_cast<Color*>(g_object_get_data(G_OBJECT(button), "utn-border"));

                    auto* tools = ctrl->getToolHandler();
                    tools->setAnswerBoxBackgroundColor(*background);
                    tools->setAnswerBoxBorderColor(*border);
                    tools->setAnswerBoxBorderWidth(1.2);
                    tools->setAnswerBoxPadding(6.0);
                    tools->setAnswerBoxCornerRadius(5.0);
                }),
                nullptr);

        gtk_box_append(presetBox, button);
    }
    gtk_box_append(panel, GTK_WIDGET(presetBox));

    GdkRGBA background = Util::argb_to_GdkRGBA(tools->getAnswerBoxBackgroundColor());
    GtkWidget* backgroundButton = gtk_color_button_new_with_rgba(&background);
    gtk_color_chooser_set_use_alpha(GTK_COLOR_CHOOSER(backgroundButton), true);
    g_signal_connect(
            backgroundButton,
            "color-set",
            G_CALLBACK(+[](GtkColorButton* button, gpointer data) {
                auto* ctrl = static_cast<Control*>(data);
                GdkRGBA color{};
                gtk_color_chooser_get_rgba(GTK_COLOR_CHOOSER(button), &color);
                ctrl->getToolHandler()->setAnswerBoxBackgroundColor(Util::GdkRGBA_to_argb(color));
            }),
            control);
    appendRow(_("Background"), backgroundButton);

    GdkRGBA border = Util::argb_to_GdkRGBA(tools->getAnswerBoxBorderColor());
    GtkWidget* borderButton = gtk_color_button_new_with_rgba(&border);
    gtk_color_chooser_set_use_alpha(GTK_COLOR_CHOOSER(borderButton), true);
    g_signal_connect(
            borderButton,
            "color-set",
            G_CALLBACK(+[](GtkColorButton* button, gpointer data) {
                auto* ctrl = static_cast<Control*>(data);
                GdkRGBA color{};
                gtk_color_chooser_get_rgba(GTK_COLOR_CHOOSER(button), &color);
                ctrl->getToolHandler()->setAnswerBoxBorderColor(Util::GdkRGBA_to_argb(color));
            }),
            control);
    appendRow(_("Border"), borderButton);

    GtkWidget* borderWidth = gtk_scale_new_with_range(GTK_ORIENTATION_HORIZONTAL, 0.0, 8.0, 0.2);
    gtk_range_set_value(GTK_RANGE(borderWidth), tools->getAnswerBoxBorderWidth());
    gtk_scale_set_digits(GTK_SCALE(borderWidth), 1);
    gtk_widget_set_size_request(borderWidth, 140, -1);
    g_signal_connect(
            borderWidth,
            "value-changed",
            G_CALLBACK(+[](GtkRange* range, gpointer data) {
                static_cast<Control*>(data)->getToolHandler()->setAnswerBoxBorderWidth(gtk_range_get_value(range));
            }),
            control);
    appendRow(_("Border width"), borderWidth);

    GtkWidget* padding = gtk_scale_new_with_range(GTK_ORIENTATION_HORIZONTAL, 0.0, 24.0, 1.0);
    gtk_range_set_value(GTK_RANGE(padding), tools->getAnswerBoxPadding());
    gtk_scale_set_digits(GTK_SCALE(padding), 0);
    gtk_widget_set_size_request(padding, 140, -1);
    g_signal_connect(
            padding,
            "value-changed",
            G_CALLBACK(+[](GtkRange* range, gpointer data) {
                static_cast<Control*>(data)->getToolHandler()->setAnswerBoxPadding(gtk_range_get_value(range));
            }),
            control);
    appendRow(_("Padding"), padding);

    GtkWidget* radius = gtk_scale_new_with_range(GTK_ORIENTATION_HORIZONTAL, 0.0, 24.0, 1.0);
    gtk_range_set_value(GTK_RANGE(radius), tools->getAnswerBoxCornerRadius());
    gtk_scale_set_digits(GTK_SCALE(radius), 0);
    gtk_widget_set_size_request(radius, 140, -1);
    g_signal_connect(
            radius,
            "value-changed",
            G_CALLBACK(+[](GtkRange* range, gpointer data) {
                static_cast<Control*>(data)->getToolHandler()->setAnswerBoxCornerRadius(gtk_range_get_value(range));
            }),
            control);
    appendRow(_("Corner radius"), radius);

    GtkMenuButton* menuButton = GTK_MENU_BUTTON(gtk_menu_button_new());
    gtk_widget_set_can_focus(GTK_WIDGET(menuButton), false);
    gtk_widget_set_tooltip_text(GTK_WIDGET(menuButton), _("Answer Box Style"));
    gtk_menu_button_set_popover(menuButton, GTK_WIDGET(popover));
    gtk_menu_button_set_direction(menuButton, horizontal ? GTK_ARROW_DOWN : GTK_ARROW_RIGHT);

    GtkBox* box = GTK_BOX(gtk_box_new(horizontal ? GTK_ORIENTATION_HORIZONTAL : GTK_ORIENTATION_VERTICAL, 0));
    gtk_box_append(box, toggle);
    gtk_box_append(box, GTK_WIDGET(menuButton));

    gtk_widget_show_all(GTK_WIDGET(panel));
    return xoj::util::WidgetSPtr(GTK_WIDGET(box), xoj::util::adopt);
}

auto ToolAnswerBox::getToolDisplayName() const -> std::string {
    return _("Answer Box");
}

auto ToolAnswerBox::getNewToolIcon() const -> GtkWidget* {
    return gtk_image_new_from_icon_name(iconName.c_str(), GTK_ICON_SIZE_LARGE_TOOLBAR);
}
