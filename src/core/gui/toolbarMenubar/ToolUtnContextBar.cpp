/*
 * Ultimate Teacher Notepad
 *
 * Context-sensitive teacher toolbar
 *
 * Based on Xournal++ GPLv2+
 */

#include "ToolUtnContextBar.h"

#include <algorithm>
#include <array>
#include <cstdint>
#include <utility>

#include "HighlightModes.h"  // for HIGHLIGHT_MODES
#include "control/Control.h"
#include "control/ToolHandler.h"
#include "control/actions/ActionDatabase.h"
#include "control/settings/Settings.h"
#include "control/tools/TextEditor.h"
#include "model/Font.h"
#include "model/Text.h"
#include "model/TextAlignment.h"
#include "util/Color.h"
#include "util/Util.h"
#include "util/gtk4_helper.h"
#include "util/i18n.h"
#include "util/raii/GVariantSPtr.h"

namespace {
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

const char* toolTitle(ToolType tool) {
    switch (tool) {
        case TOOL_PEN:
            return _("Pen");
        case TOOL_ERASER:
            return _("Eraser");
        case TOOL_HIGHLIGHTER:
            return _("Highlight");
        case TOOL_TEXT:
            return _("Text");
        case TOOL_SELECT_RECT:
        case TOOL_SELECT_REGION:
        case TOOL_SELECT_OBJECT:
        case TOOL_SELECT_MULTILAYER_RECT:
        case TOOL_SELECT_MULTILAYER_REGION:
            return _("Select");
        case TOOL_HAND:
            return _("Hand");
        case TOOL_IMAGE:
            return _("Image");
        default:
            return _("Tool");
    }
}

void clearBox(GtkBox* box) {
    if (box == nullptr) {
        return;
    }

    GList* children = gtk_container_get_children(GTK_CONTAINER(box));
    for (GList* child = children; child != nullptr; child = child->next) {
        gtk_container_remove(GTK_CONTAINER(box), GTK_WIDGET(child->data));
    }
    g_list_free(children);
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

    const char* title = toolTitle(tool);
    if (tool == TOOL_PEN && control->getToolHandler()->getDrawingType() != DRAWING_TYPE_DEFAULT) {
        title = _("Shape");
    } else if (tool == TOOL_TEXT && control->getToolHandler()->hasTeacherStamp()) {
        title = _("Feedback");
    } else if (tool == TOOL_TEXT && control->getToolHandler()->isAnswerBoxEnabled()) {
        title = _("Answer Box");
    }

    GtkWidget* heading = gtk_label_new(title);
    gtk_widget_add_css_class(heading, "utn-context-badge");
    gtk_widget_set_size_request(heading, 90, -1);
    gtk_box_append(box, heading);
    appendSeparator();

    if (tool == TOOL_TEXT && control->getToolHandler()->isAnswerBoxEnabled()) {
        GtkWidget* remove = gtk_button_new_with_label(_("Delete Box"));
        gtk_widget_set_tooltip_text(remove, _("Delete the Answer Box being edited. Undo restores it."));
        gtk_widget_set_can_focus(remove, false);
        g_signal_connect(remove, "clicked", G_CALLBACK(+[](GtkButton*, gpointer data) {
            static_cast<Control*>(data)->deleteEditedAnswerBox();
        }), control);
        gtk_box_append(box, remove);
        appendSeparator();
    }

    switch (tool) {
        case TOOL_PEN: {
            if (control->getToolHandler()->getDrawingType() != DRAWING_TYPE_DEFAULT) {
                appendShapeControls(control->getToolHandler()->getDrawingType());
                break;
            }

            appendColorButton();
            appendSizeButtons(TOOL_PEN);
            appendSeparator();
            appendLabel(_("Profile"));

            GtkWidget* profile = gtk_combo_box_text_new();
            gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(profile), _("Custom"));
            gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(profile), _("Pen"));
            gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(profile), _("Pencil"));
            gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(profile), _("Brush"));
            gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(profile), _("Marker"));
            gtk_combo_box_set_active(GTK_COMBO_BOX(profile), 0);
            gtk_widget_set_tooltip_text(profile, _("Quick pen profile"));

            g_signal_connect(
                    profile,
                    "changed",
                    G_CALLBACK(+[](GtkComboBox* combo, gpointer data) {
                        auto* ctrl = static_cast<Control*>(data);
                        auto* tools = ctrl->getToolHandler();

                        switch (gtk_combo_box_get_active(combo)) {
                            case 1:
                                tools->setPenSize(TOOL_SIZE_MEDIUM);
                                tools->setColor(Colors::xopp_royalblue, false);
                                break;
                            case 2:
                                tools->setPenSize(TOOL_SIZE_VERY_FINE);
                                tools->setColor(Colors::gray, false);
                                break;
                            case 3:
                                tools->setPenSize(TOOL_SIZE_THICK);
                                tools->setColor(Colors::black, false);
                                break;
                            case 4:
                                tools->setPenSize(TOOL_SIZE_THICK);
                                tools->setColor(Colors::xopp_darkorange, false);
                                break;
                            default:
                                return;
                        }

                        tools->selectTool(TOOL_PEN);
                        tools->setDrawingType(DRAWING_TYPE_DEFAULT);
                        tools->fireToolChanged();
                    }),
                    control);

            gtk_box_append(box, profile);

            appendSeparator();

            {
                GtkWidget* pressure = gtk_toggle_button_new_with_label(_("Pressure"));
                gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(pressure),
                                             control->getSettings()->isPressureSensitivity());
                g_signal_connect(
                        pressure,
                        "toggled",
                        G_CALLBACK(+[](GtkToggleButton* button, gpointer data) {
                            static_cast<Control*>(data)->getSettings()->setPressureSensitivity(
                                    gtk_toggle_button_get_active(button));
                        }),
                        control);
                gtk_box_append(box, pressure);
            }
            break;
        }

        case TOOL_ERASER:
            appendEraserControls();
            break;

        case TOOL_HIGHLIGHTER:
            appendMarkupControls();
            break;

        case TOOL_TEXT:
            appendTextControls();
            break;

        case TOOL_SELECT_RECT:
        case TOOL_SELECT_REGION:
        case TOOL_SELECT_OBJECT:
        case TOOL_SELECT_MULTILAYER_RECT:
        case TOOL_SELECT_MULTILAYER_REGION:
            appendSelectionControls();
            break;

        case TOOL_HAND:
            appendGenericMessage(_("Pan the lesson canvas"));
            break;

        case TOOL_IMAGE:
            appendGenericMessage(_("Tap the page to place an image"));
            break;

        default:
            appendColorButton();
            break;
    }

    gtk_widget_show_all(GTK_WIDGET(box));
    if (auto* viewport = gtk_widget_get_ancestor(GTK_WIDGET(box), GTK_TYPE_SCROLLED_WINDOW)) {
        auto* adjustment = gtk_scrolled_window_get_hadjustment(GTK_SCROLLED_WINDOW(viewport));
        gtk_adjustment_set_value(adjustment, gtk_adjustment_get_lower(adjustment));
    }
}

