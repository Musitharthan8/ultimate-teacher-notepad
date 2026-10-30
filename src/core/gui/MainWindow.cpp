#include "MainWindow.h"

#include "control/UtnLayout.h"  // for isTeacherLayout
#include "gui/UtnToast.h"        // for UtnToast
#include <regex>

#include <gdk-pixbuf/gdk-pixbuf.h>  // for gdk_pixbuf_new_fr...
#include <gdk/gdk.h>                // for gdk_screen_get_de...
#include <gio/gio.h>                // for g_cancellable_is_...
#include <gtk/gtkcssprovider.h>     // for gtk_css_provider_...

#include "control/AudioController.h"                    // for AudioController
#include "control/Control.h"                            // for Control
#include "control/ToolHandler.h"                        // for presentation overlay state
#include "control/DeviceListHelper.h"                   // for getSourceMapping
#include "control/ScrollHandler.h"                      // for ScrollHandler
#include "control/actions/ActionDatabase.h"             // for ActionDatabase
#include "control/jobs/XournalScheduler.h"              // for XournalScheduler
#include "control/layer/LayerController.h"              // for LayerController
#include "control/settings/Settings.h"                  // for Settings
#include "control/settings/SettingsEnums.h"             // for SCROLLBAR_HIDE_HO...
#include "control/zoom/ZoomControl.h"                   // for ZoomControl
#include "gui/FloatingToolbox.h"                        // for FloatingToolbox
#include "gui/GladeGui.h"                               // for GladeGui
#include "gui/PdfFloatingToolbox.h"                     // for PdfFloatingToolbox
#include "gui/ResponsiveLayout.h"                       // for nextCompactLayout, railLabelsVisible
#include "gui/PageView.h"                               // for repaintPage
#include "gui/SearchBar.h"                              // for SearchBar
#include "gui/inputdevices/InputEvents.h"               // for INPUT_DEVICE_TOUC...
#include "gui/menus/menubar/Menubar.h"                  // for Menubar
#include "gui/menus/menubar/ToolbarSelectionSubmenu.h"  // for ToolbarSelectionSubmenu
#include "gui/scroll/ScrollHandling.h"                  // for ScrollHandling
#include "gui/sidebar/Sidebar.h"                        // for Sidebar
#include "gui/toolbarMenubar/DockCustomizeDialog.h"     // for showDockCustomizeDialog
#include "gui/toolbarMenubar/ToolMenuHandler.h"         // for ToolMenuHandler
#include "gui/toolbarMenubar/ToolStudentView.h"          // for ToolStudentView
#include "gui/toolbarMenubar/ToolUtnContextBar.h"       // for ToolUtnContextBar
#include "gui/toolbarMenubar/UtnWidgets.h"              // for createColourSwatch, setAccessibleName, setSwatchSelected
#include "gui/toolbarMenubar/model/ToolbarData.h"       // for ToolbarData
#include "gui/toolbarMenubar/model/ToolbarModel.h"      // for ToolbarModel
#include "gui/widgets/SpinPageAdapter.h"                // for SpinPageAdapter
#include "gui/widgets/XournalWidget.h"                  // for gtk_xournal_get_l...
#include "util/GListView.h"                             // for GListView, GListV...
#include "util/GtkUtil.h"                               // for getWidgetDPI
#include "util/PathUtil.h"                              // for getConfigFile
#include "util/StringUtils.h"                           // for char_cast
#include "util/Util.h"                                  // for execInUiThread, npos
#include "util/XojMsgBox.h"                             // for XojMsgBox
#include "util/glib_casts.h"                            // for wrap_for_once_v
#include "util/gtk4_helper.h"                           // for gtk_widget_get_width
#include "util/i18n.h"                                  // for FS, _F
#include "util/raii/CStringWrapper.h"                   // for OwnedCString

#include "GladeSearchpath.h"     // for GladeSearchpath
#include "ToolbarDefinitions.h"  // for TOOLBAR_DEFINITIO...
#include "XournalView.h"         // for XournalView
#include "config-dev.h"          // for TOOLBAR_CONFIG
#include "filesystem.h"          // for path, exists

#ifdef __APPLE__
// the following header file contains a definition of struct Point that conflicts with model/Point.h
#define Point Point_CF
#include <CoreFoundation/CoreFoundation.h>
#undef Point
#endif

using std::string;


static void themeCallback(GObject*, GParamSpec*, gpointer data) { static_cast<MainWindow*>(data)->updateColorscheme(); }

namespace {
/// A toolbar inside a GtkScrolledWindow does not scroll to the control that takes keyboard focus (GtkToolbar is not
/// scrollable, so the viewport has no focus adjustments). Without this, Tab walks into tools that are off screen.
void scrollFocusedToolIntoView(GtkContainer* toolbar, GtkWidget* focused, gpointer data) {
    if (!focused) {
        return;
    }
    auto* scroller = GTK_SCROLLED_WINDOW(data);
    const bool vertical = gtk_orientable_get_orientation(GTK_ORIENTABLE(toolbar)) == GTK_ORIENTATION_VERTICAL;
    GtkAdjustment* adjustment = vertical ? gtk_scrolled_window_get_vadjustment(scroller) :
                                           gtk_scrolled_window_get_hadjustment(scroller);
    int x = 0;
    int y = 0;
    if (!adjustment || !gtk_widget_translate_coordinates(focused, GTK_WIDGET(toolbar), 0, 0, &x, &y)) {
        return;
    }
    const int start = vertical ? y : x;
    const int size = vertical ? gtk_widget_get_allocated_height(focused) : gtk_widget_get_allocated_width(focused);
    constexpr int MARGIN = 6;  // keep the focus ring and a little context in view
    gtk_adjustment_clamp_page(adjustment, start - MARGIN, start + size + MARGIN);
}
}  // namespace

MainWindow::MainWindow(GladeSearchpath* gladeSearchPath, Control* control, GtkApplication* parent):
        GladeGui(gladeSearchPath, "main.glade", "mainWindow"),
        control(control),
        toolbar(std::make_unique<ToolMenuHandler>(control, this)),
        menubar(std::make_unique<Menubar>()) {
    gtk_window_set_application(GTK_WINDOW(getWindow()), parent);
    g_signal_connect(this->window, "notify::title", G_CALLBACK(+[](GObject*, GParamSpec*, gpointer data) {
                         static_cast<MainWindow*>(data)->updateDocumentHeader();
                     }), this);

    panedContainerWidget.reset(get("panelMainContents"), xoj::util::ref);
    boxContainerWidget.reset(get("mainContentContainer"), xoj::util::ref);
    mainContentWidget.reset(get("boxContents"), xoj::util::ref);
    sidebarWidget.reset(get("sidebar"), xoj::util::ref);
    inspectorPaned = get("utnInspectorPaned");
    inspectorHost = get("utnInspectorHost");
    railScroller = get("tbLeft1Scroller");
    railHost = get("utnRailHost");
    expandInspectorButton = get("utnExpandInspector");
    gtk_button_set_image(GTK_BUTTON(expandInspectorButton),
                         gtk_image_new_from_icon_name("document-properties-symbolic", GTK_ICON_SIZE_BUTTON));
    gtk_button_set_always_show_image(GTK_BUTTON(expandInspectorButton), true);
    gtk_widget_set_tooltip_text(expandInspectorButton, _("Show tool settings"));
    utn::setAccessibleName(expandInspectorButton, _("Show tool settings"));
    g_signal_connect(expandInspectorButton, "clicked", G_CALLBACK(+[](GtkButton*, gpointer data) {
                         static_cast<MainWindow*>(data)->setTeacherWorkspace(0);
                     }), this);
    // Keep the pane and canvas alive while the canvas is reparented for compact workspace states.
    inspectorPanedOwner.reset(inspectorPaned, xoj::util::ref);
    inspectorContentOwner.reset(boxContainerWidget.get(), xoj::util::ref);
    auto& workspace = control->getSettings()->getCustomElement("utn");
    workspace.getInt("inspectorWidth", inspectorWidth);
    workspace.getInt("workspaceTools", teacherWorkspace);
    workspace.getInt("toolLabels", toolLabels);
    std::string savedInterfaceMode;
    workspace.getString("interfaceMode", savedInterfaceMode);  // absent in an existing profile: Custom, nothing changes
    interfaceMode = utn::layout::parseInterfaceMode(savedInterfaceMode);
    workspace.getBool("inkDockEnabled", inkDockEnabled);
    inspectorWidth = std::clamp(inspectorWidth, 150, 420);
    teacherWorkspace = std::clamp(teacherWorkspace, 0, 2);
    toolLabels = std::clamp(toolLabels, 0, 2);
    gtk_paned_set_position(GTK_PANED(inspectorPaned), inspectorWidth);
    gtk_paned_set_wide_handle(GTK_PANED(inspectorPaned), true);
    // Avoid a pane-wide tooltip that obscures the worksheet during ordinary hovering.
    g_signal_connect(get("utnCollapseInspector"), "clicked", G_CALLBACK(+[](GtkButton*, gpointer data) {
                         static_cast<MainWindow*>(data)->setTeacherWorkspace(1);
                     }), this);
    gtk_widget_add_events(inspectorPaned, GDK_BUTTON_RELEASE_MASK | GDK_KEY_RELEASE_MASK);
    auto saveInspectorWidth = +[](GtkWidget*, GdkEvent*, gpointer data) -> gboolean {
        auto* self = static_cast<MainWindow*>(data);
        if (!self->inspectorApplying && self->inspectorAttached && self->teacherWorkspace == 0 &&
            self->selectedToolbar && utn::isTeacherLayout(self->selectedToolbar->getId())) {
            const int position = gtk_paned_get_position(GTK_PANED(self->inspectorPaned));
            if (position >= 150) {
                self->inspectorWidth = position;
                self->control->getSettings()->getCustomElement("utn").setInt("inspectorWidth", position);
                self->updateInspectorBounds();
            }
        }
        return FALSE;
    };
    g_signal_connect(inspectorPaned, "button-release-event", G_CALLBACK(saveInspectorWidth), this);
    g_signal_connect(inspectorPaned, "key-release-event", G_CALLBACK(saveInspectorWidth), this);
    g_signal_connect(inspectorPaned, "size-allocate", G_CALLBACK(+[](GtkWidget*, GtkAllocation*, gpointer data) {
                         static_cast<MainWindow*>(data)->updateInspectorBounds();
                     }), this);

    loadMainCSS(gladeSearchPath, "xournalpp.css");

    // UTN interface density is independent from document rendering.
    bool touchUi = false;
    control->getSettings()->getCustomElement("utn").getBool("touchUi", touchUi);
    if (touchUi) {
        gtk_style_context_add_class(gtk_widget_get_style_context(GTK_WIDGET(this->window)), "utnTouch");
    }

    GtkOverlay* overlay = GTK_OVERLAY(get("mainOverlay"));
    this->pdfFloatingToolBox = std::make_unique<PdfFloatingToolbox>(this, overlay);
    this->floatingToolbox = std::make_unique<FloatingToolbox>(this, overlay);
    this->toast = std::make_unique<UtnToast>(overlay);
    createInkDock(overlay);

    // Spotlight/Curtain swallow page input, so a visible banner always names the active mode and offers the way out.
    presentationBanner = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 12);
    gtk_widget_add_css_class(presentationBanner, "utn-toast");
    gtk_widget_add_css_class(presentationBanner, "utn-presentation-banner");
    gtk_widget_set_halign(presentationBanner, GTK_ALIGN_CENTER);
    gtk_widget_set_valign(presentationBanner, GTK_ALIGN_START);
    gtk_widget_set_margin_top(presentationBanner, 12);
    presentationBannerLabel = gtk_label_new(nullptr);
    gtk_box_append(GTK_BOX(presentationBanner), presentationBannerLabel);
    GtkWidget* exitButton = gtk_button_new_with_label(_("Show the whole page (Esc)"));
    gtk_widget_set_focus_on_click(exitButton, false);
    utn::setAccessibleName(exitButton, _("Exit Spotlight or Curtain"));
    g_signal_connect(exitButton, "clicked", G_CALLBACK(+[](GtkButton*, gpointer data) {
                         static_cast<MainWindow*>(data)->exitPresentationOverlay();
                     }),
                     this);
    gtk_box_append(GTK_BOX(presentationBanner), exitButton);
    gtk_widget_show_all(presentationBanner);
    gtk_widget_set_no_show_all(presentationBanner, true);
    gtk_widget_hide(presentationBanner);
    gtk_overlay_add_overlay(overlay, presentationBanner);
    gtk_overlay_set_overlay_pass_through(overlay, presentationBanner, false);
    {
        std::weak_ptr<bool> weakAlive = this->alive;
        control->getToolHandler()->addToolChangedListener([this, weakAlive](ToolType) {
            if (!weakAlive.expired()) {
                updatePresentationBanner();
                updateInkDock();
            }
        });
    }

    for (size_t i = 0; i < TOOLBAR_DEFINITIONS_LEN; i++) {
        this->toolbarWidgets[i].reset(get(TOOLBAR_DEFINITIONS[i].guiName), xoj::util::ref);
    }
    // The rail and the app bar scroll when the window is small; keyboard focus must never land off screen
    g_signal_connect_after(get("tbLeft1"), "set-focus-child", G_CALLBACK(scrollFocusedToolIntoView), railScroller);
    g_signal_connect_after(get("tbTop1"), "set-focus-child", G_CALLBACK(scrollFocusedToolIntoView),
                           get("tbTop1Scroller"));

    initXournalWidget();

    setSidebarVisible(control->getSettings()->isSidebarVisible());

    // Window handler
    g_signal_connect(this->window, "delete-event", xoj::util::wrap_for_g_callback_v<deleteEventCallback>,
                     this->control);
#if GTK_MAJOR_VERSION == 3
    g_signal_connect(this->window, "notify::is-maximized", xoj::util::wrap_for_g_callback_v<windowMaximizedCallback>,
                     this);
#else
    g_signal_connect(this->window, "notify::maximized", xoj::util::wrap_for_g_callback_v<windowMaximizedCallback>,
                     this);
