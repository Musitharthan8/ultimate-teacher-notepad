/*
 * Ultimate Teacher Notepad
 * Classroom material and marking workflows, based on Xournal++ GPLv2+.
 */
#include "ToolClassroomWorkflow.h"

#include <utility>

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

void addAction(GtkBox* panel, GtkPopover* popover, const char* label, const char* tooltip, Action action) {
    auto* button = gtk_button_new_with_label(label);
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
    auto* button = gtk_button_new_with_label(label);
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
                        auto* tools = ctrl->getToolHandler();
                        tools->clearTeacherStamp();
                        tools->setAnswerBoxEnabled(true);
                        tools->setAnswerBoxTextColor(Color(160U, 35U, 35U));
                        tools->setAnswerBoxBackgroundColor(Color(255U, 248U, 214U, 230U));
                        tools->setAnswerBoxBorderColor(Color(160U, 35U, 35U));
                        tools->setAnswerBoxBorderWidth(1);
                        tools->setAnswerBoxPadding(6);
                        tools->setAnswerBoxCornerRadius(5);
                        tools->selectTool(TOOL_TEXT);
                        tools->fireToolChanged();
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
    addCommand(materials, popover, control, _("Add Scan / Photo Page"),
               _("Insert a worksheet image after this page; a copy is saved alongside the journal"), Command::SCAN);
    addCommand(materials, popover, control, _("Paste Picture"), _("Paste only the clipboard image onto this page"),
               Command::PASTE_IMAGE);
    addCommand(materials, popover, control, _("Place Image from File"),
               _("Choose an image and place it on the worksheet"), Command::IMAGE);
    addAction(materials, popover, _("Open PDF for Annotation"),
              _("Open a PDF using the existing save-before-open workflow"),
              Action::ANNOTATE_PDF);
    addHint(materials, _("Keep the journal and its attached image files together. Scans need OCR for text selection."));

    auto* pages = makePanel(tabs, _("Pages"));
    addAction(pages, popover, _("Insert Working Page"), _("Insert a page after the current worksheet"),
              Action::NEW_PAGE_AFTER);
    addAction(pages, popover, _("Duplicate Page"), _("Duplicate the page and its annotations; Undo available"),
              Action::DUPLICATE_PAGE);
    addAction(pages, popover, _("Move Page Earlier"), _("Move this page one position towards the beginning"),
              Action::MOVE_PAGE_TOWARDS_BEGINNING);
    addAction(pages, popover, _("Move Page Later"), _("Move this page one position towards the end"),
              Action::MOVE_PAGE_TOWARDS_END);
    addAction(pages, popover, _("Delete Page"), _("Delete the current page; Undo available"), Action::DELETE_PAGE);
    addAction(pages, popover, _("Export Page Range…"),
              _("Choose PDF format and a page range to extract pages into a new file"), Action::EXPORT_AS);
    addHint(pages,
              _("Export page ranges to extract or split a worksheet. Combining different PDF sources is not yet supported."));

    auto* marking = makePanel(tabs, _("Marking"));
    addCommand(marking, popover, control, _("Marking Workspace"),
              _("Switch to the marking layout without changing the document"), Command::MARKING);
    addCommand(marking, popover, control, _("Teaching Workspace"), _("Return to the teacher layout"),
              Command::TEACHING);
    addCommand(marking, popover, control, _("Place Written Comment"),
              _("Place an editable comment box using the Text tool"), Command::COMMENT);
    addAction(marking, popover, _("Save Marked Copy…"),
              _("Save the annotated native document under a new filename"), Action::SAVE_AS);
    addAction(marking, popover, _("Export Marked Pages…"), _("Export the chosen pages and visible annotations"),
              Action::EXPORT_AS);
    addHint(marking,
              _("Use Feedback for marking symbols and reusable comments. Save a separate copy to keep the original submission."));

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

auto ToolClassroomWorkflow::getToolDisplayName() const -> std::string { return _("Classroom Workflow"); }
auto ToolClassroomWorkflow::getNewToolIcon() const -> GtkWidget* {
    return gtk_image_new_from_icon_name(iconName.c_str(), GTK_ICON_SIZE_LARGE_TOOLBAR);
}
