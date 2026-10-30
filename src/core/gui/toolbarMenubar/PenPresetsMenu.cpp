#include "PenPresetsMenu.h"

#include <functional>
#include <memory>
#include <utility>

#include "control/Control.h"
#include "model/PenPresets.h"
#include "util/gtk4_helper.h"
#include "util/i18n.h"

#include "UtnWidgets.h"

namespace utn {

namespace {
constexpr const char* PRESET_ID_KEY = "utn-preset-id";

/// A small modal dialog asking for a name. `accepted` runs with the trimmed-by-the-store text on Save.
void promptForName(GtkWindow* parent, const char* title, const std::string& initial,
                   std::function<void(const std::string&)> accepted) {
    GtkWidget* dialog = gtk_dialog_new_with_buttons(title, parent, GTK_DIALOG_MODAL, _("Cancel"), GTK_RESPONSE_CANCEL,
                                                    _("Save"), GTK_RESPONSE_ACCEPT, nullptr);
    gtk_dialog_set_default_response(GTK_DIALOG(dialog), GTK_RESPONSE_ACCEPT);
    GtkWidget* entry = gtk_entry_new();
    gtk_entry_set_text(GTK_ENTRY(entry), initial.c_str());
    gtk_entry_set_max_length(GTK_ENTRY(entry), static_cast<gint>(PenPresetStore::MAX_NAME_BYTES));
    gtk_entry_set_activates_default(GTK_ENTRY(entry), true);
    utn::setAccessibleName(entry, _("Preset name"));
    gtk_widget_set_margin_start(entry, 12);
    gtk_widget_set_margin_end(entry, 12);
    gtk_widget_set_margin_top(entry, 12);
    gtk_widget_set_margin_bottom(entry, 12);
    gtk_container_add(GTK_CONTAINER(gtk_dialog_get_content_area(GTK_DIALOG(dialog))), entry);
    gtk_widget_show_all(dialog);
    gtk_widget_grab_focus(entry);

    auto* callback = new std::function<void(const std::string&)>(std::move(accepted));
    g_object_set_data_full(G_OBJECT(dialog), "utn-accepted", callback,
                           +[](gpointer data) { delete static_cast<std::function<void(const std::string&)>*>(data); });
    g_signal_connect(dialog, "response", G_CALLBACK(+[](GtkDialog* dlg, gint response, gpointer entryWidget) {
                         if (response == GTK_RESPONSE_ACCEPT) {
                             const std::string text = gtk_entry_get_text(GTK_ENTRY(entryWidget));
                             auto* accepted = static_cast<std::function<void(const std::string&)>*>(
                                     g_object_get_data(G_OBJECT(dlg), "utn-accepted"));
                             (*accepted)(text);
                         }
                         gtk_widget_destroy(GTK_WIDGET(dlg));
                     }),
                     entry);
}

void showManageDialog(Control* control) {
    GtkWidget* dialog = gtk_dialog_new_with_buttons(_("Manage presets"), control->getGtkWindow(), GTK_DIALOG_MODAL,
                                                    _("Restore default presets"), GTK_RESPONSE_REJECT, _("Close"),
                                                    GTK_RESPONSE_CLOSE, nullptr);
    GtkWidget* list = gtk_box_new(GTK_ORIENTATION_VERTICAL, 6);
    gtk_widget_set_margin_start(list, 12);
    gtk_widget_set_margin_end(list, 12);
    gtk_widget_set_margin_top(list, 12);
    gtk_widget_set_margin_bottom(list, 12);

    // One row per preset: an entry to rename it and a button to delete it. Rows are rebuilt after every change.
    auto* rebuild = new std::function<void()>();
    *rebuild = [control, list, rebuild]() {
        gtk_container_foreach(GTK_CONTAINER(list), +[](GtkWidget* child, gpointer) { gtk_widget_destroy(child); }, nullptr);
        for (const PenPreset& preset: control->getPenPresets().all()) {
            GtkWidget* row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
            GtkWidget* name = gtk_entry_new();
            gtk_entry_set_text(GTK_ENTRY(name), preset.name.c_str());
            gtk_entry_set_max_length(GTK_ENTRY(name), static_cast<gint>(PenPresetStore::MAX_NAME_BYTES));
            gtk_widget_set_hexpand(name, true);
            utn::setAccessibleName(name, _("Preset name"));
            gtk_widget_set_tooltip_text(name, _("Press Enter to rename this preset"));
            GtkWidget* remove = gtk_button_new_with_label(_("Delete"));
            utn::setAccessibleName(remove, (std::string(_("Delete preset")) + ": " + preset.name).c_str());
            auto* id = new std::string(preset.id);
            g_object_set_data_full(G_OBJECT(name), PRESET_ID_KEY, id, +[](gpointer p) { delete static_cast<std::string*>(p); });
            g_object_set_data(G_OBJECT(name), "utn-control", control);
            g_signal_connect(name, "activate", G_CALLBACK(+[](GtkEntry* entry, gpointer) {
                                 auto* ctrl = static_cast<Control*>(g_object_get_data(G_OBJECT(entry), "utn-control"));
                                 const auto* presetId = static_cast<std::string*>(
                                         g_object_get_data(G_OBJECT(entry), PRESET_ID_KEY));
                                 const std::string wanted = gtk_entry_get_text(entry);
                                 ctrl->updatePenPresets([&](PenPresetStore& store) { store.rename(*presetId, wanted); });
                                 if (const PenPreset* kept = ctrl->getPenPresets().find(*presetId)) {
                                     gtk_entry_set_text(entry, kept->name.c_str());  // shows the name the store settled on
                                 }
                             }),
                             nullptr);
            g_object_set_data_full(G_OBJECT(remove), PRESET_ID_KEY, new std::string(preset.id),
                                   +[](gpointer p) { delete static_cast<std::string*>(p); });
            g_object_set_data(G_OBJECT(remove), "utn-rebuild", rebuild);
            g_object_set_data(G_OBJECT(remove), "utn-control", control);
            g_signal_connect(remove, "clicked", G_CALLBACK(+[](GtkButton* button, gpointer) {
                                 auto* ctrl = static_cast<Control*>(g_object_get_data(G_OBJECT(button), "utn-control"));
                                 const auto* presetId = static_cast<std::string*>(
                                         g_object_get_data(G_OBJECT(button), PRESET_ID_KEY));
                                 auto* again = static_cast<std::function<void()>*>(
                                         g_object_get_data(G_OBJECT(button), "utn-rebuild"));
                                 const std::string idCopy = *presetId;  // the button is destroyed by the rebuild
                                 ctrl->updatePenPresets([&](PenPresetStore& store) { store.remove(idCopy); });
                                 (*again)();
                             }),
                             nullptr);
            gtk_box_pack_start(GTK_BOX(row), name, true, true, 0);
            gtk_box_pack_start(GTK_BOX(row), remove, false, false, 0);
            gtk_box_pack_start(GTK_BOX(list), row, false, false, 0);
        }
        gtk_widget_show_all(list);
    };
    (*rebuild)();
    gtk_container_add(GTK_CONTAINER(gtk_dialog_get_content_area(GTK_DIALOG(dialog))), list);
    g_object_set_data_full(G_OBJECT(dialog), "utn-rebuild", rebuild,
                           +[](gpointer p) { delete static_cast<std::function<void()>*>(p); });
    g_object_set_data(G_OBJECT(dialog), "utn-control", control);
    g_signal_connect(dialog, "response", G_CALLBACK(+[](GtkDialog* dlg, gint response, gpointer) {
                         auto* ctrl = static_cast<Control*>(g_object_get_data(G_OBJECT(dlg), "utn-control"));
                         if (response == GTK_RESPONSE_REJECT) {
                             ctrl->updatePenPresets([](PenPresetStore& store) { store.restoreDefaults(); });
                             auto* again = static_cast<std::function<void()>*>(
                                     g_object_get_data(G_OBJECT(dlg), "utn-rebuild"));
                             (*again)();
                             return;  // stay open: the list now shows the restored presets
                         }
                         gtk_widget_destroy(GTK_WIDGET(dlg));
                     }),
                     nullptr);
    gtk_widget_show_all(dialog);
}

void fillPopover(GtkWidget* popoverWidget, Control* control) {
    auto* popover = GTK_POPOVER(popoverWidget);
    if (GtkWidget* old = gtk_bin_get_child(GTK_BIN(popover))) {
        gtk_widget_destroy(old);
    }
    GtkWidget* panel = gtk_box_new(GTK_ORIENTATION_VERTICAL, 2);
    gtk_widget_set_margin_start(panel, 8);
    gtk_widget_set_margin_end(panel, 8);
    gtk_widget_set_margin_top(panel, 8);
    gtk_widget_set_margin_bottom(panel, 8);

    for (const PresetMenuEntry& entry: presetMenuEntries(control)) {
        // The mark is a shape, not only a colour, and the screen reader hears it too
        const std::string label = (entry.active ? "\xE2\x97\x8F " : "   ") + entry.name;
        GtkWidget* button = utn::appendMenuButton(GTK_BOX(panel), popover, label.c_str());
        utn::setAccessibleName(button, (entry.active ? entry.name + ", " + _("selected") : entry.name).c_str());
        g_object_set_data_full(G_OBJECT(button), PRESET_ID_KEY, new std::string(entry.id),
                               +[](gpointer p) { delete static_cast<std::string*>(p); });
        g_object_set_data(G_OBJECT(button), "utn-control", control);
        g_signal_connect(button, "clicked", G_CALLBACK(+[](GtkButton* b, gpointer) {
                             auto* ctrl = static_cast<Control*>(g_object_get_data(G_OBJECT(b), "utn-control"));
                             ctrl->applyPenPreset(*static_cast<std::string*>(g_object_get_data(G_OBJECT(b), PRESET_ID_KEY)));
                         }),
                         nullptr);
    }
    gtk_box_pack_start(GTK_BOX(panel), gtk_separator_new(GTK_ORIENTATION_HORIZONTAL), false, false, 4);

    GtkWidget* save = utn::appendMenuButton(GTK_BOX(panel), popover, _("Save current as preset…"),
                                            _("Keep this colour, size and opacity as a favourite"));
    gtk_widget_set_sensitive(save, control->currentPenPresetValues().has_value());
    g_object_set_data(G_OBJECT(save), "utn-control", control);
    g_signal_connect(save, "clicked", G_CALLBACK(+[](GtkButton* b, gpointer) {
                         auto* ctrl = static_cast<Control*>(g_object_get_data(G_OBJECT(b), "utn-control"));
                         promptForName(ctrl->getGtkWindow(), _("Save preset"), _("My pen"),
                                       [ctrl](const std::string& name) {
                                           // Taken when the name is confirmed, so it is the tool as it is then
                                           auto values = ctrl->currentPenPresetValues();
                                           if (!values) {
                                               return;
                                           }
                                           values->name = name;
                                           ctrl->updatePenPresets([&](PenPresetStore& store) { store.add(*values); });
                                       });
                     }),
                     nullptr);
    GtkWidget* manage = utn::appendMenuButton(GTK_BOX(panel), popover, _("Manage presets…"),
                                              _("Rename or delete favourites, or bring the built-in ones back"));
    g_object_set_data(G_OBJECT(manage), "utn-control", control);
    g_signal_connect(manage, "clicked", G_CALLBACK(+[](GtkButton* b, gpointer) {
                         showManageDialog(static_cast<Control*>(g_object_get_data(G_OBJECT(b), "utn-control")));
                     }),
                     nullptr);
    gtk_container_add(GTK_CONTAINER(popover), panel);
    gtk_widget_show_all(panel);
}
}  // namespace

std::vector<PresetMenuEntry> presetMenuEntries(const Control* control) {
    const std::string active = control->activePenPresetId();
    std::vector<PresetMenuEntry> entries;
    for (const PenPreset& preset: control->getPenPresets().all()) {
        entries.push_back({preset.id, preset.name, preset.id == active});
    }
    return entries;
}

GtkWidget* createPresetsMenuButton(Control* control) {
    GtkMenuButton* button = GTK_MENU_BUTTON(gtk_menu_button_new());
    gtk_button_set_label(GTK_BUTTON(button), _("Presets"));
    gtk_widget_set_focus_on_click(GTK_WIDGET(button), false);  // keyboard users can still Tab to it
    gtk_widget_set_tooltip_text(GTK_WIDGET(button), _("Apply a saved pen or highlighter setup in one click"));
    utn::setAccessibleName(GTK_WIDGET(button), _("Presets"));
    GtkWidget* popover = gtk_popover_new(nullptr);
    fillPopover(popover, control);
    // Built each time it opens, so the list and the mark on the active preset are never stale
    g_signal_connect(popover, "show", G_CALLBACK(+[](GtkWidget* widget, gpointer data) {
                         fillPopover(widget, static_cast<Control*>(data));
                     }),
                     control);
    gtk_menu_button_set_popover(button, popover);
    return GTK_WIDGET(button);
}

}  // namespace utn