void ToolUtnContextBar::appendLabel(const char* text) {
    GtkWidget* label = gtk_label_new(text);
    gtk_widget_add_css_class(label, "utn-context-title");
    gtk_box_append(box, label);
}

void ToolUtnContextBar::appendSeparator() {
    gtk_box_append(box, gtk_separator_new(GTK_ORIENTATION_VERTICAL));
}

void ToolUtnContextBar::appendColorButton() {
    auto* tools = control->getToolHandler();

    Color current = tools->getColor();
    bool allowAlpha = true;

    if (tools->getToolType() == TOOL_TEXT && tools->hasTeacherStamp()) {
        current = tools->getTeacherStampColor();
        allowAlpha = false;
    } else if (tools->getToolType() == TOOL_TEXT && tools->isAnswerBoxEnabled()) {
        current = tools->getAnswerBoxTextColor();
        allowAlpha = false;
    }

    GdkRGBA color = Util::argb_to_GdkRGBA(current);
    GtkWidget* button = gtk_color_button_new_with_rgba(&color);
    gtk_color_chooser_set_use_alpha(GTK_COLOR_CHOOSER(button), allowAlpha);
    gtk_widget_set_tooltip_text(button, _("Tool colour"));

    g_signal_connect(
            button,
            "color-set",
            G_CALLBACK(+[](GtkColorButton* button, gpointer data) {
                auto* ctrl = static_cast<Control*>(data);
                auto* tools = ctrl->getToolHandler();

                GdkRGBA color{};
                gtk_color_chooser_get_rgba(GTK_COLOR_CHOOSER(button), &color);
                Color chosen = Util::GdkRGBA_to_argb(color);

                if (tools->getToolType() == TOOL_TEXT && tools->hasTeacherStamp()) {
                    tools->setTeacherStampColor(chosen);
                } else if (tools->getToolType() == TOOL_TEXT && tools->isAnswerBoxEnabled()) {
                    tools->setAnswerBoxTextColor(chosen);
                } else {
                    tools->setColor(chosen, true);
                }

                if (tools->getToolType() == TOOL_TEXT) {
                    if (auto* editor = ctrl->getTextEditor(); editor) {
                        editor->setColor(chosen);
                    }
                }
            }),
            control);

    gtk_box_append(box, button);
}

