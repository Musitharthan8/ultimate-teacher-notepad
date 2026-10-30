/*
 * Ultimate Teacher Notepad
 *
 * Classroom presentation toolkit
 *
 * Based on Xournal++ GPLv2+
 */

#include "ToolPresentationKit.h"

#include <memory>
#include <utility>

#include "control/Control.h"
#include "control/ToolHandler.h"
#include "control/actions/ActionDatabase.h"
#include "enums/Action.enum.h"
#include "gui/MainWindow.h"
#include "gui/PageView.h"
#include "gui/XournalView.h"
#include "model/ClassroomMode.h"
#include "util/gtk4_helper.h"
#include "util/raii/GVariantSPtr.h"
#include "util/i18n.h"

#include "ToolStudentView.h"
#include "UtnWidgets.h"

namespace {
void repaintPresentationOverlay(Control* control) {
    if (auto* window = control->getWindow()) {
        if (auto* xournal = window->getXournal()) {
            if (auto* pageView = xournal->getViewFor(control->getCurrentPageNo())) {
                pageView->repaintPage();
            }
        }
    }
}

// One exit path for the menu, the banner button and Escape
void exitPresentationOverlay(Control* control) {
    if (auto* window = control->getWindow()) {
        window->exitPresentationOverlay();
        return;
    }
    control->getToolHandler()->clearPresentationOverlay();
    control->getToolHandler()->fireToolChanged();
}

// Every tool switch goes through Control so text edits, selections and special modes are finished properly
void selectPresentationTool(Control* control, ToolType tool, bool spotlight = false, bool curtain = false) {
    auto* tools = control->getToolHandler();
    // A teacher tapping the active temporary screen effect again expects it to close.
    if ((spotlight && tools->isSpotlightEnabled()) || (curtain && tools->isCurtainEnabled())) {
        exitPresentationOverlay(control);
        return;
    }
    tools->clearPresentationOverlay();
    control->selectTool(tool);
    if (spotlight) {
        tools->setSpotlightEnabled(true);
    }
    if (curtain) {
        tools->setCurtainEnabled(true);
    }
    tools->fireToolChanged();
    repaintPresentationOverlay(control);
}

void toggleBoolAction(Control* control, Action action) {
    auto gaction = control->getActionDatabase()->getAction(action);
    xoj::util::GVariantSPtr state(g_action_get_state(G_ACTION(gaction.get())), xoj::util::adopt);
    const bool enabled = state && g_variant_get_boolean(state.get());
    control->getActionDatabase()->fireChangeActionState(action, !enabled);
}

/// The entries of the Present menu whose wording and availability follow the active overlay
struct PresentState {
    GtkWidget* button = nullptr;  ///< the app-bar button
    GtkWidget* spotlight = nullptr;
    GtkWidget* curtain = nullptr;
    GtkWidget* exit = nullptr;
    GtkWidget* status = nullptr;  ///< says in words what is on
    GtkWidget* back = nullptr;    ///< "Back to normal": overlays, Freeze and Blank together
    GtkWidget* freeze = nullptr;
    GtkWidget* blank = nullptr;
    GtkWidget* studentWindow = nullptr;
};

/// Return everything to the ordinary document view. Saved work is never touched.
void backToNormal(Control* control, ToolStudentView* students) {
    exitPresentationOverlay(control);
    if (students) {
        students->restoreNormal();
    }
}

/// Say which overlay is on in words (never colour alone), and offer the way back only while there is one
void refreshPresentState(const PresentState& ui, Control* control, ToolStudentView* students) {
    auto* tools = control->getToolHandler();
    const bool spotlight = tools->isSpotlightEnabled();
    const bool curtain = tools->isCurtainEnabled();
    const bool frozen = students && students->isFrozen();
    const bool blanked = students && students->isBlanked();
    const auto summary = utn::summariseClassroom({spotlight, curtain, frozen, blanked});
    if (ui.status) {
        gtk_label_set_text(GTK_LABEL(ui.status), summary.label.c_str());
    }
    if (ui.back) {
        gtk_widget_set_sensitive(ui.back, summary.anyActive());
    }
    if (students) {
        if (ui.freeze) {
            utn::setMenuButtonLabel(ui.freeze, frozen ? _("Resume live view") : _("Freeze students' screen"));
            gtk_widget_set_sensitive(ui.freeze, students->isWindowOpen());
        }
        if (ui.blank) {
            utn::setMenuButtonLabel(ui.blank, blanked ? _("Show the page to students again") : _("Blank students' screen"));
        }
        if (ui.studentWindow) {
            utn::setMenuButtonLabel(ui.studentWindow, students->isWindowOpen() ? _("Close Student View") :
                                                                                   _("Open Student View on the projector"));
        }
    }
    if (ui.spotlight) {
        utn::setMenuButtonLabel(ui.spotlight, spotlight ? _("Spotlight is on: choose to turn off") : _("Spotlight"));
    }
    if (ui.curtain) {
        utn::setMenuButtonLabel(ui.curtain, curtain ? _("Curtain is on: choose to turn off") : _("Curtain"));
    }
    if (ui.exit) {
        gtk_widget_set_sensitive(ui.exit, spotlight || curtain);
    }
    if (ui.button) {
        auto* context = gtk_widget_get_style_context(ui.button);
        if (summary.anyActive()) {
            gtk_style_context_add_class(context, "utn-present-active");
        } else {
            gtk_style_context_remove_class(context, "utn-present-active");
        }
        // The tooltip says what is on in words, so the app-bar button never relies on its colour alone
        gtk_widget_set_tooltip_text(ui.button, summary.anyActive() ?
                                                       (summary.label + _(". Open to turn it off.")).c_str() :
                                                       _("Classroom controls: Spotlight, Curtain, Freeze, Blank and "
                                                         "Student View, and temporary ink"));
    }
}

void clearTemporaryInk(Control* control) {
    if (auto* window = control->getWindow()) {
        if (auto* xournal = window->getXournal()) {
            for (const auto& pageView: xournal->getViewPages()) {
                pageView->clearTemporaryPresentationInk();
            }
        }
    }
}
}  // namespace

