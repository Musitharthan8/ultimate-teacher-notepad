/*
 * Xournal++
 *
 * The Main window
 *
 * @author Xournal++ Team
 * https://github.com/xournalpp/xournalpp
 *
 * @license GNU GPLv2 or later
 */

#pragma once

#include <array>    // for array
#include <atomic>   // for atomic_bool
#include <cstddef>  // for size_t
#include <memory>   // for unique_ptr
#include <string>   // for string
#include <vector>   // for vector

#include <gdk/gdk.h>      // for GdkDragContext, GdkEvent
#include <glib-object.h>  // for GClosure
#include <glib.h>         // for gpointer, gboolean, gint
#include <gtk/gtk.h>      // for GtkWidget, GtkCheckMenu...

#include "gui/ResponsiveLayout.h"  // for InterfaceMode
#include "util/Point.h"
#include "util/raii/GObjectSPtr.h"

#include "GladeGui.h"            // for GladeGui
#include "ToolbarDefinitions.h"  // for TOOLBAR_DEFINITIONS_LEN

class Control;
class Layout;
class SpinPageAdapter;
class ScrollHandling;
class ToolMenuHandler;
class ToolbarData;
class ToolbarModel;
class XournalView;
class PdfFloatingToolbox;
class FloatingToolbox;
class UtnToast;
class GladeSearchpath;

class Menubar;
class ToolUtnContextBar;

typedef std::array<xoj::util::WidgetSPtr, TOOLBAR_DEFINITIONS_LEN> ToolbarWidgetArray;

class MainWindow: public GladeGui {
public:
    MainWindow(GladeSearchpath* gladeSearchPath, Control* control, GtkApplication* parent);
    ~MainWindow() override;

    void populate(GladeSearchpath* gladeSearchPath);

public:
    GMenuModel* getMenuModel() const;

    void show(GtkWindow* parent) override;

    void toolbarSelected(const std::string& id);
    void toolbarSelected(ToolbarData* d);
    ToolbarData* getSelectedToolbar() const;

private:
    const ToolbarData* clearToolbar();
    void loadToolbar(ToolbarData* d);

public:
    /**
     * reloadToolbars reloads the currently selected toolbar
     *
     * This is especially useful when a change in the SettingsDialog should be reflected right away
     */
    void reloadToolbars();

    void updatePageNumbers(size_t page, size_t pagecount, size_t pdfpage);

    void setMaximized(bool maximized);
    bool isMaximized() const;

    void setFullscreen(bool enabled) const;

    bool isDarkTheme() const;

    XournalView* getXournal() const;

    void setMenubarVisible(bool visible);
    void setSidebarVisible(bool visible);
    void setToolbarVisible(bool visible);

    Control* getControl() const;

    PdfFloatingToolbox* getPdfToolbox() const;
    FloatingToolbox* getFloatingToolbox() const;

    /// UTN: short message at the bottom of the canvas; with @p offerUndo it carries an Undo button
    void showToast(const std::string& message, bool offerUndo = false);
    UtnToast* getToast() const;

    /// UTN: leave Spotlight/Curtain and show the whole page again (Escape, the banner button and the Present menu)
    void exitPresentationOverlay();
    /// UTN: banner naming the active Spotlight/Curtain with an Exit button; hidden otherwise
    void updatePresentationBanner();
    bool isPresentationBannerVisible() const;
    std::string getPresentationBannerText() const;

    /// UTN: icon-only rail and tighter chrome in narrow windows (under 1080 px)
    void updateCompactLayout(int width, bool fromAllocation = false);
    bool isCompactLayout() const { return compactLayout; }
    /// UTN: the app bar is fitted to its window by measuring it (see fitAppBar), so it adapts to the font, the display
    /// scale and the language. Call this when something changed that can change the width the bar asks for, such as
    /// the Students pill changing its word.
    void refitAppBar();
    /// The header's save/export/recovery status follows the document status model (see model/DocumentStatus.h)
    void refreshDocumentStatus() {
        const std::string before = documentHeaderStatus ? gtk_label_get_text(GTK_LABEL(documentHeaderStatus)) : "";
        updateDocumentHeader();
        const std::string after = documentHeaderStatus ? gtk_label_get_text(GTK_LABEL(documentHeaderStatus)) : "";
        if (before != after) {
            refitAppBar();  // a different status word is a different width; an ordinary edit changes nothing
        }
    }
    /// The app bar's wording is in its short form (Students pill shows one word)
    bool isAppBarTight() const { return appBarTight; }
    /// Some app-bar buttons have moved into the "More" menu
    bool isAppBarOverflowing() const { return appBarOverflowing; }
    /// UTN: the page comes first. Below ~1400 px the thumbnail sidebar steps aside, below ~960 px the settings inspector
    /// folds to its "Show settings" handle. Neither touches the saved settings, and both come back when the window
    /// widens, unless the teacher chose otherwise in between.
    bool isThumbnailsAutoHidden() const { return thumbnailsAutoHidden; }
    bool isInspectorAutoFolded() const { return inspectorAutoFolded; }
    /// UTN: tool captions under the rail icons: 0 = automatic (follows the width), 1 = show, 2 = hide.
    /// Re-applies at the last width given to updateCompactLayout().
    void setToolLabelMode(int mode);
    /// UTN: how much the interface explains itself (see ResponsiveLayout.h). Beginner shows tool names and hints, Expert
    /// icons only and no hints; both offer the same tools. Choosing one sets the tool labels once; a later by-hand
    /// label choice that differs makes the mode Custom rather than being overridden. Workspaces never change it.
    void setInterfaceMode(utn::layout::InterfaceMode mode);
    utn::layout::InterfaceMode getInterfaceMode() const { return interfaceMode; }
    int getToolLabelMode() const { return toolLabels; }