void ToolUtnContextBar::appendSizeButtons(ToolType tool) {
    appendLabel(_("Size"));

    GtkWidget* combo = gtk_combo_box_text_new();
    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(combo), "XS");
    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(combo), "S");
    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(combo), "M");
    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(combo), "L");
    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(combo), "XL");

    auto* tools = control->getToolHandler();
    ToolSize current = tool == TOOL_HIGHLIGHTER ? tools->getHighlighterSize() : tools->getPenSize();
    gtk_combo_box_set_active(GTK_COMBO_BOX(combo), static_cast<int>(current));

    g_object_set_data(G_OBJECT(combo), "utn-control", control);
    g_object_set_data(G_OBJECT(combo), "utn-tool", GINT_TO_POINTER(static_cast<int>(tool)));

    g_signal_connect(
            combo,
            "changed",
            G_CALLBACK(+[](GtkComboBox* combo, gpointer) {
                int active = gtk_combo_box_get_active(combo);
                if (active < static_cast<int>(TOOL_SIZE_VERY_FINE) ||
                    active > static_cast<int>(TOOL_SIZE_VERY_THICK)) {
                    return;
                }

                auto* ctrl = static_cast<Control*>(g_object_get_data(G_OBJECT(combo), "utn-control"));
                auto tool =
                        static_cast<ToolType>(GPOINTER_TO_INT(g_object_get_data(G_OBJECT(combo), "utn-tool")));
                auto size = static_cast<ToolSize>(active);

                auto* tools = ctrl->getToolHandler();
                if (tool == TOOL_HIGHLIGHTER) {
                    tools->setHighlighterSize(size);
                } else {
                    tools->setPenSize(size);
                }
            }),
            nullptr);

    gtk_widget_set_tooltip_text(combo, _("Tool size"));
    gtk_box_append(box, combo);
}

void ToolUtnContextBar::appendEraserControls() {
    auto* tools = control->getToolHandler();

    appendLabel(_("Size"));
    GtkWidget* scale = gtk_scale_new_with_range(GTK_ORIENTATION_HORIZONTAL, 0.5, 30.0, 0.5);
    gtk_range_set_value(GTK_RANGE(scale), tools->getEraserThickness());
    gtk_scale_set_digits(GTK_SCALE(scale), 1);
    gtk_widget_set_size_request(scale, 180, -1);
    gtk_widget_set_tooltip_text(scale, _("Continuous eraser size"));

    g_signal_connect(
            scale,
            "value-changed",
            G_CALLBACK(+[](GtkRange* range, gpointer data) {
                static_cast<Control*>(data)->getToolHandler()->setEraserThickness(gtk_range_get_value(range));
            }),
            control);
    gtk_box_append(box, scale);

    appendLabel(_("[ / ] also resize"));
    appendSeparator();
    appendGenericMessage(_("Ink only. Select objects to delete them."));
}

void ToolUtnContextBar::appendMarkupControls() {
    appendColorButton();
    appendSizeButtons(TOOL_HIGHLIGHTER);

    appendLabel(_("Opacity"));
    GtkWidget* opacity = gtk_scale_new_with_range(GTK_ORIENTATION_HORIZONTAL, 10.0, 100.0, 5.0);
    Color markupColor = control->getToolHandler()->getTool(TOOL_HIGHLIGHTER).getColor();
    gtk_range_set_value(GTK_RANGE(opacity), 100.0 * static_cast<double>(markupColor.alpha) / 255.0);
    gtk_scale_set_digits(GTK_SCALE(opacity), 0);
    gtk_widget_set_size_request(opacity, 100, -1);
    g_signal_connect(
            opacity,
            "value-changed",
            G_CALLBACK(+[](GtkRange* range, gpointer data) {
                auto* ctrl = static_cast<Control*>(data);
                auto* tools = ctrl->getToolHandler();
                Color color = tools->getTool(TOOL_HIGHLIGHTER).getColor();
                color.alpha = static_cast<uint8_t>(std::clamp(gtk_range_get_value(range), 10.0, 100.0) * 2.55);
                tools->setColor(color, false);
            }),
            control);
    gtk_box_append(box, opacity);

    appendSeparator();
    appendLabel(_("Mode"));

    GtkWidget* mode = gtk_combo_box_text_new();
    for (const auto& entry: utn::HIGHLIGHT_MODES) {
        gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(mode), _(entry.label));
    }

    auto* tools = control->getToolHandler();
    gtk_combo_box_set_active(GTK_COMBO_BOX(mode), utn::highlightModeIndex(tools->isSmartHighlighterEnabled(),
                                                                         tools->getSmartHighlighterSnapMode()));

    g_signal_connect(
            mode,
            "changed",
            G_CALLBACK(+[](GtkComboBox* combo, gpointer data) {
                auto* tools = static_cast<Control*>(data)->getToolHandler();
                int active = gtk_combo_box_get_active(combo);
                if (active < 0) {
                    return;
                }
                const auto& entry = utn::HIGHLIGHT_MODES[static_cast<size_t>(active)];
                tools->setSmartHighlighterEnabled(entry.snap.has_value());
                if (entry.snap) {
                    tools->setSmartHighlighterSnapMode(*entry.snap);
                }
            }),
            control);

    gtk_widget_set_tooltip_text(mode, _("How Highlight follows the page text"));
    gtk_box_append(box, mode);
}