#endif

    g_signal_connect(
            this->window, "configure-event", G_CALLBACK(+[](GtkWidget* widget, GdkEvent*, gpointer self) -> gboolean {
                auto win = static_cast<MainWindow*>(self);
                GdkWindow* gdkWindow = gtk_widget_get_window(widget);
                GdkDisplay* display = gdkWindow ? gdk_window_get_display(gdkWindow) : nullptr;
                GdkMonitor* monitor = display ? gdk_display_get_monitor_at_window(display, gdkWindow) : nullptr;
                if (monitor && monitor != win->lastMonitor) {
                    win->lastMonitor = monitor;
                    const char* monitorName = gdk_monitor_get_model(monitor);
                    g_debug("Window moved to monitor \"%s\"", monitorName);
                    win->setDPI();
                }
                win->updateInspectorBounds();
                return false;
            }),
            this);

    g_signal_connect(this->window, "size-allocate",
                     G_CALLBACK(+[](GtkWidget*, GtkAllocation* allocation, gpointer self) {
                         auto* win = static_cast<MainWindow*>(self);
                         win->updateCompactLayout(allocation->width, true);
                     }), this);

    // "watch over" all key events; Escape is an obvious exit for temporary classroom overlays.
    // Keep native GTK handling whenever no classroom overlay is active.
    auto keyPropagate = +[](GtkWidget* w, GdkEvent* e, gpointer data) -> gboolean {
        if (e->type == GDK_KEY_PRESS &&
            static_cast<MainWindow*>(data)->workspaceShortcut(reinterpret_cast<GdkEventKey*>(e))) {
            return TRUE;
        }
        if (e->type == GDK_KEY_PRESS && reinterpret_cast<GdkEventKey*>(e)->keyval == GDK_KEY_Escape) {
            auto* self = static_cast<MainWindow*>(data);
            if (self->escapeClassroom()) {
                return TRUE;
            }
        }
        return gtk_window_propagate_key_event(GTK_WINDOW(w), reinterpret_cast<GdkEventKey*>(e));
    };
    g_signal_connect(this->window, "key-press-event", G_CALLBACK(keyPropagate), this);
    g_signal_connect(this->window, "key-release-event", G_CALLBACK(keyPropagate), this);

    updateScrollbarSidebarPosition();

    gtk_window_set_default_size(GTK_WINDOW(this->window), control->getSettings()->getMainWndWidth(),
                                control->getSettings()->getMainWndHeight());

    if (control->getSettings()->isMainWndMaximized()) {
        gtk_window_maximize(GTK_WINDOW(this->window));
    } else {
        gtk_window_unmaximize(GTK_WINDOW(this->window));
    }

    Util::execInUiThread([=]() {
        // Execute after the window is visible, else the check won't work
        control->setShowMenubar(control->getSettings()->isMenubarVisible());
    });

    // Drag and Drop
    g_signal_connect(this->window, "drag-data-received", G_CALLBACK(dragDataRecived), this);

    gtk_drag_dest_set(this->window, GTK_DEST_DEFAULT_ALL, nullptr, 0, GDK_ACTION_COPY);
    gtk_drag_dest_add_uri_targets(this->window);
    gtk_drag_dest_add_image_targets(this->window);
    gtk_drag_dest_add_text_targets(this->window);

    g_signal_connect(gtk_widget_get_settings(this->window), "notify::gtk-theme-name", G_CALLBACK(themeCallback), this);
    g_signal_connect(gtk_widget_get_settings(this->window), "notify::gtk-application-prefer-dark-theme",
                     G_CALLBACK(themeCallback), this);

    updateColorscheme();
}

void MainWindow::populate(GladeSearchpath* gladeSearchPath) {

    toolbar->populate(gladeSearchPath);
    menubar->populate(gladeSearchPath, this);

    // need to create tool buttons registered in plugins, so they can be added to toolbars
    control->registerPluginToolButtons(this->toolbar.get());

    createToolbar();

    setToolbarVisible(control->getSettings()->isToolbarVisible());
}

GMenuModel* MainWindow::getMenuModel() const { return menubar->getModel(); }

MainWindow::~MainWindow() {
    // GtkSettings is shared and can outlive this window.
    g_signal_handlers_disconnect_by_data(gtk_widget_get_settings(this->window), this);
    destroyPenSettingsMenu();
    penSettingsBar.reset();
    // Release dynamic toolbar widgets before their signal-handler owners, including detached/hidden toolbars.
    clearToolbar();
}

struct ThemeProperties {
    bool dark;
    bool darkSuffix;
    std::string rootname;  ///< Name without any putative -dark suffix
};
static ThemeProperties getThemeProperties(GtkWidget* w) {
    xoj::util::OwnedCString name;
    [[maybe_unused]] bool useEnv = false;
    // Gtk prioritizes GTK_THEME over GtkSettings content
    // cf https://gitlab.gnome.org/GNOME/gtk/blob/90d84a2af8b367bd5a5312b3fa3b67563462c0ef/gtk/gtksettings.c#L1567-L1622
    if (auto* p = g_getenv("GTK_THEME")) {
        *(name.contentReplacer()) = g_strdup(p);
        useEnv = true;
    } else {
        g_object_get(gtk_widget_get_settings(w), "gtk-theme-name", name.contentReplacer(), nullptr);
    }

    // Try to figure out if the theme is dark or light
    // Some themes handle their dark variant via "gtk-application-prefer-dark-theme" while other just append "-dark"
    const std::regex nameparser("([a-zA-Z0-9_\\.-]+?)([:-][dD]ark)?");
    std::cmatch sm;
    std::regex_match(name.get(), sm, nameparser);

    ThemeProperties props;
    if (sm.size() < 3) {
        g_warning("Fails to extract theme root name from: \"%s\"", name.get());
        props.rootname = name.get();
        props.darkSuffix = false;
    } else {
        props.rootname = sm[1];
        props.darkSuffix = sm[2].length() > 0;
    }
    gboolean dark = false;

#ifdef __APPLE__
    if (!useEnv) {
        CFStringRef interfaceStyle =
                (CFStringRef)CFPreferencesCopyAppValue(CFSTR("AppleInterfaceStyle"), kCFPreferencesCurrentApplication);
        if (interfaceStyle) {
            char buffer[128];
            if (CFStringGetCString(interfaceStyle, buffer, sizeof(buffer), kCFStringEncodingUTF8)) {
                std::string style = buffer;
                if (auto pos = style.find("Dark"); pos != std::string::npos) {
                    dark = true;
                }
            }
        }
    }
#else
    g_object_get(gtk_widget_get_settings(w), "gtk-application-prefer-dark-theme", &dark, nullptr);
#endif

    g_debug("Extracted theme info: Name = %s, rootname = %s, dark = %s", name.get(), props.rootname.c_str(),
            dark ? "true" : "false");

    props.dark = props.darkSuffix || dark;  // Some themes handle their dark variant via this setting

    return props;
}

void MainWindow::updateColorscheme() {
    g_signal_handlers_block_by_func(gtk_widget_get_settings(this->window),
                                    reinterpret_cast<gpointer>(G_CALLBACK(themeCallback)), this);
    auto variant = control->getSettings()->getThemeVariant();
    if (variant == THEME_VARIANT_USE_SYSTEM) {
        gtk_settings_reset_property(gtk_widget_get_settings(this->window), "gtk-application-prefer-dark-theme");
        if (modifiedGtkSettingsTheme) {
            // Some bug in Gtk makes an infinite loop despite us blocking the signals
            gtk_settings_reset_property(gtk_widget_get_settings(this->window), "gtk-theme-name");
            modifiedGtkSettingsTheme = false;
        }
    }
    auto props = getThemeProperties(this->window);

    const bool highContrast = variant == THEME_VARIANT_HIGH_CONTRAST;
    this->darkMode = highContrast || (props.dark && variant != THEME_VARIANT_FORCE_LIGHT) ||
                     variant == THEME_VARIANT_FORCE_DARK;

    // Set up icons
    {
        const auto themeFile = this->getGladeSearchPath()->findFile("iconsColor-light/hicolor", "index.theme");
        const auto uiPath = themeFile.parent_path().parent_path().parent_path();
        const auto lightColorIcons = (uiPath / "iconsColor-light");
        const auto darkColorIcons = (uiPath / "iconsColor-dark");
        const auto lightLucideIcons = (uiPath / "iconsLucide-light");
        const auto darkLucideIcons = (uiPath / "iconsLucide-dark");

        // icon load order from lowest priority to highest priority
        std::vector<fs::path> iconLoadOrder = {};
        const auto chosenTheme = control->getSettings()->getIconTheme();
        switch (chosenTheme) {
            case ICON_THEME_COLOR:
                iconLoadOrder = {darkLucideIcons, lightLucideIcons, darkColorIcons, lightColorIcons};
                break;
            case ICON_THEME_LUCIDE:
                iconLoadOrder = {darkColorIcons, lightColorIcons, darkLucideIcons, lightLucideIcons};
                break;
            default:
                g_message("Unknown icon theme!");
        }

        if (this->darkMode) {
            for (size_t i = 0; 2 * i + 1 < iconLoadOrder.size(); ++i) {
                std::swap(iconLoadOrder[2 * i], iconLoadOrder[2 * i + 1]);
            }
        }

        for (auto& p: iconLoadOrder) {
            gtk_icon_theme_prepend_search_path(gtk_icon_theme_get_default(), char_cast(p.u8string().c_str()));
        }
    }

    GtkStyleContext* context = gtk_widget_get_style_context(GTK_WIDGET(this->window));

    if (this->darkMode) {
        gtk_style_context_add_class(context, "darkMode");
        g_object_set(gtk_widget_get_settings(this->window), "gtk-application-prefer-dark-theme", true, nullptr);
    } else {
        gtk_style_context_remove_class(context, "darkMode");
        g_object_set(gtk_widget_get_settings(this->window), "gtk-application-prefer-dark-theme", false, nullptr);
        if (props.darkSuffix) {  // The active theme is all dark. Remove the trailing "-dark"
            g_object_set(gtk_widget_get_settings(this->window), "gtk-theme-name", props.rootname.c_str(), nullptr);
            modifiedGtkSettingsTheme = true;
        }
    }

    if (highContrast) {
        gtk_style_context_add_class(context, "utnHighContrast");
    } else {
        gtk_style_context_remove_class(context, "utnHighContrast");
    }

    {
        gchar* name = nullptr;
        g_object_get(gtk_widget_get_settings(this->window), "gtk-theme-name", &name, nullptr);
        g_debug("Theme name: %s", name);
        g_debug("Modified in GtkSettings: %s", modifiedGtkSettingsTheme ? "true" : "false");
        g_free(name);
        gboolean gtkdark = true;
        g_object_get(gtk_widget_get_settings(this->window), "gtk-application-prefer-dark-theme", &gtkdark, nullptr);
        g_debug("Theme variant: %s", gtkdark ? "dark" : "light");
        g_debug("Icon theme: %s", iconThemeToString(control->getSettings()->getIconTheme()));
    }
    g_signal_handlers_unblock_by_func(gtk_widget_get_settings(this->window), reinterpret_cast<gpointer>(themeCallback),
                                      this);
}

void MainWindow::initXournalWidget() {
    winXournal = gtk_scrolled_window_new();

    setGtkTouchscreenScrollingForDeviceMapping();

    gtk_box_append(GTK_BOX(get("boxContents")), winXournal);

    scrollHandling = std::make_unique<ScrollHandling>(GTK_SCROLLED_WINDOW(winXournal));

    this->xournal = std::make_unique<XournalView>(winXournal, control, scrollHandling.get());

    control->getZoomControl()->initZoomHandler(this->window, winXournal, xournal.get(), control);
    gtk_widget_show_all(winXournal);
}

void MainWindow::setGtkTouchscreenScrollingForDeviceMapping() {
    InputDeviceClass touchscreenClass =
            DeviceListHelper::getSourceMapping(GDK_SOURCE_TOUCHSCREEN, this->getControl()->getSettings());

    setGtkTouchscreenScrollingEnabled(touchscreenClass == INPUT_DEVICE_TOUCHSCREEN &&
                                      !control->getSettings()->getTouchDrawingEnabled());
}

void MainWindow::setGtkTouchscreenScrollingEnabled(bool enabled) {
    if (!control->getSettings()->getGtkTouchInertialScrollingEnabled()) {
        enabled = false;
    }
    gtk_scrolled_window_set_kinetic_scrolling(GTK_SCROLLED_WINDOW(winXournal), enabled);
}

auto MainWindow::getLayout() const -> Layout* { return this->xournal->getLayout(); }

auto cancellable_cancel(GCancellable* cancel) -> bool {
    g_cancellable_cancel(cancel);

    g_warning("Timeout... Cancel loading URL");

    return false;
}

void MainWindow::dragDataRecived(GtkWidget* widget, GdkDragContext* dragContext, gint x, gint y, GtkSelectionData* data,
                                 guint info, guint time, MainWindow* win) {
    GtkWidget* source = gtk_drag_get_source_widget(dragContext);
    if (source && widget == gtk_widget_get_toplevel(source)) {
        gtk_drag_finish(dragContext, false, false, time);
        return;
    }

    guchar* text = gtk_selection_data_get_text(data);
    if (text) {
        win->control->clipboardPasteText(reinterpret_cast<const char*>(text));

        g_free(text);
        gtk_drag_finish(dragContext, true, false, time);
        return;
    }

    xoj::util::GObjectSPtr<GdkPixbuf> image(gtk_selection_data_get_pixbuf(data), xoj::util::adopt);
    if (image) {
        win->control->clipboardPasteImage(image.get());

        gtk_drag_finish(dragContext, true, false, time);
        return;
    }

    gchar** uris = gtk_selection_data_get_uris(data);
    if (uris) {
        for (int i = 0; uris[i] != nullptr && i < 3; i++) {
            const char* uri = uris[i];

            GCancellable* cancel = g_cancellable_new();
            auto cancelTimeout = g_timeout_add(3000, xoj::util::wrap_for_once_v<cancellable_cancel>, cancel);

            xoj::util::GObjectSPtr<GFile> file(g_file_new_for_uri(uri), xoj::util::adopt);
            GError* err = nullptr;
            GFileInputStream* in = g_file_read(file.get(), cancel, &err);
            if (g_cancellable_is_cancelled(cancel)) {
                continue;
            }

            if (err == nullptr) {
                xoj::util::GObjectSPtr<GdkPixbuf> pixbuf(
                        gdk_pixbuf_new_from_stream(G_INPUT_STREAM(in), cancel, nullptr), xoj::util::adopt);
                if (g_cancellable_is_cancelled(cancel)) {
                    continue;
                }
                g_input_stream_close(G_INPUT_STREAM(in), cancel, nullptr);
                if (g_cancellable_is_cancelled(cancel)) {
                    continue;
                }

                if (pixbuf) {
                    win->control->clipboardPasteImage(pixbuf.get());
                }
            } else {
                g_error_free(err);
            }

            if (!g_cancellable_is_cancelled(cancel)) {
                g_source_remove(cancelTimeout);
            }
            g_object_unref(cancel);
        }

        gtk_drag_finish(dragContext, true, false, time);

        g_strfreev(uris);
    }

    gtk_drag_finish(dragContext, false, false, time);
}

