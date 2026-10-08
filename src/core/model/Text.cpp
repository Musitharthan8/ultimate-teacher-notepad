#include "Text.h"

#include <algorithm>
#include <memory>
#include <utility>  // for move

#include <glib.h>  // for g_warning
#include <pango/pangocairo.h>

#include "model/AudioContent.h"  // for AudioContent
#include "model/Element.h"        // for ELEMENT_TEXT, Eleme...
#include "model/Font.h"           // for XojFont
#include "pdf/base/XojPdfPage.h"  // for XojPdfRectangle
#include "util/Rectangle.h"       // for Rectangle
#include "util/Stacktrace.h"      // for Stacktrace
#include "util/StringUtils.h"
#include "util/matrix/RectangleMultiply.h"
#include "util/raii/GObjectSPtr.h"
#include "util/raii/PangoSPtr.h"
#include "util/safe_casts.h"                      // for round_cast
#include "util/serializing/ObjectInputStream.h"   // for ObjectInputStream
#include "util/serializing/ObjectOutputStream.h"  // for ObjectOutputStream

using xoj::util::Rectangle;

Text::Text(): RectangularElement(ELEMENT_TEXT) {
    this->font.setName("Sans");
    this->font.setSize(12);
}

Text::~Text() = default;

auto Text::cloneText() const -> std::unique_ptr<Text> {
    auto text = std::make_unique<Text>();
    static_cast<RectangularElement&>(*text) = *this;
    static_cast<AudioContent&>(*text) = *this;

    text->font = this->font;
    text->text = this->text;
    text->inEditing = this->inEditing;
    text->wrapWidth = this->wrapWidth;
    text->align = this->align;
    text->justify = this->justify;
    text->underlined = this->underlined;
    text->strikethrough = this->strikethrough;
    text->lineSpacing = this->lineSpacing;
    text->boxEnabled = this->boxEnabled;
    text->boxBackgroundColor = this->boxBackgroundColor;
    text->boxBorderColor = this->boxBorderColor;
    text->boxBorderWidth = this->boxBorderWidth;
    text->boxPadding = this->boxPadding;
    text->boxCornerRadius = this->boxCornerRadius;

    return text;
}

auto Text::clone() const -> ElementPtr { return cloneText(); }

auto Text::getFont() -> XojFont& { return font; }
auto Text::getFont() const -> const XojFont& { return font; }

void Text::setFont(const XojFont& font) {
    this->font = font;
    sizeCalculated = false;
}

auto Text::getFontSize() const -> double { return font.getSize(); }

auto Text::getFontName() const -> std::string { return font.getName(); }

auto Text::getText() const -> const std::string& { return this->text; }

void Text::setText(std::string text) {
    this->text = std::move(text);
    sizeCalculated = false;
}

void Text::setWrap(double wrap) {
    this->wrapWidth = wrap;
    sizeCalculated = false;
}

void Text::setAlignment(TextAlignment a) {
    this->align = a;
    sizeCalculated = false;
}

auto Text::isUnderlined() const -> bool { return this->underlined; }

void Text::setUnderlined(bool enabled) {
    this->underlined = enabled;
    sizeCalculated = false;
}

auto Text::isStrikethrough() const -> bool { return this->strikethrough; }

void Text::setStrikethrough(bool enabled) {
    this->strikethrough = enabled;
    sizeCalculated = false;
}

auto Text::getLineSpacing() const -> double { return this->lineSpacing; }

void Text::setLineSpacing(double spacing) {
    this->lineSpacing = std::clamp(spacing, 0.8, 2.5);
    sizeCalculated = false;
}

auto Text::isBoxEnabled() const -> bool { return this->boxEnabled; }

void Text::setBoxEnabled(bool enabled) {
    this->boxEnabled = enabled;
    sizeCalculated = false;
}

auto Text::getBoxBackgroundColor() const -> Color { return this->boxBackgroundColor; }

void Text::setBoxBackgroundColor(Color color) { this->boxBackgroundColor = color; }

auto Text::getBoxBorderColor() const -> Color { return this->boxBorderColor; }

void Text::setBoxBorderColor(Color color) { this->boxBorderColor = color; }

auto Text::getBoxBorderWidth() const -> double { return this->boxBorderWidth; }

void Text::setBoxBorderWidth(double width) {
    this->boxBorderWidth = std::max(0.0, width);
    sizeCalculated = false;
}

auto Text::getBoxPadding() const -> double { return this->boxPadding; }

void Text::setBoxPadding(double padding) {
    this->boxPadding = std::max(0.0, padding);
    sizeCalculated = false;
}

