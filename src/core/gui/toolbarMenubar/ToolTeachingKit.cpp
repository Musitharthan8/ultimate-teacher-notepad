/*
 * Ultimate Teacher Notepad
 *
 * Consolidated classroom drawing and STEM tools
 *
 * Based on Xournal++ GPLv2+
 */

#include "ToolTeachingKit.h"

#include <array>
#include <utility>

#include "control/Control.h"
#include "control/ToolHandler.h"
#include "control/actions/ActionDatabase.h"
#include "enums/Action.enum.h"
#include "util/gtk4_helper.h"
#include "util/i18n.h"

namespace {
struct DrawingEntry {
    const char* label;
    DrawingType type;
    const char* icon;
};

constexpr std::array<DrawingEntry, 7> DRAWING_TOOLS{{
        {N_("Line"), DRAWING_TYPE_LINE, "draw-line"},
        {N_("Rectangle"), DRAWING_TYPE_RECTANGLE, "draw-rect"},
        {N_("Ellipse"), DRAWING_TYPE_ELLIPSE, "draw-ellipse"},
        {N_("Arrow"), DRAWING_TYPE_ARROW, "draw-arrow"},
        {N_("Double Arrow"), DRAWING_TYPE_DOUBLE_ARROW, "draw-double-arrow"},
        {N_("Axes"), DRAWING_TYPE_COORDINATE_SYSTEM, "draw-coordinate-system"},
        {N_("Smart Shape"), DRAWING_TYPE_SHAPE_RECOGNIZER, "shape-recognizer"},
}};

// Shapes are a Pen mode: select Pen (which starts freehand) and then apply the requested shape.
// Control::setToolDrawingType refreshes the toolbar and properties bar.
void chooseDrawingTool(Control* control, DrawingType type) {
    control->selectTool(TOOL_PEN);
    control->setToolDrawingType(type);
}
}  // namespace

ToolTeachingKit::ToolTeachingKit(std::string id, Control* control, IconNameHelper iconNameHelper):
        AbstractToolItem(std::move(id), Category::TOOLS),
        control(control),
        icons(iconNameHelper),
        iconName(iconNameHelper.iconName("utn-teaching-tools")) {}