void ToolPresentationKit::runClassroomAction(Control* control, Action action) {
    switch (action) {
        case Action::CLASSROOM_SPOTLIGHT:
            selectPresentationTool(control, TOOL_HAND, true, false);
            break;
        case Action::CLASSROOM_CURTAIN:
            selectPresentationTool(control, TOOL_HAND, false, true);
            break;
        case Action::CLASSROOM_EXIT_OVERLAY:
            exitPresentationOverlay(control);
            break;
        case Action::CLASSROOM_TEMPORARY_INK:
            selectPresentationTool(control, TOOL_LASER_POINTER_PEN);
            break;
        case Action::CLASSROOM_TEMPORARY_HIGHLIGHT:
            selectPresentationTool(control, TOOL_LASER_POINTER_HIGHLIGHTER);
            break;
        case Action::CLASSROOM_CLEAR_TEMPORARY_INK:
            clearTemporaryInk(control);
            break;
        default:
            xoj_assert(false);
    }
}

ToolPresentationKit::ToolPresentationKit(std::string id, Control* control, IconNameHelper iconNameHelper,
                                         ToolStudentView* students):
        AbstractToolItem(std::move(id), Category::MISC),
        control(control),
        students(students),
        iconName(iconNameHelper.iconName("utn-presentation")) {}

