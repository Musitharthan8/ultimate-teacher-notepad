#include "ToolMenuHandler.h"

#include "control/UtnLayout.h"  // for isTeacherLayout
#include <algorithm>
#include <array>
#include <cstdint>  // for max
#include <sstream>    // for istringstream

#include "control/Control.h"                         // for Control
#include "control/PageBackgroundChangeController.h"  // for PageBackgroundChangeController
#include "control/ScrollHandler.h"                   // for ScrollHandler
#include "control/actions/ActionDatabase.h"          // for ActionDatabase
#include "control/settings/Settings.h"               // for Settings
#include "gui/GladeGui.h"                            // for GladeGui
#include "gui/GladeSearchpath.h"
#include "gui/ToolitemDragDrop.h"  // for ToolitemDragDrop
#include "gui/menus/popoverMenus/PageTypeSelectionPopover.h"
#include "gui/toolbarMenubar/model/ColorPalette.h"  // for Palette
#include "gui/toolbarMenubar/model/ToolbarData.h"   // for ToolbarData
#include "gui/toolbarMenubar/model/ToolbarEntry.h"  // for ToolbarEntry
#include "gui/toolbarMenubar/model/ToolbarItem.h"   // for ToolbarItem
#include "gui/toolbarMenubar/model/ToolbarModel.h"  // for ToolbarModel
#include "plugin/Plugin.h"                          // for ToolbarButtonEntr<
#include "util/GVariantTemplate.h"                  // for gVariantType
#include "util/GtkUtil.h"
#include "util/NamedColor.h"  // for NamedColor
#include "util/PathUtil.h"
#include "util/StringUtils.h"  // for StringUtils
#include "util/XojMsgBox.h"
#include "util/gtk4_helper.h"
#include "util/i18n.h"  // for _
#include "util/Util.h"  // for GdkRGBA conversion
#include "control/tools/TextEditor.h"  // for text/answer-box recolouring
#include "util/raii/CStringWrapper.h"  // for OwnedCString

#include "AbstractToolItem.h"            // for AbstractToolItem
#include "ColorSelectorToolItem.h"       // for ColorSelectorToolItem
#include "ColorToolItem.h"               // for ColorToolItem
#include "gui/toolbarMenubar/icon/ColorIcon.h"  // for a visible current-colour icon
#include "DrawingTypeComboToolButton.h"  // for DrawingTypeComboToolButton
#include "FontButton.h"                  // for FontButton
#include "PluginPlaceholderLabel.h"      // for PluginPlaceholderLabel
#include "PluginToolButton.h"            // for PluginToolButton
#include "SeparatorItem.h"
#include "SpacerItem.h"
#include "StylePopoverFactory.h"     // for ToolButtonWithStylePopover
#include "ToolButton.h"              // for ToolButton
#include "ToolAppearance.h"          // for ToolAppearance
#include "ToolTextMode.h"
#include "ToolWorkspaceSwitch.h"  // for ToolWorkspaceSwitch
#include "ToolClassroomWorkflow.h"
#include "ToolClear.h"               // for ToolClear
#include "ToolLessonNavigator.h"     // for ToolLessonNavigator
#include "ToolPageLayer.h"           // for ToolPageLayer
#include "ToolPageSpinner.h"         // for ToolPageSpinner
#include "ToolPrepareReveal.h"        // for ToolPrepareReveal
#include "ToolPresentationKit.h"      // for ToolPresentationKit
#include "ToolProfileSelector.h"      // for ToolProfileSelector
#include "ToolPdfCombocontrol.h"     // for ToolPdfCombocontrol
#include "ToolSelectCombocontrol.h"  // for ToolSelectComboc...
#include "ToolEraserSizeSlider.h"    // for ToolEraserSizeSlider
#include "ToolSmartHighlighter.h"    // for ToolSmartHighlighter
#include "ToolStudentView.h"         // for ToolStudentView
#include "ToolTeacherStamp.h"       // for ToolTeacherStamp
#include "ToolTeachingKit.h"        // for ToolTeachingKit
#include "UtnWidgets.h"  // for round colour swatches and popover panels
#include "ToolUtnContextBar.h"       // for ToolUtnContextBar
#include "ToolZoomSlider.h"          // for ToolZoomSlider
#include "TooltipToolButton.h"       // for TooltipToolButton
#include "config-dev.h"              // for TOOLBAR_CONFIG
#include "config-features.h"         // for ENABLE_PLUGINS
#include "filesystem.h"              // for exists


