/*
 * Ultimate Teacher Notepad
 *
 * Context-sensitive teacher properties bar
 *
 * Based on Xournal++ GPLv2+
 */

#include "ToolUtnContextBar.h"

#include <algorithm>
#include <array>
#include <cstdint>
#include <string>
#include <utility>

#include "control/Control.h"
#include "control/ToolHandler.h"
#include "control/actions/ActionDatabase.h"
#include "control/settings/Settings.h"
#include "control/tools/TextEditor.h"
#include "gui/MainWindow.h"
#include "gui/XournalView.h"
#include "model/Font.h"
#include "model/Text.h"
#include "model/TextAlignment.h"
#include "util/Color.h"
#include "util/Util.h"
#include "util/gtk4_helper.h"
#include "util/i18n.h"
#include "util/raii/GVariantSPtr.h"

#include "AnswerBoxStyles.h"  // for ANSWER_BOX_STYLES
#include "HighlightModes.h"   // for HIGHLIGHT_MODES
#include "UtnWidgets.h"

namespace {

struct NamedSwatch {
    const char* name;  ///< untranslated
    Color colour;
};

// Few, clearly distinct classroom colours; "More colours" opens the full chooser
constexpr std::array<NamedSwatch, 5> INK_SWATCHES{{
        {N_("Black"), Color{0x00U, 0x00U, 0x00U}},
        {N_("Blue"), Color{0x1EU, 0x5BU, 0xD8U}},
        {N_("Red"), Color{0xD3U, 0x2FU, 0x2FU}},
        {N_("Green"), Color{0x2EU, 0x7DU, 0x32U}},
        {N_("Purple"), Color{0x6AU, 0x1BU, 0x9AU}},
}};

constexpr std::array<NamedSwatch, 5> HIGHLIGHT_SWATCHES{{
        {N_("Yellow"), Color{0xFFU, 0xEBU, 0x3BU}},
        {N_("Green"), Color{0x8BU, 0xE0U, 0x4EU}},
        {N_("Pink"), Color{0xFFU, 0x7EU, 0xB6U}},
        {N_("Blue"), Color{0x6EU, 0xC6U, 0xFFU}},
        {N_("Orange"), Color{0xFFU, 0xA7U, 0x26U}},
}};

struct SizeEntry {
    const char* label;  ///< shown on the button
    const char* name;   ///< untranslated, for tooltips and screen readers
    ToolSize size;
};

constexpr std::array<SizeEntry, 5> SIZES{{
        {"XS", N_("Extra fine"), TOOL_SIZE_VERY_FINE},
        {"S", N_("Fine"), TOOL_SIZE_FINE},
        {"M", N_("Medium"), TOOL_SIZE_MEDIUM},
        {"L", N_("Thick"), TOOL_SIZE_THICK},
        {"XL", N_("Extra thick"), TOOL_SIZE_VERY_THICK},
}};

bool sameHue(Color a, Color b) { return a.red == b.red && a.green == b.green && a.blue == b.blue; }

XojFont currentTextFont(Control* control) {
    if (auto* editor = control->getTextEditor(); editor && editor->getTextElement()) {
        return editor->getTextElement()->getFont();
    }
    auto action = control->getActionDatabase()->getAction(Action::FONT);
    xoj::util::GVariantSPtr state(g_action_get_state(G_ACTION(action.get())), xoj::util::adopt);
    return state ? XojFont(g_variant_get_string(state.get(), nullptr)) : control->getSettings()->getFont();
}

void applyTextFont(Control* control, const XojFont& font) {
    control->fontChanged(font);
    control->getActionDatabase()->setActionState(Action::FONT, font.asString().c_str());
}

/// Change bold or italic on the current text font
void applyFontStyle(Control* control, bool bold, bool active) {
    XojFont font = currentTextFont(control);
    PangoFontDescription* desc = pango_font_description_from_string(font.asString().c_str());
    if (bold) {
        pango_font_description_set_weight(desc, active ? PANGO_WEIGHT_BOLD : PANGO_WEIGHT_NORMAL);
    } else {
        pango_font_description_set_style(desc, active ? PANGO_STYLE_ITALIC : PANGO_STYLE_NORMAL);
    }
    gchar* value = pango_font_description_to_string(desc);
    XojFont updated(value);
    g_free(value);
    pango_font_description_free(desc);
    applyTextFont(control, updated);
}

/// The colour the colour controls show for the active tool and text mode
Color currentColour(ToolHandler* tools) {
    if (tools->getToolType() == TOOL_TEXT) {
        switch (tools->getTextMode()) {
            case TextMode::Feedback:
                return tools->getTeacherStampColor();
            case TextMode::AnswerBox:
                return tools->getAnswerBoxTextColor();
            case TextMode::Plain:
                break;
        }
    }
    return tools->getColor();
}

/// Apply a colour to whatever the colour controls stand for: tool ink, a Feedback or Answer Box text, the text
/// being edited, or the current selection
void applyColour(Control* control, Color chosen) {
    auto* tools = control->getToolHandler();
    if (tools->getToolType() == TOOL_TEXT && tools->getTextMode() == TextMode::Feedback) {
        tools->setTeacherStampColor(chosen);
    } else if (tools->getToolType() == TOOL_TEXT && tools->getTextMode() == TextMode::AnswerBox) {
        tools->setAnswerBoxTextColor(chosen);
    } else {
        tools->setColor(chosen, true);  // keeps the tool's opacity, recolours a selection
    }
    if (tools->getToolType() == TOOL_TEXT) {
        if (auto* editor = control->getTextEditor(); editor) {
            editor->setColor(chosen);
        }
    }
}

void applyAnswerBoxStyleToEditor(Control* control) {
    auto* tools = control->getToolHandler();
    if (auto* editor = control->getTextEditor(); editor) {
        editor->setColor(tools->getAnswerBoxTextColor());
        editor->setBoxBackgroundColor(tools->getAnswerBoxBackgroundColor());
        editor->setBoxBorderColor(tools->getAnswerBoxBorderColor());
        editor->setBoxBorderWidth(tools->getAnswerBoxBorderWidth());
        editor->setBoxPadding(tools->getAnswerBoxPadding());
        editor->setBoxCornerRadius(tools->getAnswerBoxCornerRadius());
    }
}

const char* drawingTypeName(DrawingType type) {
    switch (type) {
        case DRAWING_TYPE_LINE:
            return _("Line");
        case DRAWING_TYPE_RECTANGLE:
            return _("Rectangle");
        case DRAWING_TYPE_ELLIPSE:
            return _("Ellipse");
        case DRAWING_TYPE_ARROW:
            return _("Arrow");
        case DRAWING_TYPE_DOUBLE_ARROW:
            return _("Double Arrow");
        case DRAWING_TYPE_COORDINATE_SYSTEM:
            return _("Axes");
        case DRAWING_TYPE_SHAPE_RECOGNIZER:
            return _("Smart Shape");
        case DRAWING_TYPE_SPLINE:
            return _("Curve");
        default:
            return _("Shape");
    }
}

/// Name on the badge: the tool, or what it will place
const char* badgeName(ToolHandler* tools, ToolType tool) {
    switch (tool) {
        case TOOL_PEN:
            return tools->getDrawingType() != DRAWING_TYPE_DEFAULT ? drawingTypeName(tools->getDrawingType()) :
                                                                     _("Pen");
        case TOOL_HIGHLIGHTER:
            return _("Highlight");
        case TOOL_ERASER:
            return _("Eraser");
        case TOOL_TEXT:
            switch (tools->getTextMode()) {
                case TextMode::Feedback:
                    return _("Feedback");
                case TextMode::AnswerBox:
                    return _("Answer Box");
                case TextMode::Plain:
                    return _("Text");
            }
            return _("Text");
        case TOOL_SELECT_RECT:
        case TOOL_SELECT_REGION:
        case TOOL_SELECT_OBJECT:
        case TOOL_SELECT_MULTILAYER_RECT:
        case TOOL_SELECT_MULTILAYER_REGION:
            return _("Select");
        case TOOL_HAND:
            return _("Move Page");
        case TOOL_IMAGE:
            return _("Picture");
        case TOOL_LASER_POINTER_PEN:
            return _("Temporary ink");
        case TOOL_LASER_POINTER_HIGHLIGHTER:
            return _("Temporary highlight");
        case TOOL_LATEX:
            return _("Equation");
        default:
            return _("Tool");
    }
}

void clearBox(GtkBox* box) {
    GList* children = gtk_container_get_children(GTK_CONTAINER(box));
    for (GList* child = children; child != nullptr; child = child->next) {
        gtk_container_remove(GTK_CONTAINER(box), GTK_WIDGET(child->data));
    }
    g_list_free(children);
}

/// A labelled row inside a menu: text on the left, the control on the right
void appendMenuRow(GtkBox* panel, const char* label, GtkWidget* widget) {
    GtkBox* row = GTK_BOX(gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 12));
    GtkWidget* text = gtk_label_new(label);
    gtk_widget_set_halign(text, GTK_ALIGN_START);
    gtk_widget_set_hexpand(text, true);
    gtk_box_append(row, text);
    gtk_box_append(row, widget);
    gtk_box_append(panel, GTK_WIDGET(row));
    utn::setAccessibleName(widget, label);
}

