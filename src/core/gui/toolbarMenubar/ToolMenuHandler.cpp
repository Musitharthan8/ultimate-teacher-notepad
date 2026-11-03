#include "ToolMenuHandler.h"

#include "control/UtnLayout.h"  // for isTeacherLayout
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>  // for max
#include <cstdio>   // for snprintf
#include <sstream>    // for istringstream

#include "control/Control.h"                         // for Control
#include "control/PageBackgroundChangeController.h"  // for PageBackgroundChangeController
#include "control/ScrollHandler.h"                   // for ScrollHandler
#include "control/actions/ActionDatabase.h"          // for ActionDatabase
#include "gui/MainWindow.h"                           // for MainWindow::presentKeepsToolsAway
#include "gui/toolbarMenubar/ShapePicker.h"
#include "model/TeacherPalette.h"                    // for utn::palette
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

/// The colour the palette shows: the one every colour surface shows (Feedback and Answer Box text have their own)
Color teacherCurrentColour(Control* ctrl) { return ctrl->getToolHandler()->getDisplayedColor(); }

/// The one path every colour surface uses; the palette adds only the gate. Keeps the highlighter's opacity.
/// @p expectedKind is the palette kind (ink or highlighter) the control was drawn for, or -1 for a control that fits any
/// tool with a colour. A click that was already queued when a preset or shortcut changed the tool must not recolour the
/// new tool with a swatch of the old kind.
void teacherApplyColour(Control* ctrl, Color selected, int expectedKind = -1) {
    if (!ToolUtnContextBar::toolControlsReachable(ctrl)) {
        return;  // gone busy, covered, or Present put the tools away since the palette opened
    }
    auto* tools = ctrl->getToolHandler();
    if (!tools->hasCapability(TOOL_CAP_COLOR)) {
        return;  // the tool changed to one with no colour
    }
    if (expectedKind >= 0 && expectedKind != static_cast<int>(utn::palette::kindFor(tools->getToolType() == TOOL_HIGHLIGHTER))) {
        return;  // the tool changed to one this swatch is not for
    }
    ctrl->applyDisplayedColour(selected);
}

/// The rail's colour dot. The colour it shows is also kept on the button ("utn-rail-colour", 0xAARRGGBB) for tests.
void showRailColour(GtkWidget* menu, Color current) {
    gtk_button_set_child(GTK_BUTTON(menu), ColorIcon::newGtkImage(current, 22, true));
    g_object_set_data(G_OBJECT(menu), "utn-rail-colour", GUINT_TO_POINTER(static_cast<uint32_t>(current)));
}

/// Whether the colour controls may act: not while UTN is busy, not over the Blank cover, not in Present while its tools
/// are kept away (the same rules as the Command Palette and the selection toolbar)
bool paletteReachable(Control* ctrl) { return ToolUtnContextBar::toolControlsReachable(ctrl); }

/// A small upper-case section caption of the popover (COLOUR, WIDTH, STROKE)
GtkWidget* sectionCaption(const char* text) {
    gchar* upper = g_utf8_strup(text, -1);
    GtkWidget* label = gtk_label_new(upper);
    g_free(upper);
    gtk_widget_set_halign(label, GTK_ALIGN_START);
    gtk_widget_add_css_class(label, "utn-pop-caption");
    return label;
}

/// Which tab of the Teacher Annotation Palette stands for the active tool; -1 for a tool the palette does not paint
int paletteTabOf(ToolHandler* tools) {
    switch (tools->getToolType()) {
        case TOOL_PEN:
            return tools->getDrawingType() == DRAWING_TYPE_DEFAULT ? 0 : 2;
        case TOOL_HIGHLIGHTER:
            return 1;
        case TOOL_TEXT:
            return 3;
        default:
            return -1;
    }
}

const char* sizeWords(ToolSize size) {
    switch (size) {
        case TOOL_SIZE_VERY_FINE:
            return N_("Extra fine");
        case TOOL_SIZE_FINE:
            return N_("Fine");
        case TOOL_SIZE_MEDIUM:
            return N_("Medium");
        case TOOL_SIZE_THICK:
            return N_("Thick");
        case TOOL_SIZE_VERY_THICK:
            return N_("Extra thick");
        default:
            return nullptr;
    }
}

/// What the section under the swatches is built for: the tab and the text mode. Thickness, shape and snap controls live
/// in the palette for good and follow the tool in place, so a slider being dragged is never destroyed by the change it
/// causes.
std::string paletteSectionSignature(Control* ctrl) {
    auto* tools = ctrl->getToolHandler();
    return std::to_string(paletteTabOf(tools)) + "/" + std::to_string(static_cast<int>(tools->getTextMode()));
}

/// The pen or highlighter size the palette's width slider shows, or TOOL_SIZE_NONE for a tool that has no width here
ToolSize paletteSize(ToolHandler* tools) {
    return tools->getToolType() == TOOL_HIGHLIGHTER ? tools->getHighlighterSize() :
           tools->getToolType() == TOOL_PEN         ? tools->getPenSize() :
                                                      TOOL_SIZE_NONE;
}

/// The thickness in points that @p size writes with for the active tool
double paletteThicknessPt(ToolHandler* tools, ToolSize size) {
    return tools->getTool(tools->getToolType() == TOOL_HIGHLIGHTER ? TOOL_HIGHLIGHTER : TOOL_PEN).getThickness(size);
}

/// "2.5 pt"
std::string widthWords(double thickness) {
    char text[32];
    std::snprintf(text, sizeof(text), "%.1f", thickness);
    return std::string(text) + " " + _("pt");
}

/// Show the tool's size on the width slider, its words and the wave preview, without sending the change back
void syncPaletteWidth(GtkPopover* popover, Control* ctrl) {
    auto* scale = GTK_WIDGET(g_object_get_data(G_OBJECT(popover), "utn-width-scale"));
    auto* label = GTK_WIDGET(g_object_get_data(G_OBJECT(popover), "utn-width-label"));
    auto* preview = GTK_WIDGET(g_object_get_data(G_OBJECT(popover), "utn-width-preview"));
    if (!scale || !label || !preview) {
        return;
    }
    auto* tools = ctrl->getToolHandler();
    const ToolSize size = paletteSize(tools);
    if (size != TOOL_SIZE_NONE) {
        g_object_set_data(G_OBJECT(popover), "utn-width-syncing", GINT_TO_POINTER(1));
        gtk_range_set_value(GTK_RANGE(scale), static_cast<double>(size));
        g_object_set_data(G_OBJECT(popover), "utn-width-syncing", nullptr);
        const std::string words = widthWords(paletteThicknessPt(tools, size));
        gtk_label_set_text(GTK_LABEL(label), words.c_str());
        // The value is spoken with the name, as a slider's step number alone says nothing
        utn::setAccessibleName(scale, (std::string(_("Stroke width")) + ": " + words).c_str());
    }
    gtk_widget_queue_draw(preview);
}

