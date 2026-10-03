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

#include "control/Control.h"
#include "control/ToolHandler.h"
#include "control/actions/ActionDatabase.h"
#include "control/settings/Settings.h"
#include "control/tools/TextEditor.h"
#include "model/Font.h"
#include "model/TextAlignment.h"
#include "util/Color.h"
#include "util/Util.h"
#include "util/gtk4_helper.h"
#include "util/i18n.h"

namespace {
const char* toolTitle(ToolType tool) {
    switch (tool) {
        case TOOL_PEN:
            return _("Pen");
        case TOOL_ERASER:
            return _("Eraser");
        case TOOL_HIGHLIGHTER:
            return _("Markup");
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
    return xoj::util::WidgetSPtr(GTK_WIDGET(box), xoj::util::adopt);
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

    appendLabel(title);
    appendSeparator();

    switch (tool) {
        case TOOL_PEN: {
            if (control->getToolHandler()->getDrawingType() != DRAWING_TYPE_DEFAULT) {
                appendShapeControls(control->getToolHandler()->getDrawingType());
                break;
            }

            appendLabel(_("Profile"));

            struct ProfileEntry {
                const char* label;
                ToolSize size;
                Color color;
            };

            const std::array<ProfileEntry, 4> profiles{{
                    {_("Pen"), TOOL_SIZE_MEDIUM, Colors::xopp_royalblue},
                    {_("Pencil"), TOOL_SIZE_VERY_FINE, Colors::gray},
                    {_("Brush"), TOOL_SIZE_THICK, Colors::black},
                    {_("Marker"), TOOL_SIZE_THICK, Colors::xopp_darkorange},
            }};

            for (const auto& profile: profiles) {
                GtkWidget* button = gtk_button_new_with_label(profile.label);
                g_object_set_data(G_OBJECT(button), "utn-control", control);
                g_object_set_data(G_OBJECT(button), "utn-size", GINT_TO_POINTER(static_cast<int>(profile.size)));

                auto* colorData = new Color(profile.color);
                g_object_set_data_full(G_OBJECT(button), "utn-color", colorData,
                                       +[](gpointer data) { delete static_cast<Color*>(data); });

                g_signal_connect(
                        button,
                        "clicked",
                        G_CALLBACK(+[](GtkButton* button, gpointer) {
                            auto* ctrl = static_cast<Control*>(g_object_get_data(G_OBJECT(button), "utn-control"));
                            auto size = static_cast<ToolSize>(
                                    GPOINTER_TO_INT(g_object_get_data(G_OBJECT(button), "utn-size")));
                            auto* color = static_cast<Color*>(g_object_get_data(G_OBJECT(button), "utn-color"));

                            auto* tools = ctrl->getToolHandler();
                            tools->selectTool(TOOL_PEN);
                            tools->setDrawingType(DRAWING_TYPE_DEFAULT);
                            tools->setPenSize(size);
                            tools->setColor(*color, false);
                            tools->fireToolChanged();
                        }),
                        nullptr);

                gtk_box_append(box, button);
            }

            appendSeparator();
            appendColorButton();
            appendSizeButtons(TOOL_PEN);

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

    appendLabel(_("Size"));

    for (const auto& entry: sizes) {
        GtkWidget* button = gtk_button_new_with_label(entry.label);
        g_object_set_data(G_OBJECT(button), "utn-control", control);
        g_object_set_data(G_OBJECT(button), "utn-tool", GINT_TO_POINTER(static_cast<int>(tool)));
        g_object_set_data(G_OBJECT(button), "utn-size", GINT_TO_POINTER(static_cast<int>(entry.size)));

        g_signal_connect(
                button,
                "clicked",
                G_CALLBACK(+[](GtkButton* button, gpointer) {
                    auto* ctrl = static_cast<Control*>(g_object_get_data(G_OBJECT(button), "utn-control"));
                    auto tool = static_cast<ToolType>(
                            GPOINTER_TO_INT(g_object_get_data(G_OBJECT(button), "utn-tool")));
                    auto size = static_cast<ToolSize>(
                            GPOINTER_TO_INT(g_object_get_data(G_OBJECT(button), "utn-size")));

                    auto* tools = ctrl->getToolHandler();
                    if (tool == TOOL_PEN) {
                        tools->setPenSize(size);
                    } else if (tool == TOOL_HIGHLIGHTER) {
                        tools->setHighlighterSize(size);
                    }
                }),
                nullptr);

        gtk_box_append(box, button);
    }
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
}

void ToolUtnContextBar::appendMarkupControls() {
    appendColorButton();
    appendSizeButtons(TOOL_HIGHLIGHTER);

    appendLabel(_("Opacity"));
    GtkWidget* opacity = gtk_scale_new_with_range(GTK_ORIENTATION_HORIZONTAL, 10.0, 100.0, 5.0);
    Color markupColor = control->getToolHandler()->getTool(TOOL_HIGHLIGHTER).getColor();
    gtk_range_set_value(GTK_RANGE(opacity), 100.0 * static_cast<double>(markupColor.alpha) / 255.0);
    gtk_scale_set_digits(GTK_SCALE(opacity), 0);
    gtk_widget_set_size_request(opacity, 110, -1);
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

    GtkWidget* freehand = gtk_button_new_with_label(_("Freehand"));
    g_signal_connect(
            freehand,
            "clicked",
            G_CALLBACK(+[](GtkButton*, gpointer data) {
                auto* tools = static_cast<Control*>(data)->getToolHandler();
                tools->setSmartHighlighterEnabled(false);
            }),
            control);
    gtk_box_append(box, freehand);

    auto addMode = [this](const char* label, SmartHighlighterSnapMode mode) {
        GtkWidget* button = gtk_button_new_with_label(label);
        g_object_set_data(G_OBJECT(button), "utn-control", control);
        g_object_set_data(G_OBJECT(button), "utn-mode", GINT_TO_POINTER(static_cast<int>(mode)));

        g_signal_connect(
                button,
                "clicked",
                G_CALLBACK(+[](GtkButton* button, gpointer) {
                    auto* ctrl = static_cast<Control*>(g_object_get_data(G_OBJECT(button), "utn-control"));
                    auto mode = static_cast<SmartHighlighterSnapMode>(
                            GPOINTER_TO_INT(g_object_get_data(G_OBJECT(button), "utn-mode")));
                    auto* tools = ctrl->getToolHandler();
                    tools->setSmartHighlighterEnabled(true);
                    tools->setSmartHighlighterSnapMode(mode);
                }),
                nullptr);

        gtk_box_append(box, button);
    };

    addMode(_("Straight"), SmartHighlighterSnapMode::Straight);
    addMode(_("Word"), SmartHighlighterSnapMode::Word);
    addMode(_("Line"), SmartHighlighterSnapMode::Line);
}

void ToolUtnContextBar::appendTextControls() {
    appendColorButton();

    XojFont current = control->getSettings()->getFont();

    GtkWidget* fontButton = gtk_font_button_new_with_font(current.asString().c_str());
    gtk_font_button_set_use_size(GTK_FONT_BUTTON(fontButton), false);
    gtk_font_button_set_show_style(GTK_FONT_BUTTON(fontButton), true);
    gtk_widget_set_tooltip_text(fontButton, _("Font family"));

    g_signal_connect(
            fontButton,
            "font-set",
            G_CALLBACK(+[](GtkFontButton* button, gpointer data) {
                auto* ctrl = static_cast<Control*>(data);
                XojFont chosen(gtk_font_button_get_font_name(button));
                XojFont font = ctrl->getSettings()->getFont();

                // Keep the explicit UTN size control authoritative when changing families/styles.
                font.setName(chosen.getName());
                ctrl->fontChanged(font);
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
                XojFont font = ctrl->getSettings()->getFont();
                font.setSize(gtk_spin_button_get_value(spin));
                ctrl->fontChanged(font);
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
                XojFont font = ctrl->getSettings()->getFont();
                PangoFontDescription* desc = pango_font_description_from_string(font.asString().c_str());

                pango_font_description_set_weight(
                        desc, gtk_toggle_button_get_active(button) ? PANGO_WEIGHT_BOLD : PANGO_WEIGHT_NORMAL);

                gchar* value = pango_font_description_to_string(desc);
                XojFont updated(value);
                g_free(value);
                pango_font_description_free(desc);
                ctrl->fontChanged(updated);
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
                XojFont font = ctrl->getSettings()->getFont();
                PangoFontDescription* desc = pango_font_description_from_string(font.asString().c_str());

                pango_font_description_set_style(
                        desc, gtk_toggle_button_get_active(button) ? PANGO_STYLE_ITALIC : PANGO_STYLE_NORMAL);

                gchar* value = pango_font_description_to_string(desc);
                XojFont updated(value);
                g_free(value);
                pango_font_description_free(desc);
                ctrl->fontChanged(updated);
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

    GtkWidget* strike = gtk_toggle_button_new_with_label("S");
    gtk_widget_set_tooltip_text(strike, _("Strikethrough"));
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
    gtk_box_append(box, strike);

    appendLabel(_("Spacing"));
    GtkWidget* lineSpacing = gtk_spin_button_new_with_range(0.8, 2.5, 0.1);
    gtk_spin_button_set_digits(GTK_SPIN_BUTTON(lineSpacing), 1);
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(lineSpacing), control->getToolHandler()->getTextLineSpacing());
    gtk_widget_set_tooltip_text(lineSpacing, _("Line spacing"));
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
    gtk_box_append(box, lineSpacing);

    appendSeparator();

    struct AlignEntry {
        const char* label;
        TextAlignment::Value alignment;
    };

    constexpr std::array<AlignEntry, 3> alignments{{
            {"L", TextAlignment::LEFT},
            {"C", TextAlignment::CENTER},
            {"R", TextAlignment::RIGHT},
    }};

    for (const auto& entry: alignments) {
        GtkWidget* button = gtk_button_new_with_label(entry.label);
        g_object_set_data(G_OBJECT(button), "utn-control", control);
        g_object_set_data(G_OBJECT(button), "utn-align", GINT_TO_POINTER(static_cast<int>(entry.alignment)));

        g_signal_connect(
                button,
                "clicked",
                G_CALLBACK(+[](GtkButton* button, gpointer) {
                    auto* ctrl = static_cast<Control*>(g_object_get_data(G_OBJECT(button), "utn-control"));
                    auto alignment = static_cast<TextAlignment::Value>(
                            GPOINTER_TO_INT(g_object_get_data(G_OBJECT(button), "utn-align")));
                    ctrl->getActionDatabase()->fireChangeActionState(Action::TEXT_ALIGNMENT, alignment);
                }),
                nullptr);

        gtk_box_append(box, button);
    }

    GtkWidget* justify = gtk_toggle_button_new_with_label(_("Justify"));
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(justify), control->getToolHandler()->getTextJustify());
    g_signal_connect(
            justify,
            "toggled",
            G_CALLBACK(+[](GtkToggleButton* button, gpointer data) {
                auto* ctrl = static_cast<Control*>(data);
                ctrl->getActionDatabase()->fireChangeActionState(
                        Action::TEXT_JUSTIFY, gtk_toggle_button_get_active(button));
            }),
            control);
    gtk_box_append(box, justify);

    if (control->getToolHandler()->isAnswerBoxEnabled()) {
        appendSeparator();

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

        gtk_box_append(box, preset);

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
        gtk_box_append(box, backgroundButton);

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
        gtk_box_append(box, borderButton);
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
                auto* ctrl = static_cast<Control*>(data);
                ctrl->setToolDrawingType(DRAWING_TYPE_DEFAULT);
                ctrl->getToolHandler()->fireToolChanged();
            }),
            control);
    gtk_box_append(box, freehand);
}

void ToolUtnContextBar::appendSelectionControls() {
    auto* tools = control->getToolHandler();

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

    GtkWidget* remove = gtk_button_new_with_label(_("Delete"));
    g_signal_connect(
            remove,
            "clicked",
            G_CALLBACK(+[](GtkButton*, gpointer data) {
                static_cast<Control*>(data)->deleteSelection();
            }),
            control);
    gtk_box_append(box, remove);

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