namespace {

struct ClassroomColour {
    const char* name;
    Color value;
};

// The fast colour set intentionally covers both handwriting and highlighting. The full
// GTK chooser below supports arbitrary colours, including those outside these presets.
constexpr std::array<ClassroomColour, 12> CLASSROOM_COLOURS{{
        {N_("Black"), Color{0x00, 0x00, 0x00}},
        {N_("Blue"), Color{0x1E, 0x5B, 0xD8}},
        {N_("Red"), Color{0xE5, 0x26, 0x29}},
        {N_("Green"), Color{0x39, 0xC6, 0x5E}},
        {N_("Purple"), Color{0x8A, 0x45, 0xC8}},
        {N_("Yellow"), Color{0xFF, 0xEA, 0x36}},
        {N_("Orange"), Color{0xFF, 0xA7, 0x26}},
        {N_("Pink"), Color{0xFF, 0x7E, 0xB6}},
        {N_("Cyan"), Color{0x00, 0xD5, 0xE8}},
        {N_("Lime"), Color{0x8B, 0xE0, 0x4E}},
        {N_("Brown"), Color{0xA5, 0x62, 0x38}},
        {N_("White"), Color{0xFF, 0xFF, 0xFF}},
}};

Color teacherCurrentColour(Control* ctrl) {
    auto* tools = ctrl->getToolHandler();
    if (tools->getToolType() == TOOL_TEXT) {
        if (tools->getTextMode() == TextMode::Feedback) {
            return tools->getTeacherStampColor();
        }
        if (tools->getTextMode() == TextMode::AnswerBox) {
            return tools->getAnswerBoxTextColor();
        }
    }
    return tools->getColor();
}

void teacherApplyColour(Control* ctrl, Color selected) {
    auto* tools = ctrl->getToolHandler();
    // Keep highlighter opacity (and any other current-tool alpha) when changing only hue.
    selected.alpha = teacherCurrentColour(ctrl).alpha;
    if (tools->getToolType() == TOOL_TEXT && tools->getTextMode() == TextMode::Feedback) {
        tools->setTeacherStampColor(selected);
    } else if (tools->getToolType() == TOOL_TEXT && tools->getTextMode() == TextMode::AnswerBox) {
        tools->setAnswerBoxTextColor(selected);
    } else {
        tools->setColor(selected, true);  // Also recolours an active object selection.
    }
    if (tools->getToolType() == TOOL_TEXT) {
        if (auto* editor = ctrl->getTextEditor()) {
            editor->setColor(selected);
        }
    }
}

void refreshClassroomColours(GtkPopover* popover, Control* ctrl) {
    auto* grid = GTK_GRID(g_object_get_data(G_OBJECT(popover), "utn-colour-grid"));
    auto* custom = GTK_COLOR_BUTTON(g_object_get_data(G_OBJECT(popover), "utn-custom-colour"));
    const Color current = teacherCurrentColour(ctrl);
    // Colour preview in the left rail follows the tool selected when the palette is opened.
    if (GtkWidget* owner = gtk_popover_get_relative_to(popover); GTK_IS_MENU_BUTTON(owner)) {
        gtk_button_set_child(GTK_BUTTON(owner), ColorIcon::newGtkImage(current, 22, false));
    }
    // The palette has no meaning for Eraser/Hand; avoid silently modifying their tool state.
    const bool available = ctrl->getToolHandler()->hasCapability(TOOL_CAP_COLOR);
    gtk_widget_set_sensitive(GTK_WIDGET(grid), available);
    gtk_widget_set_sensitive(GTK_WIDGET(custom), available);
    GList* swatches = gtk_container_get_children(GTK_CONTAINER(grid));
    for (GList* item = swatches; item; item = item->next) {
        GtkWidget* swatch = GTK_WIDGET(item->data);
        const auto raw = GPOINTER_TO_UINT(g_object_get_data(G_OBJECT(swatch), "utn-classroom-colour"));
        const Color value(raw);
        utn::setSwatchSelected(swatch, available && current.red == value.red &&
                                                      current.green == value.green && current.blue == value.blue);
    }
    g_list_free(swatches);
    GdkRGBA rgba = Util::argb_to_GdkRGBA(current);
    rgba.alpha = 1.0;  // Opacity stays in the tool's own properties.
    gtk_color_chooser_set_rgba(GTK_COLOR_CHOOSER(custom), &rgba);
}

// A compact vertical-rail colour control. Unlike the top ribbon's existing swatches,
// this popover appears beside the user's tools, like the teaching reference screenshot.
class ToolClassroomColours final: public AbstractToolItem {
public:
    explicit ToolClassroomColours(Control* ctrl):
            AbstractToolItem("UTN_COLOURS", Category::TOOLS), control(ctrl) {}

    xoj::util::WidgetSPtr createItem(bool horizontal) override {
        auto [popover, panel] = utn::createPopoverPanel(6);
        utn::appendPopoverHeading(panel, _("Colours"));

        GtkWidget* hint = gtk_label_new(_("Choose a colour for the selected tool"));
        gtk_label_set_xalign(GTK_LABEL(hint), 0.0F);
        gtk_box_append(panel, hint);

        GtkGrid* grid = GTK_GRID(gtk_grid_new());
        gtk_grid_set_column_spacing(grid, 3);
        gtk_grid_set_row_spacing(grid, 3);
        for (size_t i = 0; i < CLASSROOM_COLOURS.size(); ++i) {
            const auto& entry = CLASSROOM_COLOURS[i];
            GtkWidget* button = utn::createColourSwatch(entry.value, _(entry.name));
            g_object_set_data(G_OBJECT(button), "utn-classroom-colour",
                              GUINT_TO_POINTER(static_cast<uint32_t>(entry.value)));
            g_object_set_data(G_OBJECT(button), "utn-popover", popover);
            g_signal_connect(button, "clicked", G_CALLBACK(+[](GtkButton* button, gpointer data) {
                                 auto colour = Color(GPOINTER_TO_UINT(
                                         g_object_get_data(G_OBJECT(button), "utn-classroom-colour")));
                                 // Close while this widget is still alive; applying the colour may
                                 // trigger a toolbar rebuild that destroys the popover.
                                 auto* popup = GTK_POPOVER(g_object_get_data(G_OBJECT(button), "utn-popover"));
                                 gtk_popover_popdown(popup);
                                 teacherApplyColour(static_cast<Control*>(data), colour);
                             }), control);
            gtk_grid_attach(grid, button, static_cast<int>(i % 4), static_cast<int>(i / 4), 1, 1);
        }
        gtk_box_append(panel, GTK_WIDGET(grid));

        GtkBox* customRow = GTK_BOX(gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8));
        GtkWidget* customLabel = gtk_label_new(_("Custom colour"));
        GtkWidget* custom = gtk_color_button_new();
        gtk_color_chooser_set_use_alpha(GTK_COLOR_CHOOSER(custom), false);
        gtk_widget_set_tooltip_text(custom, _("Open the full colour picker"));
        utn::setAccessibleName(custom, _("Custom colour picker"));
        g_signal_connect(custom, "color-set", G_CALLBACK(+[](GtkColorButton* button, gpointer data) {
                             GdkRGBA rgba{};
                             gtk_color_chooser_get_rgba(GTK_COLOR_CHOOSER(button), &rgba);
                             teacherApplyColour(static_cast<Control*>(data), Util::GdkRGBA_to_rgb(rgba));
                         }), control);
        gtk_box_append(customRow, customLabel);
        gtk_box_append(customRow, custom);
        gtk_box_append(panel, GTK_WIDGET(customRow));

        g_object_set_data(G_OBJECT(popover), "utn-colour-grid", grid);
        g_object_set_data(G_OBJECT(popover), "utn-custom-colour", custom);
        g_signal_connect(popover, "map", G_CALLBACK(+[](GtkWidget* widget, gpointer data) {
                             refreshClassroomColours(GTK_POPOVER(widget), static_cast<Control*>(data));
                         }), control);

        GtkMenuButton* menu = GTK_MENU_BUTTON(gtk_menu_button_new());
        gtk_button_set_child(GTK_BUTTON(menu), ColorIcon::newGtkImage(teacherCurrentColour(control), 22, false));
        gtk_widget_set_tooltip_text(GTK_WIDGET(menu), _("Quick colours and custom colour picker"));
        utn::setAccessibleName(GTK_WIDGET(menu), _("Colours"));
        gtk_menu_button_set_popover(menu, GTK_WIDGET(popover));
        gtk_menu_button_set_direction(menu, horizontal ? GTK_ARROW_DOWN : GTK_ARROW_RIGHT);
        gtk_widget_show_all(GTK_WIDGET(panel));
        return xoj::util::WidgetSPtr(GTK_WIDGET(menu), xoj::util::adopt);
    }