/// "Red, Medium": the colour and thickness the active tool writes with, in words
void updatePaletteIndicator(GtkPopover* popover, Control* ctrl) {
    auto* label = GTK_LABEL(g_object_get_data(G_OBJECT(popover), "utn-indicator"));
    if (!label) {
        return;
    }
    auto* tools = ctrl->getToolHandler();
    const Color current = teacherCurrentColour(ctrl);
    const auto kind = utn::palette::kindFor(tools->getToolType() == TOOL_HIGHLIGHTER);
    const char* name = utn::palette::nameOf(kind, static_cast<uint32_t>(current));
    std::string words = *name ? std::string(_(name)) : utn::palette::hex(static_cast<uint32_t>(current));
    const ToolSize size = tools->getToolType() == TOOL_HIGHLIGHTER ? tools->getHighlighterSize() :
                          tools->getToolType() == TOOL_PEN         ? tools->getPenSize() :
                                                                     TOOL_SIZE_NONE;
    if (const char* sizeName = sizeWords(size)) {
        words += std::string(", ") + _(sizeName);
    }
    gtk_label_set_text(label, words.c_str());
    // The accessible name is the value shown, not a fixed phrase that hides it
    utn::setAccessibleName(GTK_WIDGET(label), (std::string(_("Active colour and thickness")) + ": " + words).c_str());
    syncPaletteWidth(popover, ctrl);  // the slider, its words and the preview follow the same size and colour
}

GtkWidget* findOpacityScale(GtkWidget* widget) {
    if (g_object_get_data(G_OBJECT(widget), "utn-opacity")) {
        return widget;
    }
    if (!GTK_IS_CONTAINER(widget)) {
        return nullptr;
    }
    GtkWidget* found = nullptr;
    GList* children = gtk_container_get_children(GTK_CONTAINER(widget));
    for (GList* child = children; child && !found; child = child->next) {
        found = findOpacityScale(GTK_WIDGET(child->data));
    }
    g_list_free(children);
    return found;
}

/// Rebuild the part of the palette that depends on the tool (the size, opacity, shape or text-size row) when the tool
/// changed, keep the opacity slider in step with the highlighter in place, and make it follow the gate
void refreshPaletteSection(GtkPopover* popover, Control* ctrl) {
    auto* holder = GTK_BOX(g_object_get_data(G_OBJECT(popover), "utn-section"));
    if (!holder) {
        return;
    }
    const int tab = paletteTabOf(ctrl->getToolHandler());
    const std::string signature = paletteSectionSignature(ctrl);
    auto* built = static_cast<const char*>(g_object_get_data(G_OBJECT(popover), "utn-section-signature"));
    if (!built || signature != built) {
        GList* old = gtk_container_get_children(GTK_CONTAINER(holder));
        for (GList* item = old; item; item = item->next) {
            gtk_widget_destroy(GTK_WIDGET(item->data));
        }
        g_list_free(old);
        if (tab >= 0) {
            gtk_box_append(holder, ToolUtnContextBar::createPaletteSection(
                                           ctrl, static_cast<ToolUtnContextBar::PaletteTab>(tab)));
        }
        g_object_set_data_full(G_OBJECT(popover), "utn-section-signature", g_strdup(signature.c_str()), g_free);
    } else if (GtkWidget* scale = findOpacityScale(GTK_WIDGET(holder));
               scale && ctrl->getToolHandler()->getToolType() == TOOL_HIGHLIGHTER) {
        const double percent = 100.0 * static_cast<double>(ctrl->getToolHandler()->getColor().alpha) / 255.0;
        if (std::lround(gtk_range_get_value(GTK_RANGE(scale))) != std::lround(percent)) {
            gtk_range_set_value(GTK_RANGE(scale), percent);
        }
    }
    gtk_widget_set_sensitive(GTK_WIDGET(holder), paletteReachable(ctrl));
    // Only the highlighter (opacity) and text (size) have something here; the pen has its controls in the palette itself
    gtk_widget_set_visible(GTK_WIDGET(holder), tab == 1 || tab == 3);
}

/// The words and icon of the header: the tool the palette is painting for
void syncPaletteHeader(GtkPopover* popover, int tab) {
    auto* name = GTK_LABEL(g_object_get_data(G_OBJECT(popover), "utn-pop-name"));
    auto* hint = GTK_LABEL(g_object_get_data(G_OBJECT(popover), "utn-pop-hint"));
    auto* icon = GTK_IMAGE(g_object_get_data(G_OBJECT(popover), "utn-pop-icon"));
    if (!name || !hint || !icon) {
        return;
    }
    const char* title = tab == 0 ? _("Pen") : tab == 1 ? _("Highlighter") : tab == 2 ? _("Shapes") : tab == 3 ? _("Text") :
                                                                                                         _("Colours");
    const char* iconName = tab == 1 ? "xopp-tool-highlighter" : tab == 2 ? "xopp-shape-recognizer" :
                           tab == 3 ? "xopp-tool-text" : "xopp-tool-pencil";
    gtk_label_set_text(name, title);
    gtk_label_set_text(hint, tab == 3 ? _("Applies to new text") : _("Applies to new strokes"));
    gtk_image_set_from_icon_name(icon, iconName, GTK_ICON_SIZE_SMALL_TOOLBAR);
}

/// Width, stroke tiles, snap switch: shown for the tools they change, set to the tool's own state, never sending that
/// state back as a change
void syncPaletteBody(GtkPopover* popover, Control* ctrl, int tab, bool usable) {
    auto* tools = ctrl->getToolHandler();
    syncPaletteHeader(popover, tab);

    auto* width = GTK_WIDGET(g_object_get_data(G_OBJECT(popover), "utn-width-box"));
    auto* stroke = GTK_WIDGET(g_object_get_data(G_OBJECT(popover), "utn-stroke-box"));
    gtk_widget_set_visible(width, tab >= 0 && tab <= 2);
    gtk_widget_set_visible(stroke, tab == 0 || tab == 2);
    gtk_widget_set_sensitive(width, usable);
    gtk_widget_set_sensitive(stroke, usable);
    syncPaletteWidth(popover, ctrl);

    g_object_set_data(G_OBJECT(popover), "utn-syncing", GINT_TO_POINTER(1));
    for (int i = 0; i < static_cast<int>(utn::SHAPE_CHOICES.size()); ++i) {
        auto* tile = GTK_WIDGET(g_object_get_data(G_OBJECT(popover), (std::string("utn-stroke-") + std::to_string(i)).c_str()));
        const auto type = static_cast<DrawingType>(GPOINTER_TO_INT(g_object_get_data(G_OBJECT(tile), "utn-drawing-type")));
        gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(tile), tools->getToolType() == TOOL_PEN && type == tools->getDrawingType());
    }
    if (auto* snap = GTK_WIDGET(g_object_get_data(G_OBJECT(popover), "utn-snap-switch"))) {
        gtk_switch_set_active(GTK_SWITCH(snap), ctrl->getSettings()->getSnapRecognizedShapesEnabled());
    }
    g_object_set_data(G_OBJECT(popover), "utn-syncing", nullptr);
}

