/*
 * Ultimate Teacher Notepad
 *
 * Prepare and reveal toolbar item
 *
 * Based on Xournal++ GPLv2+
 */

#include "ToolPrepareReveal.h"

#include <string>
#include <utility>

#include "control/Control.h"
#include "gui/MainWindow.h"
#include "util/gtk4_helper.h"
#include "util/i18n.h"

#include "UtnWidgets.h"


ToolPrepareReveal::ToolPrepareReveal(std::string id, Control* control, IconNameHelper iconNameHelper):
        AbstractToolItem(std::move(id), Category::TOOLS),
        control(control),
        iconName(iconNameHelper.iconName("utn-reveal")) {}

auto ToolPrepareReveal::createItem(bool horizontal) -> xoj::util::WidgetSPtr {
    auto [popover, panel] = utn::createPopoverPanel();
    utn::appendPopoverHeading(panel, _("Hide & Reveal"));

    auto add = [this, panel = panel, popover = popover](const char* label, const char* hint, void (*run)(Control*)) {
        GtkWidget* button = utn::appendMenuButton(panel, popover, label, hint);
        g_object_set_data(G_OBJECT(button), "utn-run", reinterpret_cast<gpointer>(run));
        g_signal_connect(button, "clicked", G_CALLBACK(+[](GtkButton* b, gpointer data) {
                             auto run = reinterpret_cast<void (*)(Control*)>(g_object_get_data(G_OBJECT(b), "utn-run"));
                             run(static_cast<Control*>(data));
                         }),
                         control);
    };

    add(_("Start an answers layer"), _("Write answers on a new layer, then choose Hide answers before the lesson"),
        [](Control* c) {
            c->startAnswersLayer();
            if (auto* win = c->getWindow()) {
                win->showToast(_("Writing on the answers layer. Choose Hide answers before students arrive."));
            }
        });
    add(_("Hide answers"), _("Hide every answers layer in the whole lesson from students"), [](Control* c) {
        const size_t changed = c->setAnswersRevealed(false, true);
        if (auto* win = c->getWindow()) {
            win->showToast(changed ? _("Answers are hidden from students") : _("No answers are showing"));
        }
    });
    add(_("Reveal answers on this page"), _("Show this page's hidden answers to the class"), [](Control* c) {
        const size_t changed = c->setAnswersRevealed(true, false);
        if (auto* win = c->getWindow()) {
            win->showToast(changed ? _("Answers on this page are now showing") : _("No hidden answers on this page"));
        }
    });
    add(_("Reveal all answers"), _("Show every hidden answer in the lesson"), [](Control* c) {
        const size_t changed = c->setAnswersRevealed(true, true);
        if (auto* win = c->getWindow()) {
            win->showToast(changed ? _("All answers are now showing") : _("No hidden answers in this lesson"));
        }
    });

    GtkMenuButton* menuButton = GTK_MENU_BUTTON(gtk_menu_button_new());
    gtk_widget_set_focus_on_click(GTK_WIDGET(menuButton), false);  // keyboard users can still Tab to it
    gtk_widget_set_tooltip_text(GTK_WIDGET(menuButton), getToolDisplayName().c_str());
    utn::setAccessibleName(GTK_WIDGET(menuButton), getToolDisplayName().c_str());
    gtk_button_set_child(GTK_BUTTON(menuButton), getNewToolIcon());
    gtk_menu_button_set_popover(menuButton, GTK_WIDGET(popover));
    gtk_menu_button_set_direction(menuButton, horizontal ? GTK_ARROW_DOWN : GTK_ARROW_RIGHT);

    gtk_widget_show_all(GTK_WIDGET(panel));
    return xoj::util::WidgetSPtr(GTK_WIDGET(menuButton), xoj::util::adopt);
}

auto ToolPrepareReveal::getToolDisplayName() const -> std::string {
    return _("Hide & Reveal");
}

auto ToolPrepareReveal::getNewToolIcon() const -> GtkWidget* {
    return gtk_image_new_from_icon_name(iconName.c_str(), GTK_ICON_SIZE_LARGE_TOOLBAR);
}