    std::string getToolDisplayName() const override { return _("Colours"); }
    GtkWidget* getNewToolIcon() const override {
        return ColorIcon::newGtkImage(teacherCurrentColour(control), 22, false);
    }

private:
    Control* control;
};

class UtnBrandItem: public AbstractToolItem {
public:
    UtnBrandItem(): AbstractToolItem("UTN_BRAND", Category::MISC) {}

    xoj::util::WidgetSPtr createItem(bool) override {
        auto* label = gtk_label_new("UTN");
        gtk_widget_add_css_class(label, "utn-brand");
        gtk_widget_set_tooltip_text(label, _("Ultimate Teacher Notepad. Your classroom workspace."));
        return xoj::util::WidgetSPtr(label, xoj::util::adopt);
    }

    std::string getToolDisplayName() const override { return "UTN"; }
    GtkWidget* getNewToolIcon() const override {
        return gtk_image_new_from_icon_name("document-edit", GTK_ICON_SIZE_SMALL_TOOLBAR);
    }
};
}  // namespace

using std::string;

ToolMenuHandler::ToolMenuHandler(Control* control, GladeGui* gui):
        parent(GTK_WINDOW(gui->getWindow())),
        control(control),
        zoom(control->getZoomControl()),
        gui(gui),
        toolHandler(control->getToolHandler()),
        tbModel(std::make_unique<ToolbarModel>()),
        pageBackgroundChangeController(control->getPageBackgroundChangeController()),
        iconNameHelper(control->getSettings()),
        pageTypeSelectionPopup(std::make_unique<PageTypeSelectionPopover>(control, control->getSettings(),
                                                                          GTK_APPLICATION_WINDOW(parent))) {}

void ToolMenuHandler::populate(const GladeSearchpath* gladeSearchPath) {
    initToolItems();

    auto file = gladeSearchPath->findFile("", "toolbar.ini");
    if (!tbModel->parse(file, true, this->control->getPalette())) {
        std::string msg = FS(_F("Could not parse general toolbar.ini file: {1}\n"
                                "No Toolbars will be available") %
                             file.u8string());
        XojMsgBox::showErrorToUser(control->getGtkWindow(), msg);
    }

    file = Util::getConfigFile(TOOLBAR_CONFIG);
    if (fs::exists(file)) {
        if (!tbModel->parse(file, false, this->control->getPalette())) {
            string msg = FS(_F("Could not parse custom toolbar.ini file: {1}\n"
                               "Toolbars will not be available") %
                            file.u8string());
            XojMsgBox::showErrorToUser(control->getGtkWindow(), msg);
        }
    }
}

ToolMenuHandler::~ToolMenuHandler() = default;

void ToolMenuHandler::freeDynamicToolbarItems() { this->toolbarColorItems.clear(); }

void ToolMenuHandler::unloadToolbar(GtkWidget* toolbar) {
    for (int i = gtk_toolbar_get_n_items(GTK_TOOLBAR(toolbar)) - 1; i >= 0; i--) {
        GtkToolItem* tbItem = gtk_toolbar_get_nth_item(GTK_TOOLBAR(toolbar), i);
        gtk_container_remove(GTK_CONTAINER(toolbar), GTK_WIDGET(tbItem));
    }

    gtk_widget_hide(toolbar);
}

namespace {
// Teacher tools use a compact icon rail. Descriptive tooltips and accessible names
// replace permanently visible labels, keeping every action reachable on small screens.
/// Visible name and one-line hover explanation for teacher-layout toolbar items.
struct TeacherToolText {
    const char* id;
    const char* label;  ///< nullptr: use the item's display name
    const char* hint;
};

constexpr TeacherToolText TEACHER_TOOL_TEXT[] = {
        {"UTN_SELECT", nullptr, N_("Click an object to move, resize or delete it")},
        {"PEN", nullptr, N_("Write and draw freehand")},
        {"SMART_HIGHLIGHTER", nullptr, N_("Highlight text. The arrow chooses freehand, snapping or underline")},
        {"UTN_COLOURS", N_("Colours"), N_("Quick swatches and a full custom colour picker")},
        {"ERASER", nullptr, N_("Erase ink. Text and Answer Boxes stay; use Select to delete them")},
        {"UTN_TEXT", nullptr, N_("Click the page to type")},
        {"ANSWER_BOX", nullptr, N_("Click the page to add a styled box for an answer")},
        {"TEACHER_STAMP", N_("Feedback"), N_("Place a comment from the Feedback Bank")},
        {"TEACHING_KIT", N_("Shapes & Lines"), N_("Lines, rectangles, arrows and maths tools")},
        {"PREPARE_REVEAL", nullptr, N_("Prepare answers in advance, hide them and reveal them during the lesson")},
        {"SELECT", N_("Area Select"), N_("Drag around several objects to select them")},
        {"HAND", N_("Move Page"), N_("Drag to move around the page")},
};

/// GTK shows the tooltip of the deepest widget under the pointer, so composite items (a tool toggle plus a mode
/// arrow) need the explanation on their main button as well as on the tool item.
void setTeacherTooltip(GtkWidget* item, const char* markup) {
    gtk_widget_set_tooltip_markup(item, markup);
    GtkWidget* child = GTK_IS_BIN(item) ? gtk_bin_get_child(GTK_BIN(item)) : nullptr;
    if (child && GTK_IS_BUTTON(child)) {
        gtk_widget_set_tooltip_markup(child, markup);
    } else if (child && GTK_IS_BOX(child)) {
        GList* children = gtk_container_get_children(GTK_CONTAINER(child));
        if (children && GTK_IS_TOGGLE_BUTTON(children->data)) {
            gtk_widget_set_tooltip_markup(GTK_WIDGET(children->data), markup);
        }
        g_list_free(children);
    }
}

const TeacherToolText* teacherToolText(const std::string& id) {
    for (const auto& entry: TEACHER_TOOL_TEXT) {
        if (id == entry.id) {
            return &entry;
        }
    }
    return nullptr;
}
}  // namespace