void refreshClassroomColours(GtkPopover* popover, Control* ctrl) {
    auto* grid = GTK_WIDGET(g_object_get_data(G_OBJECT(popover), "utn-colour-grid"));
    auto* custom = GTK_COLOR_BUTTON(g_object_get_data(G_OBJECT(popover), "utn-custom-colour"));
    auto* customState = GTK_WIDGET(g_object_get_data(G_OBJECT(popover), "utn-custom-state"));
    const Color current = teacherCurrentColour(ctrl);
    // Colour preview in the left rail follows the tool selected when the palette is opened.
    if (GtkWidget* owner = gtk_popover_get_relative_to(popover); GTK_IS_MENU_BUTTON(owner)) {
        showRailColour(owner, current);
    }
    // The palette has no meaning for Eraser/Hand; avoid silently modifying their tool state.
    const bool available = ctrl->getToolHandler()->hasCapability(TOOL_CAP_COLOR);
    // Like the other tool controls: not while UTN is busy, under the Blank cover, or in Present with its tools away
    const bool reachable = paletteReachable(ctrl);
    const bool usable = available && reachable;
    // An open palette that stops being usable (a save starts, the Blank cover goes up, Present puts the tools away) is
    // closed, as the History panel is; opened afterwards it shows its controls disabled with the rest of the dock
    const bool wasReachable = g_object_get_data(G_OBJECT(popover), "utn-unreachable") == nullptr;
    g_object_set_data(G_OBJECT(popover), "utn-unreachable", reachable ? nullptr : GINT_TO_POINTER(1));
    if (wasReachable && !reachable && gtk_widget_get_visible(GTK_WIDGET(popover))) {
        gtk_popover_popdown(popover);
        if (!gtk_widget_get_mapped(GTK_WIDGET(popover))) {
            gtk_widget_hide(GTK_WIDGET(popover));  // its button is hidden with the dock: there is no fade to wait for
        }
    }
    const auto kind = utn::palette::kindFor(ctrl->getToolHandler()->getToolType() == TOOL_HIGHLIGHTER);

    // The tabs show the tool in use; choosing one is choosing the tool, so they need the same gate
    const int tab = paletteTabOf(ctrl->getToolHandler());
    g_object_set_data(G_OBJECT(popover), "utn-syncing", GINT_TO_POINTER(1));
    for (int i = 0; i < 4; ++i) {
        auto* button = GTK_WIDGET(g_object_get_data(G_OBJECT(popover), (std::string("utn-tab-") + std::to_string(i)).c_str()));
        gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(button), i == tab);
        gtk_widget_set_sensitive(button, reachable);
    }
    g_object_set_data(G_OBJECT(popover), "utn-syncing", nullptr);

    gtk_widget_set_sensitive(GTK_WIDGET(grid), usable);
    gtk_widget_set_sensitive(GTK_WIDGET(custom), usable);
    GList* swatches = gtk_container_get_children(GTK_CONTAINER(grid));
    for (GList* item = swatches; item; item = item->next) {
        auto* swatch = GTK_WIDGET(item->data);
        const auto raw = GPOINTER_TO_UINT(g_object_get_data(G_OBJECT(swatch), "utn-classroom-colour"));
        const Color value(raw);
        const bool ofKind = GPOINTER_TO_INT(g_object_get_data(G_OBJECT(swatch), "utn-palette-kind")) ==
                            static_cast<int>(kind);
        gtk_widget_set_visible(swatch, ofKind);
        utn::setSwatchSelected(swatch, available && current.red == value.red && current.green == value.green &&
                                               current.blue == value.blue);
    }
    g_list_free(swatches);
    // A colour that is no swatch is the teacher's Custom colour; say which, as the swatches cannot show it
    const bool isCustom = available && !utn::palette::find(kind, static_cast<uint32_t>(current));
    const std::string customHint = isCustom ? std::string(_("Custom colour ")) + utn::palette::hex(static_cast<uint32_t>(current)) :
                                              std::string(_("Open the full colour picker"));
    gtk_widget_set_tooltip_text(GTK_WIDGET(custom), customHint.c_str());
    // Words, not just a colour, say that the Custom colour is the one in use
    gtk_label_set_text(GTK_LABEL(customState), isCustom ? _("In use") : "");
    gtk_widget_set_visible(customState, isCustom);
    GdkRGBA rgba = Util::argb_to_GdkRGBA(current);
    rgba.alpha = 1.0;  // Opacity stays in the tool's own properties.
    gtk_color_chooser_set_rgba(GTK_COLOR_CHOOSER(custom), &rgba);
    updatePaletteIndicator(popover, ctrl);
    refreshPaletteSection(popover, ctrl);
    syncPaletteBody(popover, ctrl, tab, usable);
}

/// Every Teacher Annotation Palette popover that exists, so a change of the tool or of what may be used reaches the
/// open ones whoever caused it (a preset, a shortcut, another colour surface, a save, the Blank cover, Present)
std::vector<GtkPopover*>& colourPalettePopovers() {
    static std::vector<GtkPopover*> popovers;
    return popovers;
}

/// Look again at the tool and the gate, once, from the main loop: the change may have been made by one of the
/// palette's own controls, which must not be rebuilt while it is still running its signal
void schedulePaletteRefresh(GtkPopover* popover) {
    if (g_object_get_data(G_OBJECT(popover), "utn-refresh-source")) {
        return;
    }
    const guint source = g_idle_add_full(
            G_PRIORITY_HIGH_IDLE,
            +[](gpointer data) -> gboolean {
                auto* popup = GTK_POPOVER(data);
                g_object_set_data(G_OBJECT(popup), "utn-refresh-source", nullptr);
                refreshClassroomColours(popup, static_cast<Control*>(g_object_get_data(G_OBJECT(popup), "utn-control")));
                return G_SOURCE_REMOVE;
            },
            popover, nullptr);
    g_object_set_data(G_OBJECT(popover), "utn-refresh-source", GUINT_TO_POINTER(source));
}

void registerColourPalette(GtkPopover* popover, Control* ctrl) {
    g_object_set_data(G_OBJECT(popover), "utn-control", ctrl);
    colourPalettePopovers().push_back(popover);
    g_signal_connect(popover, "destroy", G_CALLBACK(+[](GtkWidget* widget, gpointer) {
                         if (auto source = GPOINTER_TO_UINT(g_object_get_data(G_OBJECT(widget), "utn-refresh-source"))) {
                             g_source_remove(source);
                             g_object_set_data(G_OBJECT(widget), "utn-refresh-source", nullptr);
                         }
                         auto& all = colourPalettePopovers();
                         all.erase(std::remove(all.begin(), all.end(), GTK_POPOVER(widget)), all.end());
                     }),
                     nullptr);
}