auto ToolPresentationKit::createItem(bool horizontal) -> xoj::util::WidgetSPtr {
    auto [popover, panel] = utn::createPopoverPanel();

    auto connect = [this](GtkWidget* button, void (*handler)(Control*)) {
        g_signal_connect(button, "clicked", G_CALLBACK(+[](GtkButton* b, gpointer data) {
                             auto run = reinterpret_cast<void (*)(Control*)>(g_object_get_data(G_OBJECT(b), "utn-run"));
                             run(static_cast<Control*>(data));
                         }),
                         control);
        g_object_set_data(G_OBJECT(button), "utn-run", reinterpret_cast<gpointer>(handler));
    };

    auto ui = std::make_shared<PresentState>();
    utn::appendPopoverHeading(panel, _("Classroom controls"));
    ui->status = gtk_label_new("");
    gtk_widget_set_name(ui->status, "utnClassroomStatus");
    gtk_widget_set_halign(ui->status, GTK_ALIGN_START);
    gtk_widget_add_css_class(ui->status, "utn-context-hint");
    gtk_box_append(panel, ui->status);
    ui->back = utn::appendMenuButton(panel, popover, _("Back to normal"),
                                     _("Turn off Spotlight, Curtain, Freeze and Blank. Saved work is not changed."));
    ToolStudentView* const students = this->students;
    g_object_set_data(G_OBJECT(ui->back), "utn-students", students);
    g_signal_connect(ui->back, "clicked", G_CALLBACK(+[](GtkButton* b, gpointer data) {
                         backToNormal(static_cast<Control*>(data),
                                      static_cast<ToolStudentView*>(g_object_get_data(G_OBJECT(b), "utn-students")));
                     }),
                     control);

    utn::appendPopoverHeading(panel, _("During the lesson"));
    connect(utn::appendMenuButton(panel, popover, _("Temporary ink"), _("Ink that fades away; it is never saved")),
            [](Control* c) { selectPresentationTool(c, TOOL_LASER_POINTER_PEN); });
    connect(utn::appendMenuButton(panel, popover, _("Temporary highlight"),
                                  _("Highlight that fades away; it is never saved")),
            [](Control* c) { selectPresentationTool(c, TOOL_LASER_POINTER_HIGHLIGHTER); });
    ui->spotlight = utn::appendMenuButton(panel, popover, _("Spotlight"),
                                          _("Darken the page around the pointer. Choose again to show the whole page."));
    connect(ui->spotlight, [](Control* c) { selectPresentationTool(c, TOOL_HAND, true, false); });
    ui->curtain = utn::appendMenuButton(panel, popover, _("Curtain"),
                                        _("Cover the page. Choose again to show the whole page."));
    connect(ui->curtain, [](Control* c) { selectPresentationTool(c, TOOL_HAND, false, true); });
    ui->exit = utn::appendMenuButton(panel, popover, _("Show the whole page"),
                                     _("Turn off Spotlight or Curtain. Escape does the same. Saved work is not changed."));
    connect(ui->exit, [](Control* c) { exitPresentationOverlay(c); });

    if (students) {
        utn::appendPopoverHeading(panel, _("Students' screen"));
        ui->freeze = utn::appendMenuButton(panel, popover, _("Freeze students' screen"),
                                           _("Keep showing this page while you prepare the next step"));
        g_signal_connect(ui->freeze, "clicked", G_CALLBACK(+[](GtkButton*, gpointer data) {
                             auto* s = static_cast<ToolStudentView*>(data);
                             s->setFrozen(!s->isFrozen());
                         }),
                         students);
        ui->blank = utn::appendMenuButton(panel, popover, _("Blank students' screen"),
                                          _("Hide the page from students until you turn this off"));
        g_signal_connect(ui->blank, "clicked", G_CALLBACK(+[](GtkButton*, gpointer data) {
                             auto* s = static_cast<ToolStudentView*>(data);
                             s->setBlanked(!s->isBlanked());
                         }),
                         students);
        ui->studentWindow = utn::appendMenuButton(panel, popover, _("Open Student View on the projector"),
                                                  _("Show students the page without your tools or private notes"));
        g_signal_connect(ui->studentWindow, "clicked", G_CALLBACK(+[](GtkButton*, gpointer data) {
                             auto* s = static_cast<ToolStudentView*>(data);
                             if (s->isWindowOpen()) {
                                 s->closeStudentView();
                             } else {
                                 s->showStudentView();
                             }
                         }),
                         students);
    }

    utn::appendPopoverHeading(panel, _("Tidy up"));
    connect(utn::appendMenuButton(panel, popover, _("Clear temporary ink"),
                                  _("Remove temporary ink and highlights from all pages. Saved work is not affected.")),
            clearTemporaryInk);
    utn::appendPopoverHeading(panel, _("Screen"));
    connect(utn::appendMenuButton(panel, popover, _("Presentation mode on or off"),
                                  _("Show one page at a time without the toolbars")),
            [](Control* c) { toggleBoolAction(c, Action::PRESENTATION_MODE); });
    connect(utn::appendMenuButton(panel, popover, _("Full screen on or off")),
            [](Control* c) { toggleBoolAction(c, Action::FULLSCREEN); });

    GtkMenuButton* menuButton = GTK_MENU_BUTTON(gtk_menu_button_new());
    gtk_widget_set_focus_on_click(GTK_WIDGET(menuButton), false);  // keyboard users can still Tab to it
    gtk_widget_set_tooltip_text(GTK_WIDGET(menuButton), getToolDisplayName().c_str());
    auto* heading = GTK_BOX(gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 5));
    gtk_box_append(heading, getNewToolIcon());
    auto* headingLabel = gtk_label_new(_("Classroom"));
    gtk_widget_add_css_class(headingLabel, "utn-compact-hidden");  // icon only in compact windows; name stays
    gtk_box_append(heading, headingLabel);
    utn::setAccessibleName(GTK_WIDGET(menuButton), _("Classroom controls"));
    gtk_widget_show_all(GTK_WIDGET(heading));
    gtk_button_set_child(GTK_BUTTON(menuButton), GTK_WIDGET(heading));
    gtk_menu_button_set_popover(menuButton, GTK_WIDGET(popover));
    gtk_menu_button_set_direction(menuButton, horizontal ? GTK_ARROW_DOWN : GTK_ARROW_RIGHT);
    gtk_widget_add_css_class(GTK_WIDGET(menuButton), "utn-present-btn");

    gtk_widget_show_all(GTK_WIDGET(panel));

    // Keep the menu and the button in step with Spotlight and Curtain, however they were changed (menu, banner,
    // Escape, tool switch). The listener holds only a weak reference: ToolHandler cannot remove it, and a rebuilt
    // toolbar destroys this widget.
    ui->button = GTK_WIDGET(menuButton);
    g_object_set_data_full(G_OBJECT(menuButton), "utn-present-state", new std::shared_ptr<PresentState>(ui),
                           +[](gpointer data) { delete static_cast<std::shared_ptr<PresentState>*>(data); });
    g_signal_connect(menuButton, "destroy", G_CALLBACK(+[](GtkWidget*, gpointer data) {
                         *static_cast<PresentState*>(data) = PresentState{};
                     }),
                     ui.get());
    refreshPresentState(*ui, control, students);
    control->getToolHandler()->addToolChangedListener(
            [weak = std::weak_ptr<PresentState>(ui), control = this->control, students](ToolType) {
                if (auto state = weak.lock(); state && state->button) {
                    refreshPresentState(*state, control, students);
                }
            });
    if (students) {
        // Freeze and Blank change in Student View, which never fires a tool change
        students->addStateListener([weak = std::weak_ptr<PresentState>(ui), control = this->control, students]() {
            auto state = weak.lock();
            if (!state || !state->button) {
                return false;
            }
            refreshPresentState(*state, control, students);
            return true;
        });
    }
    return xoj::util::WidgetSPtr(GTK_WIDGET(menuButton), xoj::util::adopt);
}

auto ToolPresentationKit::getToolDisplayName() const -> std::string {
    return _("Classroom controls");
}

auto ToolPresentationKit::getNewToolIcon() const -> GtkWidget* {
    return gtk_image_new_from_icon_name(iconName.c_str(), GTK_ICON_SIZE_LARGE_TOOLBAR);
}