    /// Teacher workspace: 0 = inspector, 1 = rail only, 2 = canvas only.
    void setTeacherWorkspace(int mode);
    int getTeacherWorkspace() const { return teacherWorkspace; }
    int getInspectorWidth() const { return inspectorWidth; }

    void updateScrollbarSidebarPosition();

    void setUndoDescription(const std::string& description);
    void setRedoDescription(const std::string& description);

    ToolbarModel* getToolbarModel() const;
    ToolMenuHandler* getToolMenuHandler() const;

    void setDynamicallyGeneratedSubmenuDisabled(bool disabled);

    void updateToolbarMenu();
    void updateColorscheme();

    const ToolbarWidgetArray& getToolbarWidgets() const;
    const char* getToolbarName(GtkToolbar* toolbar) const;

    Layout* getLayout() const;

    [[maybe_unused]] Menubar* getMenubar() const;

    /**
     * Disable kinetic scrolling if there is a touchscreen device that was manually mapped to another enabled input
     * device class. This is required so the GtkScrolledWindow does not swallow all the events.
     */
    void setGtkTouchscreenScrollingForDeviceMapping();
    void setGtkTouchscreenScrollingEnabled(bool enabled);

    /// Infer the window's DPI from available monitor info and use it to set the default zoom value.
    void setDPI() const;

private:
    void initXournalWidget();

    void createToolbar();
    void updateTeacherWorkspace();
    void updateInspectorBounds();
    void appendWorkspaceMenu();
    void appendDocumentHeader();
    void updateDocumentHeader();
    void appendPenSettingsMenu();
    void destroyPenSettingsMenu();
    void createInkDock(GtkOverlay* overlay);
    void updateInkDock();

    /**
     * Update the position of the separator in the paned container, adjusting it to the saved sidebar width.
     * @param contentWidth should be the width of the paned container. The caller should retrieve the width
     * of the container before any modifications to it, as that will reset its allocation.
     */
    void updatePanedPosition(int contentWidth);

    /**
     * Window close Button is pressed
     */
    static bool deleteEventCallback(GtkWidget* widget, GdkEvent* event, Control* control);

    /**
     * Window is maximized/minimized
     */
    static void windowMaximizedCallback(GObject* window, GParamSpec*, MainWindow* win);

    /**
     * Callback for drag & drop files
     */
    static void dragDataRecived(GtkWidget* widget, GdkDragContext* dragContext, gint x, gint y, GtkSelectionData* data,
                                guint info, guint time, MainWindow* win);

    /**
     * Load Overall CSS file with custom icons, other styling and potentially, user changes
     */
    static void loadMainCSS(GladeSearchpath* gladeSearchPath, const gchar* cssFilename);

private:
    Control* control;

    std::unique_ptr<XournalView> xournal;
    GtkWidget* winXournal = nullptr;
    std::unique_ptr<ScrollHandling> scrollHandling;

    std::atomic_bool gtkTouchscreenScrollingEnabled{true};

    std::unique_ptr<PdfFloatingToolbox> pdfFloatingToolBox;
    std::unique_ptr<FloatingToolbox> floatingToolbox;
    std::unique_ptr<UtnToast> toast;
    GtkWidget* presentationBanner = nullptr;
    GtkWidget* presentationBannerLabel = nullptr;
    /// 0 = off, 1 = Spotlight, 2 = Curtain; lets Student View be told only when this really changes
    int presentationOverlayState = 0;
    std::shared_ptr<bool> alive = std::make_shared<bool>(true);
    bool compactLayout = false;
    bool compactHeader = false;
    bool compactLayoutDirty = true;  ///< re-apply after toolbars are rebuilt
    bool thumbnailsAway = false;         ///< width policy: the sidebar should be out of the way
    bool inspectorFolded = false;        ///< width policy: the inspector should be folded
    bool thumbnailsAutoHidden = false;   ///< the policy (not the teacher) hid the sidebar
    bool inspectorAutoFolded = false;    ///< the policy (not the teacher) folded the inspector
    bool applyingPanelPolicy = false;
    bool thumbnailsManualChoice = false;  ///< the teacher chose since the last width crossing: the policy must not undo it
    bool inspectorManualChoice = false;
    bool inkDockEnabled = false;
    bool syncingInkDock = false;
    bool panelPolicyPending = false;
    void updatePanelPolicy(int width, bool fromAllocation);
    void applyPanelPolicy();