void ToolUtnContextBar::appendTextControls() {
    appendColorButton();

    XojFont current = currentTextFont(control);

    GtkWidget* fontButton = gtk_font_button_new_with_font(current.asString().c_str());
    gtk_font_button_set_use_size(GTK_FONT_BUTTON(fontButton), false);
    gtk_font_button_set_show_style(GTK_FONT_BUTTON(fontButton), false);
    gtk_font_button_set_use_font(GTK_FONT_BUTTON(fontButton), false);
    gtk_widget_set_tooltip_text(fontButton, _("Font family"));

    g_signal_connect(
            fontButton,
            "font-set",
            G_CALLBACK(+[](GtkFontButton* button, gpointer data) {
                auto* ctrl = static_cast<Control*>(data);
                XojFont chosen(gtk_font_button_get_font_name(button));
                XojFont font = currentTextFont(ctrl);

                // Keep the explicit UTN size control authoritative when changing families/styles.
                font.setName(chosen.getName());
                applyTextFont(ctrl, font);
            }),
            control);
    gtk_box_append(box, fontButton);

    GtkWidget* size = gtk_spin_button_new_with_range(6.0, 96.0, 1.0);
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(size), current.getSize());
    gtk_widget_set_tooltip_text(size, _("Font size"));

    g_signal_connect(
            size,
            "value-changed",
            G_CALLBACK(+[](GtkSpinButton* spin, gpointer data) {
                auto* ctrl = static_cast<Control*>(data);
                XojFont font = currentTextFont(ctrl);
                font.setSize(gtk_spin_button_get_value(spin));
                applyTextFont(ctrl, font);
            }),
            control);
    gtk_box_append(box, size);

    appendSeparator();

    PangoFontDescription* currentDesc = pango_font_description_from_string(current.asString().c_str());
    const bool currentBold = pango_font_description_get_weight(currentDesc) >= PANGO_WEIGHT_SEMIBOLD;
    const bool currentItalic = pango_font_description_get_style(currentDesc) != PANGO_STYLE_NORMAL;
    pango_font_description_free(currentDesc);

    GtkWidget* bold = gtk_toggle_button_new_with_label("B");
    gtk_widget_set_tooltip_text(bold, _("Bold"));
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(bold), currentBold);
    g_signal_connect(
            bold,
            "toggled",
            G_CALLBACK(+[](GtkToggleButton* button, gpointer data) {
                auto* ctrl = static_cast<Control*>(data);
                XojFont font = currentTextFont(ctrl);
                PangoFontDescription* desc = pango_font_description_from_string(font.asString().c_str());

                pango_font_description_set_weight(
                        desc, gtk_toggle_button_get_active(button) ? PANGO_WEIGHT_BOLD : PANGO_WEIGHT_NORMAL);

                gchar* value = pango_font_description_to_string(desc);
                XojFont updated(value);
                g_free(value);
                pango_font_description_free(desc);
                applyTextFont(ctrl, updated);
            }),
            control);
    gtk_box_append(box, bold);

    GtkWidget* italic = gtk_toggle_button_new_with_label("I");
    gtk_widget_set_tooltip_text(italic, _("Italic"));
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(italic), currentItalic);
    g_signal_connect(
            italic,
            "toggled",
            G_CALLBACK(+[](GtkToggleButton* button, gpointer data) {
                auto* ctrl = static_cast<Control*>(data);
                XojFont font = currentTextFont(ctrl);
                PangoFontDescription* desc = pango_font_description_from_string(font.asString().c_str());

                pango_font_description_set_style(
                        desc, gtk_toggle_button_get_active(button) ? PANGO_STYLE_ITALIC : PANGO_STYLE_NORMAL);

                gchar* value = pango_font_description_to_string(desc);
                XojFont updated(value);
                g_free(value);
                pango_font_description_free(desc);
                applyTextFont(ctrl, updated);
            }),
            control);
    gtk_box_append(box, italic);

    GtkWidget* underline = gtk_toggle_button_new_with_label("U");
    gtk_widget_set_tooltip_text(underline, _("Underline"));
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(underline), control->getToolHandler()->getTextUnderline());
    g_signal_connect(
            underline,
            "toggled",
            G_CALLBACK(+[](GtkToggleButton* button, gpointer data) {
                auto* ctrl = static_cast<Control*>(data);
                bool enabled = gtk_toggle_button_get_active(button);
                ctrl->getToolHandler()->setTextUnderline(enabled);
                if (auto* editor = ctrl->getTextEditor(); editor) {
                    editor->setUnderline(enabled);
                }
            }),
            control);
    gtk_box_append(box, underline);

    appendSeparator();

    GtkWidget* alignment = gtk_combo_box_text_new();
    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(alignment), _("Left"));
    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(alignment), _("Centre"));
    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(alignment), _("Right"));
    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(alignment), _("Justify"));

    int alignmentIndex = control->getToolHandler()->getTextJustify() ?
                                 3 :
                                 static_cast<int>(control->getToolHandler()->getTextAlignment());
    gtk_combo_box_set_active(GTK_COMBO_BOX(alignment), alignmentIndex);

    g_signal_connect(
            alignment,
            "changed",
            G_CALLBACK(+[](GtkComboBox* combo, gpointer data) {
                auto* ctrl = static_cast<Control*>(data);
                int active = gtk_combo_box_get_active(combo);

                if (active == 3) {
                    ctrl->getActionDatabase()->fireChangeActionState(Action::TEXT_ALIGNMENT, TextAlignment::LEFT);
                    ctrl->getActionDatabase()->fireChangeActionState(Action::TEXT_JUSTIFY, true);
                } else if (active >= 0 && active <= 2) {
                    ctrl->getActionDatabase()->fireChangeActionState(Action::TEXT_JUSTIFY, false);
                    ctrl->getActionDatabase()->fireChangeActionState(
                            Action::TEXT_ALIGNMENT, static_cast<TextAlignment::Value>(active));
                }
            }),
            control);
    gtk_widget_set_tooltip_text(alignment, _("Text alignment"));
    gtk_box_append(box, alignment);

    GtkPopover* morePopover = GTK_POPOVER(gtk_popover_new());
    gtk_widget_add_css_class(GTK_WIDGET(morePopover), "toolbar");

    GtkBox* morePanel = GTK_BOX(gtk_box_new(GTK_ORIENTATION_VERTICAL, 6));
    gtk_widget_set_margin_start(GTK_WIDGET(morePanel), 8);
    gtk_widget_set_margin_end(GTK_WIDGET(morePanel), 8);
    gtk_widget_set_margin_top(GTK_WIDGET(morePanel), 8);
    gtk_widget_set_margin_bottom(GTK_WIDGET(morePanel), 8);
    gtk_popover_set_child(morePopover, GTK_WIDGET(morePanel));

    GtkWidget* strike = gtk_toggle_button_new_with_label(_("Strikethrough"));
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(strike), control->getToolHandler()->getTextStrikethrough());
    g_signal_connect(
            strike,
            "toggled",
            G_CALLBACK(+[](GtkToggleButton* button, gpointer data) {
                auto* ctrl = static_cast<Control*>(data);
                bool enabled = gtk_toggle_button_get_active(button);
                ctrl->getToolHandler()->setTextStrikethrough(enabled);
                if (auto* editor = ctrl->getTextEditor(); editor) {
                    editor->setStrikethrough(enabled);
                }
            }),
            control);
    gtk_box_append(morePanel, strike);

    GtkBox* spacingRow = GTK_BOX(gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8));
    GtkWidget* spacingLabel = gtk_label_new(_("Line spacing"));
    gtk_widget_set_halign(spacingLabel, GTK_ALIGN_START);
    gtk_widget_set_hexpand(spacingLabel, true);
    GtkWidget* lineSpacing = gtk_spin_button_new_with_range(0.8, 2.5, 0.1);
    gtk_spin_button_set_digits(GTK_SPIN_BUTTON(lineSpacing), 1);
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(lineSpacing), control->getToolHandler()->getTextLineSpacing());
    g_signal_connect(
            lineSpacing,
            "value-changed",
            G_CALLBACK(+[](GtkSpinButton* spin, gpointer data) {
                auto* ctrl = static_cast<Control*>(data);
                double spacing = gtk_spin_button_get_value(spin);
                ctrl->getToolHandler()->setTextLineSpacing(spacing);
                if (auto* editor = ctrl->getTextEditor(); editor) {
                    editor->setLineSpacing(spacing);
                }
            }),
            control);
    gtk_box_append(spacingRow, spacingLabel);
    gtk_box_append(spacingRow, lineSpacing);
    gtk_box_append(morePanel, GTK_WIDGET(spacingRow));

    GtkWidget* bullets = gtk_button_new_with_label(_("Bullet list"));
    g_signal_connect(
            bullets,
            "clicked",
            G_CALLBACK(+[](GtkButton*, gpointer data) {
                if (auto* editor = static_cast<Control*>(data)->getTextEditor(); editor) {
                    editor->toggleBulletList();
                }
            }),
            control);
    gtk_box_append(morePanel, bullets);

    GtkWidget* numbers = gtk_button_new_with_label(_("Numbered list"));
    g_signal_connect(
            numbers,
            "clicked",
            G_CALLBACK(+[](GtkButton*, gpointer data) {
                if (auto* editor = static_cast<Control*>(data)->getTextEditor(); editor) {
                    editor->toggleNumberedList();
                }
            }),
            control);
    gtk_box_append(morePanel, numbers);

    GtkMenuButton* more = GTK_MENU_BUTTON(gtk_menu_button_new());
    gtk_button_set_label(GTK_BUTTON(more), _("Paragraph"));
    gtk_widget_set_tooltip_text(GTK_WIDGET(more), _("More text formatting"));
    gtk_menu_button_set_popover(more, GTK_WIDGET(morePopover));
    gtk_menu_button_set_direction(more, GTK_ARROW_DOWN);
    gtk_box_append(box, GTK_WIDGET(more));
    gtk_widget_show_all(GTK_WIDGET(morePanel));

    if (control->getToolHandler()->isAnswerBoxEnabled()) {
        appendSeparator();
        GtkPopover* answerPopover = GTK_POPOVER(gtk_popover_new());
        gtk_widget_add_css_class(GTK_WIDGET(answerPopover), "toolbar");
        GtkBox* answerPanel = GTK_BOX(gtk_box_new(GTK_ORIENTATION_VERTICAL, 8));
        gtk_widget_set_margin_start(GTK_WIDGET(answerPanel), 12);
        gtk_widget_set_margin_end(GTK_WIDGET(answerPanel), 12);
        gtk_widget_set_margin_top(GTK_WIDGET(answerPanel), 12);
        gtk_widget_set_margin_bottom(GTK_WIDGET(answerPanel), 12);
        gtk_popover_set_child(answerPopover, GTK_WIDGET(answerPanel));
        gtk_box_append(answerPanel, gtk_label_new(_("Answer Box appearance")));

        GtkWidget* preset = gtk_combo_box_text_new();
        gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(preset), _("Model Answer"));
        gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(preset), _("Definition"));
        gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(preset), _("Warning"));
        gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(preset), _("Note"));
        gtk_combo_box_set_active(GTK_COMBO_BOX(preset), 0);
        gtk_widget_set_tooltip_text(preset, _("Answer Box preset"));

        g_signal_connect(
                preset,
                "changed",
                G_CALLBACK(+[](GtkComboBox* combo, gpointer data) {
                    auto* ctrl = static_cast<Control*>(data);
                    auto* tools = ctrl->getToolHandler();

                    switch (gtk_combo_box_get_active(combo)) {
                        case 1:
                            tools->setAnswerBoxBackgroundColor(Color{224U, 240U, 255U, 230U});
                            tools->setAnswerBoxBorderColor(Color{50U, 110U, 180U, 255U});
                            break;
                        case 2:
                            tools->setAnswerBoxBackgroundColor(Color{255U, 228U, 232U, 235U});
                            tools->setAnswerBoxBorderColor(Color{190U, 55U, 70U, 255U});
                            break;
                        case 3:
                            tools->setAnswerBoxBackgroundColor(Color{232U, 247U, 232U, 230U});
                            tools->setAnswerBoxBorderColor(Color{60U, 135U, 75U, 255U});
                            break;
                        case 0:
                        default:
                            tools->setAnswerBoxBackgroundColor(Color{255U, 248U, 214U, 230U});
                            tools->setAnswerBoxBorderColor(Color{180U, 140U, 20U, 255U});
                            break;
                    }

                    if (auto* editor = ctrl->getTextEditor(); editor) {
                        editor->setBoxBackgroundColor(tools->getAnswerBoxBackgroundColor());
                        editor->setBoxBorderColor(tools->getAnswerBoxBorderColor());
                    }
                }),
                control);

        gtk_box_append(answerPanel, preset);

        GdkRGBA background = Util::argb_to_GdkRGBA(control->getToolHandler()->getAnswerBoxBackgroundColor());
        GtkWidget* backgroundButton = gtk_color_button_new_with_rgba(&background);
        gtk_color_chooser_set_use_alpha(GTK_COLOR_CHOOSER(backgroundButton), true);
        gtk_widget_set_tooltip_text(backgroundButton, _("Answer Box fill"));
        g_signal_connect(
                backgroundButton,
                "color-set",
                G_CALLBACK(+[](GtkColorButton* button, gpointer data) {
                    GdkRGBA color{};
                    gtk_color_chooser_get_rgba(GTK_COLOR_CHOOSER(button), &color);
                    auto* ctrl = static_cast<Control*>(data);
                    Color chosen = Util::GdkRGBA_to_argb(color);
                    ctrl->getToolHandler()->setAnswerBoxBackgroundColor(chosen);
                    if (auto* editor = ctrl->getTextEditor(); editor) {
                        editor->setBoxBackgroundColor(chosen);
                    }
                }),
                control);
        gtk_box_append(answerPanel, gtk_label_new(_("Fill colour and opacity")));
        gtk_box_append(answerPanel, backgroundButton);

        GdkRGBA border = Util::argb_to_GdkRGBA(control->getToolHandler()->getAnswerBoxBorderColor());
        GtkWidget* borderButton = gtk_color_button_new_with_rgba(&border);
        gtk_color_chooser_set_use_alpha(GTK_COLOR_CHOOSER(borderButton), true);
        gtk_widget_set_tooltip_text(borderButton, _("Answer Box border"));
        g_signal_connect(
                borderButton,
                "color-set",
                G_CALLBACK(+[](GtkColorButton* button, gpointer data) {
                    GdkRGBA color{};
                    gtk_color_chooser_get_rgba(GTK_COLOR_CHOOSER(button), &color);
                    auto* ctrl = static_cast<Control*>(data);
                    Color chosen = Util::GdkRGBA_to_argb(color);
                    ctrl->getToolHandler()->setAnswerBoxBorderColor(chosen);
                    if (auto* editor = ctrl->getTextEditor(); editor) {
                        editor->setBoxBorderColor(chosen);
                    }
                }),
                control);
        gtk_box_append(answerPanel, gtk_label_new(_("Border colour")));
        gtk_box_append(answerPanel, borderButton);

        GtkBox* stylePanel = answerPanel;

        auto appendStyleRow = [stylePanel](const char* label, GtkWidget* widget) {
            GtkBox* row = GTK_BOX(gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8));
            GtkWidget* text = gtk_label_new(label);
            gtk_widget_set_halign(text, GTK_ALIGN_START);
            gtk_widget_set_hexpand(text, true);
            gtk_box_append(row, text);
            gtk_box_append(row, widget);
            gtk_box_append(stylePanel, GTK_WIDGET(row));
        };

        GtkWidget* borderWidth = gtk_scale_new_with_range(GTK_ORIENTATION_HORIZONTAL, 0.0, 8.0, 0.2);
        gtk_range_set_value(GTK_RANGE(borderWidth), control->getToolHandler()->getAnswerBoxBorderWidth());
        gtk_scale_set_digits(GTK_SCALE(borderWidth), 1);
        gtk_widget_set_size_request(borderWidth, 140, -1);
        g_signal_connect(
                borderWidth,
                "value-changed",
                G_CALLBACK(+[](GtkRange* range, gpointer data) {
                    auto* ctrl = static_cast<Control*>(data);
                    double value = gtk_range_get_value(range);
                    ctrl->getToolHandler()->setAnswerBoxBorderWidth(value);
                    if (auto* editor = ctrl->getTextEditor(); editor) {
                        editor->setBoxBorderWidth(value);
                    }
                }),
                control);
        appendStyleRow(_("Border width"), borderWidth);

        GtkWidget* padding = gtk_scale_new_with_range(GTK_ORIENTATION_HORIZONTAL, 0.0, 24.0, 1.0);
        gtk_range_set_value(GTK_RANGE(padding), control->getToolHandler()->getAnswerBoxPadding());
        gtk_scale_set_digits(GTK_SCALE(padding), 0);
        gtk_widget_set_size_request(padding, 140, -1);
        g_signal_connect(
                padding,
                "value-changed",
                G_CALLBACK(+[](GtkRange* range, gpointer data) {
                    auto* ctrl = static_cast<Control*>(data);
                    double value = gtk_range_get_value(range);
                    ctrl->getToolHandler()->setAnswerBoxPadding(value);
                    if (auto* editor = ctrl->getTextEditor(); editor) {
                        editor->setBoxPadding(value);
                    }
                }),
                control);
        appendStyleRow(_("Padding"), padding);

        GtkWidget* radius = gtk_scale_new_with_range(GTK_ORIENTATION_HORIZONTAL, 0.0, 24.0, 1.0);
        gtk_range_set_value(GTK_RANGE(radius), control->getToolHandler()->getAnswerBoxCornerRadius());
        gtk_scale_set_digits(GTK_SCALE(radius), 0);
        gtk_widget_set_size_request(radius, 140, -1);
        g_signal_connect(
                radius,
                "value-changed",
                G_CALLBACK(+[](GtkRange* range, gpointer data) {
                    auto* ctrl = static_cast<Control*>(data);
                    double value = gtk_range_get_value(range);
                    ctrl->getToolHandler()->setAnswerBoxCornerRadius(value);
                    if (auto* editor = ctrl->getTextEditor(); editor) {
                        editor->setBoxCornerRadius(value);
                    }
                }),
                control);
        appendStyleRow(_("Corners"), radius);

        GtkMenuButton* appearance = GTK_MENU_BUTTON(gtk_menu_button_new());
        gtk_button_set_label(GTK_BUTTON(appearance), _("Box appearance"));
        gtk_widget_set_tooltip_text(GTK_WIDGET(appearance), _("Presets, border colour, padding and rounded corners"));
        gtk_menu_button_set_popover(appearance, GTK_WIDGET(answerPopover));
        gtk_box_append(box, GTK_WIDGET(appearance));
        gtk_widget_show_all(GTK_WIDGET(answerPanel));

        gtk_widget_show_all(GTK_WIDGET(stylePanel));
    }
}