void ToolMenuHandler::load(const ToolbarData* d, GtkWidget* toolbar, const char* toolbarName, bool horizontal) {
    int count = 0;
    const auto palette = this->control->getPalette();

    const auto& layoutId = d->getId();
    const bool teacherLayout = utn::isTeacherLayout(layoutId);
    if (!horizontal) {
        gtk_widget_set_hexpand(toolbar, false);
    }
    const auto& recolorParams = control->getSettings()->getRecolorParameters();
    auto recolor = recolorParams.recolorizeMainView ? std::make_optional(recolorParams.recolor) : std::nullopt;

    for (const ToolbarEntry& e: d->contents) {
        if (e.getName() == toolbarName) {
            for (const ToolbarItem& dataItem: e.getItems()) {
                std::string name = dataItem.getName();

                if (!this->control->getAudioController() &&
                    (name == "AUDIO_RECORDING" || name == "AUDIO_SEEK_BACKWARDS" || name == "AUDIO_PAUSE_PLAYBACK" ||
                     name == "AUDIO_STOP_PLAYBACK" || name == "AUDIO_SEEK_FORWARDS" || name == "PLAY_OBJECT")) {
                    continue;
                }

                if (StringUtils::startsWith(name, "COLOR(") && StringUtils::endsWith(name, ")")) {
                    std::string arg = name.substr(6, name.length() - 7);

                    size_t paletteIndex{};
                    std::istringstream colorStream(arg);
                    colorStream >> paletteIndex;
                    if (!colorStream.eof() || colorStream.fail()) {
                        g_warning("Toolbar:COLOR(N) has wrong format: %s", arg.c_str());
                        continue;
                    }

                    count++;
                    const NamedColor& namedColor = palette.getColorAt(paletteIndex);
                    auto& item =
                            this->toolbarColorItems.emplace_back(std::make_unique<ColorToolItem>(namedColor, recolor));

                    auto it = item->createToolItem(horizontal);
                    gtk_toolbar_insert(GTK_TOOLBAR(toolbar), GTK_TOOL_ITEM(it.get()), -1);

                    ToolitemDragDrop::attachMetadataColor(it.get(), dataItem.getId(), paletteIndex, item.get());

                    continue;
                }

                bool found = false;
                for (auto& item: this->toolItems) {
                    if (name == item->getId()) {
                        count++;
                        auto it = item->createToolItem(horizontal);
                        if (teacherLayout) {
                            const TeacherToolText* text = teacherToolText(name);
                            const std::string label =
                                    text && text->label ? _(text->label) : item->getToolDisplayName();
                            if (text) {
                                // Name first, then what the tool does: readable for new teachers, quick to scan.
                                auto markup = xoj::util::OwnedCString::assumeOwnership(g_markup_printf_escaped(
                                        "<b>%s</b>\n%s", label.c_str(), _(text->hint)));
                                setTeacherTooltip(it.get(), markup.get());
                            } else {
                                gtk_widget_set_tooltip_text(it.get(), label.c_str());
                            }
                            if (!horizontal) {
                                utn::setAccessibleName(it.get(), label.c_str());
                                if (GtkWidget* child = gtk_bin_get_child(GTK_BIN(it.get()))) {
                                    utn::setAccessibleName(child, label.c_str());
                                }
                            }
                        }
                        gtk_toolbar_insert(GTK_TOOLBAR(toolbar), GTK_TOOL_ITEM(it.get()), -1);
                        if (teacherLayout && !horizontal) {
                            g_signal_connect(it.get(), "toolbar-reconfigured", G_CALLBACK(+[](GtkToolItem* it, gpointer) {
                                auto* child = gtk_bin_get_child(GTK_BIN(it));
                                if (GTK_IS_BOX(child)) {
                                    gtk_orientable_set_orientation(GTK_ORIENTABLE(child), GTK_ORIENTATION_HORIZONTAL);
                                }
                            }), nullptr);
                            auto* child = gtk_bin_get_child(GTK_BIN(it.get()));
                            if (GTK_IS_BOX(child)) {
                                gtk_orientable_set_orientation(GTK_ORIENTABLE(child), GTK_ORIENTATION_HORIZONTAL);
                            }
                        }

                        ToolitemDragDrop::attachMetadata(it.get(), dataItem.getId(), item.get());

                        found = true;
                        break;
                    }
                }
                if (!found) {
                    g_warning("Toolbar item \"%s\" not found!", name.c_str());
                }
            }

            break;
        }
    }

    if (count == 0) {
        gtk_widget_hide(toolbar);
    } else {
        gtk_widget_show(toolbar);
    }
}

void ToolMenuHandler::removeColorToolItem(AbstractToolItem* it) {
    g_return_if_fail(it != nullptr);
    this->toolbarColorItems.erase(std::find_if(this->toolbarColorItems.begin(), this->toolbarColorItems.end(),
                                               [it](const auto& p) { return p.get() == it; }));
}

void ToolMenuHandler::addColorToolItem(std::unique_ptr<ColorToolItem> it) {
    g_return_if_fail(it != nullptr);
    this->toolbarColorItems.emplace_back(std::move(it));
}

template <class tool_item, class... Args>
tool_item& ToolMenuHandler::emplaceItem(Args&&... args) {
    return static_cast<tool_item&>(*toolItems.emplace_back(std::make_unique<tool_item>(std::forward<Args>(args)...)));
}

#ifdef ENABLE_PLUGINS
void ToolMenuHandler::addPluginItem(ToolbarButtonEntry* t) { emplaceItem<PluginToolButton>(t); }
void ToolMenuHandler::addPluginPlaceholderItem(ToolbarPlaceholderEntry* entry) {
    emplaceItem<PluginPlaceholderLabel>(entry);
}
#endif /* ENABLE_PLUGINS */