GtkWidget* newScale(double min, double max, double step, double value, int digits) {
    GtkWidget* scale = gtk_scale_new_with_range(GTK_ORIENTATION_HORIZONTAL, min, max, step);
    gtk_range_set_value(GTK_RANGE(scale), value);
    gtk_scale_set_digits(GTK_SCALE(scale), digits);
    gtk_scale_set_value_pos(GTK_SCALE(scale), GTK_POS_RIGHT);
    gtk_widget_set_size_request(scale, 140, -1);
    return scale;
}

/// A toolbar-style menu button with a text label and a popover
GtkWidget* newMenuButton(const char* label, const char* hint, GtkPopover* popover) {
    GtkMenuButton* button = GTK_MENU_BUTTON(gtk_menu_button_new());
    gtk_button_set_label(GTK_BUTTON(button), label);
    gtk_widget_set_tooltip_text(GTK_WIDGET(button), hint);
    gtk_menu_button_set_popover(button, GTK_WIDGET(popover));
    gtk_menu_button_set_direction(button, GTK_ARROW_DOWN);
    return GTK_WIDGET(button);
}
}  // namespace

ToolUtnContextBar::ToolUtnContextBar(std::string id, Control* control):
        AbstractToolItem(std::move(id), Category::TOOLS), control(control) {
    control->getToolHandler()->addToolChangedListener([this](ToolType tool) {
        if (this->box != nullptr) {
            rebuild(tool);
        }
    });
}

