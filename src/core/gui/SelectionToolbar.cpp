#include "SelectionToolbar.h"

#include <cmath>  // for lround

#include <gdk/gdkkeysyms.h>  // for GDK_KEY_Escape

#include "control/Control.h"                         // for Control
#include "control/ToolEnums.h"                       // for isSelectToolType
#include "control/ToolHandler.h"                     // for ToolHandler
#include "control/UtnLayout.h"                       // for isTeacherLayout
#include "control/settings/Settings.h"               // for Settings
#include "control/tools/EditSelection.h"             // for EditSelection
#include "gui/Layout.h"                              // for Layout
#include "gui/MainWindow.h"                          // for MainWindow
#include "gui/PageView.h"                            // for XojPageView
#include "gui/XournalView.h"                         // for XournalView
#include "gui/toolbarMenubar/ToolMenuHandler.h"      // for ToolMenuHandler
#include "gui/toolbarMenubar/ToolStudentView.h"      // for ToolStudentView
#include "gui/toolbarMenubar/UtnWidgets.h"           // for createColourSwatch, INK_SWATCHES
#include "gui/toolbarMenubar/model/ToolbarData.h"    // for ToolbarData
#include "util/gtk4_helper.h"                        // for gtk_box_append
#include "util/i18n.h"                               // for _

namespace {
/// Space between the selection's box (and its handles) and the toolbar, in pixels
constexpr double GAP = 14;

GtkWidget* newButton(GtkWidget* box, const char* label, const char* hint) {
    GtkWidget* button = gtk_button_new_with_label(label);
    gtk_widget_set_focus_on_click(button, false);  // a click leaves the keyboard on the page; Tab still reaches it
    gtk_widget_set_tooltip_text(button, hint);
    utn::setAccessibleName(button, label);
    gtk_box_append(GTK_BOX(box), button);
    return button;
}
}  // namespace