auto Text::getBoxCornerRadius() const -> double { return this->boxCornerRadius; }

void Text::setBoxCornerRadius(double radius) { this->boxCornerRadius = std::max(0.0, radius); }

static auto computeAnswerBoxBounds(const Text::Boxes& boxes, double padding, double borderWidth)
        -> xoj::util::Rectangle<double> {
    const double extra = padding + 0.5 * borderWidth;
    const double left = std::min(0.0, boxes.effectiveBounds.x) - extra;
    const double top = std::min(0.0, boxes.effectiveBounds.y) - extra;
    const double right =
            std::max(boxes.theoreticalSize.width, boxes.effectiveBounds.x + boxes.effectiveBounds.width) + extra;
    const double bottom =
            std::max(boxes.theoreticalSize.height, boxes.effectiveBounds.y + boxes.effectiveBounds.height) + extra;

    return {left, top, right - left, bottom - top};
}

auto Text::getBoxBounds() const -> xoj::util::Rectangle<double> {
    if (!this->sizeCalculated) {
        this->calcSize();
    }

    Boxes boxes{this->naturalSize, this->effectiveBounds};
    return computeAnswerBoxBounds(boxes, this->boxPadding, this->boxBorderWidth);
}

Text::Boxes Text::computeBoxesForLayout(PangoLayout* layout, double wrapWidth) {
    PangoRectangle box;
    pango_layout_get_extents(layout, nullptr, &box);

    xoj::util::Point<double> offset{static_cast<double>(box.x) / PANGO_SCALE, static_cast<double>(box.y) / PANGO_SCALE};

    Boxes res;

    res.effectiveBounds.width = static_cast<double>(box.width) / PANGO_SCALE;
    res.effectiveBounds.height = static_cast<double>(box.height) / PANGO_SCALE;
    res.effectiveBounds.x = offset.x;
    res.effectiveBounds.y = offset.y;

    if (wrapWidth != NO_WRAP) {
        res.theoreticalSize.width = wrapWidth;
    } else {
        res.theoreticalSize.width = res.effectiveBounds.width + offset.x;
    }
    res.theoreticalSize.height = res.effectiveBounds.height + offset.y;

    return res;
}

void Text::calcSize() const {
    auto layout = createPangoLayout();
    pango_layout_set_text(layout.get(), this->text.c_str(), static_cast<int>(this->text.length()));

    auto boxes = computeBoxesForLayout(layout.get(), this->wrapWidth);
    this->naturalSize = boxes.theoreticalSize;
    this->effectiveBounds = boxes.effectiveBounds;

    const auto& matrix = this->getTransformation();
    if (this->boxEnabled) {
        auto boxBounds = computeAnswerBoxBounds(boxes, this->boxPadding, this->boxBorderWidth);
        this->boundingBox = matrix * boxBounds;
        this->snappedBounds = matrix * boxBounds;
    } else {
        this->boundingBox = matrix * this->effectiveBounds;
        this->snappedBounds = matrix * xoj::util::Rectangle<double>{{0, 0}, this->naturalSize};
    }

    this->sizeCalculated = true;
}

void Text::setInEditing(bool inEditing) { this->inEditing = inEditing; }

auto Text::createPangoLayout() const -> xoj::util::GObjectSPtr<PangoLayout> {
    xoj::util::GObjectSPtr<PangoContext> c(pango_font_map_create_context(pango_cairo_font_map_get_default()),
                                           xoj::util::adopt);
    pango_context_set_round_glyph_positions(c.get(), false);  // Avoid weird glyph positioning on small fonts
    xoj::util::GObjectSPtr<PangoLayout> layout(pango_layout_new(c.get()), xoj::util::adopt);

    pango_layout_set_width(layout.get(),
                           this->wrapWidth == NO_WRAP ? -1 : round_cast<int>(this->wrapWidth * PANGO_SCALE));

    pango_layout_set_justify(layout.get(), this->justify);
    pango_layout_set_alignment(layout.get(), this->align.toPango());

#if PANGO_VERSION_CHECK(1, 48, 5)  // see https://gitlab.gnome.org/GNOME/pango/-/issues/499
    pango_layout_set_line_spacing(layout.get(), this->lineSpacing);
#endif

    // UTN applies underline/strike to the whole text object.
    xoj::util::PangoAttrListSPtr attrs(pango_attr_list_new(), xoj::util::adopt);
    if (this->underlined) {
        pango_attr_list_insert(attrs.get(), pango_attr_underline_new(PANGO_UNDERLINE_SINGLE));
    }
    if (this->strikethrough) {
        pango_attr_list_insert(attrs.get(), pango_attr_strikethrough_new(true));
    }
    pango_layout_set_attributes(layout.get(), attrs.get());

    updatePangoFont(layout.get());

    return layout;
}