auto ToolUtnContextBar::createItem(bool horizontal) -> xoj::util::WidgetSPtr {
    box = GTK_BOX(gtk_box_new(horizontal ? GTK_ORIENTATION_HORIZONTAL : GTK_ORIENTATION_VERTICAL, 6));
    gtk_widget_set_name(GTK_WIDGET(box), "utnContextBar");
    gtk_widget_set_margin_start(GTK_WIDGET(box), 4);
    gtk_widget_set_margin_end(GTK_WIDGET(box), 4);
    gtk_widget_set_valign(GTK_WIDGET(box), GTK_ALIGN_CENTER);

    g_signal_connect(
            box,
            "destroy",
            G_CALLBACK(+[](GtkWidget* widget, gpointer data) {
                auto* self = static_cast<ToolUtnContextBar*>(data);
                if (GTK_WIDGET(self->box) == widget) {
                    self->box = nullptr;
                }
            }),
            this);

    rebuild(control->getToolHandler()->getToolType());
    gtk_widget_show_all(GTK_WIDGET(box));
    if (!horizontal) {
        return xoj::util::WidgetSPtr(GTK_WIDGET(box), xoj::util::adopt);
    }

    // Sizing chain: the toolbar wraps this viewport in an expanding GtkToolItem (expandsInToolbar), so the bar
    // receives the full row width; the viewport itself requests the controls' natural width.
    GtkWidget* viewport = createPropertiesViewport(GTK_WIDGET(box));
    gtk_widget_show_all(viewport);
    return xoj::util::WidgetSPtr(viewport, xoj::util::adopt);
}

auto ToolUtnContextBar::createPropertiesViewport(GtkWidget* content) -> GtkWidget* {
    GtkWidget* viewport = gtk_scrolled_window_new(nullptr, nullptr);
    gtk_widget_set_name(viewport, "utnContextViewport");
    auto* scroller = GTK_SCROLLED_WINDOW(viewport);
    // Without natural-width propagation a GtkScrolledWindow requests ~0 natural width; inside a GtkToolbar (which
    // ignores hexpand) that collapsed the bar to a clipped badge.
    gtk_scrolled_window_set_policy(scroller, GTK_POLICY_AUTOMATIC, GTK_POLICY_NEVER);
    gtk_scrolled_window_set_propagate_natural_width(scroller, true);
    gtk_scrolled_window_set_min_content_width(scroller, 160);
    gtk_scrolled_window_set_overlay_scrolling(scroller, true);
    gtk_scrolled_window_set_shadow_type(scroller, GTK_SHADOW_NONE);
    gtk_widget_set_hexpand(viewport, true);
    gtk_widget_set_vexpand(viewport, false);
    gtk_container_add(GTK_CONTAINER(viewport), content);
    return viewport;
}

void ToolUtnContextBar::rebuild(ToolType tool) {
    if (box == nullptr) {
        return;
    }
    clearBox(box);
    auto* tools = control->getToolHandler();
    appendBadge(badgeName(tools, tool));

    switch (tool) {
        case TOOL_PEN:
            if (tools->getDrawingType() != DRAWING_TYPE_DEFAULT) {
                appendShapeControls(tools->getDrawingType());
            } else {
                appendPenControls();
            }
            break;
        case TOOL_HIGHLIGHTER:
            appendHighlightControls();
            break;
        case TOOL_ERASER:
            appendEraserControls();
            break;
        case TOOL_TEXT:
            appendTextControls();
            break;
        case TOOL_SELECT_RECT:
        case TOOL_SELECT_REGION:
        case TOOL_SELECT_OBJECT:
        case TOOL_SELECT_MULTILAYER_RECT:
        case TOOL_SELECT_MULTILAYER_REGION:
            appendSelectionControls(tool);
            break;
        case TOOL_HAND:
            appendHint(_("Drag to move around the page"));
            break;
        case TOOL_IMAGE:
            appendHint(_("Click the page where the picture should go"));
            break;
        case TOOL_LASER_POINTER_PEN:
        case TOOL_LASER_POINTER_HIGHLIGHTER:
            appendColourControls(tool == TOOL_LASER_POINTER_PEN ? Palette::Ink : Palette::Highlight);
            appendSeparator();
            appendHint(_("Fades away after a few seconds and is never saved"));
            break;
        default:
            if (tools->hasCapability(TOOL_CAP_COLOR)) {
                appendColourControls(Palette::Ink);
            }
            break;
    }

    gtk_widget_show_all(GTK_WIDGET(box));
    if (auto* viewport = gtk_widget_get_ancestor(GTK_WIDGET(box), GTK_TYPE_SCROLLED_WINDOW)) {
        auto* adjustment = gtk_scrolled_window_get_hadjustment(GTK_SCROLLED_WINDOW(viewport));
        gtk_adjustment_set_value(adjustment, gtk_adjustment_get_lower(adjustment));
    }
}

// --- shared building blocks ------------------------------------------------------------------------------------

void ToolUtnContextBar::appendBadge(const char* name) {
    GtkWidget* badge = gtk_label_new(name);
    gtk_widget_add_css_class(badge, "utn-context-badge");
    gtk_widget_set_tooltip_text(badge, _("The tool you are using"));
    gtk_box_append(box, badge);
}

void ToolUtnContextBar::appendSeparator() { gtk_box_append(box, gtk_separator_new(GTK_ORIENTATION_VERTICAL)); }

void ToolUtnContextBar::appendHint(const char* text) {
    GtkWidget* label = gtk_label_new(text);
    gtk_widget_add_css_class(label, "utn-context-hint");
    gtk_label_set_ellipsize(GTK_LABEL(label), PANGO_ELLIPSIZE_END);
    gtk_box_append(box, label);
}

auto ToolUtnContextBar::appendButton(const char* label, const char* hint) -> GtkWidget* {
    GtkWidget* button = gtk_button_new_with_label(label);
    gtk_widget_set_focus_on_click(button, false);  // keyboard users can still Tab to it
    if (hint) {
        gtk_widget_set_tooltip_text(button, hint);
    }
    gtk_box_append(box, button);
    return button;
}

