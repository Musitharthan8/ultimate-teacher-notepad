/*
 * Ultimate Teacher Notepad
 * Classroom material and marking workflows, based on Xournal++ GPLv2+.
 */
#include "ToolClassroomWorkflow.h"

#include <utility>

#include "AnswerBoxStyles.h"  // for MARKING_COMMENT_STYLE
#include "control/Control.h"
#include "control/ToolHandler.h"
#include "enums/Action.enum.h"
#include "gui/MainWindow.h"
#include "util/gtk4_helper.h"
#include "util/i18n.h"

namespace {
enum class Command { SCAN, PASTE_IMAGE, IMAGE, COMMENT, MARKING, TEACHING };

GtkBox* makePanel(GtkNotebook* tabs, const char* label) {
    auto* box = GTK_BOX(gtk_box_new(GTK_ORIENTATION_VERTICAL, 5));
    gtk_widget_set_margin_start(GTK_WIDGET(box), 10);
    gtk_widget_set_margin_end(GTK_WIDGET(box), 10);
    gtk_widget_set_margin_top(GTK_WIDGET(box), 10);
    gtk_widget_set_margin_bottom(GTK_WIDGET(box), 10);
    gtk_notebook_append_page(tabs, GTK_WIDGET(box), gtk_label_new(label));
    return box;
}

void addHint(GtkBox* panel, const char* text) {
    auto* label = gtk_label_new(text);
    gtk_label_set_line_wrap(GTK_LABEL(label), true);
    gtk_label_set_max_width_chars(GTK_LABEL(label), 42);
    gtk_widget_set_halign(label, GTK_ALIGN_START);
    gtk_widget_add_css_class(label, "utn-context-hint");
    gtk_box_append(panel, label);
}

// Menu entries look like the other UTN menus: flat, left-aligned text
GtkWidget* newMenuEntry(const char* label) {
    auto* button = gtk_button_new_with_label(label);
    gtk_button_set_relief(GTK_BUTTON(button), GTK_RELIEF_NONE);
    gtk_label_set_xalign(GTK_LABEL(gtk_bin_get_child(GTK_BIN(button))), 0.0F);
    return button;
}

void addAction(GtkBox* panel, GtkPopover* popover, const char* label, const char* tooltip, Action action) {
    auto* button = newMenuEntry(label);
    gtk_widget_set_tooltip_text(button, tooltip);
    gtk_widget_set_can_focus(button, false);
    const std::string name = std::string("win.") + Action_toString(action);
    gtk_actionable_set_action_name(GTK_ACTIONABLE(button), name.c_str());
    g_signal_connect_object(button, "clicked", G_CALLBACK(+[](GtkButton*, gpointer p) {
                                gtk_popover_popdown(GTK_POPOVER(p));
                            }),
                            popover, GConnectFlags(0));
    gtk_box_append(panel, button);
}

void addCommand(GtkBox* panel, GtkPopover* popover, Control* control, const char* label, const char* tooltip,
                Command command) {
    auto* button = newMenuEntry(label);
    gtk_widget_set_tooltip_text(button, tooltip);
    gtk_widget_set_can_focus(button, false);
    g_object_set_data(G_OBJECT(button), "utn-control", control);
    g_object_set_data(G_OBJECT(button), "utn-command", GINT_TO_POINTER(static_cast<int>(command)));
    g_signal_connect_object(
            button, "clicked", G_CALLBACK(+[](GtkButton* button, gpointer p) {
                auto* ctrl = static_cast<Control*>(g_object_get_data(G_OBJECT(button), "utn-control"));
                auto command = static_cast<Command>(GPOINTER_TO_INT(g_object_get_data(G_OBJECT(button), "utn-command")));
                // Changing workspace may destroy this popover.
                gtk_popover_popdown(GTK_POPOVER(p));
                switch (command) {
                    case Command::SCAN:
                        ctrl->askInsertScanPage();
                        break;
                    case Command::PASTE_IMAGE:
                        ctrl->pasteImage();
                        break;
                    case Command::IMAGE:
                        ctrl->clearSelectionEndText();
                        ctrl->selectTool(TOOL_IMAGE);
                        break;
                    case Command::COMMENT: {
                        ctrl->clearSelectionEndText();
                        utn::applyAnswerBoxStyle(*ctrl->getToolHandler(), utn::MARKING_COMMENT_STYLE);
                        ctrl->selectTextMode(TextMode::AnswerBox);
                        break;
                    }
                    case Command::MARKING:
                    case Command::TEACHING:
                        ctrl->clearSelectionEndText();
                        if (auto* window = ctrl->getWindow()) {
                            window->toolbarSelected(command == Command::MARKING ? "UTN Marking" : "UTN Teacher");
                        }
                        break;
                }
            }),
            popover, GConnectFlags(0));
    gtk_box_append(panel, button);
}
}  // namespace

