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
#include "util/gtk4_helper.h"
#include "util/raii/GVariantSPtr.h"
#include "util/i18n.h"

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
};

/// Say which overlay is on in words (never colour alone), and offer the way back only while there is one
void refreshPresentState(const PresentState& ui, Control* control) {
    auto* tools = control->getToolHandler();
    const bool spotlight = tools->isSpotlightEnabled();
    const bool curtain = tools->isCurtainEnabled();
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
        if (spotlight || curtain) {
            gtk_style_context_add_class(context, "utn-present-active");
        } else {
            gtk_style_context_remove_class(context, "utn-present-active");
        }
        gtk_widget_set_tooltip_text(ui.button, spotlight ? _("Spotlight is on. The page is darkened.") :
                                               curtain   ? _("Curtain is on. The page is covered.") :
                                                           _("Spotlight & Curtain: darken or cover the page, and write temporary ink"));
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

ToolPresentationKit::ToolPresentationKit(std::string id, Control* control, IconNameHelper iconNameHelper):
        AbstractToolItem(std::move(id), Category::MISC),
        control(control),
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

    utn::appendPopoverHeading(panel, _("During the lesson"));
    connect(utn::appendMenuButton(panel, popover, _("Temporary ink"), _("Ink that fades away; it is never saved")),
            [](Control* c) { selectPresentationTool(c, TOOL_LASER_POINTER_PEN); });
    connect(utn::appendMenuButton(panel, popover, _("Temporary highlight"),
                                  _("Highlight that fades away; it is never saved")),
            [](Control* c) { selectPresentationTool(c, TOOL_LASER_POINTER_HIGHLIGHTER); });
    auto ui = std::make_shared<PresentState>();
    ui->spotlight = utn::appendMenuButton(panel, popover, _("Spotlight"),
                                          _("Darken the page around the pointer. Choose again to show the whole page."));
    connect(ui->spotlight, [](Control* c) { selectPresentationTool(c, TOOL_HAND, true, false); });
    ui->curtain = utn::appendMenuButton(panel, popover, _("Curtain"),
                                        _("Cover the page. Choose again to show the whole page."));
    connect(ui->curtain, [](Control* c) { selectPresentationTool(c, TOOL_HAND, false, true); });
    ui->exit = utn::appendMenuButton(panel, popover, _("Show the whole page"),
                                     _("Turn off Spotlight or Curtain. Escape does the same. Saved work is not changed."));
    connect(ui->exit, [](Control* c) { exitPresentationOverlay(c); });

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
    auto* headingLabel = gtk_label_new(_("Spotlight & Curtain"));
    gtk_widget_add_css_class(headingLabel, "utn-compact-hidden");  // icon only in compact windows; name stays
    gtk_box_append(heading, headingLabel);
    utn::setAccessibleName(GTK_WIDGET(menuButton), _("Spotlight and Curtain"));
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
    refreshPresentState(*ui, control);
    control->getToolHandler()->addToolChangedListener(
            [weak = std::weak_ptr<PresentState>(ui), control = this->control](ToolType) {
                if (auto state = weak.lock(); state && state->button) {
                    refreshPresentState(*state, control);
                }
            });
    return xoj::util::WidgetSPtr(GTK_WIDGET(menuButton), xoj::util::adopt);
}

auto ToolPresentationKit::getToolDisplayName() const -> std::string {
    return _("Spotlight & Curtain");
}

auto ToolPresentationKit::getNewToolIcon() const -> GtkWidget* {
    return gtk_image_new_from_icon_name(iconName.c_str(), GTK_ICON_SIZE_LARGE_TOOLBAR);
}