void ToolUtnContextBar::appendColourControls(Palette palette) {
    const auto& swatches = palette == Palette::Highlight ? HIGHLIGHT_SWATCHES : INK_SWATCHES;
    const Color current = currentColour(control->getToolHandler());
    const bool hasSelection = [this] {
        auto* window = control->getWindow();
        return window && window->getXournal() && window->getXournal()->getSelection();
    }();

    GtkBox* group = GTK_BOX(gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 2));
    gtk_widget_add_css_class(GTK_WIDGET(group), "utn-swatches");
    for (const auto& swatch: swatches) {
        GtkWidget* button = utn::createColourSwatch(swatch.colour, _(swatch.name));
        gtk_widget_set_focus_on_click(button, false);  // keyboard users can still Tab to it
        // A selection has no single colour, so no swatch is marked while editing one
        utn::setSwatchSelected(button, !hasSelection && sameHue(swatch.colour, current));
        g_object_set_data(G_OBJECT(button), "utn-colour", GUINT_TO_POINTER(uint32_t(swatch.colour)));
        g_signal_connect(button, "clicked", G_CALLBACK(+[](GtkButton* button, gpointer data) {
                             auto colour = Color(GPOINTER_TO_UINT(g_object_get_data(G_OBJECT(button), "utn-colour")));
                             applyColour(static_cast<Control*>(data), colour);
                             // Colour changes do not rebuild the bar, so move the mark here
                             // Recolouring a selection can rebuild the bar; then there is nothing left to mark
                             GtkWidget* parent = gtk_widget_get_parent(GTK_WIDGET(button));
                             if (!parent) {
                                 return;
                             }
                             GList* siblings = gtk_container_get_children(GTK_CONTAINER(parent));
                             for (GList* it = siblings; it; it = it->next) {
                                 if (g_object_get_data(G_OBJECT(it->data), "utn-colour")) {
                                     utn::setSwatchSelected(GTK_WIDGET(it->data), it->data == button);
                                 }
                             }
                             g_list_free(siblings);
                         }),
                         control);
        gtk_box_append(group, button);
    }

    // A selection reports white as its tool colour; start the chooser from black instead
    GdkRGBA rgba = Util::argb_to_GdkRGBA(hasSelection ? Color{0U, 0U, 0U} : current);
    rgba.alpha = 1.0;
    GtkWidget* more = gtk_color_button_new_with_rgba(&rgba);
    gtk_widget_set_focus_on_click(more, false);  // keyboard users can still Tab to it
    gtk_color_chooser_set_use_alpha(GTK_COLOR_CHOOSER(more), false);
    gtk_widget_set_tooltip_text(more, _("More colours"));
    utn::setAccessibleName(more, _("More colours"));
    g_signal_connect(more, "color-set", G_CALLBACK(+[](GtkColorButton* button, gpointer data) {
                         GdkRGBA chosen{};
                         gtk_color_chooser_get_rgba(GTK_COLOR_CHOOSER(button), &chosen);
                         applyColour(static_cast<Control*>(data), Util::GdkRGBA_to_argb(chosen));
                         GtkWidget* parent = gtk_widget_get_parent(GTK_WIDGET(button));
                         if (!parent) {
                             return;
                         }
                         GList* siblings = gtk_container_get_children(GTK_CONTAINER(parent));
                         for (GList* it = siblings; it; it = it->next) {
                             if (g_object_get_data(G_OBJECT(it->data), "utn-colour")) {
                                 utn::setSwatchSelected(GTK_WIDGET(it->data), false);
                             }
                         }
                         g_list_free(siblings);
                     }),
                     control);
    gtk_box_append(group, more);
    gtk_box_append(box, GTK_WIDGET(group));
}

void ToolUtnContextBar::appendSizeControls(ToolType tool) {
    auto* tools = control->getToolHandler();
    const bool forSelection = tool != TOOL_PEN && tool != TOOL_HIGHLIGHTER;
    const ToolSize current = tool == TOOL_HIGHLIGHTER ? tools->getHighlighterSize() :
                             tool == TOOL_PEN         ? tools->getPenSize() :
                                                        TOOL_SIZE_NONE;

    GtkBox* group = GTK_BOX(gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0));
    gtk_widget_add_css_class(GTK_WIDGET(group), "linked");
    gtk_widget_add_css_class(GTK_WIDGET(group), "utn-sizes");
    utn::setAccessibleName(GTK_WIDGET(group), _("Size"));

    GtkWidget* first = nullptr;
    for (const auto& entry: SIZES) {
        // Radio buttons show the current size; a selection may mix sizes, so it gets plain buttons
        GtkWidget* button = forSelection ? gtk_button_new_with_label(entry.label) :
                            first        ? gtk_radio_button_new_with_label_from_widget(GTK_RADIO_BUTTON(first),
                                                                                       entry.label) :
                                           gtk_radio_button_new_with_label(nullptr, entry.label);
        if (!forSelection) {
            first = first ? first : button;
            gtk_toggle_button_set_mode(GTK_TOGGLE_BUTTON(button), false);
            gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(button), entry.size == current);
        }
        gtk_widget_set_focus_on_click(button, false);  // keyboard users can still Tab to it
        gtk_widget_set_tooltip_text(button, _(entry.name));
        utn::setAccessibleName(button, _(entry.name));
        g_object_set_data(G_OBJECT(button), "utn-size", GINT_TO_POINTER(static_cast<int>(entry.size)));
        g_object_set_data(G_OBJECT(button), "utn-tool", GINT_TO_POINTER(static_cast<int>(tool)));

        g_signal_connect(button, "clicked", G_CALLBACK(+[](GtkButton* button, gpointer data) {
                             if (GTK_IS_TOGGLE_BUTTON(button) &&
                                 !gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(button))) {
                                 return;  // the radio being switched off
                             }
                             auto* ctrl = static_cast<Control*>(data);
                             auto size = static_cast<ToolSize>(
                                     GPOINTER_TO_INT(g_object_get_data(G_OBJECT(button), "utn-size")));
                             auto tool = static_cast<ToolType>(
                                     GPOINTER_TO_INT(g_object_get_data(G_OBJECT(button), "utn-tool")));
                             if (tool == TOOL_HIGHLIGHTER) {
                                 ctrl->getToolHandler()->setHighlighterSize(size);
                             } else if (tool == TOOL_PEN) {
                                 ctrl->getToolHandler()->setPenSize(size);
                             } else {
                                 ctrl->setToolSize(size);  // resizes the selection
                             }
                         }),
                         control);
        gtk_box_append(group, button);
    }
    gtk_box_append(box, GTK_WIDGET(group));
}