/// The palette's tab @p tab was chosen: choose the tool it stands for (and a shape, for Shapes)
void choosePaletteTab(Control* ctrl, int tab) {
    auto* tools = ctrl->getToolHandler();
    switch (tab) {
        case 0:
            ctrl->selectTool(TOOL_PEN);
            ctrl->setToolDrawingType(DRAWING_TYPE_DEFAULT);
            break;
        case 1:
            ctrl->selectTool(TOOL_HIGHLIGHTER);
            break;
        case 2:
            ctrl->selectTool(TOOL_PEN);
            if (tools->getDrawingType() == DRAWING_TYPE_DEFAULT) {
                ctrl->setToolDrawingType(DRAWING_TYPE_RECTANGLE);
            }
            break;
        default:
            ctrl->selectTool(TOOL_TEXT);
            break;
    }
}

// The Teacher Annotation Palette: a compact vertical-rail control whose popover has one tab per kind of mark (Pen,
// Highlighter, Shapes, Text), the colours for it, then what that mark also needs. It only shows and sets the one tool
// state; nothing here is a second copy of it.
class ToolClassroomColours final: public AbstractToolItem {
public:
    explicit ToolClassroomColours(Control* ctrl):
            AbstractToolItem("UTN_COLOURS", Category::TOOLS), control(ctrl) {}

    xoj::util::WidgetSPtr createItem(bool horizontal) override {
        auto [popover, panel] = utn::createPopoverPanel(8);
        gtk_widget_add_css_class(GTK_WIDGET(popover), "utn-pen-popover");
        gtk_widget_add_css_class(GTK_WIDGET(panel), "utn-pen-panel");
        gtk_widget_set_size_request(GTK_WIDGET(panel), 316, -1);

        // Header: the tool this palette paints for, and what a change applies to
        GtkWidget* header = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
        GtkWidget* headerIcon = gtk_image_new_from_icon_name("xopp-tool-pencil", GTK_ICON_SIZE_SMALL_TOOLBAR);
        GtkWidget* headerName = gtk_label_new(_("Pen"));
        gtk_widget_add_css_class(headerName, "utn-pop-name");
        GtkWidget* headerHint = gtk_label_new(_("Applies to new strokes"));
        gtk_widget_add_css_class(headerHint, "utn-pop-hint");
        gtk_widget_set_hexpand(headerHint, true);
        gtk_widget_set_halign(headerHint, GTK_ALIGN_END);
        gtk_label_set_ellipsize(GTK_LABEL(headerHint), PANGO_ELLIPSIZE_END);
        gtk_box_append(GTK_BOX(header), headerIcon);
        gtk_box_append(GTK_BOX(header), headerName);
        gtk_box_append(GTK_BOX(header), headerHint);
        gtk_box_append(panel, header);
        g_object_set_data(G_OBJECT(popover), "utn-pop-icon", headerIcon);
        g_object_set_data(G_OBJECT(popover), "utn-pop-name", headerName);
        g_object_set_data(G_OBJECT(popover), "utn-pop-hint", headerHint);

        // The tools of the palette, as a small secondary control that follows the active tool: a linked radio group, so
        // exactly one is on and it is not shown by colour alone. Highlighter, Shapes and Text stay reachable through it.
        GtkWidget* tabs = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
        gtk_widget_add_css_class(tabs, "linked");
        gtk_widget_add_css_class(tabs, "utn-palette-tabs");
        const char* tabNames[] = {N_("Pen"), N_("Highlighter"), N_("Shapes"), N_("Text")};
        const char* tabHints[] = {N_("Write freehand with the pen"), N_("Highlight with a see-through colour"),
                                  N_("Draw shapes and lines"), N_("Type text on the page")};
        GtkWidget* firstTab = nullptr;
        for (int i = 0; i < 4; ++i) {
            GtkWidget* tab = firstTab ? gtk_radio_button_new_with_label_from_widget(GTK_RADIO_BUTTON(firstTab), _(tabNames[i])) :
                                        gtk_radio_button_new_with_label(nullptr, _(tabNames[i]));
            firstTab = firstTab ? firstTab : tab;
            gtk_toggle_button_set_mode(GTK_TOGGLE_BUTTON(tab), false);
            gtk_widget_set_hexpand(tab, true);
            gtk_widget_set_tooltip_text(tab, _(tabHints[i]));
            utn::setAccessibleName(tab, _(tabNames[i]));
            g_object_set_data(G_OBJECT(tab), "utn-palette-tab", GINT_TO_POINTER(i));
            g_object_set_data(G_OBJECT(tab), "utn-popover", popover);
            g_object_set_data(G_OBJECT(popover), (std::string("utn-tab-") + std::to_string(i)).c_str(), tab);
            g_signal_connect(tab, "toggled", G_CALLBACK(+[](GtkToggleButton* button, gpointer data) {
                                 auto* popup = GTK_POPOVER(g_object_get_data(G_OBJECT(button), "utn-popover"));
                                 if (!gtk_toggle_button_get_active(button) ||
                                     g_object_get_data(G_OBJECT(popup), "utn-syncing")) {
                                     return;
                                 }
                                 auto* ctrl = static_cast<Control*>(data);
                                 if (!paletteReachable(ctrl)) {
                                     refreshClassroomColours(popup, ctrl);  // puts the tabs back as the tool is
                                     return;
                                 }
                                 choosePaletteTab(ctrl, GPOINTER_TO_INT(g_object_get_data(G_OBJECT(button), "utn-palette-tab")));
                                 refreshClassroomColours(popup, ctrl);
                             }), control);
            gtk_box_append(GTK_BOX(tabs), tab);
        }
        gtk_box_append(panel, tabs);

        // COLOUR: the section caption, with the colour and thickness in words beside it
        GtkWidget* colourHead = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
        gtk_box_append(GTK_BOX(colourHead), sectionCaption(_("Colour")));
        GtkWidget* indicator = gtk_label_new("");
        gtk_label_set_xalign(GTK_LABEL(indicator), 1.0F);
        gtk_widget_set_hexpand(indicator, true);
        gtk_widget_set_halign(indicator, GTK_ALIGN_END);
        gtk_widget_add_css_class(indicator, "utn-palette-indicator");
        gtk_box_append(GTK_BOX(colourHead), indicator);
        gtk_box_append(panel, colourHead);
        g_object_set_data(G_OBJECT(popover), "utn-indicator", indicator);

        // One row of round swatches, named for screen readers and marked by a ring and a tick, not by hue alone.
        // Ink and highlighter colours share the row; the ones that do not fit the tool are hidden (see
        // refreshClassroomColours).
        GtkWidget* grid = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
        gtk_widget_add_css_class(grid, "utn-palette-grid");
        for (const utn::palette::Kind kind: {utn::palette::Kind::Ink, utn::palette::Kind::Highlighter}) {
            for (size_t i = 0; i < utn::palette::count(kind); ++i) {
                const auto& entry = utn::palette::swatch(kind, i);
                const Color value{0xFF000000U | entry.rgb};
                GtkWidget* button = utn::createColourSwatch(value, _(entry.name));
                gtk_widget_set_tooltip_text(button, _(entry.name));
                g_object_set_data(G_OBJECT(button), "utn-classroom-colour", GUINT_TO_POINTER(static_cast<uint32_t>(value)));
                g_object_set_data(G_OBJECT(button), "utn-popover", popover);
                g_object_set_data(G_OBJECT(button), "utn-palette-kind", GINT_TO_POINTER(static_cast<int>(kind)));
                g_signal_connect(button, "clicked", G_CALLBACK(+[](GtkButton* button, gpointer data) {
                                     auto colour = Color(GPOINTER_TO_UINT(
                                             g_object_get_data(G_OBJECT(button), "utn-classroom-colour")));
                                     // Close while this widget is still alive; applying the colour may
                                     // trigger a toolbar rebuild that destroys the popover.
                                     auto* popup = GTK_POPOVER(g_object_get_data(G_OBJECT(button), "utn-popover"));
                                     gtk_popover_popdown(popup);
                                     teacherApplyColour(static_cast<Control*>(data), colour,
                                                        GPOINTER_TO_INT(g_object_get_data(G_OBJECT(button), "utn-palette-kind")));
                                 }), control);
                // The row is shown whole by show_all; the refresh shows or hides each swatch for the tool
                gtk_widget_show(button);
                gtk_widget_set_no_show_all(button, true);
                gtk_box_append(GTK_BOX(grid), button);
            }
        }
        gtk_box_append(panel, grid);

        GtkBox* customRow = GTK_BOX(gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8));
        GtkWidget* customLabel = gtk_label_new(_("Custom colour"));
        gtk_widget_add_css_class(customLabel, "utn-palette-caption");
        GtkWidget* custom = gtk_color_button_new();
        g_object_set_data(G_OBJECT(custom), "utn-popover", popover);
        gtk_color_chooser_set_use_alpha(GTK_COLOR_CHOOSER(custom), false);
        gtk_widget_set_tooltip_text(custom, _("Open the full colour picker"));
        utn::setAccessibleName(custom, _("Custom colour picker"));
        g_signal_connect(custom, "color-set", G_CALLBACK(+[](GtkColorButton* button, gpointer data) {
                             GdkRGBA rgba{};
                             gtk_color_chooser_get_rgba(GTK_COLOR_CHOOSER(button), &rgba);
                             auto* ctrl = static_cast<Control*>(data);
                             teacherApplyColour(ctrl, Util::GdkRGBA_to_rgb(rgba));
                             if (!paletteReachable(ctrl)) {
                                 if (auto* popup = GTK_POPOVER(g_object_get_data(G_OBJECT(button), "utn-popover"))) {
                                     refreshClassroomColours(popup, ctrl);  // shows the colour the tool still has
                                 }
                             }
                         }), control);
        GtkWidget* customState = gtk_label_new("");
        gtk_widget_set_no_show_all(customState, true);
        gtk_widget_add_css_class(customState, "utn-palette-caption");
        gtk_box_append(customRow, customLabel);
        gtk_box_append(customRow, custom);
        gtk_box_append(customRow, customState);
        gtk_box_append(panel, GTK_WIDGET(customRow));