SelectionToolbar::SelectionToolbar(MainWindow* win, Control* control, GtkOverlay* overlay):
        win(win), control(control), overlay(overlay) {
    control->getSettings()->getCustomElement("utn").getBool(SETTING, enabled);

    widget = gtk_event_box_new();
    gtk_event_box_set_visible_window(GTK_EVENT_BOX(widget), false);
    gtk_widget_set_name(widget, "utnSelectionToolbar");
    gtk_widget_set_halign(widget, GTK_ALIGN_START);
    gtk_widget_set_valign(widget, GTK_ALIGN_START);
    GtkWidget* box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 4);
    gtk_widget_add_css_class(box, "utn-selection-toolbar");
    gtk_container_add(GTK_CONTAINER(widget), box);
    utn::setAccessibleName(box, _("Selection actions"));

    deleteButton = newButton(box, _("Delete"), _("Delete the selected objects. Undo restores them."));
    gtk_widget_add_css_class(deleteButton, "destructive-action");
    g_signal_connect(deleteButton, "clicked", G_CALLBACK(+[](GtkButton*, gpointer data) {
                         static_cast<Control*>(data)->deleteSelection();
                     }),
                     control);

    duplicateButton = newButton(box, _("Duplicate"), nullptr);
    g_signal_connect(duplicateButton, "clicked", G_CALLBACK(+[](GtkButton*, gpointer data) {
                         static_cast<Control*>(data)->duplicateSelection();
                     }),
                     control);

    // Same colours and the same path as the properties panel: the tool colour, applied to the selection
    colourButton = gtk_menu_button_new();
    gtk_button_set_label(GTK_BUTTON(colourButton), _("Colour"));
    gtk_widget_set_focus_on_click(colourButton, false);
    gtk_widget_set_tooltip_text(colourButton, _("Change the colour of the selected objects"));
    utn::setAccessibleName(colourButton, _("Colour"));
    GtkWidget* popover = gtk_popover_new(colourButton);
    GtkWidget* swatches = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 4);
    gtk_widget_add_css_class(swatches, "utn-swatches");
    gtk_widget_set_margin_start(swatches, 6);
    gtk_widget_set_margin_end(swatches, 6);
    gtk_widget_set_margin_top(swatches, 6);
    gtk_widget_set_margin_bottom(swatches, 6);
    for (const auto& swatch: utn::INK_SWATCHES) {
        GtkWidget* button = utn::createColourSwatch(swatch.colour, _(swatch.name));
        g_object_set_data(G_OBJECT(button), "utn-colour", GUINT_TO_POINTER(uint32_t(swatch.colour)));
        g_signal_connect(button, "clicked", G_CALLBACK(+[](GtkButton* button, gpointer data) {
                             auto colour = Color(GPOINTER_TO_UINT(g_object_get_data(G_OBJECT(button), "utn-colour")));
                             if (GtkWidget* open = gtk_widget_get_ancestor(GTK_WIDGET(button), GTK_TYPE_POPOVER)) {
                                 gtk_popover_popdown(GTK_POPOVER(open));
                             }
                             static_cast<Control*>(data)->getToolHandler()->setColor(colour, true);  // recolours it
                         }),
                         control);
        gtk_box_append(GTK_BOX(swatches), button);
    }
    gtk_widget_show_all(swatches);
    gtk_container_add(GTK_CONTAINER(popover), swatches);
    gtk_menu_button_set_popover(GTK_MENU_BUTTON(colourButton), popover);
    gtk_box_append(GTK_BOX(box), colourButton);

    frontButton = newButton(box, _("Bring to front"), _("Place the selection above other objects"));
    g_signal_connect(frontButton, "clicked", G_CALLBACK(+[](GtkButton*, gpointer data) {
                         static_cast<Control*>(data)->reorderSelection(EditSelection::OrderChange::BringToFront);
                     }),
                     control);
    backButton = newButton(box, _("Send to back"), _("Place the selection behind other objects"));
    g_signal_connect(backButton, "clicked", G_CALLBACK(+[](GtkButton*, gpointer data) {
                         static_cast<Control*>(data)->reorderSelection(EditSelection::OrderChange::SendToBack);
                     }),
                     control);

    g_signal_connect(widget, "key-press-event", G_CALLBACK(+[](GtkWidget*, GdkEventKey* event, gpointer data) -> gboolean {
                         if (event->keyval != GDK_KEY_Escape) {
                             return false;
                         }
                         static_cast<SelectionToolbar*>(data)->dismiss();
                         return true;
                     }),
                     this);

    // Shown only by update(): the window's show_all must not bring it up
    gtk_widget_show_all(widget);
    gtk_widget_set_no_show_all(widget, true);
    gtk_widget_hide(widget);
    gtk_overlay_add_overlay(overlay, widget);
    gtk_overlay_set_overlay_pass_through(overlay, widget, false);
    g_object_ref(widget);
    g_object_ref(overlay);
    g_signal_connect(overlay, "get-child-position", G_CALLBACK(childPosition), this);

    // Scrolling, zooming and moving the selection all redraw the page: follow them from there
    canvas = win->getXournal()->getWidget();
    g_object_ref(canvas);
    g_signal_connect_after(canvas, "draw", G_CALLBACK(+[](GtkWidget*, cairo_t*, gpointer data) -> gboolean {
                               static_cast<SelectionToolbar*>(data)->scheduleUpdate();
                               return false;
                           }),
                           this);

    registerListener(control);
}

SelectionToolbar::~SelectionToolbar() {
    unregisterListener();
    if (pendingUpdate) {
        g_source_remove(pendingUpdate);
    }
    g_signal_handlers_disconnect_by_data(canvas, this);
    g_signal_handlers_disconnect_by_data(overlay, this);
    g_signal_handlers_disconnect_by_data(widget, this);
    g_object_unref(canvas);
    g_object_unref(overlay);
    g_object_unref(widget);
}

bool SelectionToolbar::wanted(const EditSelection* selection) const {
    if (!enabled || control->isBlocked() || selection == dismissedFor) {
        return false;
    }
    const ToolbarData* layout = win->getSelectedToolbar();
    if (!layout || !utn::isTeacherLayout(layout->getId())) {
        return false;
    }
    if (!isSelectToolType(control->getToolHandler()->getToolType())) {
        return false;
    }
    // The cover hides the teacher's canvas from the class; nothing may sit on top of it
    if (auto* menus = win->getToolMenuHandler()) {
        if (auto* students = menus->getStudentView(); students && students->isTeacherCoverShown()) {
            return false;
        }
    }
    const XojPageView* view = selection->getView();
    const PageRef current = control->getCurrentPage();
    return view && current && view->getPage() == current;
}

std::optional<utn::ToolbarPosition> SelectionToolbar::placeFor(const EditSelection* selection) const {
    auto* xournal = win->getXournal();
    if (!xournal || !xournal->getLayout() || !gtk_widget_get_ancestor(canvas, GTK_TYPE_SCROLLED_WINDOW)) {
        return std::nullopt;
    }
    GtkRequisition natural{};
    gtk_widget_get_preferred_size(widget, nullptr, &natural);
    const auto visible = xournal->getLayout()->getVisibleRect();
    const auto box = selection->getBoundingBoxInView();
    return utn::placeSelectionToolbar({box.x, box.y, box.width, box.height}, natural.width, natural.height,
                                      {visible.x, visible.y, visible.width, visible.height}, GAP);
}