// --- tools ------------------------------------------------------------------------------------------------------

void ToolUtnContextBar::appendPenControls() {
    appendColourControls(Palette::Ink);
    appendSizeControls(TOOL_PEN);
    appendSeparator();

    GtkWidget* pressure = gtk_check_button_new_with_label(_("Pressure"));
    gtk_widget_set_focus_on_click(pressure, false);  // keyboard users can still Tab to it
    gtk_widget_set_tooltip_text(pressure, _("Lines get thicker when you press harder with a stylus"));
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(pressure), control->getSettings()->isPressureSensitivity());
    g_signal_connect(pressure, "toggled", G_CALLBACK(+[](GtkToggleButton* button, gpointer data) {
                         static_cast<Control*>(data)->getSettings()->setPressureSensitivity(
                                 gtk_toggle_button_get_active(button));
                     }),
                     control);
    gtk_box_append(box, pressure);
}

void ToolUtnContextBar::appendShapeControls(DrawingType) {
    appendColourControls(Palette::Ink);
    appendSizeControls(TOOL_PEN);
    appendSeparator();

    GtkWidget* fill = gtk_check_button_new_with_label(_("Filled"));
    gtk_widget_set_focus_on_click(fill, false);  // keyboard users can still Tab to it
    gtk_widget_set_tooltip_text(fill, _("Fill closed shapes with the pen colour"));
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(fill), control->getToolHandler()->getPenFillEnabled());
    g_signal_connect(fill, "toggled", G_CALLBACK(+[](GtkToggleButton* button, gpointer data) {
                         static_cast<Control*>(data)->setFill(gtk_toggle_button_get_active(button));
                     }),
                     control);
    gtk_box_append(box, fill);

    appendSeparator();
    GtkWidget* freehand = appendButton(_("Back to Pen"), _("Stop drawing shapes and write freehand"));
    g_signal_connect(freehand, "clicked", G_CALLBACK(+[](GtkButton*, gpointer data) {
                         static_cast<Control*>(data)->setToolDrawingType(DRAWING_TYPE_DEFAULT);
                     }),
                     control);
}

void ToolUtnContextBar::appendHighlightControls() {
    appendColourControls(Palette::Highlight);
    appendSizeControls(TOOL_HIGHLIGHTER);
    appendSeparator();

    Color colour = control->getToolHandler()->getTool(TOOL_HIGHLIGHTER).getColor();
    GtkWidget* opacity = newScale(10.0, 100.0, 5.0, 100.0 * static_cast<double>(colour.alpha) / 255.0, 0);
    gtk_widget_set_size_request(opacity, 110, -1);
    gtk_widget_set_tooltip_text(opacity, _("Opacity: lower values let more of the page show through"));
    utn::setAccessibleName(opacity, _("Opacity"));
    g_signal_connect(opacity, "value-changed", G_CALLBACK(+[](GtkRange* range, gpointer data) {
                         auto* tools = static_cast<Control*>(data)->getToolHandler();
                         Color colour = tools->getTool(TOOL_HIGHLIGHTER).getColor();
                         colour.alpha =
                                 static_cast<uint8_t>(std::clamp(gtk_range_get_value(range), 10.0, 100.0) * 2.55);
                         tools->setColor(colour, false);
                     }),
                     control);
    GtkWidget* opacityLabel = gtk_label_new(_("Opacity"));
    gtk_widget_add_css_class(opacityLabel, "utn-context-title");
    gtk_box_append(box, opacityLabel);
    gtk_box_append(box, opacity);

    appendSeparator();
    GtkWidget* mode = gtk_combo_box_text_new();
    for (const auto& entry: utn::HIGHLIGHT_MODES) {
        gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(mode), _(entry.label));
    }
    auto* tools = control->getToolHandler();
    gtk_combo_box_set_active(GTK_COMBO_BOX(mode), utn::highlightModeIndex(tools->isSmartHighlighterEnabled(),
                                                                         tools->getSmartHighlighterSnapMode()));
    gtk_widget_set_tooltip_text(mode, _("How Highlight follows the page text"));
    utn::setAccessibleName(mode, _("Highlight mode"));
    g_signal_connect(mode, "changed", G_CALLBACK(+[](GtkComboBox* combo, gpointer data) {
                         int active = gtk_combo_box_get_active(combo);
                         if (active < 0) {
                             return;
                         }
                         auto* tools = static_cast<Control*>(data)->getToolHandler();
                         const auto& entry = utn::HIGHLIGHT_MODES[static_cast<size_t>(active)];
                         tools->setSmartHighlighterEnabled(entry.snap.has_value());
                         if (entry.snap) {
                             tools->setSmartHighlighterSnapMode(*entry.snap);
                         }
                     }),
                     control);
    gtk_box_append(box, mode);
}