void Text::updatePangoFont(PangoLayout* layout) const {
    PangoFontDescription* desc = pango_font_description_from_string(this->getFontName().c_str());
    pango_font_description_set_absolute_size(desc, this->getFontSize() * PANGO_SCALE);

    pango_layout_set_font_description(layout, desc);
    pango_font_description_free(desc);
}

auto Text::isInEditing() const -> bool { return this->inEditing; }

void Text::serialize(ObjectOutputStream& out) const {
    out.writeObject("Text");

    this->RectangularElement::serialize(out);
    this->AudioContent::serialize(out);

    out.writeString(this->text);

    font.serialize(out);

    out.writeDouble(this->wrapWidth);
    out.writeInt(static_cast<int>(this->align));
    out.writeInt(this->justify);

    // Plain text keeps the upstream clipboard format.
    if (underlined || strikethrough || lineSpacing != 1.0 || boxEnabled) {
        out.writeObject("UTNTextStyle");
        out.writeInt(underlined);
        out.writeInt(strikethrough);
        out.writeDouble(lineSpacing);
        out.writeInt(boxEnabled);
        out.writeUInt(uint32_t(boxBackgroundColor));
        out.writeUInt(uint32_t(boxBorderColor));
        out.writeDouble(boxBorderWidth);
        out.writeDouble(boxPadding);
        out.writeDouble(boxCornerRadius);
        out.endObject();
    }

    out.endObject();
}

void Text::readSerialized(ObjectInputStream& in) {
    in.readObject("Text");

    this->RectangularElement::readSerialized(in);
    this->AudioContent::readSerialized(in);

    this->text = in.readString();

    font.readSerialized(in);

    this->wrapWidth = in.readDouble();
    this->align = static_cast<TextAlignment::Value>(in.readInt());
    this->align.validate();
    this->justify = in.readInt() != 0;

    // Older clipboard payloads end here and use the default UTN style.
    const Text defaults;
    setUnderlined(defaults.isUnderlined());
    setStrikethrough(defaults.isStrikethrough());
    setLineSpacing(defaults.getLineSpacing());
    setBoxEnabled(defaults.isBoxEnabled());
    setBoxBackgroundColor(defaults.getBoxBackgroundColor());
    setBoxBorderColor(defaults.getBoxBorderColor());
    setBoxBorderWidth(defaults.getBoxBorderWidth());
    setBoxPadding(defaults.getBoxPadding());
    setBoxCornerRadius(defaults.getBoxCornerRadius());
    if (in.hasNextObject("UTNTextStyle")) {
        in.readObject("UTNTextStyle");
        setUnderlined(in.readInt() != 0);
        setStrikethrough(in.readInt() != 0);
        setLineSpacing(in.readDouble());
        setBoxEnabled(in.readInt() != 0);
        setBoxBackgroundColor(Color(in.readUInt()));
        setBoxBorderColor(Color(in.readUInt()));
        setBoxBorderWidth(in.readDouble());
        setBoxPadding(in.readDouble());
        setBoxCornerRadius(in.readDouble());
        in.endObject();
    }

    in.endObject();
}

auto Text::findText(const std::string& search) const -> std::vector<XojPdfRectangle> {
    size_t patternLength = search.length();
    if (patternLength == 0) {
        return {};
    }

    auto layout = this->createPangoLayout();
    pango_layout_set_text(layout.get(), this->text.c_str(), static_cast<int>(this->text.length()));


    std::string text = StringUtils::toLowerCase(this->text);
    std::string pattern = StringUtils::toLowerCase(search);

    const auto& origin = this->getOrigin();

    std::vector<XojPdfRectangle> list;

    for (size_t pos = text.find(pattern); pos != std::string::npos; pos = text.find(pattern, pos + 1)) {
        XojPdfRectangle mark;
        PangoRectangle rect = {0};
        pango_layout_index_to_pos(layout.get(), static_cast<int>(pos), &rect);
        mark.x1 = (static_cast<double>(rect.x)) / PANGO_SCALE + origin.x;
        mark.y1 = (static_cast<double>(rect.y)) / PANGO_SCALE + origin.y;

        pango_layout_index_to_pos(layout.get(), static_cast<int>(pos + patternLength - 1), &rect);
        mark.x2 = (static_cast<double>(rect.x) + rect.width) / PANGO_SCALE + origin.x;
        mark.y2 = (static_cast<double>(rect.y) + rect.height) / PANGO_SCALE + origin.y;

        list.push_back(mark);
    }

    return list;
}
