/*
 * Ultimate Teacher Notepad
 *
 * Command Palette (Ctrl+K): type a few words, pick a command, press Enter. It has no commands of its own: every entry
 * runs the action or method behind the matching button, menu entry or shortcut, and is greyed out and explained while
 * that one is unavailable. Page labels are never listed, so nothing private shows if the teacher's screen is shared.
 *
 * Based on Xournal++ GPLv2+
 */

#pragma once

#include <functional>
#include <optional>
#include <string>
#include <vector>

#include <gtk/gtk.h>

#include "enums/Action.enum.h"
#include "model/CommandPalette.h"

class Control;
class MainWindow;

class CommandPalette {
public:
    CommandPalette(Control* control, MainWindow* window);
    ~CommandPalette();

    /// Show the palette over the main window. The commands are read afresh, so what is greyed out is current.
    void open();
    /// Close it and give the keyboard back to where it was before it opened
    void close();
    bool isOpen() const { return dialog != nullptr; }

    /// The palette window while it is open, otherwise nullptr
    GtkWidget* getWidget() const { return dialog; }

    /// Every command as a person sees it, for the current state of the application (also used by the tests)
    std::vector<utn::PaletteCommand> describeCommands();

private:
    struct Entry {
        utn::PaletteCommand meta;
        std::function<bool()> available;
        std::function<void()> run;
    };

    void buildEntries();
    /// A command that runs @p action, available while that action is enabled
    void addAction(const std::string& id, const std::string& title, const std::string& category,
                   const std::string& keywords, int priority, Action action, std::function<void()> run,
                   const std::string& unavailable = {});
    void add(Entry entry);

    void filter();
    void selectRow(int delta);
    void activateEntry(const Entry* entry);
    void activateSelected();

    static gboolean onKeyPress(GtkWidget* widget, GdkEventKey* event, gpointer data);

private:
    Control* control;
    MainWindow* window;

    GtkWidget* dialog = nullptr;
    GtkWidget* search = nullptr;
    GtkWidget* list = nullptr;
    GtkWidget* status = nullptr;
    GtkWidget* previousFocus = nullptr;

    std::vector<Entry> entries;
    std::optional<Entry> pageEntry;  ///< "Go to page 12", offered while the query is a page number
    std::vector<const Entry*> shown;
    std::string lastQuery;  ///< The query `shown` was made for; the entry reports a change only after a short delay
};
