/*
 * Ultimate Teacher Notepad
 *
 * Classroom presentation toolkit
 *
 * Based on Xournal++ GPLv2+
 */

#include "ToolPresentationKit.h"

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

// Every tool switch goes through Control so text edits, selections and special modes are finished properly
void selectPresentationTool(Control* control, ToolType tool, bool spotlight = false, bool curtain = false) {
    auto* tools = control->getToolHandler();
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
    connect(utn::appendMenuButton(panel, popover, _("Spotlight"), _("Darken the page except around the pointer")),
            [](Control* c) { selectPresentationTool(c, TOOL_HAND, true, false); });
    connect(utn::appendMenuButton(panel, popover, _("Curtain"), _("Cover the page and drag to reveal it gradually")),
            [](Control* c) { selectPresentationTool(c, TOOL_HAND, false, true); });

    utn::appendPopoverHeading(panel, _("Tidy up"));
    connect(utn::appendMenuButton(panel, popover, _("Clear temporary ink"),
                                  _("Remove temporary ink and highlights from all pages. Saved work is not affected.")),
            clearTemporaryInk);
    connect(utn::appendMenuButton(panel, popover, _("Show the whole page"),
                                  _("Turn off Spotlight or Curtain without changing the tool")),
            [](Control* c) {
                c->getToolHandler()->clearPresentationOverlay();
                repaintPresentationOverlay(c);
            });

    utn::appendPopoverHeading(panel, _("Screen"));
    connect(utn::appendMenuButton(panel, popover, _("Presentation mode on or off"),
                                  _("Show one page at a time without the toolbars")),
            [](Control* c) { toggleBoolAction(c, Action::PRESENTATION_MODE); });
    connect(utn::appendMenuButton(panel, popover, _("Full screen on or off")),
            [](Control* c) { toggleBoolAction(c, Action::FULLSCREEN); });

    GtkMenuButton* menuButton = GTK_MENU_BUTTON(gtk_menu_button_new());
    gtk_widget_set_can_focus(GTK_WIDGET(menuButton), false);
    gtk_widget_set_tooltip_text(GTK_WIDGET(menuButton), getToolDisplayName().c_str());
    auto* heading = GTK_BOX(gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 5));
    gtk_box_append(heading, getNewToolIcon());
    gtk_box_append(heading, gtk_label_new(_("Present")));
    gtk_widget_show_all(GTK_WIDGET(heading));
    gtk_button_set_child(GTK_BUTTON(menuButton), GTK_WIDGET(heading));
    gtk_menu_button_set_popover(menuButton, GTK_WIDGET(popover));
    gtk_menu_button_set_direction(menuButton, horizontal ? GTK_ARROW_DOWN : GTK_ARROW_RIGHT);

    gtk_widget_show_all(GTK_WIDGET(panel));
    return xoj::util::WidgetSPtr(GTK_WIDGET(menuButton), xoj::util::adopt);
}

auto ToolPresentationKit::getToolDisplayName() const -> std::string {
    return _("Present");
}

auto ToolPresentationKit::getNewToolIcon() const -> GtkWidget* {
    return gtk_image_new_from_icon_name(iconName.c_str(), GTK_ICON_SIZE_LARGE_TOOLBAR);
}