void ToolUtnContextBar::appendEraserControls() {
    GtkWidget* scale = newScale(0.5, 30.0, 0.5, control->getToolHandler()->getEraserThickness(), 1);
    gtk_widget_set_size_request(scale, 180, -1);
    gtk_widget_set_tooltip_text(scale, _("Eraser size. The [ and ] keys also change it."));
    utn::setAccessibleName(scale, _("Eraser size"));
    g_signal_connect(scale, "value-changed", G_CALLBACK(+[](GtkRange* range, gpointer data) {
                         static_cast<Control*>(data)->getToolHandler()->setEraserThickness(gtk_range_get_value(range));
                     }),
                     control);
    GtkWidget* label = gtk_label_new(_("Size"));
    gtk_widget_add_css_class(label, "utn-context-title");
    gtk_box_append(box, label);
    gtk_box_append(box, scale);

    appendSeparator();
    appendHint(_("Erases ink only. Use Select to delete text and boxes."));
}

void ToolUtnContextBar::appendTextControls() {
    auto* tools = control->getToolHandler();
    const TextMode mode = tools->getTextMode();

    if (mode == TextMode::Feedback) {
        appendColourControls(Palette::Ink);
        appendSeparator();
        const std::string hint = std::string(_("Click the page to place: ")) + tools->getTeacherStampText();
        appendHint(hint.c_str());
        return;
    }

    if (mode == TextMode::AnswerBox) {
        GtkWidget* remove = appendButton(_("Delete Box"), _("Delete the Answer Box being edited. Undo restores it."));
        gtk_widget_add_css_class(remove, "destructive-action");
        g_signal_connect(remove, "clicked", G_CALLBACK(+[](GtkButton*, gpointer data) {
                             static_cast<Control*>(data)->deleteEditedAnswerBox();
                         }),
                         control);
        appendSeparator();
    }

    appendColourControls(Palette::Ink);
    appendSeparator();

    XojFont current = currentTextFont(control);
    GtkWidget* fontButton = gtk_font_button_new_with_font(current.asString().c_str());
    gtk_widget_set_focus_on_click(fontButton, false);  // keyboard users can still Tab to it
    gtk_font_button_set_use_size(GTK_FONT_BUTTON(fontButton), false);
    gtk_font_button_set_show_style(GTK_FONT_BUTTON(fontButton), false);
    gtk_font_button_set_show_size(GTK_FONT_BUTTON(fontButton), false);
    gtk_widget_set_tooltip_text(fontButton, _("Font"));
    utn::setAccessibleName(fontButton, _("Font"));
    g_signal_connect(fontButton, "font-set", G_CALLBACK(+[](GtkFontButton* button, gpointer data) {
                         auto* ctrl = static_cast<Control*>(data);
                         XojFont chosen(gtk_font_button_get_font_name(button));
                         XojFont font = currentTextFont(ctrl);
                         // The size box stays in charge of the size when changing font
                         font.setName(chosen.getName());
                         applyTextFont(ctrl, font);
                     }),
                     control);
    gtk_box_append(box, fontButton);

    GtkWidget* size = gtk_spin_button_new_with_range(6.0, 96.0, 1.0);
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(size), current.getSize());
    gtk_widget_set_tooltip_text(size, _("Text size"));
    utn::setAccessibleName(size, _("Text size"));
    g_signal_connect(size, "value-changed", G_CALLBACK(+[](GtkSpinButton* spin, gpointer data) {
                         auto* ctrl = static_cast<Control*>(data);
                         XojFont font = currentTextFont(ctrl);
                         font.setSize(gtk_spin_button_get_value(spin));
                         applyTextFont(ctrl, font);
                     }),
                     control);
    gtk_box_append(box, size);

    // B I U S: the familiar word-processor row
    PangoFontDescription* desc = pango_font_description_from_string(current.asString().c_str());
    const bool isBold = pango_font_description_get_weight(desc) >= PANGO_WEIGHT_SEMIBOLD;
    const bool isItalic = pango_font_description_get_style(desc) != PANGO_STYLE_NORMAL;
    pango_font_description_free(desc);

    struct StyleToggle {
        const char* markup;
        const char* name;  ///< untranslated
        bool active;
        void (*apply)(Control*, bool);
    };
    const std::array<StyleToggle, 4> styles{{
            {"<b>B</b>", N_("Bold"), isBold, [](Control* c, bool on) { applyFontStyle(c, true, on); }},
            {"<i>I</i>", N_("Italic"), isItalic, [](Control* c, bool on) { applyFontStyle(c, false, on); }},
            {"<u>U</u>", N_("Underline"), tools->getTextUnderline(),
             [](Control* c, bool on) {
                 c->getToolHandler()->setTextUnderline(on);
                 if (auto* editor = c->getTextEditor(); editor) {
                     editor->setUnderline(on);
                 }
             }},
            {"<s>S</s>", N_("Strikethrough"), tools->getTextStrikethrough(),
             [](Control* c, bool on) {
                 c->getToolHandler()->setTextStrikethrough(on);
                 if (auto* editor = c->getTextEditor(); editor) {
                     editor->setStrikethrough(on);
                 }
             }},
    }};

    GtkBox* styleGroup = GTK_BOX(gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0));
    gtk_widget_add_css_class(GTK_WIDGET(styleGroup), "linked");
    for (const auto& style: styles) {
        GtkWidget* toggle = gtk_toggle_button_new();
        GtkWidget* label = gtk_label_new(nullptr);
        gtk_label_set_markup(GTK_LABEL(label), style.markup);
        gtk_button_set_child(GTK_BUTTON(toggle), label);
        gtk_widget_set_focus_on_click(toggle, false);  // keyboard users can still Tab to it
        gtk_widget_set_tooltip_text(toggle, _(style.name));
        utn::setAccessibleName(toggle, _(style.name));
        gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(toggle), style.active);
        g_object_set_data(G_OBJECT(toggle), "utn-apply", reinterpret_cast<gpointer>(style.apply));
        g_signal_connect(toggle, "toggled", G_CALLBACK(+[](GtkToggleButton* button, gpointer data) {
                             auto apply = reinterpret_cast<void (*)(Control*, bool)>(
                                     g_object_get_data(G_OBJECT(button), "utn-apply"));
                             apply(static_cast<Control*>(data), gtk_toggle_button_get_active(button));
                         }),
                         control);
        gtk_box_append(styleGroup, toggle);
    }
    gtk_box_append(box, GTK_WIDGET(styleGroup));

    GtkWidget* alignment = gtk_combo_box_text_new();
    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(alignment), _("Left"));
    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(alignment), _("Centre"));
    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(alignment), _("Right"));
    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(alignment), _("Justified"));
    gtk_combo_box_set_active(GTK_COMBO_BOX(alignment),
                             tools->getTextJustify() ? 3 : static_cast<int>(tools->getTextAlignment()));
    gtk_widget_set_tooltip_text(alignment, _("Alignment"));
    utn::setAccessibleName(alignment, _("Alignment"));
    g_signal_connect(alignment, "changed", G_CALLBACK(+[](GtkComboBox* combo, gpointer data) {
                         auto* db = static_cast<Control*>(data)->getActionDatabase();
                         int active = gtk_combo_box_get_active(combo);
                         if (active == 3) {
                             db->fireChangeActionState(Action::TEXT_ALIGNMENT, TextAlignment::LEFT);
                             db->fireChangeActionState(Action::TEXT_JUSTIFY, true);
                         } else if (active >= 0 && active <= 2) {
                             db->fireChangeActionState(Action::TEXT_JUSTIFY, false);
                             db->fireChangeActionState(Action::TEXT_ALIGNMENT,
                                                       static_cast<TextAlignment::Value>(active));
                         }
                     }),
                     control);
    gtk_box_append(box, alignment);

    // Paragraph: line spacing and lists
    auto [paragraphPopover, paragraphPanel] = utn::createPopoverPanel(6);
    GtkWidget* lineSpacing = gtk_spin_button_new_with_range(0.8, 2.5, 0.1);
    gtk_spin_button_set_digits(GTK_SPIN_BUTTON(lineSpacing), 1);
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(lineSpacing), tools->getTextLineSpacing());
    g_signal_connect(lineSpacing, "value-changed", G_CALLBACK(+[](GtkSpinButton* spin, gpointer data) {
                         auto* ctrl = static_cast<Control*>(data);
                         double spacing = gtk_spin_button_get_value(spin);
                         ctrl->getToolHandler()->setTextLineSpacing(spacing);
                         if (auto* editor = ctrl->getTextEditor(); editor) {
                             editor->setLineSpacing(spacing);
                         }
                     }),
                     control);
    appendMenuRow(paragraphPanel, _("Line spacing"), lineSpacing);

    GtkWidget* bullets = utn::appendMenuButton(paragraphPanel, paragraphPopover, _("Bullet list"),
                                               _("Start or end a bullet list in the text being edited"));
    g_signal_connect(bullets, "clicked", G_CALLBACK(+[](GtkButton*, gpointer data) {
                         if (auto* editor = static_cast<Control*>(data)->getTextEditor(); editor) {
                             editor->toggleBulletList();
                         }
                     }),
                     control);
    GtkWidget* numbers = utn::appendMenuButton(paragraphPanel, paragraphPopover, _("Numbered list"),
                                               _("Start or end a numbered list in the text being edited"));
    g_signal_connect(numbers, "clicked", G_CALLBACK(+[](GtkButton*, gpointer data) {
                         if (auto* editor = static_cast<Control*>(data)->getTextEditor(); editor) {
                             editor->toggleNumberedList();
                         }
                     }),
                     control);
    gtk_widget_show_all(GTK_WIDGET(paragraphPanel));
    gtk_box_append(box, newMenuButton(_("Paragraph"), _("Line spacing and lists"), paragraphPopover));

    if (mode == TextMode::AnswerBox) {
        appendSeparator();
        appendAnswerBoxStyleMenu();
    }
}