        // WIDTH: the pen's or highlighter's existing size steps as a slider, with the thickness in points and a wave
        // drawn in the colour and thickness it writes with
        GtkWidget* widthBox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 6);
        gtk_box_append(GTK_BOX(widthBox), sectionCaption(_("Width")));
        GtkWidget* widthRow = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
        GtkWidget* widthScale = gtk_scale_new_with_range(GTK_ORIENTATION_HORIZONTAL, TOOL_SIZE_VERY_FINE, TOOL_SIZE_VERY_THICK, 1);
        gtk_scale_set_draw_value(GTK_SCALE(widthScale), false);
        gtk_widget_set_hexpand(widthScale, true);
        gtk_widget_add_css_class(widthScale, "utn-pop-slider");
        gtk_widget_set_focus_on_click(widthScale, false);  // keyboard users can still Tab to it
        utn::setAccessibleName(widthScale, _("Stroke width"));
        g_object_set_data(G_OBJECT(widthScale), "utn-popover", popover);
        g_signal_connect(widthScale, "value-changed", G_CALLBACK(+[](GtkRange* range, gpointer data) {
                             auto* popup = GTK_POPOVER(g_object_get_data(G_OBJECT(range), "utn-popover"));
                             if (g_object_get_data(G_OBJECT(popup), "utn-width-syncing")) {
                                 return;
                             }
                             auto* ctrl = static_cast<Control*>(data);
                             auto* tools = ctrl->getToolHandler();
                             const ToolType type = tools->getToolType();
                             if (!paletteReachable(ctrl) || (type != TOOL_PEN && type != TOOL_HIGHLIGHTER)) {
                                 syncPaletteWidth(popup, ctrl);  // gone busy, covered or another tool: show what it still has
                                 return;
                             }
                             const auto size = static_cast<ToolSize>(std::clamp(
                                     static_cast<int>(std::lround(gtk_range_get_value(range))),
                                     static_cast<int>(TOOL_SIZE_VERY_FINE), static_cast<int>(TOOL_SIZE_VERY_THICK)));
                             if (type == TOOL_HIGHLIGHTER) {
                                 tools->setHighlighterSize(size);
                             } else {
                                 tools->setPenSize(size);
                             }
                             syncPaletteWidth(popup, ctrl);
                         }), control);
        GtkWidget* widthValue = gtk_label_new("");
        gtk_label_set_xalign(GTK_LABEL(widthValue), 1.0F);
        gtk_widget_add_css_class(widthValue, "utn-pop-value");
        gtk_box_append(GTK_BOX(widthRow), widthScale);
        gtk_box_append(GTK_BOX(widthRow), widthValue);
        gtk_box_append(GTK_BOX(widthBox), widthRow);
        GtkWidget* wave = gtk_drawing_area_new();
        gtk_widget_set_size_request(wave, -1, 52);
        gtk_widget_add_css_class(wave, "utn-pop-wave");
        gtk_widget_set_tooltip_text(wave, _("Preview of the selected colour and width"));
        utn::setAccessibleName(wave, _("Preview of the selected colour and width"));
        gtk_drawing_area_set_draw_func(GTK_DRAWING_AREA(wave),
                                       +[](GtkDrawingArea*, cairo_t* cr, int width, int height, gpointer data) {
                                           auto* ctrl = static_cast<Control*>(data);
                                           auto* tools = ctrl->getToolHandler();
                                           const Color colour = teacherCurrentColour(ctrl);
                                           GdkRGBA rgba = Util::argb_to_GdkRGBA(colour);
                                           const bool marker = tools->getToolType() == TOOL_HIGHLIGHTER;
                                           if (!marker) {
                                               rgba.alpha = 1.0;
                                           }
                                           const double thickness = paletteThicknessPt(tools, paletteSize(tools));
                                           cairo_set_source_rgba(cr, rgba.red, rgba.green, rgba.blue, rgba.alpha);
                                           cairo_set_line_width(cr, marker ? std::clamp(thickness * 0.9, 3.0, 18.0) :
                                                                             std::clamp(thickness * 1.6, 1.4, 12.0));
                                           cairo_set_line_cap(cr, CAIRO_LINE_CAP_ROUND);
                                           cairo_set_line_join(cr, CAIRO_LINE_JOIN_ROUND);
                                           cairo_move_to(cr, width * 0.08, height * 0.55);
                                           cairo_curve_to(cr, width * 0.22, height * 0.20, width * 0.34, height * 0.20,
                                                          width * 0.48, height * 0.52);
                                           cairo_curve_to(cr, width * 0.60, height * 0.80, width * 0.76, height * 0.24,
                                                          width * 0.92, height * 0.45);
                                           cairo_stroke(cr);
                                       }, control, nullptr);
        gtk_box_append(GTK_BOX(widthBox), wave);
        gtk_widget_show_all(widthBox);
        gtk_widget_set_no_show_all(widthBox, true);
        gtk_box_append(panel, widthBox);
        g_object_set_data(G_OBJECT(popover), "utn-width-box", widthBox);
        g_object_set_data(G_OBJECT(popover), "utn-width-scale", widthScale);
        g_object_set_data(G_OBJECT(popover), "utn-width-label", widthValue);
        g_object_set_data(G_OBJECT(popover), "utn-width-preview", wave);

        // STROKE: the pen's existing shape types as icon tiles, and the existing Smart Shape grid-snap setting
        GtkWidget* strokeBox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 6);
        gtk_box_append(GTK_BOX(strokeBox), sectionCaption(_("Stroke")));
        GtkWidget* tileRow = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
        gtk_box_set_homogeneous(GTK_BOX(tileRow), true);
        struct StrokeTile {
            std::size_t choice;  // index into utn::SHAPE_CHOICES
            utn::Glyph glyph;
        };
        // Freehand, Line, Arrow, Rectangle, Ellipse, Smart Shape: the concept's order
        const StrokeTile tiles[] = {{0, utn::Glyph::Freehand}, {3, utn::Glyph::Line}, {4, utn::Glyph::Arrow},
                                    {1, utn::Glyph::Rectangle}, {2, utn::Glyph::Ellipse}, {5, utn::Glyph::SmartShape}};
        int tileIndex = 0;
        for (const StrokeTile& entry: tiles) {
            const auto& choice = utn::SHAPE_CHOICES[entry.choice];
            GtkWidget* tile = gtk_toggle_button_new();
            gtk_button_set_child(GTK_BUTTON(tile), utn::createGlyph(entry.glyph));
            gtk_widget_show_all(tile);
            gtk_widget_add_css_class(tile, "utn-stroke-tile");
            gtk_widget_set_focus_on_click(tile, false);  // keyboard users can still Tab to it
            gtk_widget_set_tooltip_text(tile, _(choice.hint));
            utn::setAccessibleName(tile, _(choice.label));
            g_object_set_data(G_OBJECT(tile), "utn-drawing-type", GINT_TO_POINTER(static_cast<int>(choice.type)));
            g_object_set_data(G_OBJECT(tile), "utn-popover", popover);
            g_object_set_data(G_OBJECT(popover), (std::string("utn-stroke-") + std::to_string(tileIndex++)).c_str(), tile);
            g_signal_connect(tile, "toggled", G_CALLBACK(+[](GtkToggleButton* button, gpointer data) {
                                 auto* popup = GTK_POPOVER(g_object_get_data(G_OBJECT(button), "utn-popover"));
                                 if (g_object_get_data(G_OBJECT(popup), "utn-syncing")) {
                                     return;
                                 }
                                 auto* ctrl = static_cast<Control*>(data);
                                 // Another click on the shape in use keeps it; a gate or another tool puts the tiles back
                                 if (!gtk_toggle_button_get_active(button) || !paletteReachable(ctrl) ||
                                     ctrl->getToolHandler()->getToolType() != TOOL_PEN) {
                                     refreshClassroomColours(popup, ctrl);
                                     return;
                                 }
                                 ctrl->setToolDrawingType(static_cast<DrawingType>(
                                         GPOINTER_TO_INT(g_object_get_data(G_OBJECT(button), "utn-drawing-type"))));
                                 refreshClassroomColours(popup, ctrl);
                             }), control);
            gtk_box_append(GTK_BOX(tileRow), tile);
        }
        gtk_box_append(GTK_BOX(strokeBox), tileRow);
        GtkWidget* snapRow = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
        GtkWidget* snapLabel = gtk_label_new(_("Snap shapes to grid"));
        gtk_widget_set_halign(snapLabel, GTK_ALIGN_START);
        gtk_widget_set_hexpand(snapLabel, true);
        GtkWidget* snap = gtk_switch_new();
        gtk_widget_set_valign(snap, GTK_ALIGN_CENTER);
        gtk_widget_set_tooltip_text(snap, _("When Smart Shape tidies a shape, place it on the page grid"));
        utn::setAccessibleName(snap, _("Snap shapes to grid"));
        g_object_set_data(G_OBJECT(snap), "utn-popover", popover);
        g_signal_connect(snap, "notify::active", G_CALLBACK(+[](GObject* object, GParamSpec*, gpointer data) {
                             auto* popup = GTK_POPOVER(g_object_get_data(object, "utn-popover"));
                             if (g_object_get_data(G_OBJECT(popup), "utn-syncing")) {
                                 return;
                             }
                             auto* ctrl = static_cast<Control*>(data);
                             if (paletteReachable(ctrl)) {
                                 ctrl->getSettings()->setSnapRecognizedShapesEnabled(gtk_switch_get_active(GTK_SWITCH(object)));
                             }
                             refreshClassroomColours(popup, ctrl);
                         }), control);
        gtk_box_append(GTK_BOX(snapRow), snapLabel);
        gtk_box_append(GTK_BOX(snapRow), snap);
        gtk_box_append(GTK_BOX(strokeBox), snapRow);
        gtk_widget_show_all(strokeBox);
        gtk_widget_set_no_show_all(strokeBox, true);
        gtk_box_append(panel, strokeBox);
        g_object_set_data(G_OBJECT(popover), "utn-stroke-box", strokeBox);
        g_object_set_data(G_OBJECT(popover), "utn-snap-switch", snap);

        // What this kind of mark also needs: highlighter opacity or text size. Rebuilt for the tool.
        GtkWidget* section = gtk_box_new(GTK_ORIENTATION_VERTICAL, 4);
        gtk_widget_set_no_show_all(section, true);
        gtk_box_append(panel, section);

        // Footer: the popover closes with Esc and keeps what was chosen, as every change applies at once. The settings
        // inspector can be pinned beside the page, as from Pen settings.
        gtk_box_append(panel, gtk_separator_new(GTK_ORIENTATION_HORIZONTAL));
        GtkWidget* footer = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
        GtkWidget* escHint = gtk_label_new(_("Esc closes \xC2\xB7 keeps your last choice"));
        gtk_widget_add_css_class(escHint, "utn-pop-hint");
        gtk_widget_set_halign(escHint, GTK_ALIGN_START);
        gtk_widget_set_hexpand(escHint, true);
        gtk_label_set_xalign(GTK_LABEL(escHint), 0.0F);
        GtkWidget* pin = gtk_button_new_with_label(_("Pin to side"));
        gtk_button_set_relief(GTK_BUTTON(pin), GTK_RELIEF_NONE);
        gtk_widget_add_css_class(pin, "utn-pop-link");
        gtk_widget_set_tooltip_text(pin, _("Keep the full tool settings panel open beside the worksheet"));
        utn::setAccessibleName(pin, _("Pin settings to side"));
        g_object_set_data(G_OBJECT(pin), "utn-popover", popover);
        g_signal_connect(pin, "clicked", G_CALLBACK(+[](GtkButton* button, gpointer data) {
                             auto* ctrl = static_cast<Control*>(data);
                             gtk_popover_popdown(GTK_POPOVER(g_object_get_data(G_OBJECT(button), "utn-popover")));
                             if (MainWindow* window = ctrl->getWindow()) {
                                 window->setTeacherWorkspace(0);
                             }
                         }), control);
        gtk_box_append(GTK_BOX(footer), escHint);
        gtk_box_append(GTK_BOX(footer), pin);
        gtk_box_append(panel, footer);

        g_object_set_data(G_OBJECT(popover), "utn-colour-grid", grid);
        g_object_set_data(G_OBJECT(popover), "utn-custom-colour", custom);
        g_object_set_data(G_OBJECT(popover), "utn-custom-state", customState);
        g_object_set_data(G_OBJECT(popover), "utn-section", section);
        registerColourPalette(GTK_POPOVER(popover), control);
        g_signal_connect(popover, "map", G_CALLBACK(+[](GtkWidget* widget, gpointer data) {
                             refreshClassroomColours(GTK_POPOVER(widget), static_cast<Control*>(data));
                         }), control);

        GtkMenuButton* menu = GTK_MENU_BUTTON(gtk_menu_button_new());
        showRailColour(GTK_WIDGET(menu), teacherCurrentColour(control));
        gtk_widget_set_tooltip_text(GTK_WIDGET(menu), _("Annotation palette: colours, thickness and more"));
        utn::setAccessibleName(GTK_WIDGET(menu), _("Colours"));
        gtk_menu_button_set_popover(menu, GTK_WIDGET(popover));
        // One colour state: the dot and the words follow the tool colour and size whoever changes them (a Favourite Pen
        // Preset, the context panel, the selection toolbar, a shortcut), not only when this popover is opened.
        g_object_set_data(G_OBJECT(menu), "utn-control", control);
        auto* actions = control->getActionDatabase();
        for (const Action action: {Action::TOOL_COLOR, Action::TOOL_SIZE}) {
            g_signal_connect_object(actions->getAction(action).get(), "notify::state",
                                    G_CALLBACK(+[](GObject*, GParamSpec*, gpointer rail) {
                                        auto* ctrl = static_cast<Control*>(g_object_get_data(G_OBJECT(rail), "utn-control"));
                                        showRailColour(GTK_WIDGET(rail), teacherCurrentColour(ctrl));
                                        if (auto* popup = gtk_menu_button_get_popover(GTK_MENU_BUTTON(rail))) {
                                            updatePaletteIndicator(popup, ctrl);
                                        }
                                    }),
                                    menu, GConnectFlags(0));
        }
        gtk_menu_button_set_direction(menu, horizontal ? GTK_ARROW_DOWN : GTK_ARROW_RIGHT);
        gtk_widget_show_all(GTK_WIDGET(panel));
        return xoj::util::WidgetSPtr(GTK_WIDGET(menu), xoj::util::adopt);
    }

    std::string getToolDisplayName() const override { return _("Colours"); }
    GtkWidget* getNewToolIcon() const override {
        return ColorIcon::newGtkImage(teacherCurrentColour(control), 22, true);
    }