void ToolUtnContextBar::appendShapeControls(DrawingType type) {
    const char* name = _("Shape");
    switch (type) {
        case DRAWING_TYPE_LINE:
            name = _("Line");
            break;
        case DRAWING_TYPE_RECTANGLE:
            name = _("Rectangle");
            break;
        case DRAWING_TYPE_ELLIPSE:
            name = _("Ellipse");
            break;
        case DRAWING_TYPE_ARROW:
            name = _("Arrow");
            break;
        case DRAWING_TYPE_DOUBLE_ARROW:
            name = _("Double Arrow");
            break;
        case DRAWING_TYPE_COORDINATE_SYSTEM:
            name = _("Coordinate System");
            break;
        case DRAWING_TYPE_SHAPE_RECOGNIZER:
            name = _("Smart Shape");
            break;
        case DRAWING_TYPE_SPLINE:
            name = _("Spline");
            break;
        default:
            break;
    }

    appendLabel(name);
    appendSeparator();
    appendColorButton();
    appendSizeButtons(TOOL_PEN);

    GtkWidget* fill = gtk_toggle_button_new_with_label(_("Fill"));
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(fill), control->getToolHandler()->getPenFillEnabled());
    g_signal_connect(
            fill,
            "toggled",
            G_CALLBACK(+[](GtkToggleButton* button, gpointer data) {
                static_cast<Control*>(data)->setFill(gtk_toggle_button_get_active(button));
            }),
            control);
    gtk_box_append(box, fill);

    GtkWidget* freehand = gtk_button_new_with_label(_("Back to Pen"));
    g_signal_connect(
            freehand,
            "clicked",
            G_CALLBACK(+[](GtkButton*, gpointer data) {
                static_cast<Control*>(data)->setToolDrawingType(DRAWING_TYPE_DEFAULT);
            }),
            control);
    gtk_box_append(box, freehand);
}