void ToolUtnContextBar::appendAnswerBoxStyleMenu() {
    auto* tools = control->getToolHandler();
    auto [popover, panel] = utn::createPopoverPanel(8);

    utn::appendPopoverHeading(panel, _("Ready-made styles"));
    GtkGrid* presets = GTK_GRID(gtk_grid_new());
    gtk_grid_set_row_spacing(presets, 4);
    gtk_grid_set_column_spacing(presets, 4);
    int index = 0;
    for (const auto& style: utn::ANSWER_BOX_STYLES) {
        GtkWidget* button = gtk_button_new_with_label(_(style.label));
        g_object_set_data(G_OBJECT(button), "utn-style", const_cast<utn::AnswerBoxStyle*>(&style));
        g_signal_connect(button, "clicked", G_CALLBACK(+[](GtkButton* button, gpointer data) {
                             auto* ctrl = static_cast<Control*>(data);
                             auto* style =
                                     static_cast<utn::AnswerBoxStyle*>(g_object_get_data(G_OBJECT(button), "utn-style"));
                             utn::applyAnswerBoxStyle(*ctrl->getToolHandler(), *style);
                             applyAnswerBoxStyleToEditor(ctrl);
                             // Rebuild so every control shows the new style
                             ctrl->getToolHandler()->fireToolChanged();
                         }),
                         control);
        gtk_grid_attach(presets, button, index % 2, index / 2, 1, 1);
        ++index;
    }
    gtk_box_append(panel, GTK_WIDGET(presets));

    utn::appendPopoverHeading(panel, _("Adjust"));
    auto addColour = [this, panel](const char* label, Color value, void (*apply)(Control*, Color)) {
        GdkRGBA rgba = Util::argb_to_GdkRGBA(value);
        GtkWidget* button = gtk_color_button_new_with_rgba(&rgba);
        gtk_color_chooser_set_use_alpha(GTK_COLOR_CHOOSER(button), true);
        g_object_set_data(G_OBJECT(button), "utn-apply", reinterpret_cast<gpointer>(apply));
        g_signal_connect(button, "color-set", G_CALLBACK(+[](GtkColorButton* button, gpointer data) {
                             GdkRGBA chosen{};
                             gtk_color_chooser_get_rgba(GTK_COLOR_CHOOSER(button), &chosen);
                             auto apply = reinterpret_cast<void (*)(Control*, Color)>(
                                     g_object_get_data(G_OBJECT(button), "utn-apply"));
                             apply(static_cast<Control*>(data), Util::GdkRGBA_to_argb(chosen));
                         }),
                         control);
        appendMenuRow(panel, label, button);
    };
    addColour(_("Fill"), tools->getAnswerBoxBackgroundColor(), [](Control* c, Color v) {
        c->getToolHandler()->setAnswerBoxBackgroundColor(v);
        applyAnswerBoxStyleToEditor(c);
    });
    addColour(_("Border"), tools->getAnswerBoxBorderColor(), [](Control* c, Color v) {
        c->getToolHandler()->setAnswerBoxBorderColor(v);
        applyAnswerBoxStyleToEditor(c);
    });

    auto addScale = [this, panel](const char* label, double max, double step, double value, int digits,
                                  void (*apply)(Control*, double)) {
        GtkWidget* scale = newScale(0.0, max, step, value, digits);
        g_object_set_data(G_OBJECT(scale), "utn-apply", reinterpret_cast<gpointer>(apply));
        g_signal_connect(scale, "value-changed", G_CALLBACK(+[](GtkRange* range, gpointer data) {
                             auto apply = reinterpret_cast<void (*)(Control*, double)>(
                                     g_object_get_data(G_OBJECT(range), "utn-apply"));
                             apply(static_cast<Control*>(data), gtk_range_get_value(range));
                         }),
                         control);
        appendMenuRow(panel, label, scale);
    };
    addScale(_("Border width"), 8.0, 0.2, tools->getAnswerBoxBorderWidth(), 1, [](Control* c, double v) {
        c->getToolHandler()->setAnswerBoxBorderWidth(v);
        applyAnswerBoxStyleToEditor(c);
    });
    addScale(_("Space inside"), 24.0, 1.0, tools->getAnswerBoxPadding(), 0, [](Control* c, double v) {
        c->getToolHandler()->setAnswerBoxPadding(v);
        applyAnswerBoxStyleToEditor(c);
    });
    addScale(_("Rounded corners"), 24.0, 1.0, tools->getAnswerBoxCornerRadius(), 0, [](Control* c, double v) {
        c->getToolHandler()->setAnswerBoxCornerRadius(v);
        applyAnswerBoxStyleToEditor(c);
    });

    gtk_widget_show_all(GTK_WIDGET(panel));
    gtk_box_append(box, newMenuButton(_("Box style"), _("Fill, border and corners of the Answer Box"), popover));
}