private:
    Control* control;
};

class UtnBrandItem: public AbstractToolItem {
public:
    UtnBrandItem(): AbstractToolItem("UTN_BRAND", Category::MISC) {}

    xoj::util::WidgetSPtr createItem(bool) override {
        auto* label = gtk_label_new("UTN");
        gtk_widget_set_name(label, "utnBrand");
        gtk_widget_add_css_class(label, "utn-brand");
        gtk_widget_add_css_class(label, "utn-compact-hidden");
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
const char* teacherChromeIcon(const std::string& item) {
    if (item == "SAVE") return "document-save-symbolic";
    if (item == "OPEN") return "document-open-symbolic";
    if (item == "SAVEPDF") return "application-pdf-symbolic";
    if (item == "UNDO") return "edit-undo-symbolic";
    if (item == "REDO") return "edit-redo-symbolic";
    return nullptr;
}

/// The rail's items as the teacher arranged them (see model/DockLayout.h). With nothing customised they are returned as
/// they are.
ToolbarItemVector arrangeTeacherRail(const ToolbarItemVector& items, const utn::DockLayout& layout) {
    if (layout.isDefault()) {
        return items;
    }
    std::vector<std::string> names;
    for (const ToolbarItem& item: items) {
        names.push_back(item.getName());
    }
    ToolbarItemVector arranged;
    std::vector<bool> used(items.size(), false);
    for (const std::string& name: layout.arrange(names)) {
        std::size_t found = items.size();
        for (std::size_t i = 0; i < items.size(); ++i) {
            if (!used[i] && items[i].getName() == name) {
                found = i;
                break;
            }
        }
        if (found < items.size()) {
            used[found] = true;
            arranged.push_back(items[found]);
        } else if (name == "SEPARATOR") {
            arranged.emplace_back(ToolbarItem("SEPARATOR"));  // the favourites group has a separator of its own
        }
    }
    return arranged;
}

/// Which app-bar buttons may move into the "More" menu, and in what order. Everything here also has a menu-bar entry and
/// a shortcut; Undo and Redo go last because they are used mid-lesson.
int appBarOverflowRank(const std::string& item) {
    if (item == "SAVEPDF") return 1;
    if (item == "OPEN") return 2;
    if (item == "SAVE") return 3;
    if (item == "REDO") return 4;
    if (item == "UNDO") return 5;
    return 0;
}

GtkWidget* firstImage(GtkWidget* widget) {
    if (GTK_IS_IMAGE(widget)) {
        return widget;
    }
    if (!GTK_IS_CONTAINER(widget)) {
        return nullptr;
    }
    GList* children = gtk_container_get_children(GTK_CONTAINER(widget));
    GtkWidget* found = nullptr;
    for (GList* child = children; child && !found; child = child->next) {
        found = firstImage(GTK_WIDGET(child->data));
    }
    g_list_free(children);
    return found;
}

// Captions belong inside the primary button so the icon and name share one hit target.
bool appendTeacherCaption(GtkWidget* widget, const char* text) {
    if (GTK_IS_BUTTON(widget)) {
        GtkWidget* icon = gtk_bin_get_child(GTK_BIN(widget));
        GtkBox* content = nullptr;
        if (GTK_IS_IMAGE(icon)) {
            g_object_ref(icon);
            gtk_container_remove(GTK_CONTAINER(widget), icon);
            content = GTK_BOX(gtk_box_new(GTK_ORIENTATION_VERTICAL, 2));
            gtk_box_append(content, icon);
            g_object_unref(icon);
            gtk_button_set_child(GTK_BUTTON(widget), GTK_WIDGET(content));
        } else if (GTK_IS_BOX(icon)) {
            GList* children = gtk_container_get_children(GTK_CONTAINER(icon));
            if (g_list_length(children) == 1 && GTK_IS_IMAGE(children->data)) {
                content = GTK_BOX(icon);
                gtk_orientable_set_orientation(GTK_ORIENTABLE(content), GTK_ORIENTATION_VERTICAL);
                gtk_box_set_spacing(content, 2);
            }
            g_list_free(children);
        }
        if (content) {
            auto* label = gtk_label_new(text);
            gtk_widget_set_name(label, "utnRailLabel");
            gtk_label_set_width_chars(GTK_LABEL(label), 11);
            gtk_label_set_max_width_chars(GTK_LABEL(label), 12);
            gtk_label_set_ellipsize(GTK_LABEL(label), PANGO_ELLIPSIZE_END);
            gtk_box_append(content, label);
            gtk_widget_show_all(GTK_WIDGET(content));
            gtk_widget_set_no_show_all(label, true);
        }
        utn::setAccessibleName(widget, text);
        return true;
    }
    if (GTK_IS_CONTAINER(widget)) {
        GList* children = gtk_container_get_children(GTK_CONTAINER(widget));
        bool found = false;
        for (GList* child = children; child && !found; child = child->next) {
            found = appendTeacherCaption(GTK_WIDGET(child->data), text);
        }
        g_list_free(children);
        return found;
    }
    return false;
}

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
        {"ERASER", nullptr, N_("Choose Ink, Stroke or Object Eraser. Object Eraser removes text and Answer Boxes.")},
        {"UTN_TEXT", nullptr, N_("Click the page to type")},
        {"ANSWER_BOX", nullptr, N_("Click the page to add a styled box for an answer")},
        {"TEACHER_STAMP", N_("Feedback"), N_("Place a comment from the Feedback Bank")},
        {"TEACHING_KIT", N_("Shapes"), N_("Lines, rectangles, arrows and maths tools")},
        {"CLASSROOM_WORKFLOW", N_("Classroom"), N_("Lesson materials, pages, marking and reading tools")},
        {"PREPARE_REVEAL", N_("Reveal"), N_("Prepare answers in advance, hide them and reveal them during the lesson")},
        {"SELECT", N_("Area"), N_("Drag around several objects to select them")},
        {"HAND", N_("Move"), N_("Drag to move around the page")},
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

std::vector<std::string> ToolMenuHandler::railItemIds(const ToolbarData* d) const {
    std::vector<std::string> ids;
    if (!d) {
        return ids;
    }
    for (const ToolbarEntry& e: d->contents) {
        if (e.getName() == "toolbarLeft1") {
            for (const ToolbarItem& item: e.getItems()) {
                ids.push_back(item.getName());
            }
        }
    }
    return ids;
}

std::string ToolMenuHandler::toolDisplayName(const std::string& id) const {
    for (const auto& item: this->toolItems) {
        if (item->getId() == id) {
            const TeacherToolText* text = teacherToolText(id);
            return text && text->label ? _(text->label) : item->getToolDisplayName();
        }
    }
    return id;
}

void ToolMenuHandler::load(const ToolbarData* d, GtkWidget* toolbar, const char* toolbarName, bool horizontal) {
    int count = 0;
    const auto palette = this->control->getPalette();

    const auto& layoutId = d->getId();
    const bool teacherLayout = utn::isTeacherLayout(layoutId);
    if (!horizontal) {
        gtk_widget_set_hexpand(toolbar, teacherLayout && g_strcmp0(toolbarName, "toolbarLeft2") == 0);
    }
    const auto& recolorParams = control->getSettings()->getRecolorParameters();
    auto recolor = recolorParams.recolorizeMainView ? std::make_optional(recolorParams.recolor) : std::nullopt;

    for (const ToolbarEntry& e: d->contents) {
        if (e.getName() == toolbarName) {
            // The teacher's pinned, hidden and reordered tools. The toolbar data itself is never rewritten.
            ToolbarItemVector railItems = e.getItems();
            if (teacherLayout && !horizontal && g_strcmp0(toolbarName, "toolbarLeft1") == 0) {
                railItems = arrangeTeacherRail(railItems, control->getDockLayout());
            }
            for (const ToolbarItem& dataItem: railItems) {
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
                        if (teacherLayout && horizontal && g_strcmp0(toolbarName, "toolbarTop1") == 0) {
                            if (const char* iconName = teacherChromeIcon(name);
                                iconName && gtk_icon_theme_has_icon(gtk_icon_theme_get_default(), iconName)) {
                                if (GtkWidget* icon = firstImage(it.get())) {
                                    gtk_image_set_from_icon_name(GTK_IMAGE(icon), iconName, GTK_ICON_SIZE_LARGE_TOOLBAR);
                                }
                            }
                        }
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
                            if (horizontal && g_strcmp0(toolbarName, "toolbarTop1") == 0) {
                                if (const int rank = appBarOverflowRank(name)) {
                                    g_object_set_data(G_OBJECT(it.get()), utn::OVERFLOW_RANK_KEY, GINT_TO_POINTER(rank));
                                    g_object_set_data_full(G_OBJECT(it.get()), utn::OVERFLOW_LABEL_KEY,
                                                           g_strdup(label.c_str()), g_free);
                                }
                            }
                            if (!horizontal) {
                                utn::setAccessibleName(it.get(), label.c_str());
                                appendTeacherCaption(it.get(), label.c_str());
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

    // UTN clean student-facing preview. Created first: the classroom controls drive it.
    auto& students = emplaceItem<ToolStudentView>("STUDENT_VIEW", control);
    this->studentView = &students;
    emplaceItem<ToolStudentQuick>("STUDENT_QUICK", students);

    // UTN classroom presentation tools
    emplaceItem<ToolPresentationKit>("PRESENTATION_KIT", control, iconNameHelper, &students);

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

void ToolMenuHandler::refreshColourPalettes() {
    for (GtkPopover* popover: colourPalettePopovers()) {
        schedulePaletteRefresh(popover);
    }
}