void ToolMenuHandler::initToolItems() {
    using Cat = AbstractToolItem::Category;
    /**
     * @brief Simple button, with a GTK stock icon name
     *      The corresponding action in ActionDatabase[action] should have no state (it can have a parameter)
     **/
    auto emplaceStockItem = [this](const char* name, Cat c, Action action, const char* icon, std::string description) {
        emplaceItem<ToolButton>(name, c, action, icon, description, false);
    };
    /**
     * @brief Simple button, with a custom loaded icon
     *      The corresponding action in ActionDatabase[action] should have no state (it can have a parameter)
     **/
    auto emplaceCustomItem = [this](const char* name, Cat c, Action action, const char* icon, std::string description) {
        emplaceItem<ToolButton>(name, c, action, iconName(icon), description, false);
    };

    /**
     * @brief Toggle button, with a GTK stock icon name
     *      The corresponding action in ActionDatabase[action] should have a boolean state and no parameter
     **/
    auto emplaceStockItemTgl = [this](const char* name, Cat c, Action action, const char* icon,
                                      std::string description) {
        emplaceItem<ToolButton>(name, c, action, icon, description, true);
    };

    /**
     * @brief Toggle button, with a custom loaded icon
     *      The corresponding action in ActionDatabase[action] should have a boolean state and no parameter
     **/
    auto emplaceCustomItemTgl = [this](const char* name, Cat c, Action action, const char* icon,
                                       std::string description) {
        emplaceItem<ToolButton>(name, c, action, iconName(icon), description, true);
    };

    /**
     * @brief Toggle button linked to others sharing the same action (with a custom loaded icon)
     *      The corresponding action in ActionDatabase[action] should have a state and a parameter. The button is "on"
     *when the action state matches `target`.
     **/
    auto emplaceStockItemWithTarget = [this](const char* name, Cat c, Action action, auto target, const char* icon,
                                             std::string description) {
        emplaceItem<ToolButton>(name, c, action, makeGVariant(target), icon, description);
    };

    /**
     * @brief Toggle button linked to others sharing the same action (with a custom loaded icon)
     *      The corresponding action in ActionDatabase[action] should have a state and a parameter. The button is "on"
     *when the action state matches `target`.
     **/
    auto emplaceCustomItemWithTarget = [this](const char* name, Cat c, Action action, auto target, const char* icon,
                                              std::string description) {
        emplaceItem<ToolButton>(name, c, action, makeGVariant(target), iconName(icon), description);
    };

    /**
     * @brief Simple button (with a custom loaded icon), with a popover menu to change parameters of the tool
     *      The corresponding action in ActionDatabase[action] should have no state (it can have a parameter)
     **/
    auto emplaceCustomItemWithPopover = [this](const char* name, Cat c, Action action, const char* icon,
                                               std::string description, const PopoverFactory* popover) {
        auto&& tb = emplaceItem<ToolButton>(name, c, action, iconName(icon), description, false);
        tb.setPopoverFactory(popover);
    };

    auto emplaceCustomItemWithTargetAndMenu = [this](const char* name, Cat c, Action action, auto target,
                                                     const char* icon, std::string description,
                                                     const PopoverFactory* popover) {
        auto&& tb = emplaceItem<ToolButton>(name, c, action, makeGVariant(target), iconName(icon), description);
        tb.setPopoverFactory(popover);
    };


    /*
     * Items ordered by menu, if possible.
     * There are some entries which are not available in the menu, like the Zoom slider
     * All menu items without tool icon are not listed here - they are connected by ActionDatabase
     */

    /*
     * Menu File
     * ------------------------------------------------------------------------
     */

    emplaceCustomItem("NEW", Cat::FILES, Action::NEW_FILE, "document-new", _("New Xournal"));
    emplaceCustomItem("OPEN", Cat::FILES, Action::OPEN, "document-open", _("Open file"));
    emplaceCustomItem("SAVE", Cat::FILES, Action::SAVE, "document-save", _("Save"));
    emplaceCustomItem("SAVEPDF", Cat::FILES, Action::EXPORT_AS_PDF, "document-export-pdf", _("Export as PDF"));
    emplaceCustomItem("PRINT", Cat::FILES, Action::PRINT, "document-print", _("Print"));

    /*
     * Menu Edit
     * ------------------------------------------------------------------------
     */

    // Undo / Redo Texts are updated
    emplaceItem<TooltipToolButton>(
            "UNDO", Cat::MISC, Action::UNDO, iconName("edit-undo"), _("Undo"),
            [undoredo = control->getUndoRedoHandler()]() { return undoredo->undoDescription(); });
    emplaceItem<TooltipToolButton>(
            "REDO", Cat::MISC, Action::REDO, iconName("edit-redo"), _("Redo"),
            [undoredo = control->getUndoRedoHandler()]() { return undoredo->redoDescription(); });

    emplaceCustomItem("CUT", Cat::MISC, Action::CUT, "edit-cut", _("Cut"));
    emplaceCustomItem("COPY", Cat::MISC, Action::COPY, "edit-copy", _("Copy"));
    emplaceCustomItem("PASTE", Cat::MISC, Action::PASTE, "edit-paste", _("Paste"));

    emplaceStockItem("SEARCH", Cat::MISC, Action::SEARCH, "edit-find", _("Search"));

    emplaceStockItem("DELETE", Cat::MISC, Action::DELETE, "edit-delete", _("Delete"));

    emplaceCustomItemTgl("ROTATION_SNAPPING", Cat::MISC, Action::ROTATION_SNAPPING, "snapping-rotation",
                         _("Rotation Snapping"));
    emplaceCustomItemTgl("GRID_SNAPPING", Cat::MISC, Action::GRID_SNAPPING, "snapping-grid", _("Grid Snapping"));

    /*
     * Menu View
     * ------------------------------------------------------------------------
     */

    emplaceCustomItemTgl("PAIRED_PAGES", Cat::NAVIGATION, Action::PAIRED_PAGES_MODE, "show-paired-pages",
                         _("Paired pages"));
    emplaceCustomItemTgl("PRESENTATION_MODE", Cat::NAVIGATION, Action::PRESENTATION_MODE, "presentation-mode",
                         _("Presentation mode"));
    emplaceCustomItemTgl("FULLSCREEN", Cat::NAVIGATION, Action::FULLSCREEN, "fullscreen", _("Toggle fullscreen"));
    emplaceCustomItemTgl("SHOW_SIDEBAR", Cat::NAVIGATION, Action::SHOW_SIDEBAR, "sidebar-show", _("Toggle sidebar"));

    emplaceCustomItem("MANAGE_TOOLBAR", Cat::MISC, Action::MANAGE_TOOLBAR, "toolbars-manage", _("Manage Toolbars"));
    emplaceCustomItem("CUSTOMIZE_TOOLBAR", Cat::MISC, Action::CUSTOMIZE_TOOLBAR, "toolbars-customize",
                      _("Customize Toolbars"));

    emplaceStockItem("ZOOM_OUT", Cat::NAVIGATION, Action::ZOOM_OUT, "zoom-out", _("Zoom out"));
    emplaceStockItem("ZOOM_IN", Cat::NAVIGATION, Action::ZOOM_IN, "zoom-in", _("Zoom in"));
    emplaceStockItemTgl("ZOOM_FIT", Cat::NAVIGATION, Action::ZOOM_FIT, "zoom-fit-best", _("Zoom fit to screen"));
    emplaceStockItem("ZOOM_100", Cat::NAVIGATION, Action::ZOOM_100, "zoom-original", _("Zoom to 100%"));

    /*
     * Menu Navigation
     * ------------------------------------------------------------------------
     */

    emplaceStockItem("GOTO_FIRST", Cat::NAVIGATION, Action::GOTO_FIRST, "go-first", _("Go to first page"));
    emplaceStockItem("GOTO_BACK", Cat::NAVIGATION, Action::GOTO_PREVIOUS, "go-previous", _("Back"));
    emplaceCustomItem("NAVIGATE_BACK", Cat::NAVIGATION, Action::NAVIGATE_BACK, "navigate-back", _("Jump back"));
    emplaceCustomItem("GOTO_PAGE", Cat::NAVIGATION, Action::GOTO_PAGE, "go-to", _("Go to page"));
    emplaceCustomItem("NAVIGATE_FORWARD", Cat::NAVIGATION, Action::NAVIGATE_FORWARD, "navigate-forward",
                      _("Jump forward"));
    emplaceStockItem("GOTO_NEXT", Cat::NAVIGATION, Action::GOTO_NEXT, "go-next", _("Next"));
    emplaceStockItem("GOTO_LAST", Cat::NAVIGATION, Action::GOTO_LAST, "go-last", _("Go to last page"));

    emplaceStockItem("GOTO_PREVIOUS_LAYER", Cat::NAVIGATION, Action::LAYER_GOTO_PREVIOUS, "go-previous",
                     _("Go to previous layer"));
    emplaceStockItem("GOTO_NEXT_LAYER", Cat::NAVIGATION, Action::LAYER_GOTO_NEXT, "go-next", _("Go to next layer"));
    emplaceStockItem("GOTO_TOP_LAYER", Cat::NAVIGATION, Action::LAYER_GOTO_TOP, "go-top", _("Go to top layer"));

    emplaceCustomItem("GOTO_NEXT_ANNOTATED_PAGE", Cat::NAVIGATION, Action::GOTO_NEXT_ANNOTATED_PAGE,
                      "page-annotated-next", _("Next annotated page"));

    /* Menu Journal
     * ------------------------------------------------------------------------
     */

    emplaceCustomItemWithPopover("INSERT_NEW_PAGE", Cat::TOOLS, Action::NEW_PAGE_AFTER, "page-add", _("Insert page"),
                                 this->pageTypeSelectionPopup.get());
    emplaceCustomItem("DELETE_CURRENT_PAGE", Cat::TOOLS, Action::DELETE_PAGE, "page-delete", _("Delete current page"));

    /*
     * Menu Tool
     * ------------------------------------------------------------------------
     */
    this->penLineStylePopover = std::make_unique<StylePopoverFactory>(
            Action::TOOL_PEN_LINE_STYLE,
            std::vector<StylePopoverFactory::Entry>{{_("standard"), iconName("line-style-plain"), "plain"},
                                                    {_("dashed"), iconName("line-style-dash"), "dash"},
                                                    {_("dash-/ dotted"), iconName("line-style-dash-dot"), "dashdot"},
                                                    {_("dotted"), iconName("line-style-dot"), "dot"}});
    emplaceCustomItemWithTargetAndMenu("PEN", Cat::TOOLS, Action::SELECT_TOOL, TOOL_PEN, "tool-pencil", _("Pen"),
                                       this->penLineStylePopover.get());

    this->eraserTypePopover = std::make_unique<StylePopoverFactory>(
            Action::TOOL_ERASER_TYPE,
            std::vector<StylePopoverFactory::Entry>{{_("standard"), ERASER_TYPE_DEFAULT},
                                                    {_("whiteout"), ERASER_TYPE_WHITEOUT},
                                                    {_("delete stroke"), ERASER_TYPE_DELETE_STROKE},
                                                    {_("delete object"), ERASER_TYPE_DELETE_OBJECT}});
    emplaceCustomItemWithTargetAndMenu("ERASER", Cat::TOOLS, Action::SELECT_TOOL, TOOL_ERASER, "tool-eraser",
                                       _("Eraser"), this->eraserTypePopover.get());

    // Add individual line styles as toolbar items
    emplaceCustomItemWithTarget("PLAIN", Cat::TOOLS, Action::TOOL_PEN_LINE_STYLE, "plain", "line-style-plain-with-pen",
                                _("standard"));
    emplaceCustomItemWithTarget("DASHED", Cat::TOOLS, Action::TOOL_PEN_LINE_STYLE, "dash", "line-style-dash-with-pen",
                                _("dashed"));
    emplaceCustomItemWithTarget("DASH-/ DOTTED", Cat::TOOLS, Action::TOOL_PEN_LINE_STYLE, "dashdot",
                                "line-style-dash-dot-with-pen", _("dash-/ dotted"));
    emplaceCustomItemWithTarget("DOTTED", Cat::TOOLS, Action::TOOL_PEN_LINE_STYLE, "dot", "line-style-dot-with-pen",
                                _("dotted"));


    emplaceCustomItemWithTarget("HIGHLIGHTER", Cat::TOOLS, Action::SELECT_TOOL, TOOL_HIGHLIGHTER, "tool-highlighter",
                                _("Highlighter"));

    // UTN smart highlighter mode
    emplaceItem<ToolSmartHighlighter>("SMART_HIGHLIGHTER", control, iconNameHelper);

    emplaceCustomItemWithTarget("TEXT", Cat::TOOLS, Action::SELECT_TOOL, TOOL_TEXT, "tool-text", _("Text"));

    // UTN shell controls
    emplaceItem<ToolAppearance>("UTN_APPEARANCE", control, iconNameHelper);
    emplaceItem<ToolClear>("UTN_CLEAR", control);
    emplaceItem<ToolClassroomWorkflow>("CLASSROOM_WORKFLOW", control, iconNameHelper);
    emplaceItem<UtnBrandItem>();
    emplaceItem<ToolWorkspaceSwitch>("UTN_WORKSPACE", control);
    emplaceItem<ToolUtnContextBar>("UTN_CONTEXT", control);

    // UTN answer box text mode
    // Teacher text modes: their rail buttons are selected only for their own mode
    emplaceItem<ToolTextMode>("UTN_TEXT", control, iconNameHelper, TextMode::Plain);
    emplaceItem<ToolTextMode>("ANSWER_BOX", control, iconNameHelper, TextMode::AnswerBox);

    // UTN teacher stamps
    emplaceItem<ToolTeacherStamp>("TEACHER_STAMP", control, iconNameHelper);

    // UTN prepared answer layers
    emplaceItem<ToolPrepareReveal>("PREPARE_REVEAL", control, iconNameHelper);

    // UTN teacher tool profiles
    emplaceItem<ToolProfileSelector>("TOOL_PROFILES", control);

    // UTN classroom page labels

    // UTN classroom presentation tools
    emplaceItem<ToolPresentationKit>("PRESENTATION_KIT", control, iconNameHelper);

    // UTN clean student-facing preview
    emplaceItem<ToolStudentView>("STUDENT_VIEW", control);

    // UTN consolidated shapes, STEM and classroom utilities
    emplaceItem<ToolTeachingKit>("TEACHING_KIT", control, iconNameHelper);
    emplaceItem<ToolClassroomColours>(control);
    emplaceCustomItemWithTarget("LINK", Cat::TOOLS, Action::SELECT_TOOL, TOOL_LINK, "tool-link", _("Add/Edit Link"));
    emplaceCustomItemWithTarget("MATH_TEX", Cat::TOOLS, Action::SELECT_TOOL, TOOL_LATEX, "tool-math-tex",
                                _("Add/Edit TeX"));
    emplaceCustomItemWithTarget("IMAGE", Cat::TOOLS, Action::SELECT_TOOL, TOOL_IMAGE, "tool-image", _("Image"));
    emplaceCustomItem("DEFAULT_TOOL", Cat::TOOLS, Action::SELECT_DEFAULT_TOOL, "default", _("Default Tool"));
    emplaceCustomItemWithTarget("SELECT_PDF_TEXT_LINEAR", Cat::SELECTION, Action::SELECT_TOOL,
                                TOOL_SELECT_PDF_TEXT_LINEAR, "select-pdf-text-ht", _("Select Linear PDF Text"));
    emplaceCustomItemWithTarget("SELECT_PDF_TEXT_RECT", Cat::SELECTION, Action::SELECT_TOOL, TOOL_SELECT_PDF_TEXT_RECT,
                                "select-pdf-text-area", _("Select PDF Text in Rectangle"));

    emplaceCustomItemTgl("SETSQUARE", Cat::MISC, Action::SETSQUARE, "setsquare", _("Setsquare"));
    emplaceCustomItemTgl("COMPASS", Cat::MISC, Action::COMPASS, "compass", _("Compass"));

    emplaceCustomItemTgl("TOGGLE_TOUCH_DRAWING", Cat::MISC, Action::TOGGLE_TOUCH_DRAWING, "touch-drawing",
                         _("Toggle Touch Drawing"));


    emplaceCustomItemTgl("SHAPE_RECOGNIZER", Cat::TOOLS, Action::TOOL_DRAW_SHAPE_RECOGNIZER, "shape-recognizer",
                         _("Shape Recognizer"));
    emplaceCustomItemTgl("DRAW_RECTANGLE", Cat::TOOLS, Action::TOOL_DRAW_RECTANGLE, "draw-rect", _("Draw Rectangle"));
    emplaceCustomItemTgl("DRAW_ELLIPSE", Cat::TOOLS, Action::TOOL_DRAW_ELLIPSE, "draw-ellipse", _("Draw Ellipse"));
    emplaceCustomItemTgl("DRAW_ARROW", Cat::TOOLS, Action::TOOL_DRAW_ARROW, "draw-arrow", _("Draw Arrow"));
    emplaceCustomItemTgl("DRAW_DOUBLE_ARROW", Cat::TOOLS, Action::TOOL_DRAW_DOUBLE_ARROW, "draw-double-arrow",
                         _("Draw Double Arrow"));
    emplaceCustomItemTgl("DRAW_COORDINATE_SYSTEM", Cat::TOOLS, Action::TOOL_DRAW_COORDINATE_SYSTEM,
                         "draw-coordinate-system", _("Draw Coordinate System"));
    emplaceCustomItemTgl("RULER", Cat::TOOLS, Action::TOOL_DRAW_LINE, "draw-line", _("Draw Line"));
    emplaceCustomItemTgl("DRAW_SPLINE", Cat::TOOLS, Action::TOOL_DRAW_SPLINE, "draw-spline", _("Draw Spline"));

    emplaceCustomItemWithTarget("SELECT_REGION", Cat::SELECTION, Action::SELECT_TOOL, TOOL_SELECT_REGION,
                                "select-lasso", _("Select Region"));
    emplaceCustomItemWithTarget("SELECT_RECTANGLE", Cat::SELECTION, Action::SELECT_TOOL, TOOL_SELECT_RECT,
                                "select-rect", _("Select Rectangle"));
    emplaceCustomItemWithTarget("SELECT_MULTILAYER_REGION", Cat::SELECTION, Action::SELECT_TOOL,
                                TOOL_SELECT_MULTILAYER_REGION, "select-multilayer-lasso",
                                _("Select Multi-Layer Region"));
    emplaceCustomItemWithTarget("SELECT_MULTILAYER_RECTANGLE", Cat::SELECTION, Action::SELECT_TOOL,
                                TOOL_SELECT_MULTILAYER_RECT, "select-multilayer-rect",
                                _("Select Multi-Layer Rectangle"));
    emplaceCustomItemWithTarget("UTN_SELECT", Cat::SELECTION, Action::SELECT_TOOL, TOOL_SELECT_OBJECT,
                                "utn-select", _("Select"));
    emplaceCustomItemWithTarget("SELECT_OBJECT", Cat::SELECTION, Action::SELECT_TOOL, TOOL_SELECT_OBJECT,
                                "object-select", _("Select Object"));
    emplaceCustomItemWithTarget("VERTICAL_SPACE", Cat::SELECTION, Action::SELECT_TOOL, TOOL_VERTICAL_SPACE,
                                "vertical-space", _("Vertical Space"));
    emplaceCustomItemWithTarget("PLAY_OBJECT", Cat::SELECTION, Action::SELECT_TOOL, TOOL_PLAY_OBJECT, "object-play",
                                _("Play Object"));
    emplaceCustomItemWithTarget("HAND", Cat::SELECTION, Action::SELECT_TOOL, TOOL_HAND, "hand", _("Hand"));

    emplaceItem<FontButton>("SELECT_FONT", *control->getActionDatabase());
    emplaceStockItemTgl("FORMAT_JUSTIFY", Cat::TOOLS, Action::TEXT_JUSTIFY, "format-justify-fill", _("Justify text"));
    emplaceStockItemWithTarget("FORMAT_ALIGN_LEFT", Cat::TOOLS, Action::TEXT_ALIGNMENT, TextAlignment::LEFT,
                               "format-justify-left", _("Align text to the left"));
    emplaceStockItemWithTarget("FORMAT_ALIGN_CENTER", Cat::TOOLS, Action::TEXT_ALIGNMENT, TextAlignment::CENTER,
                               "format-justify-center", _("Center text"));
    emplaceStockItemWithTarget("FORMAT_ALIGN_RIGHT", Cat::TOOLS, Action::TEXT_ALIGNMENT, TextAlignment::RIGHT,
                               "format-justify-right", _("Align text to the right"));

    emplaceCustomItemTgl("AUDIO_RECORDING", Cat::AUDIO, Action::AUDIO_RECORD, "audio-record",
                         _("Record Audio / Stop Recording"));
    emplaceCustomItemTgl("AUDIO_PAUSE_PLAYBACK", Cat::AUDIO, Action::AUDIO_PAUSE_PLAYBACK, "audio-playback-pause",
                         _("Pause / Play"));
    emplaceCustomItem("AUDIO_STOP_PLAYBACK", Cat::AUDIO, Action::AUDIO_STOP_PLAYBACK, "audio-playback-stop", _("Stop"));
    emplaceCustomItem("AUDIO_SEEK_FORWARDS", Cat::AUDIO, Action::AUDIO_SEEK_FORWARDS, "audio-seek-forwards",
                      _("Forward"));
    emplaceCustomItem("AUDIO_SEEK_BACKWARDS", Cat::AUDIO, Action::AUDIO_SEEK_BACKWARDS, "audio-seek-backwards",
                      _("Back"));


    ///////////////////////////////////////////////////////////////////////////


    /*
     * Footer tools
     * ------------------------------------------------------------------------
     */
    toolPageSpinner = &emplaceItem<ToolPageSpinner>("PAGE_SPIN", iconNameHelper, control->getScrollHandler());
    toolLessonNavigator = &emplaceItem<ToolLessonNavigator>("LESSON_NAVIGATOR", control);

    // UTN continuous eraser size slider
    emplaceItem<ToolEraserSizeSlider>("ERASER_SIZE_SLIDER", control, iconNameHelper);

    emplaceItem<ToolZoomSlider>("ZOOM_SLIDER", zoom, iconNameHelper, *control->getActionDatabase());

    emplaceItem<ToolPageLayer>("LAYER", control->getLayerController(), iconNameHelper);

    /*
     * Non-menu items
     * ------------------------------------------------------------------------
     */

    /*
     * Color item - not in the menu
     * aka. COLOR_SELECT
     */
    emplaceItem<ColorSelectorToolItem>(*control->getActionDatabase());

    bool hideAudio = !this->control->getAudioController();
    emplaceItem<ToolSelectCombocontrol>("SELECT", this->iconNameHelper, *this->control->getActionDatabase(), hideAudio);
    emplaceItem<DrawingTypeComboToolButton>("DRAW", this->iconNameHelper, *this->control->getActionDatabase());
    emplaceItem<ToolPdfCombocontrol>("PDF_TOOL", this->iconNameHelper, *this->control->getActionDatabase());

    auto laserIcon = this->iconNameHelper.iconName("laser-pointer");
    emplaceItem<ComboToolButton>(
            "LASER_POINTER", Cat::TOOLS, laserIcon, _("Laser pointer"),
            ComboToolButton::Entries{{_("Pen"), laserIcon, TOOL_LASER_POINTER_PEN},
                                     {_("Highlighter"), laserIcon, TOOL_LASER_POINTER_HIGHLIGHTER}},
            this->control->getActionDatabase()->getAction(Action::SELECT_TOOL));

    // General tool configuration - working for every tool which support it
    emplaceCustomItemTgl("TOOL_FILL", Cat::TOOLS, Action::TOOL_FILL, "fill", _("Fill"));
    emplaceCustomItem("FILL_OPACITY", Cat::TOOLS, Action::TOOL_FILL_OPACITY, "fill-opacity", _("Fill Opacity"));

    emplaceCustomItemWithTarget("VERY_FINE", Cat::TOOLS, Action::TOOL_SIZE, TOOL_SIZE_VERY_FINE, "thickness-finer",
                                _("Very Fine"));
    emplaceCustomItemWithTarget("FINE", Cat::TOOLS, Action::TOOL_SIZE, TOOL_SIZE_FINE, "thickness-fine", _("Fine"));
    emplaceCustomItemWithTarget("MEDIUM", Cat::TOOLS, Action::TOOL_SIZE, TOOL_SIZE_MEDIUM, "thickness-medium",
                                _("Medium"));
    emplaceCustomItemWithTarget("THICK", Cat::TOOLS, Action::TOOL_SIZE, TOOL_SIZE_THICK, "thickness-thick", _("Thick"));
    emplaceCustomItemWithTarget("VERY_THICK", Cat::TOOLS, Action::TOOL_SIZE, TOOL_SIZE_VERY_THICK, "thickness-thicker",
                                _("Very Thick"));

    emplaceItem<SeparatorItem>("SEPARATOR");
    emplaceItem<SpacerItem>("SPACER");
}