auto MainWindow::getControl() const -> Control* { return control; }

void MainWindow::updateScrollbarSidebarPosition() {
    // Part 1: update scrollbar position
    if (winXournal != nullptr) {
        GtkScrolledWindow* scrolledWindow = GTK_SCROLLED_WINDOW(winXournal);

        ScrollbarHideType type = this->getControl()->getSettings()->getScrollbarHideType();

        bool scrollbarOnLeft = control->getSettings()->isScrollbarOnLeft();
        if (scrollbarOnLeft) {
            gtk_scrolled_window_set_placement(scrolledWindow, GTK_CORNER_TOP_RIGHT);
        } else {
            gtk_scrolled_window_set_placement(scrolledWindow, GTK_CORNER_TOP_LEFT);
        }

        gtk_widget_set_visible(gtk_scrolled_window_get_hscrollbar(scrolledWindow), !(type & SCROLLBAR_HIDE_HORIZONTAL));
        gtk_widget_set_visible(gtk_scrolled_window_get_vscrollbar(scrolledWindow), !(type & SCROLLBAR_HIDE_VERTICAL));

        gtk_scrolled_window_set_overlay_scrolling(scrolledWindow,
                                                  !control->getSettings()->isScrollbarFadeoutDisabled());
    }

    // Part 2: update sidebar position
    GtkPaned* paned = GTK_PANED(this->panedContainerWidget.get());

    // Allocation is reset when we switch up the contained elements. Fetch the
    // width here in case we need it afterwards.
    int contentWidth = gtk_widget_get_width(this->boxContainerWidget.get());

    bool sidebarRight = control->getSettings()->isSidebarOnRight();
    if (sidebarRight != (gtk_paned_get_child2(paned) == this->sidebarWidget.get())) {
        // switch sidebar and main content
        GtkWidget* sidebar = this->sidebarWidget.get();
        GtkWidget* mainContent = this->sidebarVisible ? this->mainContentWidget.get() : nullptr;
#if GTK_MAJOR_VERSION == 3
        if (this->sidebarVisible) {
            gtk_container_remove(GTK_CONTAINER(paned), sidebar);
            gtk_container_remove(GTK_CONTAINER(paned), mainContent);

            if (sidebarRight) {
                gtk_paned_pack1(paned, mainContent, true, false);
                gtk_paned_pack2(paned, sidebar, false, false);
            } else {
                gtk_paned_pack1(paned, sidebar, false, false);
                gtk_paned_pack2(paned, mainContent, true, false);
            }
        } else {
            // The sidebar is hidden. That means the paned widget only contains the
            // sidebar while the main contents are shown alone in the box container.
            gtk_container_remove(GTK_CONTAINER(paned), sidebar);

            if (sidebarRight) {
                gtk_paned_pack2(paned, sidebar, false, false);
            } else {
                gtk_paned_pack1(paned, sidebar, false, false);
            }
        }
#else
        gtk_paned_set_start_child(paned, nullptr);
        gtk_paned_set_end_child(paned, nullptr);
        if (sidebarRight) {
            gtk_paned_set_start_child(paned, mainContent);
            gtk_paned_set_resize_start_child(paned, true);
            gtk_paned_set_shrink_start_child(paned, false);
            gtk_paned_set_end_child(paned, sidebar);
            gtk_paned_set_resize_end_child(paned, false);
            gtk_paned_set_shrink_end_child(paned, false);
        } else {
            gtk_paned_set_end_child(paned, mainContent);
            gtk_paned_set_resize_end_child(paned, true);
            gtk_paned_set_shrink_end_child(paned, false);
            gtk_paned_set_start_child(paned, sidebar);
            gtk_paned_set_resize_start_child(paned, false);
            gtk_paned_set_shrink_start_child(paned, false);
        }
#endif
    }

    if (this->sidebarVisible) {
        updatePanedPosition(contentWidth);
    }
}

auto MainWindow::deleteEventCallback(GtkWidget* widget, GdkEvent* event, Control* control) -> bool {
    control->quit();

    return true;
}

void MainWindow::setSidebarVisible(bool visible) {
    if (!applyingPanelPolicy) {
        thumbnailsAutoHidden = false;  // a teacher's choice always wins over the width policy
        thumbnailsManualChoice = true;  // ... including over a policy change that is still queued
    }
    if (!visible && (this->control->getSidebar() != nullptr)) {
        this->control->getSidebar()->saveSize();
    }

    if (visible != this->sidebarVisible) {
        // Due to a GTK bug, we can't just hide the sidebar widget in the GtkPaned.
        // If we do this, we create a dead region where the pane separator was previously.
        // In this region, we can't use the touchscreen to start horizontal strokes.
        // As such:
        if (!visible) {
            // hide sidebar
#if GTK_MAJOR_VERSION == 3
            gtk_container_remove(GTK_CONTAINER(panedContainerWidget.get()), mainContentWidget.get());
#else
            if (control->getSettings()->isSidebarOnRight()) {
                gtk_paned_set_start_child(GTK_PANED(panedContainerWidget.get()), nullptr);
            } else {
                gtk_paned_set_end_child(GTK_PANED(panedContainerWidget.get()), nullptr);
            }
#endif
            gtk_box_remove(GTK_BOX(boxContainerWidget.get()), panedContainerWidget.get());
            gtk_box_append(GTK_BOX(boxContainerWidget.get()), mainContentWidget.get());
            this->sidebarVisible = false;
        } else {
            // show sidebar

            // Allocation is reset when we switch up the contained elements. Fetch the
            // width here in case we need it afterwards.
            int contentWidth = gtk_widget_get_width(boxContainerWidget.get());

            gtk_box_remove(GTK_BOX(boxContainerWidget.get()), mainContentWidget.get());

#if GTK_MAJOR_VERSION == 3
            if (control->getSettings()->isSidebarOnRight()) {
                gtk_paned_pack1(GTK_PANED(panedContainerWidget.get()), mainContentWidget.get(), true, false);
            } else {
                gtk_paned_pack2(GTK_PANED(panedContainerWidget.get()), mainContentWidget.get(), true, false);
            }
#else
            if (control->getSettings()->isSidebarOnRight()) {
                gtk_paned_set_start_child(GTK_PANED(panedContainerWidget.get()), mainContentWidget.get());
            } else {
                gtk_paned_set_end_child(GTK_PANED(panedContainerWidget.get()), mainContentWidget.get());
            }
#endif

            gtk_box_append(GTK_BOX(boxContainerWidget.get()), panedContainerWidget.get());
            this->sidebarVisible = true;

            updatePanedPosition(contentWidth);
        }
    }

    gtk_widget_set_visible(sidebarWidget.get(), visible);
}

/**
 * Invert the position of the paned widget and disconnect from the signal.
 * @param handlerId should be the ID of the signal handler that should be disconnected.
 */
static void invertPanedPosition(GtkWidget* widget, GtkAllocation* allocation, gulong* handlerId) {
    int newDividerPos = allocation->width - gtk_paned_get_position(GTK_PANED(widget));
    gtk_paned_set_position(GTK_PANED(widget), newDividerPos);

    // We only need to switch the position once, so disconnect the signal right away.
    g_signal_handler_disconnect(widget, *handlerId);
}

void MainWindow::updatePanedPosition(int contentWidth) {
    if (!this->control->getSettings()->isSidebarOnRight()) {
        // Sidebar is on the left side.
        gtk_paned_set_position(GTK_PANED(this->panedContainerWidget.get()),
                               this->control->getSettings()->getSidebarWidth());
    } else {
        // Sidebar is on the right side.
        if (contentWidth > 0) {
            int dividerPos = contentWidth - this->control->getSettings()->getSidebarWidth();
            gtk_paned_set_position(GTK_PANED(this->panedContainerWidget.get()), dividerPos);
        } else {
            // Allocation is unkown (window hasn't been shown yet). We have to wait for the signal.
            // Set position as if the sidebar was on the left side, and let the signal handler
            // simply invert the position when the allocation is known.
            gtk_paned_set_position(GTK_PANED(this->panedContainerWidget.get()),
                                   this->control->getSettings()->getSidebarWidth());
            gulong* signal_id = new gulong{};
            *signal_id = g_signal_connect_data(
                    this->panedContainerWidget.get(), "size-allocate",
                    xoj::util::wrap_for_g_callback_v<invertPanedPosition>, signal_id,
                    [](gpointer d, GClosure*) { delete reinterpret_cast<gulong*>(d); }, GConnectFlags(0));
        }
    }
}

void MainWindow::setToolbarVisible(bool visible) {
    Settings* settings = control->getSettings();

    settings->setToolbarVisible(visible);
    for (auto& w: this->toolbarWidgets) {
        if (!visible || (gtk_toolbar_get_n_items(GTK_TOOLBAR(w.get())) != 0)) {
            gtk_widget_set_visible(w.get(), visible);
        }
    }
    updateTeacherWorkspace();
}

void MainWindow::setMenubarVisible(bool visible) {
    gtk_application_window_set_show_menubar(GTK_APPLICATION_WINDOW(this->getWindow()), visible);
}

void MainWindow::setMaximized(bool maximized) { this->maximized = maximized; }

auto MainWindow::isMaximized() const -> bool { return this->maximized; }

auto MainWindow::setFullscreen(bool enabled) const -> void {
    if (enabled) {
        gtk_window_fullscreen(GTK_WINDOW(this->getWindow()));
    } else {
        gtk_window_unfullscreen(GTK_WINDOW(this->getWindow()));
    }
}

auto MainWindow::isDarkTheme() const -> bool { return this->darkMode; }

auto MainWindow::getXournal() const -> XournalView* { return xournal.get(); }

auto MainWindow::windowMaximizedCallback(GObject* window, GParamSpec*, MainWindow* win) -> void {
    win->setMaximized(gtk_window_is_maximized(GTK_WINDOW(window)));
}

void MainWindow::toolbarSelected(const std::string& id) {
    const auto& toolbars = toolbar->getModel()->getToolbars();
    auto it = std::find_if(toolbars.begin(), toolbars.end(), [&](const auto& d) { return d->getId() == id; });
    toolbarSelected(it == toolbars.end() ? nullptr : it->get());
}

void MainWindow::toolbarSelected(ToolbarData* d) {
    if (!d || this->selectedToolbar == d) {
        return;
    }

    Settings* settings = control->getSettings();
    // Present remembers where it was entered from and starts with its tools out of the way. A reload of the same layout
    // (the toolbars are rebuilt when the dock changes) arrives with no selected toolbar and keeps both.
    const std::string previous = this->selectedToolbar ? this->selectedToolbar->getId() : std::string();
    if (d->getId() != "UTN Present") {
        presentToolsRevealed = false;
    } else if (!previous.empty() && previous != "UTN Present") {
        presentToolsRevealed = false;
        // A layout that is not a teacher workspace is no place to return to
        workspaceBeforePresent = utn::isTeacherLayout(previous) ? previous : std::string();
    }
    settings->setSelectedToolbar(d->getId());

    destroyPenSettingsMenu();
    this->clearToolbar();
    this->loadToolbar(d);
    appendWorkspaceMenu();
    updateTeacherWorkspace();

    // UTN uses a dedicated shell style while Classic layouts stay untouched.
    GtkStyleContext* context = gtk_widget_get_style_context(GTK_WIDGET(this->window));
    const bool utnShell = utn::isTeacherLayout(d->getId());
    this->compactLayoutDirty = true;
    updateCompactLayout(gtk_widget_get_allocated_width(GTK_WIDGET(this->window)));
    // Tool state follows the layout: teacher layouts never keep hidden shape modes on Highlight.
    control->getToolHandler()->setTeacherToolPolicy(utnShell);
    control->getToolHandler()->fireToolChanged();
    if (utnShell) {
        gtk_style_context_add_class(context, "utnShell");
    } else {
        gtk_style_context_remove_class(context, "utnShell");
    }
    if (utnShell && d->getId() == "UTN Present") {
        gtk_style_context_add_class(context, "utnPresent");
    } else {
        gtk_style_context_remove_class(context, "utnPresent");
    }
}

auto MainWindow::clearToolbar() -> const ToolbarData* {
    // The "More" button is a child of the app bar and goes with it
    this->appBarMore = nullptr;
    this->beginnerModeButton = nullptr;  // the Workspace menu goes with the toolbar
    this->expertModeButton = nullptr;
    this->appBarMoved.clear();
    this->appBarTight = false;
    this->appBarOverflowing = false;
    this->appBarFitWidth = -1;
    if (this->selectedToolbar != nullptr) {
        for (size_t i = 0; i < TOOLBAR_DEFINITIONS_LEN; i++) {
            ToolMenuHandler::unloadToolbar(this->toolbarWidgets[i].get());
        }

        this->toolbar->freeDynamicToolbarItems();
    }
    return std::exchange(this->selectedToolbar, nullptr);
}

void MainWindow::loadToolbar(ToolbarData* d) {
    this->selectedToolbar = d;

    for (size_t i = 0; i < TOOLBAR_DEFINITIONS_LEN; i++) {
        this->toolbar->load(d, this->toolbarWidgets[i].get(), TOOLBAR_DEFINITIONS[i].propName,
                            TOOLBAR_DEFINITIONS[i].horizontal);
    }

    this->floatingToolbox->flagRecalculateSizeRequired();
}

