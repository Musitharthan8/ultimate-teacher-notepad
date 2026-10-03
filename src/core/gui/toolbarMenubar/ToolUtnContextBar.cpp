/*
 * Ultimate Teacher Notepad
 *
 * Context-sensitive teacher toolbar
 *
 * Based on Xournal++ GPLv2+
 */

#include "ToolUtnContextBar.h"

#include <array>
#include <utility>

#include "control/Control.h"
#include "control/ToolHandler.h"
#include "control/settings/Settings.h"
#include "model/Font.h"
#include "model/TextAlignment.h"
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
    if (tool == TOOL_TEXT && control->getToolHandler()->hasTeacherStamp()) {
        title = _("Feedback");
    } else if (tool == TOOL_TEXT && control->getToolHandler()->isAnswerBoxEnabled()) {
        title = _("Answer Box");
    }

    appendLabel(title);
    appendSeparator();

    switch (tool) {
        case TOOL_PEN:
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
            appendGenericMessage(_("Select an object to edit its properties"));
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
    appendSeparator();

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
                XojFont font(gtk_font_button_get_font_name(button));
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
                    ctrl->getToolHandler()->setTextAlignment(TextAlignment{alignment});
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
                static_cast<Control*>(data)->getToolHandler()->setTextJustify(
                        gtk_toggle_button_get_active(button));
            }),
            control);
    gtk_box_append(box, justify);

    if (control->getToolHandler()->isAnswerBoxEnabled()) {
        GtkWidget* badge = gtk_label_new(_("Answer Box"));
        gtk_widget_add_css_class(badge, "utn-context-badge");
        gtk_box_append(box, badge);
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