    // App-bar fit. The bar is measured against the window and given what it needs, in this order: its normal wording, a
    // shorter wording (Students pill), then the least important buttons moved into a "More" menu. Scrolling stays as
    // the last resort only.
    void fitAppBar(int width);
    void scheduleAppBarFit();
    void showMoreMenu(const std::vector<GtkToolItem*>& moved);
    GtkWidget* appBarMore = nullptr;  ///< the "More" GtkToolItem; null until first needed, cleared when its toolbar goes
    bool appBarTight = false;
    bool appBarOverflowing = false;
    bool appBarFitPending = false;
    std::vector<GtkToolItem*> appBarMoved;  ///< what the "More" menu holds now, so an unchanged set is not rebuilt
    int appBarFitWidth = -1;  ///< width of the last fit, so an unchanged width costs nothing
    int layoutWidth = 0;             ///< last width passed to updateCompactLayout()
    // Fresh profiles start with the rail visible and the inspector available from tool popovers.
    // A previously saved workspaceTools value still overrides this default.
    int teacherWorkspace = 1;
    int inspectorWidth = 250;
    int inspectorAvailableWidth = 0;
    int toolLabels = 2;  ///< automatic, show, hide (paper-and-ink default: icon-only rail; a saved choice still wins)
    utn::layout::InterfaceMode interfaceMode = utn::layout::InterfaceMode::Custom;
    GtkWidget* beginnerModeButton = nullptr;  ///< the Workspace menu entries, marked with the current mode
    GtkWidget* expertModeButton = nullptr;
    void updateInterfaceModeButtons();
    bool inspectorApplying = false;
    bool inspectorAttached = true;
    GtkWidget* inspectorPaned = nullptr;
    GtkWidget* inspectorHost = nullptr;
    GtkWidget* railScroller = nullptr;
    GtkWidget* railHost = nullptr;
    GtkWidget* expandInspectorButton = nullptr;
    GtkWidget* workspaceMenu = nullptr;
    GtkWidget* documentHeader = nullptr;
    GtkWidget* documentHeaderTitle = nullptr;
    GtkWidget* documentHeaderStatus = nullptr;
    std::string documentHeaderSignature;  ///< what the header shows now, so an unchanged header costs no GTK work
    GtkWidget* penSettingsButton = nullptr;
    GtkWidget* penSettingsPopover = nullptr;
    GtkWidget* penSettingsFocusAnchor = nullptr;
    GtkWidget* inkDock = nullptr;
    GtkWidget* inkDockSettingsButton = nullptr;
    GtkWidget* inkDockToggle = nullptr;
    std::array<GtkWidget*, 5> inkDockSwatches{};
    std::array<GtkWidget*, 5> inkDockSizes{};
    std::array<GtkWidget*, 8> penPropertySwatches{};
    GtkWidget* penSettingsHeading = nullptr;
    GtkWidget* penWidthScale = nullptr;
    GtkWidget* penWidthLabel = nullptr;
    GtkWidget* penStrokePreview = nullptr;
    bool syncingPenSettings = false;
    std::unique_ptr<ToolUtnContextBar> penSettingsBar;
    xoj::util::WidgetSPtr penSettingsContent;
    xoj::util::WidgetSPtr inspectorPanedOwner;
    xoj::util::WidgetSPtr inspectorContentOwner;

    // Toolbars
    std::unique_ptr<ToolMenuHandler> toolbar;
    ToolbarData* selectedToolbar = nullptr;

    std::unique_ptr<Menubar> menubar;

    bool maximized = false;
    bool darkMode = false;
    bool modifiedGtkSettingsTheme = false;

    ToolbarWidgetArray toolbarWidgets;

    bool sidebarVisible = true;

    /// The last monitor the window has been moved to -- used for setting dpi
    GdkMonitor* lastMonitor = nullptr;

    xoj::util::WidgetSPtr boxContainerWidget;
    xoj::util::WidgetSPtr panedContainerWidget;
    xoj::util::WidgetSPtr mainContentWidget;
    xoj::util::WidgetSPtr sidebarWidget;
};