void MainWindow::appendDocumentHeader() {
    GtkToolbar* top = GTK_TOOLBAR(this->toolbarWidgets[0].get());
    for (int i = 0; i < gtk_toolbar_get_n_items(top); ++i) {
        GtkToolItem* current = gtk_toolbar_get_nth_item(top, i);
        GtkWidget* child = gtk_bin_get_child(GTK_BIN(current));
        if (!child || g_strcmp0(gtk_widget_get_name(child), "utnBrand") != 0) {
            continue;
        }

        GtkToolItem* item = gtk_tool_item_new();
        GtkWidget* content = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
        gtk_widget_add_css_class(content, "utn-document-header");
        GtkWidget* title = gtk_label_new("");
        gtk_widget_set_name(title, "utnDocumentTitle");
        gtk_widget_add_css_class(title, "utn-document-title");
        gtk_label_set_ellipsize(GTK_LABEL(title), PANGO_ELLIPSIZE_END);
        gtk_label_set_max_width_chars(GTK_LABEL(title), 28);
        GtkWidget* status = gtk_label_new("");
        gtk_widget_set_name(status, "utnDocumentSaveStatus");
        gtk_widget_add_css_class(status, "utn-document-status");
        gtk_box_append(GTK_BOX(content), title);
        // The status is a button, so a keyboard reaches it and its details are not tooltip-only
        GtkWidget* statusButton = gtk_menu_button_new();
        gtk_button_set_relief(GTK_BUTTON(statusButton), GTK_RELIEF_NONE);
        gtk_widget_set_focus_on_click(statusButton, false);
        gtk_widget_add_css_class(statusButton, "utn-status-button");
        gtk_container_add(GTK_CONTAINER(statusButton), status);
        GtkWidget* statusPopover = gtk_popover_new(nullptr);
        g_signal_connect(statusPopover, "show", G_CALLBACK(+[](GtkWidget* popover, gpointer data) {
                             auto* self = static_cast<MainWindow*>(data);
                             if (GtkWidget* old = gtk_bin_get_child(GTK_BIN(popover))) {
                                 gtk_widget_destroy(old);
                             }
                             const utn::StatusSummary summary = utn::summarise(self->control->getDocumentStatus().get());
                             GtkWidget* panel = gtk_box_new(GTK_ORIENTATION_VERTICAL, 6);
                             gtk_widget_set_margin_start(panel, 12);
                             gtk_widget_set_margin_end(panel, 12);
                             gtk_widget_set_margin_top(panel, 12);
                             gtk_widget_set_margin_bottom(panel, 12);
                             GtkWidget* heading = gtk_label_new(summary.label.c_str());
                             gtk_widget_set_halign(heading, GTK_ALIGN_START);
                             gtk_widget_add_css_class(heading, "utn-context-title");
                             gtk_box_append(GTK_BOX(panel), heading);
                             for (const std::string& line: summary.details) {
                                 GtkWidget* detail = gtk_label_new(line.c_str());
                                 gtk_label_set_line_wrap(GTK_LABEL(detail), true);
                                 gtk_label_set_max_width_chars(GTK_LABEL(detail), 44);
                                 gtk_widget_set_halign(detail, GTK_ALIGN_START);
                                 gtk_box_append(GTK_BOX(panel), detail);
                             }
                             if (summary.offersSave) {
                                 // The way forward, where the problem is told: the same action as the Save button
                                 GtkWidget* save = gtk_button_new_with_label(_("Save now"));
                                 gtk_actionable_set_action_name(GTK_ACTIONABLE(save), "win.save");
                                 utn::setAccessibleName(save, _("Save now"));
                                 gtk_box_append(GTK_BOX(panel), save);
                             }
                             gtk_container_add(GTK_CONTAINER(popover), panel);
                             gtk_widget_show_all(panel);
                         }), this);
        gtk_menu_button_set_popover(GTK_MENU_BUTTON(statusButton), statusPopover);
        gtk_box_append(GTK_BOX(content), statusButton);
        documentHeaderStatusButton = statusButton;
        gtk_widget_set_tooltip_text(content, _("Document name and save status"));
        gtk_container_add(GTK_CONTAINER(item), content);
        gtk_tool_item_set_homogeneous(item, false);
        gtk_tool_item_set_expand(item, true);
        gtk_toolbar_insert(top, item, i + 1);
        documentHeader = GTK_WIDGET(item);
        documentHeaderTitle = title;
        documentHeaderStatus = status;
        g_signal_connect(item, "destroy", G_CALLBACK(+[](GtkWidget* widget, gpointer data) {
                             auto* self = static_cast<MainWindow*>(data);
                             if (self->documentHeader == widget) {
                                 self->documentHeader = nullptr;
                                 self->documentHeaderTitle = nullptr;
                                 self->documentHeaderStatus = nullptr;
                                 self->documentHeaderStatusButton = nullptr;
                                 self->documentHeaderSignature.clear();  // a rebuilt header starts from nothing
                             }
                         }), this);
        gtk_widget_show_all(GTK_WIDGET(item));
        gtk_widget_set_visible(title, !compactLayout);
        updateDocumentHeader();
        return;
    }
}

void MainWindow::updateDocumentHeader() {
    if (!documentHeaderTitle || !documentHeaderStatus) {
        return;
    }
    const char* rawTitle = gtk_window_get_title(GTK_WINDOW(window));
    std::string title = rawTitle ? rawTitle : _("Untitled document");
    const size_t appSuffix = title.rfind(" - Ultimate Teacher Notepad");
    if (appSuffix != std::string::npos) {
        title.resize(appSuffix);
    }
    const size_t pagePrefix = title.find("]  ");
    if (!title.empty() && title.front() == '[' && pagePrefix != std::string::npos) {
        title.erase(0, pagePrefix + 3);
    }
    if (!title.empty() && title.front() == '*') {
        title.erase(0, 1);
    }
    const size_t pathPrefix = title.find("] - ");
    if (!title.empty() && title.front() == '[' && pathPrefix != std::string::npos) {
        title.erase(0, pathPrefix + 4);
    }
    // The status is not read from the title: it comes from what saving, exporting and recovery really did
    const utn::StatusSummary summary = utn::summarise(control->getDocumentStatus().get());
    // A calm saved document shows a tick only; anything else says what it is, in a word and a shape
    const std::string text =
            summary.state == utn::StatusState::Saved ? summary.glyph : summary.glyph + " " + summary.label;
    std::string tooltip = summary.label;
    for (const std::string& line: summary.details) {
        tooltip += "\n" + line;
    }

    // This runs on every edit (the title is refreshed each time), and restyling and relabelling cost GTK work:
    // touch nothing unless what is shown has changed
    const std::string signature = title + '\n' + text + '\n' + summary.cssClass + '\n' + tooltip;
    if (signature == documentHeaderSignature) {
        return;
    }
    documentHeaderSignature = signature;

    gtk_label_set_text(GTK_LABEL(documentHeaderTitle), title.c_str());
    gtk_widget_set_tooltip_text(documentHeaderTitle, title.c_str());
    GtkStyleContext* context = gtk_widget_get_style_context(documentHeaderStatus);
    for (const char* cls: {"utn-status-ok", "utn-status-info", "utn-status-warning", "utn-status-error"}) {
        gtk_style_context_remove_class(context, cls);
    }
    gtk_style_context_add_class(context, summary.cssClass.c_str());
    gtk_label_set_text(GTK_LABEL(documentHeaderStatus), text.c_str());
    gtk_widget_set_tooltip_text(documentHeaderStatus, tooltip.c_str());
    utn::setAccessibleName(documentHeaderStatus, summary.accessibleName.c_str());
    if (documentHeaderStatusButton) {
        utn::setAccessibleName(documentHeaderStatusButton, summary.accessibleName.c_str());
        gtk_widget_set_tooltip_text(documentHeaderStatusButton, tooltip.c_str());
    }
}

void MainWindow::appendWorkspaceMenu() {
    GtkToolbar* top = GTK_TOOLBAR(this->toolbarWidgets[0].get());
    appendDocumentHeader();
    GtkToolItem* item = gtk_tool_item_new();
    GtkWidget* menu = gtk_menu_button_new();
    gtk_button_set_label(GTK_BUTTON(menu), _("Workspace"));
    // Icon beside the name, so compact windows can drop the word and keep the control
    gtk_button_set_image(GTK_BUTTON(menu), gtk_image_new_from_icon_name("view-dual-symbolic", GTK_ICON_SIZE_LARGE_TOOLBAR));
    gtk_button_set_always_show_image(GTK_BUTTON(menu), true);
    gtk_widget_set_tooltip_text(menu, _("Show or hide classroom tools and choose tool labels"));
    utn::setAccessibleName(menu, _("Workspace layout"));
    GtkWidget* popover = gtk_popover_new();
    GtkWidget* panel = gtk_box_new(GTK_ORIENTATION_VERTICAL, 4);
    gtk_widget_set_margin_start(panel, 8);
    gtk_widget_set_margin_end(panel, 8);
    gtk_widget_set_margin_top(panel, 8);
    gtk_widget_set_margin_bottom(panel, 8);
    const std::array<const char*, 3> workspaceLabels{{_("Tools and settings"), _("Tool rail only"), _("Canvas only")}};
    for (int mode = 0; mode < static_cast<int>(workspaceLabels.size()); ++mode) {
        GtkWidget* button = gtk_button_new_with_label(workspaceLabels[static_cast<size_t>(mode)]);
        g_object_set_data(G_OBJECT(button), "utn-workspace-mode", GINT_TO_POINTER(mode));
        g_signal_connect(button, "clicked", G_CALLBACK(+[](GtkButton* button, gpointer data) {
                             auto* self = static_cast<MainWindow*>(data);
                             self->setTeacherWorkspace(GPOINTER_TO_INT(
                                     g_object_get_data(G_OBJECT(button), "utn-workspace-mode")));
                             gtk_popover_popdown(GTK_POPOVER(gtk_widget_get_parent(gtk_widget_get_parent(
                                     GTK_WIDGET(button)))));
                         }), this);
        utn::setAccessibleName(button, workspaceLabels[static_cast<size_t>(mode)]);
        gtk_box_append(GTK_BOX(panel), button);
    }
    gtk_box_append(GTK_BOX(panel), gtk_separator_new(GTK_ORIENTATION_HORIZONTAL));
    GtkWidget* labelsTitle = gtk_label_new(_("Tool labels"));
    gtk_widget_set_halign(labelsTitle, GTK_ALIGN_START);
    gtk_widget_add_css_class(labelsTitle, "utn-context-title");
    gtk_box_append(GTK_BOX(panel), labelsTitle);
    const std::array<const char*, 3> labelChoices{{_("Automatic"), _("Show"), _("Hide")}};
    for (int mode = 0; mode < static_cast<int>(labelChoices.size()); ++mode) {
        GtkWidget* button = gtk_button_new_with_label(labelChoices[static_cast<size_t>(mode)]);
        g_object_set_data(G_OBJECT(button), "utn-label-mode", GINT_TO_POINTER(mode));
        g_signal_connect(button, "clicked", G_CALLBACK(+[](GtkButton* button, gpointer data) {
                             auto* self = static_cast<MainWindow*>(data);
                             self->setToolLabelMode(GPOINTER_TO_INT(g_object_get_data(G_OBJECT(button), "utn-label-mode")));
                         }), this);
        utn::setAccessibleName(button, labelChoices[static_cast<size_t>(mode)]);
        gtk_box_append(GTK_BOX(panel), button);
    }
    gtk_box_append(GTK_BOX(panel), gtk_separator_new(GTK_ORIENTATION_HORIZONTAL));
    GtkWidget* interfaceTitle = gtk_label_new(_("Interface"));
    gtk_widget_set_halign(interfaceTitle, GTK_ALIGN_START);
    gtk_widget_add_css_class(interfaceTitle, "utn-context-title");
    gtk_box_append(GTK_BOX(panel), interfaceTitle);
    // Same tools and actions in both; only how much the screen explains. A mode sets the tool labels, and a label
    // choice made by hand afterwards is kept.
    for (const utn::layout::InterfaceMode choice: {utn::layout::InterfaceMode::Beginner, utn::layout::InterfaceMode::Expert}) {
        GtkWidget* button = gtk_button_new();
        gtk_widget_set_tooltip_text(button, choice == utn::layout::InterfaceMode::Beginner ?
                                                    _("Show tool names and short explanations under the settings") :
                                                    _("Icons only and no explanations: for teachers who know the tools"));
        g_object_set_data(G_OBJECT(button), "utn-interface-mode", GINT_TO_POINTER(static_cast<int>(choice)));
        g_signal_connect(button, "clicked", G_CALLBACK(+[](GtkButton* button, gpointer data) {
                             static_cast<MainWindow*>(data)->setInterfaceMode(static_cast<utn::layout::InterfaceMode>(
                                     GPOINTER_TO_INT(g_object_get_data(G_OBJECT(button), "utn-interface-mode"))));
                         }), this);
        (choice == utn::layout::InterfaceMode::Beginner ? beginnerModeButton : expertModeButton) = button;
        g_signal_connect(button, "destroy", G_CALLBACK(+[](GtkWidget* widget, gpointer data) {
                             auto* self = static_cast<MainWindow*>(data);
                             if (self->beginnerModeButton == widget) {
                                 self->beginnerModeButton = nullptr;
                             }
                             if (self->expertModeButton == widget) {
                                 self->expertModeButton = nullptr;
                             }
                         }), this);
        gtk_box_append(GTK_BOX(panel), button);
    }
    updateInterfaceModeButtons();
    gtk_box_append(GTK_BOX(panel), gtk_separator_new(GTK_ORIENTATION_HORIZONTAL));
    GtkWidget* customiseDock = gtk_button_new_with_label(_("Customise tool dock…"));
    gtk_widget_set_tooltip_text(customiseDock, _("Pin, hide and reorder the tools on the left"));
    utn::setAccessibleName(customiseDock, _("Customise tool dock"));
    g_signal_connect(customiseDock, "clicked", G_CALLBACK(+[](GtkButton* button, gpointer data) {
                         if (GtkWidget* open = gtk_widget_get_ancestor(GTK_WIDGET(button), GTK_TYPE_POPOVER)) {
                             gtk_popover_popdown(GTK_POPOVER(open));
                         }
                         utn::showDockCustomizeDialog(static_cast<MainWindow*>(data)->getControl());
                     }), this);
    gtk_box_append(GTK_BOX(panel), customiseDock);
    // A popover's content is not shown by showing the toolbar item that owns it: without this the menu opens as an empty
    // bubble
    gtk_widget_show_all(panel);
    gtk_container_add(GTK_CONTAINER(popover), panel);
    gtk_menu_button_set_popover(GTK_MENU_BUTTON(menu), popover);
    gtk_container_add(GTK_CONTAINER(item), menu);
    gtk_toolbar_insert(top, item, -1);
    workspaceMenu = menu;
    gtk_widget_show_all(GTK_WIDGET(item));
    appendPenSettingsMenu();
}

