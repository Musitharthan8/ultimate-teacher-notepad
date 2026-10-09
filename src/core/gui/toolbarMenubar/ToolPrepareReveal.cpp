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
#include "control/layer/LayerController.h"
#include "util/gtk4_helper.h"
#include "util/i18n.h"

#include "UtnWidgets.h"

namespace {
constexpr const char* REVEAL_LAYER_PREFIX = "UTN Reveal";

bool isRevealLayer(const std::string& name) {
    return name.rfind(REVEAL_LAYER_PREFIX, 0) == 0;
}

void setRevealLayersVisible(LayerController* layers, bool visible) {
    const auto count = layers->getLayerCount();
    for (Layer::Index id = 1; id <= count; ++id) {
        if (isRevealLayer(layers->getLayerNameById(id))) {
            layers->setLayerVisible(id, visible);
        }
    }

    if (!visible) {
        auto current = layers->getCurrentLayerId();
        if (current > 0 && isRevealLayer(layers->getLayerNameById(current))) {
            for (Layer::Index id = count; id >= 1; --id) {
                if (!isRevealLayer(layers->getLayerNameById(id))) {
                    layers->switchToLay(id);
                    break;
                }
                if (id == 1) {
                    break;
                }
            }
        }
    }
}
}  // namespace

ToolPrepareReveal::ToolPrepareReveal(std::string id, Control* control, IconNameHelper iconNameHelper):
        AbstractToolItem(std::move(id), Category::TOOLS),
        control(control),
        iconName(iconNameHelper.iconName("utn-reveal")) {}

auto ToolPrepareReveal::createItem(bool horizontal) -> xoj::util::WidgetSPtr {
    auto [popover, panel] = utn::createPopoverPanel();
    utn::appendPopoverHeading(panel, _("Hide & Reveal"));
    GtkWidget* prepare = utn::appendMenuButton(
            panel, popover, _("Start an answers layer"),
            _("Write answers on a new layer, then choose Hide answers before the lesson"));
    GtkWidget* hide = utn::appendMenuButton(panel, popover, _("Hide answers"),
                                            _("Hide everything written on answers layers"));
    GtkWidget* reveal = utn::appendMenuButton(panel, popover, _("Reveal answers"),
                                              _("Show the hidden answers to the class"));

    g_signal_connect(
            prepare,
            "clicked",
            G_CALLBACK(+[](GtkButton*, gpointer data) {
                auto* ctrl = static_cast<Control*>(data);
                auto* layers = ctrl->getLayerController();

                layers->addNewLayer(false);

                unsigned int suffix = 1;
                std::string name = REVEAL_LAYER_PREFIX;
                while (true) {
                    bool exists = false;
                    for (Layer::Index id = 1; id <= layers->getLayerCount(); ++id) {
                        if (layers->getLayerNameById(id) == name) {
                            exists = true;
                            break;
                        }
                    }

                    if (!exists) {
                        break;
                    }

                    ++suffix;
                    name = std::string(REVEAL_LAYER_PREFIX) + " " + std::to_string(suffix);
                }

                layers->setCurrentLayerName(name);
            }),
            control);

    g_signal_connect(
            hide,
            "clicked",
            G_CALLBACK(+[](GtkButton*, gpointer data) {
                auto* ctrl = static_cast<Control*>(data);
                setRevealLayersVisible(ctrl->getLayerController(), false);
            }),
            control);

    g_signal_connect(
            reveal,
            "clicked",
            G_CALLBACK(+[](GtkButton*, gpointer data) {
                auto* ctrl = static_cast<Control*>(data);
                setRevealLayersVisible(ctrl->getLayerController(), true);
            }),
            control);


    GtkMenuButton* menuButton = GTK_MENU_BUTTON(gtk_menu_button_new());
    gtk_widget_set_can_focus(GTK_WIDGET(menuButton), false);
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