void ToolMenuHandler::setPageInfo(size_t currentPage, size_t pageCount, size_t pdfpage) {
    if (this->toolPageSpinner) {
        this->toolPageSpinner->setPageInfo(currentPage, pageCount, pdfpage);
    }
    if (this->toolLessonNavigator) {
        this->toolLessonNavigator->setPageInfo(currentPage, pageCount);
    }
}

auto ToolMenuHandler::getModel() -> ToolbarModel* { return this->tbModel.get(); }

auto ToolMenuHandler::getControl() -> Control* { return this->control; }

auto ToolMenuHandler::getToolItems() const -> const std::vector<std::unique_ptr<AbstractToolItem>>& {
    return this->toolItems;
}

auto ToolMenuHandler::getColorToolItems() const -> const std::vector<std::unique_ptr<ColorToolItem>>& {
    return this->toolbarColorItems;
}

auto ToolMenuHandler::iconName(const char* icon) -> std::string { return iconNameHelper.iconName(icon); }

void ToolMenuHandler::updateColorToolItems(const Palette& palette) {
    for (const auto& it: this->toolbarColorItems) {
        it->updateColor(palette);
    }
}

void ToolMenuHandler::updateColorToolItemsRecoloring(const std::optional<Recolor>& recolor) {
    for (const auto& it: this->toolbarColorItems) {
        it->updateSecondaryColor(recolor);
    }
}

void ToolMenuHandler::setDefaultNewPageType(const std::optional<PageType>& pt) {
    this->pageTypeSelectionPopup->setSelectedPT(pt);
    this->control->getPageBackgroundChangeController()->setPageTypeForNewPages(pt);
}
void ToolMenuHandler::setDefaultNewPaperSize(const std::optional<PaperSize>& paperSize) {
    this->pageTypeSelectionPopup->setSelectedPaperSize(paperSize);
}