void MainWindow::destroyPenSettingsMenu() {
    // Clearing these first prevents the popover's "closed" callback from
    // trying to restore focus into a toolbar that is being rebuilt/destroyed.
    penSettingsButton = nullptr;
    penSettingsFocusAnchor = nullptr;
    inkDockToggle = nullptr;
    if (penSettingsPopover) {
        gtk_widget_destroy(penSettingsPopover);
        penSettingsPopover = nullptr;
    }
    penSettingsHeading = nullptr;
    penWidthScale = nullptr;
    penWidthLabel = nullptr;
    penStrokePreview = nullptr;
    penPropertySwatches.fill(nullptr);
    penSettingsContent.reset();
    // penSettingsBar lives as long as the window: the tool handler keeps its listener, so a new bar per reload would add one more each time
}

void MainWindow::appendPenSettingsMenu() {
    GtkToolbar* top = GTK_TOOLBAR(this->toolbarWidgets[0].get());
    GtkToolItem* item = gtk_tool_item_new();
    GtkWidget* button = gtk_menu_button_new();
    gtk_widget_set_name(button, "utnPenSettingsButton");
    gtk_widget_set_focus_on_click(button, false);
    gtk_button_set_image(GTK_BUTTON(button), gtk_image_new_from_icon_name("xopp-tool-pencil", GTK_ICON_SIZE_LARGE_TOOLBAR));
    gtk_button_set_always_show_image(GTK_BUTTON(button), true);
    gtk_widget_set_tooltip_text(button, _("Pen settings"));
    utn::setAccessibleName(button, _("Pen settings"));

    GtkWidget* popover = gtk_popover_new();
    gtk_widget_add_css_class(popover, "utn-paper-popover");
    gtk_popover_set_relative_to(GTK_POPOVER(popover), button);
    GtkWidget* card = gtk_box_new(GTK_ORIENTATION_VERTICAL, 8);
    gtk_widget_add_css_class(card, "utn-paper-card");
    gtk_widget_set_margin_start(card, 12);
    gtk_widget_set_margin_end(card, 12);
    gtk_widget_set_margin_top(card, 12);
    gtk_widget_set_margin_bottom(card, 12);
    GtkWidget* heading = gtk_label_new(_("Pen settings"));
    gtk_widget_add_css_class(heading, "utn-paper-heading");
    gtk_widget_set_halign(heading, GTK_ALIGN_START);
    gtk_box_append(GTK_BOX(card), heading);
    penSettingsHeading = heading;

    if (!penSettingsBar) {
        penSettingsBar = std::make_unique<ToolUtnContextBar>("UTN_PEN_PROPERTIES_POPOVER", control);
    }
    GtkWidget* hint = gtk_label_new(_("Applies to new strokes"));
    gtk_widget_add_css_class(hint, "utn-paper-hint");
    gtk_widget_set_halign(hint, GTK_ALIGN_START);
    gtk_box_append(GTK_BOX(card), hint);

    GtkWidget* colourTitle = gtk_label_new(_("Ink colour"));
    gtk_widget_add_css_class(colourTitle, "utn-context-title");
    gtk_widget_set_halign(colourTitle, GTK_ALIGN_START);
    gtk_box_append(GTK_BOX(card), colourTitle);
    GtkWidget* colourGrid = gtk_grid_new();
    gtk_grid_set_row_spacing(GTK_GRID(colourGrid), 6);
    gtk_grid_set_column_spacing(GTK_GRID(colourGrid), 8);
    gtk_widget_add_css_class(colourGrid, "utn-paper-palette");
    const std::array<std::pair<const char*, Color>, 8> propertyColours{{
            {N_("Ink"), Color{0x20U, 0x25U, 0x2BU}}, {N_("Blue"), Color{0x1EU, 0x5BU, 0xD8U}},
            {N_("Red"), Color{0xD3U, 0x2FU, 0x2FU}}, {N_("Green"), Color{0x2EU, 0x7DU, 0x32U}},
            {N_("Purple"), Color{0x6AU, 0x1BU, 0x9AU}}, {N_("Orange"), Color{0xEFU, 0x6CU, 0x00U}},
            {N_("Gold"), Color{0xC0U, 0x8BU, 0x00U}}, {N_("Teal"), Color{0x00U, 0x7FU, 0x73U}},
    }};
    for (size_t i = 0; i < propertyColours.size(); ++i) {
        const auto& [name, colour] = propertyColours[i];
        GtkWidget* swatch = utn::createColourSwatch(colour, _(name));
        gtk_widget_set_size_request(swatch, 34, 34);
        gtk_widget_set_focus_on_click(swatch, false);
        g_object_set_data_full(G_OBJECT(swatch), "utn-property-colour", new Color(colour),
                               +[](gpointer data) { delete static_cast<Color*>(data); });
        g_signal_connect(swatch, "clicked", G_CALLBACK(+[](GtkButton* button, gpointer data) {
                             auto* self = static_cast<MainWindow*>(data);
                             auto* colour = static_cast<Color*>(g_object_get_data(G_OBJECT(button), "utn-property-colour"));
                             self->control->getToolHandler()->setColor(*colour, true);
                             self->updateInkDock();
                         }), this);
        penPropertySwatches[i] = swatch;
        gtk_grid_attach(GTK_GRID(colourGrid), swatch, static_cast<int>(i % 4), static_cast<int>(i / 4), 1, 1);
    }
    gtk_box_append(GTK_BOX(card), colourGrid);

    GtkWidget* widthTitle = gtk_label_new(_("Stroke width"));
    gtk_widget_add_css_class(widthTitle, "utn-context-title");
    gtk_widget_set_halign(widthTitle, GTK_ALIGN_START);
    gtk_box_append(GTK_BOX(card), widthTitle);
    GtkWidget* widthRow = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    penWidthScale = gtk_scale_new_with_range(GTK_ORIENTATION_HORIZONTAL, 0, 4, 1);
    gtk_scale_set_draw_value(GTK_SCALE(penWidthScale), false);
    gtk_widget_set_size_request(penWidthScale, 220, 32);
    gtk_widget_set_hexpand(penWidthScale, true);
    gtk_widget_set_name(penWidthScale, "utnPenWidthScale");
    utn::setAccessibleName(penWidthScale, _("Pen stroke width"));
    penWidthLabel = gtk_label_new("");
    gtk_widget_add_css_class(penWidthLabel, "utn-paper-width-label");
    gtk_box_append(GTK_BOX(widthRow), penWidthScale);
    gtk_box_append(GTK_BOX(widthRow), penWidthLabel);
    gtk_box_append(GTK_BOX(card), widthRow);
    g_signal_connect(penWidthScale, "value-changed", G_CALLBACK(+[](GtkRange* range, gpointer data) {
                         auto* self = static_cast<MainWindow*>(data);
                         if (self->syncingPenSettings || !self->control->getActionDatabase()) return;
                         self->control->getToolHandler()->setPenSize(
                                 static_cast<ToolSize>(static_cast<int>(std::lround(gtk_range_get_value(range)))));
                         self->updateInkDock();
                     }), this);

    penStrokePreview = gtk_drawing_area_new();
    gtk_widget_set_name(penStrokePreview, "utnPenStrokePreview");
    gtk_widget_set_size_request(penStrokePreview, 280, 42);
    gtk_widget_add_css_class(penStrokePreview, "utn-paper-stroke-preview");
    gtk_widget_set_tooltip_text(penStrokePreview, _("Preview of the selected pen colour and width"));
    gtk_drawing_area_set_draw_func(GTK_DRAWING_AREA(penStrokePreview),
                                   +[](GtkDrawingArea*, cairo_t* cr, int width, int height, gpointer data) {
                                       auto* self = static_cast<MainWindow*>(data);
                                       const auto* tools = self->control->getToolHandler();
                                       const Color colour = tools->getTool(TOOL_PEN).getColor();
                                       const GdkRGBA rgba = Util::argb_to_GdkRGBA(colour);
                                       cairo_set_source_rgba(cr, rgba.red, rgba.green, rgba.blue, 1.0);
                                       const ToolSize size = tools->getPenSize();
                                       const double line = std::clamp(tools->getTool(TOOL_PEN).getThickness(size) * 2.4, 1.5, 12.0);
                                       cairo_set_line_width(cr, line);
                                       cairo_set_line_cap(cr, CAIRO_LINE_CAP_ROUND);
                                       cairo_set_line_join(cr, CAIRO_LINE_JOIN_ROUND);
                                       cairo_move_to(cr, width * 0.08, height * 0.58);
                                       cairo_curve_to(cr, width * 0.28, height * 0.08, width * 0.45, height * 0.95,
                                                      width * 0.65, height * 0.45);
                                       cairo_curve_to(cr, width * 0.76, height * 0.18, width * 0.86, height * 0.70,
                                                      width * 0.94, height * 0.40);
                                       cairo_stroke(cr);
                                   }, this, nullptr);
    gtk_box_append(GTK_BOX(card), penStrokePreview);

    GtkWidget* advanced = gtk_expander_new(_("Shape and stroke options"));
    gtk_widget_set_name(advanced, "utnPenAdvancedOptions");
    penSettingsContent = penSettingsBar->createItem(false);
    gtk_widget_add_css_class(penSettingsContent.get(), "utn-paper-properties");
    gtk_widget_set_size_request(penSettingsContent.get(), 330, 240);
    gtk_container_add(GTK_CONTAINER(advanced), penSettingsContent.get());
    gtk_box_append(GTK_BOX(card), advanced);

    GtkWidget* footer = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    GtkWidget* pin = gtk_button_new_with_label(_("Pin to side"));
    utn::setAccessibleName(pin, _("Pin settings to side"));
    gtk_widget_set_tooltip_text(pin, _("Keep the full settings inspector beside the worksheet"));
    g_signal_connect(pin, "clicked", G_CALLBACK(+[](GtkButton*, gpointer data) {
                         auto* self = static_cast<MainWindow*>(data);
                         self->setTeacherWorkspace(0);
                         if (self->penSettingsPopover) {
                             gtk_popover_popdown(GTK_POPOVER(self->penSettingsPopover));
                         }
                     }), this);
    GtkWidget* dockToggle = gtk_check_button_new_with_label(_("Show ink dock"));
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(dockToggle), inkDockEnabled);
    utn::setAccessibleName(dockToggle, _("Show compact ink dock"));
    g_signal_connect(dockToggle, "toggled", G_CALLBACK(+[](GtkToggleButton* toggle, gpointer data) {
                         auto* self = static_cast<MainWindow*>(data);
                         self->inkDockEnabled = gtk_toggle_button_get_active(toggle);
                         self->control->getSettings()->getCustomElement("utn").setBool("inkDockEnabled",
                                                                                      self->inkDockEnabled);
                         self->updateInkDock();
                     }), this);
    inkDockToggle = dockToggle;
    gtk_widget_set_hexpand(pin, true);
    gtk_widget_set_halign(pin, GTK_ALIGN_START);
    gtk_box_append(GTK_BOX(footer), pin);
    gtk_box_append(GTK_BOX(footer), dockToggle);
    gtk_box_append(GTK_BOX(card), footer);
    gtk_container_add(GTK_CONTAINER(popover), card);
    // A popover is not a child of the tool item, so show_all on the item never reaches its card
    gtk_widget_show_all(card);
    gtk_menu_button_set_popover(GTK_MENU_BUTTON(button), popover);
    // The toolbar may destroy the button (and with it the popover) before destroyPenSettingsMenu() runs, e.g. on reloadToolbars()
    g_signal_connect(popover, "destroy", G_CALLBACK(+[](GtkWidget*, gpointer data) {
                         auto* self = static_cast<MainWindow*>(data);
                         self->penSettingsPopover = nullptr;
                         self->penSettingsHeading = nullptr;
                         self->penSettingsFocusAnchor = nullptr;
                     }), this);
    g_signal_connect(button, "destroy", G_CALLBACK(+[](GtkWidget*, gpointer data) {
                         auto* self = static_cast<MainWindow*>(data);
                         self->penSettingsButton = nullptr;
                         self->inkDockToggle = nullptr;
                     }), this);
    g_signal_connect(popover, "closed", G_CALLBACK(+[](GtkPopover*, gpointer data) {
                         auto* self = static_cast<MainWindow*>(data);
                         GtkWidget* focus = gtk_window_get_focus(GTK_WINDOW(self->window));
                         const bool focusWasInPopover = focus &&
                                                        (focus == self->penSettingsPopover ||
                                                         gtk_widget_is_ancestor(focus, self->penSettingsPopover));
                         GtkWidget* anchor = self->penSettingsFocusAnchor ? self->penSettingsFocusAnchor :
                                                                          self->penSettingsButton;
                         if (anchor && !gtk_widget_get_visible(anchor)) {
                             anchor = self->penSettingsButton;
                         }
                         if ((!focus || focusWasInPopover) && anchor && gtk_widget_get_visible(anchor)) {
                             gtk_widget_grab_focus(anchor);
                         }
                         self->penSettingsFocusAnchor = nullptr;
                         if (self->penSettingsButton && self->penSettingsPopover) {
                             gtk_popover_set_relative_to(GTK_POPOVER(self->penSettingsPopover),
                                                         self->penSettingsButton);
                         }
                     }), this);

    gtk_container_add(GTK_CONTAINER(item), button);
    gtk_toolbar_insert(top, item, -1);
    penSettingsButton = button;
    penSettingsPopover = popover;
    gtk_widget_show_all(GTK_WIDGET(item));
}