void ToolUtnContextBar::appendSelectionControls(ToolType tool) {
    auto* tools = control->getToolHandler();
    auto* window = control->getWindow();
    const bool hasSelection = window && window->getXournal() && window->getXournal()->getSelection();

    if (!hasSelection) {
        switch (tool) {
            case TOOL_SELECT_OBJECT:
                appendHint(_("Click an object to select it. Drag its handles to resize."));
                break;
            case TOOL_SELECT_REGION:
            case TOOL_SELECT_MULTILAYER_REGION:
                appendHint(_("Draw around the objects you want to select"));
                break;
            default:
                appendHint(_("Drag a box around the objects you want to select"));
                break;
        }
        return;
    }

    GtkWidget* remove = appendButton(_("Delete"), _("Delete the selected objects. Undo restores them."));
    gtk_widget_add_css_class(remove, "destructive-action");
    g_signal_connect(remove, "clicked", G_CALLBACK(+[](GtkButton*, gpointer data) {
                         static_cast<Control*>(data)->deleteSelection();
                     }),
                     control);

    if (tools->hasCapability(TOOL_CAP_COLOR)) {
        appendSeparator();
        appendColourControls(Palette::Ink);
    }
    if (tools->hasCapability(TOOL_CAP_SIZE)) {
        appendSeparator();
        appendSizeControls(tool);
    }

    appendSeparator();
    GtkWidget* front = appendButton(_("Bring to front"), _("Place the selection above other objects"));
    g_signal_connect(front, "clicked", G_CALLBACK(+[](GtkButton*, gpointer data) {
                         static_cast<Control*>(data)->reorderSelection(EditSelection::OrderChange::BringToFront);
                     }),
                     control);
    GtkWidget* back = appendButton(_("Send to back"), _("Place the selection behind other objects"));
    g_signal_connect(back, "clicked", G_CALLBACK(+[](GtkButton*, gpointer data) {
                         static_cast<Control*>(data)->reorderSelection(EditSelection::OrderChange::SendToBack);
                     }),
                     control);
}

auto ToolUtnContextBar::getToolDisplayName() const -> std::string { return _("Tool properties"); }

auto ToolUtnContextBar::getNewToolIcon() const -> GtkWidget* {
    return gtk_label_new("⋯");
}