ToolClassroomWorkflow::ToolClassroomWorkflow(std::string id, Control* control, IconNameHelper icons):
        AbstractToolItem(std::move(id), Category::MISC), control(control), iconName(icons.iconName("utn-workflow")) {}

auto ToolClassroomWorkflow::createItem(bool horizontal) -> xoj::util::WidgetSPtr {
    auto* popover = GTK_POPOVER(gtk_popover_new());
    gtk_widget_add_css_class(GTK_WIDGET(popover), "toolbar");
    auto* tabs = GTK_NOTEBOOK(gtk_notebook_new());
    gtk_popover_set_child(popover, GTK_WIDGET(tabs));

    auto* materials = makePanel(tabs, _("Materials"));
    addCommand(materials, popover, control, _("Add scan or photo as a page"),
               _("Insert a photo of a worksheet after this page. Keep the image file with your notebook."), Command::SCAN);
    addCommand(materials, popover, control, _("Paste picture"), _("Paste the copied picture onto this page"),
               Command::PASTE_IMAGE);
    addCommand(materials, popover, control, _("Insert picture from file"),
               _("Choose a picture, then click the page to place it"), Command::IMAGE);
    addAction(materials, popover, _("Open a PDF to annotate"),
              _("Open a worksheet PDF. You will be asked to save changes to the current notebook first."),
              Action::ANNOTATE_PDF);
    addHint(materials, _("Keep notebooks and their picture files in the same folder. Text in scans cannot be selected."));

    auto* pages = makePanel(tabs, _("Pages"));
    addAction(pages, popover, _("Insert blank page"), _("Add an empty page after this one"),
              Action::NEW_PAGE_AFTER);
    addAction(pages, popover, _("Duplicate page"), _("Copy this page with its annotations. Undo removes the copy."),
              Action::DUPLICATE_PAGE);
    addAction(pages, popover, _("Move page up"), _("Move this page one place earlier"),
              Action::MOVE_PAGE_TOWARDS_BEGINNING);
    addAction(pages, popover, _("Move page down"), _("Move this page one place later"),
              Action::MOVE_PAGE_TOWARDS_END);
    addAction(pages, popover, _("Delete page"), _("Delete this page. Undo restores it."), Action::DELETE_PAGE);
    addAction(pages, popover, _("Save pages as PDF…"),
              _("Choose which pages to save as a separate PDF"), Action::EXPORT_AS);
    addHint(pages,
              _("Use Save pages as PDF to split a worksheet into parts."));

    auto* marking = makePanel(tabs, _("Marking"));
    addCommand(marking, popover, control, _("Switch to marking layout"),
              _("Feedback first, for marking student work. Your document is not changed."), Command::MARKING);
    addCommand(marking, popover, control, _("Switch to teaching layout"), _("The everyday layout for lessons"),
              Command::TEACHING);
    addCommand(marking, popover, control, _("Write a marking comment"),
              _("Click the page to add a red comment box you can edit"), Command::COMMENT);
    addAction(marking, popover, _("Save a marked copy…"),
              _("Save your marking as a new notebook, keeping the original"), Action::SAVE_AS);
    addAction(marking, popover, _("Export marked pages as PDF…"), _("Save the pages with your marking as a PDF for students"),
              Action::EXPORT_AS);
    addHint(marking,
              _("Use Feedback for ticks and ready-made comments."));

    auto* menu = GTK_MENU_BUTTON(gtk_menu_button_new());
    gtk_widget_set_can_focus(GTK_WIDGET(menu), false);
    gtk_widget_set_tooltip_text(GTK_WIDGET(menu), getToolDisplayName().c_str());
    auto* heading = GTK_BOX(gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 5));
    gtk_box_append(heading, getNewToolIcon());
    gtk_box_append(heading, gtk_label_new(_("Classroom")));
    gtk_widget_show_all(GTK_WIDGET(heading));
    gtk_button_set_child(GTK_BUTTON(menu), GTK_WIDGET(heading));
    gtk_menu_button_set_popover(menu, GTK_WIDGET(popover));
    gtk_menu_button_set_direction(menu, horizontal ? GTK_ARROW_DOWN : GTK_ARROW_RIGHT);
    gtk_widget_show_all(GTK_WIDGET(tabs));
    return xoj::util::WidgetSPtr(GTK_WIDGET(menu), xoj::util::adopt);
}

auto ToolClassroomWorkflow::getToolDisplayName() const -> std::string { return _("Classroom"); }
auto ToolClassroomWorkflow::getNewToolIcon() const -> GtkWidget* {
    return gtk_image_new_from_icon_name(iconName.c_str(), GTK_ICON_SIZE_LARGE_TOOLBAR);
}