void MainWindow::createInkDock(GtkOverlay* overlay) {
    GtkWidget* dock = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    gtk_widget_set_name(dock, "utnInkDock");
    gtk_widget_add_css_class(dock, "utn-paper-dock");
    gtk_widget_set_halign(dock, GTK_ALIGN_CENTER);
    gtk_widget_set_valign(dock, GTK_ALIGN_END);
    gtk_widget_set_margin_bottom(dock, 64);
    gtk_widget_set_margin_start(dock, 12);
    gtk_widget_set_margin_end(dock, 12);

    GtkWidget* title = gtk_label_new(_("Pen"));
    gtk_widget_add_css_class(title, "utn-paper-dock-title");
    gtk_box_append(GTK_BOX(dock), title);

    const std::array<std::pair<const char*, Color>, 5> swatches{{
            {N_("Black"), Color{0x00U, 0x00U, 0x00U}},
            {N_("Blue"), Color{0x1EU, 0x5BU, 0xD8U}},
            {N_("Red"), Color{0xD3U, 0x2FU, 0x2FU}},
            {N_("Green"), Color{0x2EU, 0x7DU, 0x32U}},
            {N_("Purple"), Color{0x6AU, 0x1BU, 0x9AU}},
    }};
    for (size_t i = 0; i < swatches.size(); ++i) {
        const auto& [name, colour] = swatches[i];
        GtkWidget* swatch = utn::createColourSwatch(colour, _(name));
        g_object_set_data_full(G_OBJECT(swatch), "utn-ink-colour", new Color(colour),
                               +[](gpointer data) { delete static_cast<Color*>(data); });
        g_signal_connect(swatch, "clicked", G_CALLBACK(+[](GtkButton* button, gpointer data) {
                             auto* self = static_cast<MainWindow*>(data);
                             auto* colour = static_cast<Color*>(g_object_get_data(G_OBJECT(button), "utn-ink-colour"));
                             self->control->getToolHandler()->setColor(*colour, true);
                             self->updateInkDock();
                         }), this);
        inkDockSwatches[i] = swatch;
        gtk_box_append(GTK_BOX(dock), swatch);
    }

    const std::array<std::pair<const char*, ToolSize>, 5> sizes{{
            {"XS", TOOL_SIZE_VERY_FINE}, {"S", TOOL_SIZE_FINE}, {"M", TOOL_SIZE_MEDIUM}, {"L", TOOL_SIZE_THICK},
            {"XL", TOOL_SIZE_VERY_THICK},
    }};
    const std::array<const char*, 5> sizeNames{{N_("Very fine pen"), N_("Fine pen"), N_("Medium pen"), N_("Thick pen"), N_("Very thick pen")}};
    GtkWidget* sizeGroup = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 2);
    gtk_widget_add_css_class(sizeGroup, "utn-paper-size-group");
    for (size_t i = 0; i < sizes.size(); ++i) {
        GtkWidget* size = i == 0 ? gtk_radio_button_new_with_label(nullptr, sizes[i].first) :
                                   gtk_radio_button_new_with_label_from_widget(
                                           GTK_RADIO_BUTTON(inkDockSizes[0]), sizes[i].first);
        gtk_widget_set_tooltip_text(size, _(sizeNames[i]));
        utn::setAccessibleName(size, _(sizeNames[i]));
        g_object_set_data(G_OBJECT(size), "utn-ink-size", GINT_TO_POINTER(static_cast<int>(sizes[i].second)));
        g_signal_connect(size, "toggled", G_CALLBACK(+[](GtkToggleButton* button, gpointer data) {
                             if (!gtk_toggle_button_get_active(button)) return;
                             auto* self = static_cast<MainWindow*>(data);
                             if (self->syncingInkDock) return;
                             const auto value = static_cast<ToolSize>(GPOINTER_TO_INT(
                                     g_object_get_data(G_OBJECT(button), "utn-ink-size")));
                             // The first radio may become active while the window is being
                             // assembled, before Control::initWindow() creates ActionDatabase.
                             if (!self->control->getActionDatabase()) return;
                             self->control->getToolHandler()->setPenSize(value);
                             self->updateInkDock();
                         }), this);
        inkDockSizes[i] = size;
        gtk_box_append(GTK_BOX(sizeGroup), size);
    }
    gtk_box_append(GTK_BOX(dock), sizeGroup);

    inkDockSettingsButton = gtk_button_new_with_label(_("Pen settings"));
    gtk_widget_set_tooltip_text(inkDockSettingsButton, _("Open pen properties"));
    utn::setAccessibleName(inkDockSettingsButton, _("Open pen properties"));
    g_signal_connect(inkDockSettingsButton, "clicked", G_CALLBACK(+[](GtkButton*, gpointer data) {
                         auto* self = static_cast<MainWindow*>(data);
                         if (self->penSettingsPopover) {
                             self->penSettingsFocusAnchor = self->inkDockSettingsButton;
                             gtk_popover_set_relative_to(GTK_POPOVER(self->penSettingsPopover),
                                                         self->inkDockSettingsButton);
                             gtk_popover_popup(GTK_POPOVER(self->penSettingsPopover));
                         }
                     }), this);
    gtk_box_append(GTK_BOX(dock), inkDockSettingsButton);

    GtkWidget* close = gtk_button_new_with_label("×");
    gtk_widget_set_tooltip_text(close, _("Hide ink dock"));
    utn::setAccessibleName(close, _("Hide ink dock"));
    g_signal_connect(close, "clicked", G_CALLBACK(+[](GtkButton*, gpointer data) {
                         auto* self = static_cast<MainWindow*>(data);
                         self->inkDockEnabled = false;
                         self->control->getSettings()->getCustomElement("utn").setBool("inkDockEnabled", false);
                         if (self->inkDockToggle) {
                             gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(self->inkDockToggle), false);
                         }
                         self->updateInkDock();
                     }), this);
    gtk_box_append(GTK_BOX(dock), close);

    gtk_overlay_add_overlay(overlay, dock);
    gtk_overlay_set_overlay_pass_through(overlay, dock, false);
    inkDock = dock;
    gtk_widget_show_all(dock);
    updateInkDock();
}

void MainWindow::updateInkDock() {
    if (!inkDock) return;
    const auto* tools = control->getToolHandler();
    {
        // The card edits whichever tool is active, so its title and tooltip must name that tool
        const char* name = _("Tool");
        switch (tools->getToolType()) {
            case TOOL_PEN: name = _("Pen"); break;
            case TOOL_HIGHLIGHTER: name = _("Highlighter"); break;
            case TOOL_ERASER: name = _("Eraser"); break;
            case TOOL_TEXT: name = _("Text"); break;
            default: break;
        }
        const std::string title = FS(_F("{1} settings") % name);
        if (penSettingsHeading) gtk_label_set_text(GTK_LABEL(penSettingsHeading), title.c_str());
        if (penSettingsButton) gtk_widget_set_tooltip_text(penSettingsButton, title.c_str());
    }
    const bool pen = tools->getToolType() == TOOL_PEN;
    const bool teacher = selectedToolbar && utn::isTeacherLayout(selectedToolbar->getId());
    gtk_widget_set_visible(inkDock, inkDockEnabled && pen && teacher);
    const Color active = tools->getTool(TOOL_PEN).getColor();
    const std::array<Color, 8> propertyColours{{Color{0x20U, 0x25U, 0x2BU}, Color{0x1EU, 0x5BU, 0xD8U},
                                                Color{0xD3U, 0x2FU, 0x2FU}, Color{0x2EU, 0x7DU, 0x32U},
                                                Color{0x6AU, 0x1BU, 0x9AU}, Color{0xEFU, 0x6CU, 0x00U},
                                                Color{0xC0U, 0x8BU, 0x00U}, Color{0x00U, 0x7FU, 0x73U}}};
    for (size_t i = 0; i < penPropertySwatches.size(); ++i) {
        if (penPropertySwatches[i]) {
            utn::setSwatchSelected(penPropertySwatches[i], active == propertyColours[i]);
        }
    }
    if (penWidthScale && penWidthLabel && penStrokePreview) {
        const ToolSize activeWidth = tools->getPenSize();
        const std::array<const char*, 5> widthLabels{{N_("XS"), N_("S"), N_("M"), N_("L"), N_("XL")}};
        syncingPenSettings = true;
        gtk_range_set_value(GTK_RANGE(penWidthScale), static_cast<int>(activeWidth));
        syncingPenSettings = false;
        gtk_label_set_text(GTK_LABEL(penWidthLabel), _(widthLabels[std::clamp(static_cast<int>(activeWidth), 0, 4)]));
        gtk_widget_queue_draw(penStrokePreview);
    }
    const std::array<Color, 5> swatches{{Color{0x00U, 0x00U, 0x00U}, Color{0x1EU, 0x5BU, 0xD8U},
                                        Color{0xD3U, 0x2FU, 0x2FU}, Color{0x2EU, 0x7DU, 0x32U},
                                        Color{0x6AU, 0x1BU, 0x9AU}}};
    for (size_t i = 0; i < inkDockSwatches.size(); ++i) {
        const auto& candidate = swatches[i];
        utn::setSwatchSelected(inkDockSwatches[i], active.red == candidate.red && active.green == candidate.green &&
                                                  active.blue == candidate.blue);
    }
    const ToolSize activeSize = tools->getPenSize();
    const std::array<ToolSize, 5> sizes{{TOOL_SIZE_VERY_FINE, TOOL_SIZE_FINE, TOOL_SIZE_MEDIUM, TOOL_SIZE_THICK, TOOL_SIZE_VERY_THICK}};
    syncingInkDock = true;
    for (size_t i = 0; i < inkDockSizes.size(); ++i) {
        gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(inkDockSizes[i]), activeSize == sizes[i]);
    }
    syncingInkDock = false;
}

void MainWindow::setTeacherWorkspace(int mode) {
    inspectorAutoFolded = false;
    inspectorManualChoice = true;
    teacherWorkspace = std::clamp(mode, 0, 2);
    control->getSettings()->getCustomElement("utn").setInt("workspaceTools", teacherWorkspace);
    updateTeacherWorkspace();
}

void MainWindow::updateInspectorBounds() {
    if (!inspectorPaned || !selectedToolbar || !utn::isTeacherLayout(selectedToolbar->getId()) ||
        teacherWorkspace != 0 || !inspectorAttached) {
        return;
    }
    const int width = gtk_widget_get_allocated_width(inspectorPaned);
    if (width <= 0) {
        inspectorApplying = true;
        gtk_paned_set_position(GTK_PANED(inspectorPaned), inspectorWidth);
        inspectorApplying = false;
        return;
    }
    if (width == inspectorAvailableWidth) {
        return;
    }
    inspectorAvailableWidth = width;
    const int maximum = std::clamp(width - 520, 150, 420);
    const int minimum = std::min(150, maximum);  // same floor as the saved drag width (position >= 150)
    const int position = std::clamp(inspectorWidth, minimum, maximum);
    if (gtk_paned_get_position(GTK_PANED(inspectorPaned)) == position) {
        return;
    }
    inspectorApplying = true;
    gtk_paned_set_position(GTK_PANED(inspectorPaned), position);
    inspectorApplying = false;
}

void MainWindow::updateTeacherWorkspace() {
    if (!inspectorPaned || !boxContainerWidget || !selectedToolbar) {
        return;
    }
    const bool teacher = utn::isTeacherLayout(selectedToolbar->getId());
    const int mode = teacher ? (teacherWorkspace == 0 && inspectorAutoFolded ? 1 : teacherWorkspace) : 0;
    if (workspaceMenu) {
        gtk_widget_set_visible(workspaceMenu, teacher);
    }
    if (penSettingsButton) {
        gtk_widget_set_no_show_all(penSettingsButton, !teacher);
        gtk_widget_set_visible(penSettingsButton, teacher);
    }
    // Present gives the page the room: its tool dock waits until the teacher asks for it
    const bool presentQuiet = teacher && selectedToolbar->getId() == "UTN Present" && !presentToolsRevealed;
    gtk_widget_set_visible(railScroller, mode != 2 && !presentQuiet && control->getSettings()->isToolbarVisible());

    const bool shouldAttach = mode == 0;
    GtkWidget* mainBox = get("mainContainerBox");
    if (shouldAttach && !inspectorAttached) {
        gtk_container_remove(GTK_CONTAINER(mainBox), boxContainerWidget.get());
        gtk_paned_pack2(GTK_PANED(inspectorPaned), boxContainerWidget.get(), true, true);
        gtk_box_pack_start(GTK_BOX(mainBox), inspectorPaned, true, true, 0);
        gtk_box_reorder_child(GTK_BOX(mainBox), inspectorPaned, 1);
        inspectorAvailableWidth = 0;
        gtk_paned_set_position(GTK_PANED(inspectorPaned), inspectorWidth);
        gtk_widget_show(inspectorPaned);
        gtk_widget_show(inspectorHost);
        gtk_widget_show(boxContainerWidget.get());
        inspectorAttached = true;
    } else if (!shouldAttach && inspectorAttached) {
        gtk_container_remove(GTK_CONTAINER(inspectorPaned), boxContainerWidget.get());
        gtk_container_remove(GTK_CONTAINER(mainBox), inspectorPaned);
        gtk_box_pack_start(GTK_BOX(mainBox), boxContainerWidget.get(), true, true, 0);
        gtk_box_reorder_child(GTK_BOX(mainBox), boxContainerWidget.get(), 1);
        gtk_widget_hide(inspectorPaned);
        inspectorAttached = false;
    }
    if (shouldAttach) {
        gtk_widget_set_visible(inspectorHost, true);
        gtk_widget_queue_resize(inspectorPaned);
    }
    gtk_widget_set_visible(get("utnCollapseInspector"), teacher && mode == 0);
    gtk_widget_set_visible(get("utnExpandInspector"), teacher && mode == 1);
    updateInkDock();
}

void MainWindow::reloadToolbars() {
    ToolbarData* d = getSelectedToolbar();
    this->clearToolbar();
    this->toolbarSelected(d);
}

auto MainWindow::getSelectedToolbar() const -> ToolbarData* { return this->selectedToolbar; }

auto MainWindow::getToolbarWidgets() const -> const ToolbarWidgetArray& { return toolbarWidgets; }

auto MainWindow::getToolbarName(GtkToolbar* toolbar) const -> const char* {
    for (size_t i = 0; i < TOOLBAR_DEFINITIONS_LEN; i++) {
        if (static_cast<void*>(this->toolbarWidgets[i].get()) == static_cast<void*>(toolbar)) {
            return TOOLBAR_DEFINITIONS[i].propName;
        }
    }

    return "";
}

void MainWindow::setDynamicallyGeneratedSubmenuDisabled(bool disabled) { menubar->setDisabled(disabled); }

void MainWindow::updateToolbarMenu() {
    menubar->getToolbarSelectionSubmenu().update(toolbar.get(), this->selectedToolbar);
}

void MainWindow::createToolbar() {
    toolbarSelected(control->getSettings()->getSelectedToolbar());

    this->control->getScheduler()->unblockRerenderZoom();
}

void MainWindow::updatePageNumbers(size_t page, size_t pagecount, size_t pdfpage) {
    toolbar->setPageInfo(page, pagecount, pdfpage);
}

auto MainWindow::getMenubar() const -> Menubar* { return menubar.get(); }

void MainWindow::show(GtkWindow* parent) { gtk_widget_show(this->window); }

void MainWindow::setUndoDescription(const string& description) { menubar->setUndoDescription(description); }

