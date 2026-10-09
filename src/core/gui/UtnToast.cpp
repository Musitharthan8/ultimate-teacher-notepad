/*
 * Ultimate Teacher Notepad
 *
 * Based on Xournal++ GPLv2+
 */

#include "UtnToast.h"

#include <utility>

#include "util/gtk4_helper.h"

namespace {
constexpr guint TOAST_MILLISECONDS = 3500;
}

UtnToast::UtnToast(GtkOverlay* overlay) {
    revealer = gtk_revealer_new();
    gtk_revealer_set_transition_type(GTK_REVEALER(revealer), GTK_REVEALER_TRANSITION_TYPE_SLIDE_UP);
    gtk_widget_set_halign(revealer, GTK_ALIGN_CENTER);
    gtk_widget_set_valign(revealer, GTK_ALIGN_END);
    gtk_widget_set_margin_bottom(revealer, 64);  // above the lesson bar
    gtk_widget_set_name(revealer, "utnToast");

    GtkWidget* box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 12);
    gtk_widget_add_css_class(box, "utn-toast");
    label = gtk_label_new(nullptr);
    gtk_label_set_line_wrap(GTK_LABEL(label), true);
    gtk_label_set_max_width_chars(GTK_LABEL(label), 60);
    gtk_box_append(GTK_BOX(box), label);

    button = gtk_button_new();
    gtk_widget_set_focus_on_click(button, false);
    g_signal_connect(button, "clicked", G_CALLBACK(+[](GtkButton*, gpointer data) {
                         auto* self = static_cast<UtnToast*>(data);
                         auto run = std::move(self->action);
                         self->hide();
                         if (run) {
                             run();
                         }
                     }),
                     this);
    gtk_box_append(GTK_BOX(box), button);

    gtk_container_add(GTK_CONTAINER(revealer), box);
    gtk_widget_show_all(revealer);
    gtk_widget_hide(button);
    gtk_revealer_set_reveal_child(GTK_REVEALER(revealer), false);

    // Only the toast itself takes clicks; the canvas around it is unaffected
    gtk_overlay_add_overlay(overlay, revealer);
    gtk_overlay_set_overlay_pass_through(overlay, revealer, false);
    g_object_ref(revealer);
}

UtnToast::~UtnToast() {
    if (timeout) {
        g_source_remove(timeout);
    }
    g_object_unref(revealer);
}

void UtnToast::show(const std::string& message, const std::string& actionLabel, std::function<void()> action) {
    gtk_label_set_text(GTK_LABEL(label), message.c_str());
    this->action = std::move(action);
    if (!actionLabel.empty() && this->action) {
        gtk_button_set_label(GTK_BUTTON(button), actionLabel.c_str());
        gtk_widget_show(button);
    } else {
        gtk_widget_hide(button);
    }
    gtk_revealer_set_reveal_child(GTK_REVEALER(revealer), true);

    if (timeout) {
        g_source_remove(timeout);
    }
    timeout = g_timeout_add(TOAST_MILLISECONDS, &UtnToast::onTimeout, this);
}

void UtnToast::hide() {
    if (timeout) {
        g_source_remove(timeout);
        timeout = 0;
    }
    action = {};
    gtk_label_set_text(GTK_LABEL(label), "");
    gtk_revealer_set_reveal_child(GTK_REVEALER(revealer), false);
}

auto UtnToast::currentMessage() const -> std::string {
    return gtk_revealer_get_reveal_child(GTK_REVEALER(revealer)) ? gtk_label_get_text(GTK_LABEL(label)) : "";
}

auto UtnToast::onTimeout(gpointer data) -> gboolean {
    auto* self = static_cast<UtnToast*>(data);
    self->timeout = 0;
    self->hide();
    return G_SOURCE_REMOVE;
}