auto ToolTeachingKit::createItem(bool horizontal) -> xoj::util::WidgetSPtr {
    GtkPopover* popover = GTK_POPOVER(gtk_popover_new());
    gtk_widget_add_css_class(GTK_WIDGET(popover), "toolbar");

    GtkBox* panel = GTK_BOX(gtk_box_new(GTK_ORIENTATION_VERTICAL, 5));
    gtk_widget_set_margin_start(GTK_WIDGET(panel), 8);
    gtk_widget_set_margin_end(GTK_WIDGET(panel), 8);
    gtk_widget_set_margin_top(GTK_WIDGET(panel), 8);
    gtk_widget_set_margin_bottom(GTK_WIDGET(panel), 8);
    gtk_popover_set_child(popover, GTK_WIDGET(panel));

    GtkWidget* shapesTitle = gtk_label_new(_("Shapes & Lines"));
    gtk_widget_set_halign(shapesTitle, GTK_ALIGN_START);
    gtk_widget_add_css_class(shapesTitle, "utn-popover-title");
    gtk_box_append(panel, shapesTitle);

    GtkGrid* shapeGrid = GTK_GRID(gtk_grid_new());
    gtk_grid_set_row_spacing(shapeGrid, 4);
    gtk_grid_set_column_spacing(shapeGrid, 4);

    int column = 0;
    int row = 0;
    for (const auto& entry: DRAWING_TOOLS) {
        GtkWidget* button = gtk_button_new();
        auto* content = GTK_BOX(gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8));
        gtk_box_append(content, gtk_image_new_from_icon_name(icons.iconName(entry.icon).c_str(), GTK_ICON_SIZE_LARGE_TOOLBAR));
        gtk_box_append(content, gtk_label_new(_(entry.label)));
        gtk_button_set_child(GTK_BUTTON(button), GTK_WIDGET(content));
        gtk_widget_set_tooltip_text(button, _(entry.label));
        g_object_set_data(G_OBJECT(button), "utn-control", control);
        g_object_set_data(G_OBJECT(button), "utn-drawing-type", GINT_TO_POINTER(static_cast<int>(entry.type)));

        g_signal_connect(
                button,
                "clicked",
                G_CALLBACK(+[](GtkButton* button, gpointer data) {
                    auto* ctrl = static_cast<Control*>(g_object_get_data(G_OBJECT(button), "utn-control"));
                    auto type = static_cast<DrawingType>(
                            GPOINTER_TO_INT(g_object_get_data(G_OBJECT(button), "utn-drawing-type")));
                    chooseDrawingTool(ctrl, type);
                    gtk_popover_popdown(GTK_POPOVER(data));
                }),
                popover);

        gtk_grid_attach(shapeGrid, button, column, row, 1, 1);
        if (++column == 2) {
            column = 0;
            ++row;
        }
    }

    gtk_box_append(panel, GTK_WIDGET(shapeGrid));
    gtk_box_append(panel, gtk_separator_new(GTK_ORIENTATION_HORIZONTAL));

    GtkWidget* stemTitle = gtk_label_new(_("More Tools"));
    gtk_widget_set_halign(stemTitle, GTK_ALIGN_START);
    gtk_widget_add_css_class(stemTitle, "utn-popover-title");
    gtk_box_append(panel, stemTitle);

    GtkWidget* setSquare = gtk_button_new_with_label(_("Set Square"));
    g_signal_connect(
            setSquare,
            "clicked",
            G_CALLBACK(+[](GtkButton*, gpointer data) {
                auto* ctrl = static_cast<Control*>(data);
                ctrl->getActionDatabase()->fireChangeActionState(Action::SETSQUARE, true);
            }),
            control);
    gtk_box_append(panel, setSquare);

    GtkWidget* compass = gtk_button_new_with_label(_("Compass"));
    g_signal_connect(
            compass,
            "clicked",
            G_CALLBACK(+[](GtkButton*, gpointer data) {
                auto* ctrl = static_cast<Control*>(data);
                ctrl->getActionDatabase()->fireChangeActionState(Action::COMPASS, true);
            }),
            control);
    gtk_box_append(panel, compass);

    GtkWidget* equation = gtk_button_new_with_label(_("Equation / TeX"));
    g_signal_connect(
            equation,
            "clicked",
            G_CALLBACK(+[](GtkButton*, gpointer data) {
                static_cast<Control*>(data)->selectTool(TOOL_LATEX);
            }),
            control);
    gtk_box_append(panel, equation);

    GtkWidget* image = gtk_button_new_with_label(_("Image"));
    g_signal_connect(
            image,
            "clicked",
            G_CALLBACK(+[](GtkButton*, gpointer data) {
                static_cast<Control*>(data)->selectTool(TOOL_IMAGE);
            }),
            control);
    gtk_box_append(panel, image);

    GtkMenuButton* menuButton = GTK_MENU_BUTTON(gtk_menu_button_new());
    gtk_widget_set_can_focus(GTK_WIDGET(menuButton), false);
    gtk_widget_set_tooltip_text(GTK_WIDGET(menuButton), getToolDisplayName().c_str());
    gtk_button_set_child(GTK_BUTTON(menuButton), getNewToolIcon());
    gtk_menu_button_set_popover(menuButton, GTK_WIDGET(popover));
    gtk_menu_button_set_direction(menuButton, horizontal ? GTK_ARROW_DOWN : GTK_ARROW_RIGHT);

    gtk_widget_show_all(GTK_WIDGET(panel));
    return xoj::util::WidgetSPtr(GTK_WIDGET(menuButton), xoj::util::adopt);
}

auto ToolTeachingKit::getToolDisplayName() const -> std::string {
    return _("Shapes & Lines");
}

auto ToolTeachingKit::getNewToolIcon() const -> GtkWidget* {
    return gtk_image_new_from_icon_name(iconName.c_str(), GTK_ICON_SIZE_LARGE_TOOLBAR);
}
