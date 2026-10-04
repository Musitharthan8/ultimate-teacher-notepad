/*
 * Ultimate Teacher Notepad
 *
 * Teacher feedback bank
 *
 * Based on Xournal++ GPLv2+
 */

#include "ToolTeacherStamp.h"

#include <array>
#include <utility>

#include "control/Control.h"
#include "control/ToolHandler.h"
#include "util/Color.h"
#include "util/Util.h"
#include "util/gtk4_helper.h"
#include "util/i18n.h"

namespace {
struct StampEntry {
    const char* label;
    const char* text;
};

constexpr std::array<StampEntry, 5> QUICK_STAMPS{{
        {"✓", "✓"},
        {"✗", "✗"},
        {"?", "?"},
        {"Good", "Good"},
        {"Great", "Great"},
}};

constexpr std::array<StampEntry, 6> WRITING_STAMPS{{
        {"PEEL", "PEEL"},
        {"MEET", "MEET"},
        {"Evidence?", "Evidence?"},
        {"Explain further", "Explain further"},
        {"Develop example", "Develop your example"},
        {"Link back", "Link back to the question"},
}};

constexpr std::array<StampEntry, 5> LANGUAGE_STAMPS{{
        {"Tense", "Check tense"},
        {"SVA", "Check subject-verb agreement"},
        {"Spelling", "Check spelling"},
        {"Punctuation", "Check punctuation"},
        {"Word choice", "Improve word choice"},
}};

constexpr std::array<StampEntry, 5> COMPREHENSION_STAMPS{{
        {"Answer question", "Answer the question"},
        {"Use evidence", "Use evidence from the text"},
        {"Inference?", "What can you infer?"},
        {"Explain why", "Explain why"},
        {"Good inference", "Good inference"},
}};

template <size_t N>
GtkWidget* createStampGrid(Control* control, GtkPopover* popover, const std::array<StampEntry, N>& stamps) {
    GtkGrid* grid = GTK_GRID(gtk_grid_new());
    gtk_grid_set_row_spacing(grid, 4);
    gtk_grid_set_column_spacing(grid, 4);
    gtk_widget_set_margin_start(GTK_WIDGET(grid), 6);
    gtk_widget_set_margin_end(GTK_WIDGET(grid), 6);
    gtk_widget_set_margin_top(GTK_WIDGET(grid), 6);
    gtk_widget_set_margin_bottom(GTK_WIDGET(grid), 6);

    int column = 0;
    int row = 0;

    for (const auto& stamp: stamps) {
        GtkWidget* button = gtk_button_new_with_label(stamp.label);
        gtk_widget_set_can_focus(button, false);
        gtk_widget_set_hexpand(button, true);

        g_object_set_data(G_OBJECT(button), "utn-control", control);
        g_object_set_data_full(G_OBJECT(button), "utn-stamp-text", g_strdup(stamp.text), g_free);

        g_signal_connect(
                button,
                "clicked",
                G_CALLBACK(+[](GtkButton* button, gpointer data) {
                    auto* ctrl = static_cast<Control*>(g_object_get_data(G_OBJECT(button), "utn-control"));
                    auto* stamp = static_cast<const char*>(g_object_get_data(G_OBJECT(button), "utn-stamp-text"));

                    auto* tools = ctrl->getToolHandler();
                    tools->setAnswerBoxEnabled(false);
                    tools->setTeacherStampText(stamp ? stamp : "");
                    tools->selectTool(TOOL_TEXT);
                    tools->fireToolChanged();

                    gtk_popover_popdown(GTK_POPOVER(data));
                }),
                popover);

        gtk_grid_attach(grid, button, column, row, 1, 1);

        if (++column == 2) {
            column = 0;
            ++row;
        }
    }

    return GTK_WIDGET(grid);
}
}  // namespace

ToolTeacherStamp::ToolTeacherStamp(std::string id, Control* control, IconNameHelper iconNameHelper):
        AbstractToolItem(std::move(id), Category::TOOLS),
        control(control),
        iconName(iconNameHelper.iconName("utn-feedback")) {}

