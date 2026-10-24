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
#include "gui/PageView.h"                               // for repaintPage
#include "gui/SearchBar.h"                              // for SearchBar
#include "gui/inputdevices/InputEvents.h"               // for INPUT_DEVICE_TOUC...
#include "gui/menus/menubar/Menubar.h"                  // for Menubar
#include "gui/menus/menubar/ToolbarSelectionSubmenu.h"  // for ToolbarSelectionSubmenu
#include "gui/scroll/ScrollHandling.h"                  // for ScrollHandling
#include "gui/sidebar/Sidebar.h"                        // for Sidebar
#include "gui/toolbarMenubar/ToolMenuHandler.h"         // for ToolMenuHandler
#include "gui/toolbarMenubar/model/ToolbarData.h"       // for ToolbarData
#include "gui/toolbarMenubar/model/ToolbarModel.h"      // for ToolbarModel
#include "gui/toolbarMenubar/UtnWidgets.h"              // for setAccessibleName
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

MainWindow::MainWindow(GladeSearchpath* gladeSearchPath, Control* control, GtkApplication* parent):
        GladeGui(gladeSearchPath, "main.glade", "mainWindow"),
        control(control),
        toolbar(std::make_unique<ToolMenuHandler>(control, this)),
        menubar(std::make_unique<Menubar>()) {
    gtk_window_set_application(GTK_WINDOW(getWindow()), parent);

    panedContainerWidget.reset(get("panelMainContents"), xoj::util::ref);
    boxContainerWidget.reset(get("mainContentContainer"), xoj::util::ref);
    mainContentWidget.reset(get("boxContents"), xoj::util::ref);
    sidebarWidget.reset(get("sidebar"), xoj::util::ref);
    inspectorPaned = get("utnInspectorPaned");
    inspectorHost = get("utnInspectorHost");
    railScroller = get("tbLeft1Scroller");
    g_signal_connect(get("utnExpandInspector"), "clicked", G_CALLBACK(+[](GtkButton*, gpointer data) {
                         static_cast<MainWindow*>(data)->setTeacherWorkspace(0);
                     }), this);
    // Keep the pane and canvas alive while the canvas is reparented for compact workspace states.
    inspectorPanedOwner.reset(inspectorPaned, xoj::util::ref);
    inspectorContentOwner.reset(boxContainerWidget.get(), xoj::util::ref);
    auto& workspace = control->getSettings()->getCustomElement("utn");
    workspace.getInt("inspectorWidth", inspectorWidth);
    workspace.getInt("workspaceTools", teacherWorkspace);
    workspace.getInt("toolLabels", toolLabels);
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
            }
        });
    }

    for (size_t i = 0; i < TOOLBAR_DEFINITIONS_LEN; i++) {
        this->toolbarWidgets[i].reset(get(TOOLBAR_DEFINITIONS[i].guiName), xoj::util::ref);
    }

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
                         win->updateCompactLayout(allocation->width);
                     }), this);

    // "watch over" all key events; Escape is an obvious exit for temporary classroom overlays.
    // Keep native GTK handling whenever no classroom overlay is active.
    auto keyPropagate = +[](GtkWidget* w, GdkEvent* e, gpointer data) -> gboolean {
        if (e->type == GDK_KEY_PRESS && reinterpret_cast<GdkEventKey*>(e)->keyval == GDK_KEY_Escape) {
            auto* self = static_cast<MainWindow*>(data);
            auto* tools = self->getControl()->getToolHandler();
            if (tools->isSpotlightEnabled() || tools->isCurtainEnabled()) {
                self->exitPresentationOverlay();
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
    settings->setSelectedToolbar(d->getId());

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
}

auto MainWindow::clearToolbar() -> const ToolbarData* {
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

void MainWindow::appendWorkspaceMenu() {
    GtkToolbar* top = GTK_TOOLBAR(this->toolbarWidgets[0].get());
    GtkToolItem* item = gtk_tool_item_new();
    GtkWidget* menu = gtk_menu_button_new();
    gtk_button_set_label(GTK_BUTTON(menu), _("Workspace"));
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
                             self->toolLabels = GPOINTER_TO_INT(g_object_get_data(G_OBJECT(button), "utn-label-mode"));
                             self->control->getSettings()->getCustomElement("utn").setInt("toolLabels", self->toolLabels);
                             self->compactLayoutDirty = true;
                             self->updateCompactLayout(gtk_widget_get_allocated_width(GTK_WIDGET(self->window)));
                         }), this);
        utn::setAccessibleName(button, labelChoices[static_cast<size_t>(mode)]);
        gtk_box_append(GTK_BOX(panel), button);
    }
    gtk_container_add(GTK_CONTAINER(popover), panel);
    gtk_menu_button_set_popover(GTK_MENU_BUTTON(menu), popover);
    gtk_container_add(GTK_CONTAINER(item), menu);
    gtk_toolbar_insert(top, item, -1);
    workspaceMenu = menu;
    gtk_widget_show_all(GTK_WIDGET(item));
}

void MainWindow::setTeacherWorkspace(int mode) {
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
    const int mode = teacher ? teacherWorkspace : 0;
    if (workspaceMenu) {
        gtk_widget_set_visible(workspaceMenu, teacher);
    }
    gtk_widget_set_visible(railScroller, mode != 2 && control->getSettings()->isToolbarVisible());

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

void MainWindow::updatePresentationBanner() {
    if (!presentationBanner) {
        return;
    }
    auto* tools = control->getToolHandler();
    const bool spotlight = tools->isSpotlightEnabled();
    const bool curtain = tools->isCurtainEnabled();
    if (spotlight || curtain) {
        gtk_label_set_text(GTK_LABEL(presentationBannerLabel),
                           spotlight ? _("Spotlight is ON: the page is darkened") :
                                       _("Curtain is ON: the page is covered"));
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

void MainWindow::updateCompactLayout(int width) {
    // Hysteresis: enter compact under 1080 px, leave above 1120 px, so dragging the edge does not flicker
    const bool wasCompact = this->compactLayout;
    if (!wasCompact && width >= 200 && width < 1080) {  // ignore the 1 px size before the window is shown
        this->compactLayout = true;
    } else if (wasCompact && width > 1120) {
        this->compactLayout = false;
    }
    if (wasCompact == this->compactLayout && !this->compactLayoutDirty) {
        return;
    }
    this->compactLayoutDirty = false;
    GtkStyleContext* context = gtk_widget_get_style_context(GTK_WIDGET(this->window));
    if (this->compactLayout) {
        gtk_style_context_add_class(context, "utnCompact");
    } else {
        gtk_style_context_remove_class(context, "utnCompact");
    }
    // Compact rail: icons only; names stay in tooltips and accessible names
    for (const auto& toolbar: this->toolbarWidgets) {
        if (toolbar) {
            setRailLabelsVisible(toolbar.get(), toolLabels == 1 || (toolLabels == 0 && !this->compactLayout));
        }
    }
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