void MainWindow::setRedoDescription(const string& description) { menubar->setRedoDescription(description); }

auto MainWindow::getToolbarModel() const -> ToolbarModel* { return this->toolbar->getModel(); }

auto MainWindow::getToolMenuHandler() const -> ToolMenuHandler* { return this->toolbar.get(); }

void MainWindow::loadMainCSS(GladeSearchpath* gladeSearchPath, const gchar* cssFilename) {
    auto filepath = gladeSearchPath->findFile("", cssFilename);
    xoj::util::GObjectSPtr<GtkCssProvider> provider(gtk_css_provider_new(), xoj::util::adopt);
    gtk_css_provider_load_from_path(provider.get(), char_cast(filepath.u8string().c_str()), nullptr);
    gtk_style_context_add_provider_for_screen(gdk_screen_get_default(), GTK_STYLE_PROVIDER(provider.get()),
                                              GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
}

PdfFloatingToolbox* MainWindow::getPdfToolbox() const { return this->pdfFloatingToolBox.get(); }

FloatingToolbox* MainWindow::getFloatingToolbox() const { return this->floatingToolbox.get(); }

void MainWindow::showToast(const std::string& message, bool offerUndo) {
    if (!offerUndo) {
        toast->show(message);
        return;
    }
    toast->show(message, _("Undo"), [ctrl = this->control]() { ctrl->getActionDatabase()->fireActivateAction(Action::UNDO); });
}

UtnToast* MainWindow::getToast() const { return this->toast.get(); }

void MainWindow::exitPresentationOverlay() {
    auto* tools = control->getToolHandler();
    tools->clearPresentationOverlay();
    tools->fireToolChanged();  // updates the banner and every tool indicator
    if (auto* view = getXournal()) {
        if (auto* page = view->getViewFor(control->getCurrentPageNo())) {
            page->repaintPage();
        }
    }
}

void MainWindow::leavePresent() {
    auto exists = [&](const std::string& id) {
        const auto& toolbars = toolbar->getModel()->getToolbars();
        return std::any_of(toolbars.begin(), toolbars.end(), [&](const auto& d) { return d->getId() == id; });
    };
    // The layout Present was entered from may have been deleted or renamed since; never leave the teacher stuck
    if (workspaceBeforePresent.empty() || !exists(workspaceBeforePresent)) {
        workspaceBeforePresent = "UTN Teacher";
    }
    toolbarSelected(workspaceBeforePresent);
}

void MainWindow::setPresentToolsRevealed(bool revealed) {
    if (presentToolsRevealed == revealed) {
        return;
    }
    presentToolsRevealed = revealed;
    updateTeacherWorkspace();
}

bool MainWindow::workspaceShortcut(const GdkEventKey* event) {
    if (event->type != GDK_KEY_PRESS ||
        (event->state & gtk_accelerator_get_default_mod_mask()) != (GDK_CONTROL_MASK | GDK_SHIFT_MASK)) {
        return false;
    }
    // Typing in an entry or text view keeps its keys
    GtkWidget* focus = gtk_window_get_focus(GTK_WINDOW(this->window));
    if (focus && (GTK_IS_EDITABLE(focus) || GTK_IS_TEXT_VIEW(focus))) {
        return false;
    }
    const bool inPresent = selectedToolbar && selectedToolbar->getId() == "UTN Present";
    // Shift changes what a digit key reports, and the symbol depends on the keyboard layout: read the key as if Shift
    // were up, and also accept the US-layout symbols
    guint base = event->keyval;
    if (event->hardware_keycode != 0) {
        guint unshifted = 0;
        if (gdk_keymap_translate_keyboard_state(gdk_keymap_get_for_display(gdk_display_get_default()),
                                                event->hardware_keycode, static_cast<GdkModifierType>(0), event->group,
                                                &unshifted, nullptr, nullptr, nullptr)) {
            base = unshifted;
        }
    }
    switch (base) {
        case GDK_KEY_1:
        case GDK_KEY_exclam:
            toolbarSelected("UTN Teacher");
            return true;
        case GDK_KEY_2:
        case GDK_KEY_at:
            toolbarSelected("UTN Marking");
            return true;
        case GDK_KEY_3:
        case GDK_KEY_numbersign:
            toolbarSelected("UTN Present");
            return true;
        case GDK_KEY_p:
        case GDK_KEY_P:
            if (inPresent) {
                leavePresent();
            } else {
                toolbarSelected("UTN Present");
            }
            return true;
        case GDK_KEY_t:
        case GDK_KEY_T:
            if (!inPresent) {
                return false;  // Teach and Mark always show their tools
            }
            setPresentToolsRevealed(!presentToolsRevealed);
            return true;
        default:
            return false;
    }
}

bool MainWindow::escapeClassroom() {
    // An open popover or menu takes Escape to close itself first
    if (gtk_grab_get_current() != nullptr) {
        return false;
    }
    auto* tools = control->getToolHandler();
    bool handled = false;
    if (tools->isSpotlightEnabled() || tools->isCurtainEnabled()) {
        exitPresentationOverlay();
        handled = true;
    }
    if (auto* students = toolbar ? toolbar->getStudentView() : nullptr; students && students->isTeacherCoverShown()) {
        students->setBlanked(false);
        handled = true;
    }
    return handled;
}

void MainWindow::updatePresentationBanner() {
    if (!presentationBanner) {
        return;
    }
    auto* tools = control->getToolHandler();
    const bool spotlight = tools->isSpotlightEnabled();
    const bool curtain = tools->isCurtainEnabled();
    const int overlayState = spotlight ? 1 : (curtain ? 2 : 0);
    if (overlayState != presentationOverlayState) {
        presentationOverlayState = overlayState;
        // Student View draws the same overlay; a null page asks it to redraw at once
        control->notifyCanvasChanged(nullptr);
    }
    if (spotlight || curtain) {
        gtk_label_set_text(GTK_LABEL(presentationBannerLabel),
                           spotlight ? _("Spotlight is ON: the page is darkened") :
                                       _("Curtain is ON: the page is covered"));
        // Sit just below the app bar, never over it: the banner must not hide Freeze, Blank or Present
        gint bannerX = 0;
        gint bannerY = 0;
        if (gtk_widget_translate_coordinates(get("mainContainerBox"), get("mainOverlay"), 0, 0, &bannerX, &bannerY)) {
            gtk_widget_set_margin_top(presentationBanner, std::max(12, bannerY + 8));
        }
        gtk_widget_show(presentationBanner);
    } else {
        gtk_widget_hide(presentationBanner);
    }
}

bool MainWindow::isPresentationBannerVisible() const {
    return presentationBanner && gtk_widget_get_visible(presentationBanner);
}

std::string MainWindow::getPresentationBannerText() const {
    return isPresentationBannerVisible() ? gtk_label_get_text(GTK_LABEL(presentationBannerLabel)) : std::string();
}

namespace {
void setRailLabelsVisible(GtkWidget* widget, bool visible) {
    if (g_strcmp0(gtk_widget_get_name(widget), "utnRailLabel") == 0) {
        gtk_widget_set_visible(widget, visible);
        return;
    }
    if (GTK_IS_CONTAINER(widget)) {
        GList* children = gtk_container_get_children(GTK_CONTAINER(widget));
        for (GList* child = children; child; child = child->next) {
            setRailLabelsVisible(GTK_WIDGET(child->data), visible);
        }
        g_list_free(children);
    }
}
}  // namespace

namespace {
/// Hide what is only decoration or has another way in (the title bar already names the app) when the window is compact,
/// so Freeze, Blank, Present and Appearance keep their room in the app bar
void setCompactHidden(GtkWidget* widget, bool compact, bool preserveBrand) {
    if (gtk_style_context_has_class(gtk_widget_get_style_context(widget), "utn-compact-hidden")) {
        const bool isBrand = g_strcmp0(gtk_widget_get_name(widget), "utnBrand") == 0;
        const bool show = !compact || (isBrand && preserveBrand);
        gtk_widget_set_visible(widget, show);
        // An emptied GtkToolItem still reserves a slot in the toolbar; hide it too so the room goes to real controls
        GtkWidget* wrapper = gtk_widget_get_parent(widget);
        if (wrapper && GTK_IS_TOOL_ITEM(wrapper)) {
            gtk_widget_set_visible(wrapper, show);
        }
        return;
    }
    if (GTK_IS_CONTAINER(widget)) {
        GList* children = gtk_container_get_children(GTK_CONTAINER(widget));
        for (GList* child = children; child; child = child->next) {
            setCompactHidden(GTK_WIDGET(child->data), compact, preserveBrand);
        }
        g_list_free(children);
    }
}
}  // namespace

namespace {
/// Below the compact breakpoint the app bar must still fit its right-hand menus (Appearance, Pen settings): GtkToolbar
/// gives those wrapped widgets no overflow menu, so anything that does not fit is simply unreachable.
/// Hides every widget marked `utn-narrow-hidden`, and the separators between app-bar groups.
void setNarrowHidden(GtkWidget* widget, bool narrow) {
    if (gtk_style_context_has_class(gtk_widget_get_style_context(widget), "utn-narrow-hidden") ||
        GTK_IS_SEPARATOR_TOOL_ITEM(widget)) {
        gtk_widget_set_visible(widget, !narrow);
        return;
    }
    if (GTK_IS_CONTAINER(widget)) {
        GList* children = gtk_container_get_children(GTK_CONTAINER(widget));
        for (GList* child = children; child; child = child->next) {
            setNarrowHidden(GTK_WIDGET(child->data), narrow);
        }
        g_list_free(children);
    }
}

/// Hides the text of a button that also shows an icon. The label property, tooltip and accessible name are unchanged.
void hideButtonCaption(GtkWidget* widget, bool hide) {
    if (GTK_IS_LABEL(widget)) {
        gtk_widget_set_visible(widget, !hide);
        return;
    }
    if (GTK_IS_CONTAINER(widget)) {
        GList* children = gtk_container_get_children(GTK_CONTAINER(widget));
        for (GList* child = children; child; child = child->next) {
            hideButtonCaption(GTK_WIDGET(child->data), hide);
        }
        g_list_free(children);
    }
}
}  // namespace

namespace {
/// The app-bar buttons that may move into the "More" menu, least important first
std::vector<GtkToolItem*> overflowCandidates(GtkToolbar* bar) {
    std::vector<std::pair<int, GtkToolItem*>> ranked;
    for (int i = 0; i < gtk_toolbar_get_n_items(bar); ++i) {
        GtkToolItem* item = gtk_toolbar_get_nth_item(bar, i);
        const int rank = GPOINTER_TO_INT(g_object_get_data(G_OBJECT(item), utn::OVERFLOW_RANK_KEY));
        if (rank > 0) {
            ranked.emplace_back(rank, item);
        }
    }
    std::sort(ranked.begin(), ranked.end(), [](const auto& a, const auto& b) { return a.first < b.first; });
    std::vector<GtkToolItem*> items;
    for (const auto& entry: ranked) {
        items.push_back(entry.second);
    }
    return items;
}

/// Swaps wording: what is marked `utn-tight-hidden` is the normal form and goes, what is marked `utn-tight-only` is the
/// short form and comes. Accessible names and tooltips are not touched, so nothing is lost for assistive technology.
void setTightWording(GtkWidget* widget, bool tight) {
    GtkStyleContext* context = gtk_widget_get_style_context(widget);
    if (gtk_style_context_has_class(context, "utn-tight-hidden")) {
        gtk_widget_set_visible(widget, !tight);
        return;
    }
    if (gtk_style_context_has_class(context, "utn-tight-only")) {
        gtk_widget_set_visible(widget, tight);
        return;
    }
    if (GTK_IS_CONTAINER(widget)) {
        GList* children = gtk_container_get_children(GTK_CONTAINER(widget));
        for (GList* child = children; child; child = child->next) {
            setTightWording(GTK_WIDGET(child->data), tight);
        }
        g_list_free(children);
    }
}

int naturalWidthOf(GtkWidget* widget) {
    int minimum = 0;
    int natural = 0;
    gtk_widget_get_preferred_width(widget, &minimum, &natural);
    return natural;
}

/// The button inside a tool item that runs an action, so a menu entry can run the very same one
GtkWidget* actionButtonIn(GtkWidget* widget) {
    if (GTK_IS_ACTIONABLE(widget) && gtk_actionable_get_action_name(GTK_ACTIONABLE(widget))) {
        return widget;
    }
    GtkWidget* found = nullptr;
    if (GTK_IS_CONTAINER(widget)) {
        GList* children = gtk_container_get_children(GTK_CONTAINER(widget));
        for (GList* child = children; child && !found; child = child->next) {
            found = actionButtonIn(GTK_WIDGET(child->data));
        }
        g_list_free(children);
    }
    return found;
}

GtkWidget* newMoreIcon() {
    for (const char* name: {"view-more-symbolic", "open-menu-symbolic"}) {
        if (gtk_icon_theme_has_icon(gtk_icon_theme_get_default(), name)) {
            return gtk_image_new_from_icon_name(name, GTK_ICON_SIZE_LARGE_TOOLBAR);
        }
    }
    return gtk_label_new("\xE2\x8B\xAF");  // midline ellipsis, for an icon theme that has neither
}
}  // namespace

void MainWindow::showMoreMenu(const std::vector<GtkToolItem*>& moved) {
    auto* menuButton = GTK_MENU_BUTTON(gtk_bin_get_child(GTK_BIN(appBarMore)));
    auto [popover, panel] = utn::createPopoverPanel();
    // Entries keep the order the buttons have on the bar
    GtkToolbar* bar = GTK_TOOLBAR(get("tbTop1"));
    std::vector<GtkToolItem*> inBarOrder = moved;
    std::sort(inBarOrder.begin(), inBarOrder.end(), [&](GtkToolItem* a, GtkToolItem* b) {
        return gtk_toolbar_get_item_index(bar, a) < gtk_toolbar_get_item_index(bar, b);
    });
    for (GtkToolItem* item: inBarOrder) {
        GtkWidget* source = actionButtonIn(GTK_WIDGET(item));
        if (!source) {
            continue;
        }
        const auto* label = static_cast<const char*>(g_object_get_data(G_OBJECT(item), utn::OVERFLOW_LABEL_KEY));
        // An entry is never nameless: fall back to what the button itself is called, and skip it if that is nothing
        std::string text = label ? label : "";
        if (text.empty()) {
            if (const char* tip = gtk_widget_get_tooltip_text(source)) {
                text = tip;
            } else if (const char* accessible = atk_object_get_name(gtk_widget_get_accessible(source))) {
                text = accessible;
            }
        }
        if (text.empty()) {
            continue;
        }
        // The same action, so the entry is enabled, disabled and activated exactly as the button it stands in for
        GtkWidget* entry = utn::appendMenuButton(panel, popover, text.c_str());
        gtk_actionable_set_action_name(GTK_ACTIONABLE(entry), gtk_actionable_get_action_name(GTK_ACTIONABLE(source)));
        if (GVariant* target = gtk_actionable_get_action_target_value(GTK_ACTIONABLE(source))) {
            gtk_actionable_set_action_target_value(GTK_ACTIONABLE(entry), target);
        }
    }
    gtk_widget_show_all(GTK_WIDGET(panel));
    gtk_menu_button_set_popover(menuButton, GTK_WIDGET(popover));
}

void MainWindow::fitAppBar(int width) {
    this->appBarFitWidth = width;
    GtkWidget* barWidget = get("tbTop1");
    GtkToolbar* bar = GTK_TOOLBAR(barWidget);
    const auto candidates = overflowCandidates(bar);

    // Start from everything in its normal form, then take away only what the measurement asks for. The result depends
    // on the width and on what is really on the bar, never on how a previous width left it.
    for (GtkToolItem* item: candidates) {
        gtk_widget_show(GTK_WIDGET(item));
    }
    setTightWording(barWidget, false);
    if (appBarMore) {
        gtk_widget_hide(appBarMore);
    }
    this->appBarTight = false;
    this->appBarOverflowing = false;
    const std::vector<GtkToolItem*> previouslyMoved = std::exchange(this->appBarMoved, {});

    const bool teacher = this->selectedToolbar && utn::isTeacherLayout(this->selectedToolbar->getId());
    if (!teacher || width < utn::layout::MIN_MEANINGFUL_WIDTH) {  // a placeholder width says nothing
        return;
    }
    if (naturalWidthOf(barWidget) <= width) {
        return;
    }

    // 1. Shorter wording: the Students pill keeps its state, in one word
    setTightWording(barWidget, true);
    this->appBarTight = true;
    const int tightNatural = naturalWidthOf(barWidget);
    if (tightNatural <= width || candidates.empty()) {
        return;
    }

    // 2. Least important buttons into "More". Their widths and the button's own width are measured, not assumed.
    if (!appBarMore) {
        auto* menuButton = GTK_MENU_BUTTON(gtk_menu_button_new());
        gtk_widget_set_focus_on_click(GTK_WIDGET(menuButton), false);  // keyboard users can still Tab to it
        gtk_button_set_child(GTK_BUTTON(menuButton), newMoreIcon());
        gtk_widget_add_css_class(GTK_WIDGET(menuButton), "utn-more-btn");
        gtk_widget_set_tooltip_text(GTK_WIDGET(menuButton), _("More actions"));
        utn::setAccessibleName(GTK_WIDGET(menuButton), _("More actions"));
        gtk_menu_button_set_direction(menuButton, GTK_ARROW_DOWN);
        GtkToolItem* item = gtk_tool_item_new();
        gtk_container_add(GTK_CONTAINER(item), GTK_WIDGET(menuButton));
        gtk_widget_show_all(GTK_WIDGET(item));
        gtk_toolbar_insert(bar, item, -1);
        this->appBarMore = GTK_WIDGET(item);
    }
    gtk_widget_show(appBarMore);
    std::vector<int> widths;
    for (GtkToolItem* item: candidates) {
        widths.push_back(naturalWidthOf(GTK_WIDGET(item)));
    }
    const int moreWidth = naturalWidthOf(appBarMore);
    // tightNatural was measured without the "More" button; the function adds it back
    const size_t count = utn::layout::appBarItemsToOverflow(tightNatural, width, widths, moreWidth);
    if (count == 0) {
        gtk_widget_hide(appBarMore);
        return;
    }
    const std::vector<GtkToolItem*> moved(candidates.begin(), candidates.begin() + static_cast<std::ptrdiff_t>(count));
    for (GtkToolItem* item: moved) {
        gtk_widget_hide(GTK_WIDGET(item));
    }
    // The menu is rebuilt only when what it holds changes, so one that is open is not replaced under the teacher's hand
    if (moved != previouslyMoved) {
        showMoreMenu(moved);
    }
    this->appBarMoved = moved;
    this->appBarOverflowing = true;
}

void MainWindow::scheduleAppBarFit() {
    if (appBarFitPending) {
        return;
    }
    appBarFitPending = true;
    // Never change what the bar holds while GTK is still allocating the window: do it once the allocation is done
    Util::execInUiThread([this, weakAlive = std::weak_ptr<bool>(this->alive)]() {
        if (weakAlive.expired()) {
            return;
        }
        appBarFitPending = false;
        fitAppBar(layoutWidth);
    });
}

void MainWindow::refitAppBar() {
    this->appBarFitWidth = -1;
    if (this->layoutWidth > 0) {
        scheduleAppBarFit();
    }
}

void MainWindow::setToolLabelMode(int mode) {
    this->toolLabels = std::clamp(mode, 0, 2);
    auto& workspace = this->control->getSettings()->getCustomElement("utn");
    workspace.setInt("toolLabels", this->toolLabels);
    // A choice that disagrees with the mode is the teacher's own arrangement: the mode steps aside, never overrides it
    const auto kept = utn::layout::modeAfterLabelChoice(this->interfaceMode, this->toolLabels);
    if (kept != this->interfaceMode) {
        this->interfaceMode = kept;
        workspace.setString("interfaceMode", utn::layout::interfaceModeName(kept));
        updateInterfaceModeButtons();
    }
    this->compactLayoutDirty = true;
    updateCompactLayout(this->layoutWidth);
}

void MainWindow::setInterfaceMode(utn::layout::InterfaceMode mode) {
    this->interfaceMode = mode;
    this->control->getSettings()->getCustomElement("utn").setString("interfaceMode", utn::layout::interfaceModeName(mode));
    if (const int labels = utn::layout::toolLabelModeFor(mode); labels >= 0) {
        setToolLabelMode(labels);  // agrees with the mode, so the mode is kept
    }
    updateInterfaceModeButtons();
    // The settings panel shows or drops its hints with the mode
    this->control->getToolHandler()->fireToolChanged();
}

void MainWindow::updateInterfaceModeButtons() {
    const auto mark = [&](GtkWidget* button, const char* name, utn::layout::InterfaceMode mode) {
        if (!button) {
            return;
        }
        const bool current = this->interfaceMode == mode;
        const std::string label = std::string(current ? "\xE2\x97\x8F " : "   ") + name;
        gtk_button_set_label(GTK_BUTTON(button), label.c_str());
        utn::setAccessibleName(button, current ? (std::string(name) + ", " + _("selected")).c_str() : name);
    };
    mark(beginnerModeButton, _("Beginner: tool names and hints"), utn::layout::InterfaceMode::Beginner);
    mark(expertModeButton, _("Expert: compact, icons only"), utn::layout::InterfaceMode::Expert);
}

void MainWindow::updateCompactLayout(int width, bool fromAllocation) {
    // The decision itself (with hysteresis) lives in ResponsiveLayout.h so it is tested at any logical width
    updatePanelPolicy(width, fromAllocation);
    const bool wasCompact = this->compactLayout;
    const bool wasCompactHeader = this->compactHeader;
    this->layoutWidth = width;
    this->compactLayout = utn::layout::nextCompactLayout(wasCompact, width);
    this->compactHeader = wasCompactHeader ? width <= 1400 : width < 1320;
    const bool paperInkRail = this->selectedToolbar && utn::isTeacherLayout(this->selectedToolbar->getId());
    if (documentHeaderTitle) {
        // The name gives way to a narrow window, the status never does: a failed save must be visible at any width
        gtk_widget_set_visible(documentHeaderTitle, !this->compactLayout);
    }
    // Fit the app bar to this width once everything above has settled. A real allocation defers it, because the bar
    // must not change while GTK is still allocating the window; a direct call applies it at once.
    const auto fitAppBarToWidth = [&] {
        if (!fromAllocation) {
            fitAppBar(width);
        } else if (width != this->appBarFitWidth) {
            scheduleAppBarFit();
        }
    };
    if (wasCompact == this->compactLayout && wasCompactHeader == this->compactHeader && !this->compactLayoutDirty) {
        fitAppBarToWidth();
        return;
    }
    this->compactLayoutDirty = false;
    GtkStyleContext* context = gtk_widget_get_style_context(GTK_WIDGET(this->window));
    if (this->compactLayout) {
        gtk_style_context_add_class(context, "utnCompact");
    } else {
        gtk_style_context_remove_class(context, "utnCompact");
    }
    if (workspaceMenu) {
        hideButtonCaption(workspaceMenu, this->compactHeader);
    }
    if (expandInspectorButton) {
        // Changing the label property (rather than only hiding its GtkLabel) updates the button's
        // natural requisition too; GTK otherwise keeps the old caption width in the rail host.
        // In the Paper & Ink shell, keep the settings affordance icon-only at every width so translations cannot
        // widen the rail. Its accessible name and tooltip retain discoverability.
        gtk_button_set_label(GTK_BUTTON(expandInspectorButton), paperInkRail ? "" :
                                                                           (this->compactHeader ? "" : _("Show settings")));
    }
    if (railScroller) {
        // The native toolbar's natural width is driven by its largest child (often a tool label or popover target).
        // The Paper & Ink scroller stays at the concept width; classic toolbars retain the existing compact policy.
        const int railWidth = paperInkRail ? utn::layout::TEACHER_RAIL_WIDTH : (this->compactLayout ? 72 : -1);
        gtk_scrolled_window_set_propagate_natural_width(GTK_SCROLLED_WINDOW(railScroller),
                                                       !paperInkRail && !this->compactLayout);
        gtk_scrolled_window_set_min_content_width(GTK_SCROLLED_WINDOW(railScroller), railWidth);
        gtk_scrolled_window_set_max_content_width(GTK_SCROLLED_WINDOW(railScroller), railWidth);
        gtk_widget_set_size_request(railScroller, railWidth, -1);
    }
    if (railHost) {
        const int railWidth = paperInkRail ? utn::layout::TEACHER_RAIL_WIDTH : (this->compactLayout ? 72 : -1);
        gtk_widget_set_size_request(railHost, railWidth, -1);
        if (this->compactLayout) {
            gtk_style_context_add_class(gtk_widget_get_style_context(railHost), "utnCompactRail");
        } else {
            gtk_style_context_remove_class(gtk_widget_get_style_context(railHost), "utnCompactRail");
        }
    }
    // Hide the long app-bar captions before they squeeze the document title; at <1080 the entire
    // chrome also switches to the compact size. Names remain in tooltips and accessible names.
    if (!this->toolbarWidgets.empty() && this->toolbarWidgets[0]) {
        setNarrowHidden(GTK_WIDGET(this->toolbarWidgets[0].get()), this->compactLayout);
    }
    for (const auto& toolbar: this->toolbarWidgets) {
        if (toolbar) {
            setRailLabelsVisible(toolbar.get(), utn::layout::railLabelsVisible(toolLabels, this->compactLayout));
            setCompactHidden(GTK_WIDGET(toolbar.get()), this->compactHeader, width >= 1280);
        }
    }
    // The document title is deliberately retained in the wider compact-header state.
    if (documentHeaderTitle) {
        // The name gives way to a narrow window, the status never does: a failed save must be visible at any width
        gtk_widget_set_visible(documentHeaderTitle, !this->compactLayout);
    }
    fitAppBarToWidth();
}

void MainWindow::setDPI() const {
    if (auto dpi = this->getControl()->getSettings()->getDisplayDpi(); dpi == -1) {
        auto res = xoj::util::gtk::getWidgetDPI(this->window);
        this->getControl()->getZoomControl()->setZoom100Value(res.value_or(Util::DPI_NORMALIZATION_FACTOR) /
                                                              Util::DPI_NORMALIZATION_FACTOR);
    } else {
        this->getControl()->getZoomControl()->setZoom100Value(dpi / Util::DPI_NORMALIZATION_FACTOR);
    }
}

void MainWindow::updatePanelPolicy(int width, bool fromAllocation) {
    const bool away = utn::layout::nextThumbnailsAway(thumbnailsAway, width);
    const bool folded = utn::layout::nextInspectorFolded(inspectorFolded, width);
    if (away == thumbnailsAway && folded == inspectorFolded) {
        return;
    }
    if (away != thumbnailsAway) {
        thumbnailsManualChoice = false;  // a new width crossing hands control back to the policy
    }
    if (folded != inspectorFolded) {
        inspectorManualChoice = false;
    }
    thumbnailsAway = away;
    inspectorFolded = folded;
    if (!fromAllocation) {
        applyPanelPolicy();  // not inside an allocation: nothing to wait for
        return;
    }
    // Never reparent or hide panels while GTK is still allocating this window: apply once the allocation is done
    if (panelPolicyPending) {
        return;
    }
    panelPolicyPending = true;
    Util::execInUiThread([this, weakAlive = std::weak_ptr<bool>(this->alive)]() {
        if (weakAlive.expired()) {
            return;
        }
        panelPolicyPending = false;
        applyPanelPolicy();
    });
}

void MainWindow::applyPanelPolicy() {
    applyingPanelPolicy = true;
    Settings* settings = control->getSettings();

    if (thumbnailsAway) {
        if (sidebarVisible && !thumbnailsManualChoice) {
            setSidebarVisible(false);
            thumbnailsAutoHidden = true;
            control->getActionDatabase()->setActionState(Action::SHOW_SIDEBAR, false);
        }
    } else if (thumbnailsAutoHidden) {
        thumbnailsAutoHidden = false;
        if (settings->isSidebarVisible()) {
            setSidebarVisible(true);
            control->getActionDatabase()->setActionState(Action::SHOW_SIDEBAR, true);
        }
    }

    const bool teacher = selectedToolbar && utn::isTeacherLayout(selectedToolbar->getId());
    if (inspectorFolded) {
        if (teacher && teacherWorkspace == 0 && !inspectorAutoFolded && !inspectorManualChoice) {
            inspectorAutoFolded = true;
            updateTeacherWorkspace();
        }
    } else if (inspectorAutoFolded) {
        inspectorAutoFolded = false;
        updateTeacherWorkspace();
    }
    applyingPanelPolicy = false;
}