auto ToolTeacherStamp::createItem(bool horizontal) -> xoj::util::WidgetSPtr {
    GtkPopover* popover = GTK_POPOVER(gtk_popover_new());
    gtk_widget_add_css_class(GTK_WIDGET(popover), "toolbar");

    GtkBox* panel = GTK_BOX(gtk_box_new(GTK_ORIENTATION_VERTICAL, 6));
    gtk_widget_set_margin_start(GTK_WIDGET(panel), 8);
    gtk_widget_set_margin_end(GTK_WIDGET(panel), 8);
    gtk_widget_set_margin_top(GTK_WIDGET(panel), 8);
    gtk_widget_set_margin_bottom(GTK_WIDGET(panel), 8);
    gtk_popover_set_child(popover, GTK_WIDGET(panel));

    GtkBox* colourRow = GTK_BOX(gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8));
    GtkWidget* colourLabel = gtk_label_new(_("Feedback colour"));
    gtk_widget_set_hexpand(colourLabel, true);
    gtk_widget_set_halign(colourLabel, GTK_ALIGN_START);

    GdkRGBA stampColour = Util::argb_to_GdkRGBA(control->getToolHandler()->getTeacherStampColor());
    GtkWidget* colourButton = gtk_color_button_new_with_rgba(&stampColour);
    gtk_color_chooser_set_use_alpha(GTK_COLOR_CHOOSER(colourButton), false);
    g_signal_connect(
            colourButton,
            "color-set",
            G_CALLBACK(+[](GtkColorButton* button, gpointer data) {
                auto* ctrl = static_cast<Control*>(data);
                GdkRGBA colour{};
                gtk_color_chooser_get_rgba(GTK_COLOR_CHOOSER(button), &colour);
                ctrl->getToolHandler()->setTeacherStampColor(Util::GdkRGBA_to_argb(colour));
            }),
            control);

    gtk_box_append(colourRow, colourLabel);
    gtk_box_append(colourRow, colourButton);
    gtk_box_append(panel, GTK_WIDGET(colourRow));

    GtkNotebook* notebook = GTK_NOTEBOOK(gtk_notebook_new());
    gtk_widget_set_size_request(GTK_WIDGET(notebook), 360, 210);

    gtk_notebook_append_page(notebook, createStampGrid(control, popover, QUICK_STAMPS), gtk_label_new(_("Quick")));
    gtk_notebook_append_page(notebook, createStampGrid(control, popover, WRITING_STAMPS), gtk_label_new(_("Writing")));
    gtk_notebook_append_page(notebook, createStampGrid(control, popover, LANGUAGE_STAMPS), gtk_label_new(_("Language")));
    gtk_notebook_append_page(notebook, createStampGrid(control, popover, COMPREHENSION_STAMPS),
                             gtk_label_new(_("Comprehension")));

    GtkBox* custom = GTK_BOX(gtk_box_new(GTK_ORIENTATION_VERTICAL, 6));
    gtk_widget_set_margin_start(GTK_WIDGET(custom), 8);
    gtk_widget_set_margin_end(GTK_WIDGET(custom), 8);
    gtk_widget_set_margin_top(GTK_WIDGET(custom), 8);
    gtk_widget_set_margin_bottom(GTK_WIDGET(custom), 8);

    GtkWidget* customLabel = gtk_label_new(_("Type any feedback, then place it with one tap."));
    gtk_widget_set_halign(customLabel, GTK_ALIGN_START);
    gtk_box_append(custom, customLabel);

    GtkWidget* customEntry = gtk_entry_new();
    gtk_entry_set_placeholder_text(GTK_ENTRY(customEntry), _("e.g. Great inference"));
    gtk_box_append(custom, customEntry);

    GtkWidget* customButton = gtk_button_new_with_label(_("Use Custom Feedback"));
    g_object_set_data(G_OBJECT(customButton), "utn-control", control);
    g_object_set_data(G_OBJECT(customButton), "utn-entry", customEntry);

    g_signal_connect(
            customButton,
            "clicked",
            G_CALLBACK(+[](GtkButton* button, gpointer data) {
                auto* ctrl = static_cast<Control*>(g_object_get_data(G_OBJECT(button), "utn-control"));
                auto* entry = GTK_EDITABLE(g_object_get_data(G_OBJECT(button), "utn-entry"));
                const char* text = gtk_editable_get_text(entry);

                if (text != nullptr && text[0] != '\0') {
                    auto* tools = ctrl->getToolHandler();
                    tools->setAnswerBoxEnabled(false);
                    tools->setTeacherStampText(text);
                    tools->selectTool(TOOL_TEXT);
                    tools->fireToolChanged();
                    gtk_popover_popdown(GTK_POPOVER(data));
                }
            }),
            popover);

    gtk_box_append(custom, customButton);
    gtk_notebook_append_page(notebook, GTK_WIDGET(custom), gtk_label_new(_("Custom")));

    gtk_box_append(panel, GTK_WIDGET(notebook));

    GtkMenuButton* menuButton = GTK_MENU_BUTTON(gtk_menu_button_new());
    gtk_widget_set_can_focus(GTK_WIDGET(menuButton), false);
    gtk_widget_set_tooltip_text(GTK_WIDGET(menuButton), getToolDisplayName().c_str());
    gtk_button_set_child(GTK_BUTTON(menuButton), getNewToolIcon());
    gtk_menu_button_set_popover(menuButton, GTK_WIDGET(popover));
    gtk_menu_button_set_direction(menuButton, horizontal ? GTK_ARROW_DOWN : GTK_ARROW_RIGHT);

    gtk_widget_show_all(GTK_WIDGET(panel));
    return xoj::util::WidgetSPtr(GTK_WIDGET(menuButton), xoj::util::adopt);
}

auto ToolTeacherStamp::getToolDisplayName() const -> std::string {
    return _("Feedback Bank");
}

auto ToolTeacherStamp::getNewToolIcon() const -> GtkWidget* {
    return gtk_image_new_from_icon_name(iconName.c_str(), GTK_ICON_SIZE_LARGE_TOOLBAR);
}