void ToolUtnContextBar::appendSelectionControls() {
    auto* tools = control->getToolHandler();

    GtkWidget* remove = gtk_button_new_with_label(_("Delete"));
    g_signal_connect(
            remove,
            "clicked",
            G_CALLBACK(+[](GtkButton*, gpointer data) {
                static_cast<Control*>(data)->deleteSelection();
            }),
            control);
    gtk_widget_set_tooltip_text(remove, _("Delete selected objects. Undo restores them."));
    gtk_box_append(box, remove);
    appendSeparator();

    if (tools->hasCapability(TOOL_CAP_COLOR)) {
        appendColorButton();
    }

    if (tools->hasCapability(TOOL_CAP_SIZE)) {
        appendLabel(_("Size"));

        struct SizeEntry {
            const char* label;
            ToolSize size;
        };

        constexpr std::array<SizeEntry, 5> sizes{{
                {"XS", TOOL_SIZE_VERY_FINE},
                {"S", TOOL_SIZE_FINE},
                {"M", TOOL_SIZE_MEDIUM},
                {"L", TOOL_SIZE_THICK},
                {"XL", TOOL_SIZE_VERY_THICK},
        }};

        for (const auto& entry: sizes) {
            GtkWidget* button = gtk_button_new_with_label(entry.label);
            g_object_set_data(G_OBJECT(button), "utn-control", control);
            g_object_set_data(G_OBJECT(button), "utn-size", GINT_TO_POINTER(static_cast<int>(entry.size)));

            g_signal_connect(
                    button,
                    "clicked",
                    G_CALLBACK(+[](GtkButton* button, gpointer) {
                        auto* ctrl = static_cast<Control*>(g_object_get_data(G_OBJECT(button), "utn-control"));
                        auto size = static_cast<ToolSize>(
                                GPOINTER_TO_INT(g_object_get_data(G_OBJECT(button), "utn-size")));
                        ctrl->setToolSize(size);
                    }),
                    nullptr);

            gtk_box_append(box, button);
        }
    }

    appendSeparator();

    GtkWidget* front = gtk_button_new_with_label(_("Front"));
    g_signal_connect(
            front,
            "clicked",
            G_CALLBACK(+[](GtkButton*, gpointer data) {
                static_cast<Control*>(data)->reorderSelection(EditSelection::OrderChange::BringToFront);
            }),
            control);
    gtk_widget_set_tooltip_text(front, _("Bring selection to front"));
    gtk_box_append(box, front);

    GtkWidget* back = gtk_button_new_with_label(_("Back"));
    g_signal_connect(
            back,
            "clicked",
            G_CALLBACK(+[](GtkButton*, gpointer data) {
                static_cast<Control*>(data)->reorderSelection(EditSelection::OrderChange::SendToBack);
            }),
            control);
    gtk_widget_set_tooltip_text(back, _("Send selection to back"));
    gtk_box_append(box, back);



    if (!tools->hasCapability(TOOL_CAP_COLOR) && !tools->hasCapability(TOOL_CAP_SIZE)) {
        appendGenericMessage(_("Drag over an object to select it"));
    }
}

void ToolUtnContextBar::appendGenericMessage(const char* text) {
    GtkWidget* label = gtk_label_new(text);
    gtk_widget_add_css_class(label, "utn-context-hint");
    gtk_box_append(box, label);
}

auto ToolUtnContextBar::getToolDisplayName() const -> std::string {
    return _("UTN Context Bar");
}

auto ToolUtnContextBar::getNewToolIcon() const -> GtkWidget* {
    return gtk_label_new("⋯");
}
