/*
 * Ultimate Teacher Notepad
 *
 * A small toolbar beside the current selection: Delete, Duplicate, Colour, Bring to front, Send to back. Each button
 * runs the same Control function as the selection controls of the properties panel; a button whose action cannot run
 * for this selection is hidden (Colour, for a picture) or greyed with the reason as its tooltip (Duplicate).
 *
 * It is a child of the main window's overlay, like the ink dock and the Blank cover, never a window of its own: the
 * Blank cover hides it, and Student View and exports draw the document, never this widget. Nothing here changes a
 * layer or who may see it.
 *
 * Based on Xournal++ GPLv2+
 */

#pragma once

#include <cstddef>   // for size_t
#include <optional>  // for optional

#include <gtk/gtk.h>

#include "model/DocumentListener.h"          // for DocumentListener
#include "model/SelectionToolbarPlacement.h"  // for ToolbarPosition

class Control;
class EditSelection;
class MainWindow;

class SelectionToolbar: public DocumentListener {
public:
    /// Settings key in the "utn" element. Absent means on.
    static constexpr const char* SETTING = "selectionToolbar";

    SelectionToolbar(MainWindow* win, Control* control, GtkOverlay* overlay);
    ~SelectionToolbar() override;
    SelectionToolbar(const SelectionToolbar&) = delete;
    SelectionToolbar& operator=(const SelectionToolbar&) = delete;

    /**
     * Show, hide and place the toolbar for the current state. Shown only in a teacher workspace, with a selection tool,
     * a selection on the current page that is on screen, the setting on, the application not busy and the Blank cover
     * down. Never takes the keyboard; if the keyboard was in the toolbar when it hides, it goes back to the page.
     */
    void update();

    bool isEnabled() const { return enabled; }
    /// Turns the toolbar on or off and remembers the choice in the settings
    void setEnabled(bool enabled);

    /// Escape in the toolbar: put it away for this selection and give the keyboard back to the page
    void dismiss();
    /// The selection went (XournalView::clearSelection): the next one may get its address, and must not stay dismissed
    void forgetDismissal() { dismissedFor = nullptr; }

    GtkWidget* getWidget() const { return widget; }
    /// The row of buttons; a column while the canvas is narrower than the row
    GtkWidget* getButtonBox() const { return box; }

    // DocumentListener
    void pageSelected(size_t page) override;

private:
    bool wanted(const EditSelection* selection) const;
    /// Where the toolbar goes for @p selection, relative to the visible canvas, or nothing if it is off screen
    std::optional<utn::ToolbarPosition> placeFor(const EditSelection* selection) const;
    void syncButtons();
    /// A row while it fits in @p width pixels of canvas, otherwise a column
    void fitTo(double width);
    void scheduleUpdate();
    void giveKeyboardToPage();

    static gboolean childPosition(GtkOverlay* overlay, GtkWidget* child, GdkRectangle* allocation, gpointer data);

    MainWindow* win;
    Control* control;
    GtkOverlay* overlay;
    GtkWidget* canvas = nullptr;
    GtkWidget* widget = nullptr;  ///< event box that takes Escape
    GtkWidget* box = nullptr;
    int rowFrame = 0;  ///< what the row adds round its buttons (padding, border), measured while it is a row
    GtkWidget* deleteButton = nullptr;
    GtkWidget* duplicateButton = nullptr;
    GtkWidget* colourButton = nullptr;
    GtkWidget* frontButton = nullptr;
    GtkWidget* backButton = nullptr;

    bool enabled = true;
    /// The selection Escape put the toolbar away for. Compared by address only, so forgotten whenever a selection goes.
    const EditSelection* dismissedFor = nullptr;
    std::optional<utn::ToolbarPosition> position;
    guint pendingUpdate = 0;
};