void SelectionToolbar::syncButtons() {
    // Colour is offered only when the selection has something the colour applies to (not a picture)
    gtk_widget_set_visible(colourButton, control->getToolHandler()->hasCapability(TOOL_CAP_COLOR));
    const bool canDuplicate = control->canDuplicateSelection();
    gtk_widget_set_sensitive(duplicateButton, canDuplicate);
    gtk_widget_set_tooltip_text(duplicateButton,
                                canDuplicate ? _("Copy the selection onto the same layer, a little down and to the right") :
                                               _("Unavailable: put the selection back on the page and layer it came from"));
}

void SelectionToolbar::update() {
    auto* xournal = win->getXournal();
    const EditSelection* selection = xournal ? xournal->getSelection() : nullptr;
    if (!selection) {
        dismissedFor = nullptr;
    }
    std::optional<utn::ToolbarPosition> target;
    if (selection && wanted(selection)) {
        syncButtons();  // first: the buttons shown decide the toolbar's size
        target = placeFor(selection);
    }
    if (!target) {
        position.reset();
        if (gtk_widget_get_visible(widget)) {
            giveKeyboardToPage();
            gtk_widget_hide(widget);
        }
        return;
    }
    const bool moved = !position || position->x != target->x || position->y != target->y;
    position = target;
    if (!gtk_widget_get_visible(widget)) {
        gtk_widget_show(widget);  // never grabs the keyboard: the page keeps it
    } else if (moved) {
        gtk_widget_queue_resize(widget);
    }
}

void SelectionToolbar::setEnabled(bool on) {
    enabled = on;
    control->getSettings()->getCustomElement("utn").setBool(SETTING, on);
    update();
}

void SelectionToolbar::dismiss() {
    auto* xournal = win->getXournal();
    dismissedFor = xournal ? xournal->getSelection() : nullptr;
    giveKeyboardToPage();
    update();
}

void SelectionToolbar::pageSelected(size_t) { scheduleUpdate(); }

void SelectionToolbar::scheduleUpdate() {
    if (pendingUpdate) {
        return;
    }
    pendingUpdate = g_idle_add(+[](gpointer data) -> gboolean {
        auto* self = static_cast<SelectionToolbar*>(data);
        self->pendingUpdate = 0;
        self->update();
        return G_SOURCE_REMOVE;
    }, this);
}

void SelectionToolbar::giveKeyboardToPage() {
    GtkWidget* toplevel = gtk_widget_get_toplevel(widget);
    if (!GTK_IS_WINDOW(toplevel)) {
        return;
    }
    GtkWidget* focus = gtk_window_get_focus(GTK_WINDOW(toplevel));
    if (focus && (focus == widget || gtk_widget_is_ancestor(focus, widget))) {
        gtk_widget_grab_focus(canvas);
    }
}

gboolean SelectionToolbar::childPosition(GtkOverlay* overlay, GtkWidget* child, GdkRectangle* allocation,
                                         gpointer data) {
    auto* self = static_cast<SelectionToolbar*>(data);
    if (child != self->widget || !self->position) {
        return false;
    }
    GtkRequisition natural{};
    gtk_widget_get_preferred_size(child, nullptr, &natural);
    allocation->width = natural.width;
    allocation->height = natural.height;
    // A hidden toolbar measures 0 x 0, so the place found while it was hidden is redone at its real size
    if (auto* xournal = self->win->getXournal(); xournal && xournal->getSelection()) {
        if (auto placed = self->placeFor(xournal->getSelection())) {
            self->position = placed;
        }
    }
    // The position is relative to the visible canvas, which is the scrolled window around the page widget
    GtkWidget* scroller = gtk_widget_get_ancestor(self->canvas, GTK_TYPE_SCROLLED_WINDOW);
    const int x = static_cast<int>(std::lround(self->position->x));
    const int y = static_cast<int>(std::lround(self->position->y));
    if (!scroller || !gtk_widget_translate_coordinates(scroller, GTK_WIDGET(overlay), x, y, &allocation->x,
                                                      &allocation->y)) {
        allocation->x = x;
        allocation->y = y;
    }
    return true;
}
