#include "SaveHandler.h"

#include <cinttypes>   // for PRIx32
#include <cmath>       // for isfinite
#include <cstdint>     // for uint32_t
#include <cstdio>      // for sprintf, size_t
#include <iomanip>
#include <limits>
#include <locale>
#include <sstream>

#include <cairo.h>                  // for cairo_surface_t
#include <gdk-pixbuf/gdk-pixbuf.h>  // for gdk_pixbuf_save
#include <glib.h>                   // for g_free, g_strdup_printf

#include "control/pagetype/PageTypeHandler.h"  // for PageTypeHandler
#include "control/xml/XmlDeferredStrokeNode.h"  // for XmlDeferredStrokeNode
#include "control/xml/XmlImageNode.h"          // for XmlImageNode
#include "control/xml/XmlNode.h"               // for XmlNode
#include "control/xml/XmlPointNode.h"          // for XmlPointNode
#include "control/xml/XmlTexNode.h"            // for XmlTexNode
#include "control/xml/XmlTextNode.h"           // for XmlTextNode
#include "control/xojfile/XmlAttrs.h"          // for xml_attrs
#include "control/xojfile/XmlTags.h"           // for xml_tags
#include "control/xojfile/XmlValues.h"         // for xml_values
#include "model/AudioContent.h"                // for AudioContent
#include "model/BackgroundImage.h"             // for BackgroundImage
#include "model/Document.h"                    // for Document
#include "model/Element.h"                     // for Element, ELEMENT_IMAGE
#include "model/Font.h"                        // for XojFont
#include "model/Image.h"                       // for Image
#include "model/Layer.h"                       // for Layer
#include "model/LineStyle.h"                   // for LineStyle
#include "model/LayerAudience.h"               // for audienceToString
#include "model/Link.h"                        // for Link
#include "model/PageType.h"                    // for PageType
#include "model/Point.h"                       // for Point
#include "model/Stroke.h"                      // for Stroke, StrokeCapStyle
#include "model/eraser/MaskedStrokeCodec.h"
#include "model/StrokeStyle.h"                 // for StrokeStyle
#include "model/TexImage.h"                    // for TexImage
#include "model/Text.h"                        // for Text
#include "model/XojPage.h"                     // for XojPage
#include "pdf/base/XojPdfDocument.h"           // for XojPdfDocument
#include "util/OutputStream.h"                 // for GzOutputStream, Output...
#include "util/PathUtil.h"                     // for clearExtensions, normalizeAssetPath
#include "util/PlaceholderString.h"            // for PlaceholderString
#include "util/i18n.h"                         // for FS, _F

#include "config.h"  // for FILE_FORMAT_VERSION
#include "filesystem.h"


static constexpr auto& TAG_NAMES = xoj::xml_tags::NAMES;
using TagType = xoj::xml_tags::Type;

SaveHandler::SaveHandler() {
    this->firstPdfPageVisited = false;
    this->attachBgId = 1;
}

namespace {
/// The image that stands in for a partially erased stroke: its clipped raster and, for native saves, the editable data
std::unique_ptr<XmlImageNode> buildMaskedImageNode(const Stroke& s, const utn::eraser::MaskedStrokeFallback& fallback,
                                                   ExportAudience audience) {
    // Editable metadata contains the original unerased ink and its audio path. Only native/private saves may preserve
    // it; a student copy receives the clipped raster pixels alone.
    const auto metadata = audience == ExportAudience::Teacher ? utn::eraser::encodeMaskedStroke(s, fallback.matrix) :
                                                                std::string{};
    // new, not make_unique: the tag name is a consteval StaticStringView and cannot be forwarded through make_unique
    auto image = std::unique_ptr<XmlImageNode>(new XmlImageNode(TAG_NAMES[TagType::IMAGE]));
    image->setImage(fallback.surface.get());
    const auto& m = fallback.matrix;
    const int width = cairo_image_surface_get_width(fallback.surface.get());
    const int height = cairo_image_surface_get_height(fallback.surface.get());
    const auto precise = [](double value) {
        std::ostringstream out;
        out.imbue(std::locale::classic());
        out << std::setprecision(std::numeric_limits<double>::max_digits10) << value;
        return out.str();
    };
    image->setAttrib(xoj::xml_attrs::LEFT_POS_STR, precise(m.shift.x));
    image->setAttrib(xoj::xml_attrs::TOP_POS_STR, precise(m.shift.y));
    image->setAttrib(xoj::xml_attrs::RIGHT_POS_STR, precise(m.shift.x + width * m.xx));
    image->setAttrib(xoj::xml_attrs::BOTTOM_POS_STR, precise(m.shift.y + height * m.yy));
    image->setAttrib(xoj::xml_attrs::NATURAL_SIZE_STR, std::to_string(width) + " " + std::to_string(height));
    // Oversized editable metadata must never prevent saving the lesson. Keep the clipped image when codec limits are
    // exceeded.
    if (!metadata.empty()) {
        image->setAttrib(u8"utn-stroke-v1", metadata);
    } else if (audience == ExportAudience::Teacher) {
        g_warning("Saving complex erased stroke as a clipped image (editable metadata limit exceeded)");
    }
    return image;
}
}  // namespace

void SaveHandler::resolveDeferredStrokes() {
    for (XmlDeferredStrokeNode* node: deferredStrokes) {
        if (node->hasBeenResolved()) {
            continue;
        }
        const Stroke& s = node->getSnapshot();
        if (auto fallback = utn::eraser::rasterizeMaskedStroke(s)) {
            node->setResolved(buildMaskedImageNode(s, *fallback, exportAudience));
            continue;
        }
        // A save must never silently drop an annotation, and one stroke that cannot be drawn must not make every
        // later save and autosave fail either.
        if (exportAudience == ExportAudience::Student) {
            // The erased part may have been private, so it is never written back.
            warnings.emplace_back("A partially erased stroke could not be rendered and was left out of the student copy.");
            g_warning("%s", warnings.back().c_str());
            node->setResolved(nullptr, true);
            continue;
        }
        // Rendering fails for non-finite coordinates (damaged data) or when no surface can be allocated at all. Keep
        // every usable point; "nan" in the file would make the whole lesson unreadable.
        auto restored = std::make_unique<Stroke>();
        restored->applyStyleFrom(&s);
        for (const Point& p: s.getPointVector()) {
            if (std::isfinite(p.x) && std::isfinite(p.y) && std::isfinite(p.z)) {
                restored->addPoint(p);
            }
        }
        if (restored->getPointCount() < 2) {
            warnings.emplace_back("A damaged partially erased stroke had no usable points and was left out.");
            g_warning("%s", warnings.back().c_str());
            node->setResolved(nullptr, true);
            continue;
        }
        warnings.emplace_back("A partially erased stroke could not be rendered. It was saved with its erased part restored.");
        g_warning("%s", warnings.back().c_str());
        auto* point = new XmlPointNode(TAG_NAMES[TagType::STROKE]);
        visitStroke(point, restored.get());
        node->setResolved(std::unique_ptr<XmlNode>(point));
    }
    deferredStrokes.clear();
}

void SaveHandler::prepareSave(const Document* doc, const fs::path& target) {
    errorMessage.clear();
    warnings.clear();
    deferredStrokes.clear();
    if (this->root) {
        // cleanup old data
        backgroundImages.clear();
    }

    // Compatibility export can reference a whole source PDF/image, and LaTeX carries its source and original binary.
    // Check the whole document before visiting pages: visitPage may write an attached PDF as a side effect.
    if (this->exportAudience == ExportAudience::Student) {
        for (size_t i = 0; i < doc->getPageCount(); ++i) {
            const auto page = doc->getPage(i);
            bool unsupported = page->getBackgroundType().isPdfPage() || page->getBackgroundType().isImagePage();
            for (const Layer* layer: page->getLayersView()) {
                if (!includeLayer(*layer)) continue;
                for (const Element* element: layer->getElementsView()) {
                    unsupported = unsupported || element->getType() == ELEMENT_TEXIMAGE;
                }
            }
            if (unsupported) {
                errorMessage = _("Xournal student copies cannot safely include PDF/image backgrounds or LaTeX sources. "
                                 "Export a rendered student PNG instead.");
                return;
            }
        }
    }

    this->firstPdfPageVisited = false;
    this->attachBgId = 1;

    root.reset(new XmlNode(TAG_NAMES[TagType::XOURNAL]));

    writeHeader();

    // The thumbnail shows every visible layer, teacher-only ink included: a student copy carries none
    if (this->exportAudience == ExportAudience::Teacher) {
        auto preview = doc->getPreview();
        if (preview) {
            auto* image = new XmlImageNode(TAG_NAMES[TagType::PREVIEW]);
            image->setImage(preview.get());
            this->root->addChild(image);
        }
    }

    for (size_t i = 0; i < doc->getPageCount(); i++) {
        PageRef p = doc->getPage(i);
        p->getBackgroundImage().clearSaveState();
    }

    for (size_t i = 0; i < doc->getPageCount(); i++) {
        PageRef p = doc->getPage(i);
        visitPage(root.get(), p, doc, static_cast<int>(i), target);
    }
}

void SaveHandler::writeHeader() {
    this->root->setAttrib(xoj::xml_attrs::CREATOR_STR, PROJECT_STRING);
    this->root->setAttrib(xoj::xml_attrs::FILEVERSION_STR, FILE_FORMAT_VERSION);
    this->root->addChild(new XmlTextNode(TAG_NAMES[TagType::TITLE],
                                         std::string{"Xournal++ document - see "} + PROJECT_HOMEPAGE_URL));
}

auto SaveHandler::getColorStr(Color c, unsigned char alpha) -> std::string {
    char str[10];
    sprintf(str, "#%08" PRIx32, uint32_t(c) << 8U | alpha);
    std::string color(str);
    return color;
}

void SaveHandler::writeAudio(XmlNode* node, const AudioContent& audio) {
    if (!audio.getAudioFilename().empty()) {
        /** set stroke timestamp value to the XmlPointNode */
        node->setAttrib(xoj::xml_attrs::TIMESTAMP_STR, audio.getTimestamp());
        auto audioFilename = audio.getAudioFilename().generic_u8string();
        node->setAttrib(xoj::xml_attrs::AUDIO_FILENAME_STR, audioFilename);
    }
}

void SaveHandler::visitStroke(XmlPointNode* stroke, const Stroke* s) {
    StrokeTool t = s->getToolType();

    unsigned char alpha = 0xff;

    if (t >= StrokeTool::NAMES.size()) {
        g_warning("Unknown StrokeTool::Value: %d", static_cast<unsigned int>(t));
        t = StrokeTool::PEN;
    }

    stroke->setAttrib(xoj::xml_attrs::TOOL_STR, StrokeTool::NAMES[t]);

    if (t == StrokeTool::PEN) {
        writeAudio(stroke, *s);
    } else if (t == StrokeTool::HIGHLIGHTER) {
        alpha = 0x7f;
    }

    stroke->setAttrib(xoj::xml_attrs::COLOR_STR, getColorStr(s->getColor(), alpha).c_str());

    const auto& pts = s->getPointVector();

    stroke->setPoints(pts);

    if (s->hasPressure()) {
        std::vector<double> values;
        values.reserve(pts.size() + 1);
        values.emplace_back(s->getWidth());
        std::transform(pts.begin(), pts.end() - 1, std::back_inserter(values), [](const Point& p) { return p.z; });
        stroke->setAttrib(xoj::xml_attrs::WIDTH_STR, std::move(values));
    } else {
        stroke->setAttrib(xoj::xml_attrs::WIDTH_STR, s->getWidth());
    }

    visitStrokeExtended(stroke, s);
}

/**
 * Export the fill attributes
 */
void SaveHandler::visitStrokeExtended(XmlPointNode* stroke, const Stroke* s) {
    if (s->getFill() != -1) {
        stroke->setAttrib(xoj::xml_attrs::FILL_STR, s->getFill());
    }

    StrokeCapStyle capStyle = s->getStrokeCapStyle();

    if (capStyle >= StrokeCapStyle::NAMES.size()) {
        g_warning("Unknown stroke cap type: %d", static_cast<int>(capStyle));
        capStyle = StrokeCapStyle::ROUND;
    }

    stroke->setAttrib(xoj::xml_attrs::CAPSTYLE_STR, StrokeCapStyle::NAMES[capStyle]);

    if (s->getLineStyle().hasDashes()) {
        stroke->setAttrib(xoj::xml_attrs::STYLE_STR, StrokeStyle::formatStyle(s->getLineStyle()));
    }
}

static inline std::vector<double> makeMatrixVector(const RectangularElement& e) {
    const auto& m = e.getTransformation();
    return std::vector<double>{m.xx, m.yx, m.xy, m.yy, m.shift.x, m.shift.y};
}

static inline bool nearEnough(double a, double b) { return std::abs(a - b) < 1e-8; };

static inline bool isMatrixHomogeneousScaling(const RectangularElement& e) {
    const auto& m = e.getTransformation();
    return nearEnough(m.yx, 0.) && nearEnough(m.xy, 0.) && nearEnough(m.xx, m.yy) && m.xx > 0.;
}

static inline bool isMatrixPositiveScaling(const RectangularElement& e) {
    const auto& m = e.getTransformation();
    return nearEnough(m.yx, 0.) && nearEnough(m.xy, 0.) && m.xx > 0.;
}

void SaveHandler::visitLayer(XmlNode* page, const Layer* l) {
    auto* layer = new XmlNode(TAG_NAMES[TagType::LAYER]);
    page->addChild(layer);
    if (this->exportAudience == ExportAudience::Teacher && l->hasName()) {
        layer->setAttrib(xoj::xml_attrs::NAME_STR, l->getName().c_str());
    }
    if (l->getAudience() != LayerAudience::Everyone) {
        layer->setAttrib(xoj::xml_attrs::UTN_AUDIENCE_STR, std::string(utn::audienceToString(l->getAudience())));
    }

    for (const auto& e: l->getElementsView()) {
        if (e->getType() == ELEMENT_STROKE) {
            auto* s = dynamic_cast<const Stroke*>(e);
            if (!s->getErasures().empty()) {
                // This runs under the shared Document lock, which blocks pen-up. Only snapshot the stroke here; the
                // raster fallback is drawn by resolveDeferredStrokes() after the lock has been released.
                auto* deferred = new XmlDeferredStrokeNode(TAG_NAMES[TagType::IMAGE], s->cloneStroke());
                layer->addChild(deferred);
                deferredStrokes.push_back(deferred);
                continue;
            }
            auto* stroke = new XmlPointNode(TAG_NAMES[TagType::STROKE]);
            layer->addChild(stroke);
            visitStroke(stroke, s);
        } else if (e->getType() == ELEMENT_TEXT) {
            const Text* t = dynamic_cast<const Text*>(e);
            if (t->getText().empty()) {
                g_warning("Trying to save an empty Text element. Discarding it!");
                continue;
            }
            auto* text = new XmlTextNode(TAG_NAMES[TagType::TEXT], t->getText());
            layer->addChild(text);

            const XojFont& f = t->getFont();

            text->setAttrib(xoj::xml_attrs::FONT_STR, f.getName().c_str());
            // Keep boxed and wrapped text in its local coordinate system. Folding the scale into the font alone
            // changes the box padding/border and the wrap width when the lesson is reopened.
            if (isMatrixHomogeneousScaling(*t) && !t->isBoxEnabled() && t->getWrap() == Text::NO_WRAP) {
                // Save as file version 4 for backward compatibility
                text->setAttrib(xoj::xml_attrs::SIZE_STR, t->getTransformation().xx * f.getSize());
                const auto& origin = t->getTransformation().shift;
                text->setAttrib(xoj::xml_attrs::X_COORD_STR, origin.x);
                text->setAttrib(xoj::xml_attrs::Y_COORD_STR, origin.y);
            } else {
                text->setAttrib(xoj::xml_attrs::SIZE_STR, f.getSize());
                text->setAttrib(xoj::xml_attrs::MATRIX_STR, makeMatrixVector(*t));
            }
            text->setAttrib(xoj::xml_attrs::COLOR_STR, getColorStr(t->getColor()).c_str());
            if (auto w = t->getWrap(); w != Text::NO_WRAP) {
                text->setAttrib(xoj::xml_attrs::WRAP_STR, w);
            }
            if (auto al = t->getAlign(); al != TextAlignment::LEFT) {
                text->setAttrib(xoj::xml_attrs::ALIGN_STR, TextAlignment::NAMES[al]);
            }
            if (t->getJustify()) {
                text->setAttrib(xoj::xml_attrs::JUSTIFY_STR, xoj::xml_values::TRUE_STR);
            }

            if (t->isUnderlined()) {
                text->setAttrib(xoj::xml_attrs::UTN_UNDERLINE_STR, xoj::xml_values::TRUE_STR);
            }
            if (t->isStrikethrough()) {
                text->setAttrib(xoj::xml_attrs::UTN_STRIKETHROUGH_STR, xoj::xml_values::TRUE_STR);
            }
            if (t->getLineSpacing() != 1.0) {
                text->setAttrib(xoj::xml_attrs::UTN_LINE_SPACING_STR, t->getLineSpacing());
            }

            if (t->isBoxEnabled()) {
                text->setAttrib(xoj::xml_attrs::UTN_BOX_STR, xoj::xml_values::TRUE_STR);

                Color background = t->getBoxBackgroundColor();
                text->setAttrib(xoj::xml_attrs::UTN_BOX_BACKGROUND_STR,
                                getColorStr(background, background.alpha).c_str());

                Color border = t->getBoxBorderColor();
                text->setAttrib(xoj::xml_attrs::UTN_BOX_BORDER_STR, getColorStr(border, border.alpha).c_str());
                text->setAttrib(xoj::xml_attrs::UTN_BOX_BORDER_WIDTH_STR, t->getBoxBorderWidth());
                text->setAttrib(xoj::xml_attrs::UTN_BOX_PADDING_STR, t->getBoxPadding());
                text->setAttrib(xoj::xml_attrs::UTN_BOX_RADIUS_STR, t->getBoxCornerRadius());
            }

            writeAudio(text, *t);
        } else if (e->getType() == ELEMENT_IMAGE) {
            auto* i = dynamic_cast<const Image*>(e);
            auto* image = new XmlImageNode(TAG_NAMES[TagType::IMAGE]);
            layer->addChild(image);

            image->setImage(i->getImage());
            if (isMatrixPositiveScaling(*i)) {
                // Save as file version 4 for backward compatibility
                Range r(i->getBoundingBox());
                image->setAttrib(xoj::xml_attrs::LEFT_POS_STR, r.minX);
                image->setAttrib(xoj::xml_attrs::TOP_POS_STR, r.minY);
                image->setAttrib(xoj::xml_attrs::RIGHT_POS_STR, r.maxX);
                image->setAttrib(xoj::xml_attrs::BOTTOM_POS_STR, r.maxY);
                // Add metadata to allow for lazy loading
                const auto& s = i->getNaturalSize();
                image->setAttrib(xoj::xml_attrs::NATURAL_SIZE_STR, std::vector<double>{s.width, s.height});
            } else {
                image->setAttrib(xoj::xml_attrs::MATRIX_STR, makeMatrixVector(*i));
            }
        } else if (e->getType() == ELEMENT_TEXIMAGE) {
            auto* i = dynamic_cast<const TexImage*>(e);
            auto* image = new XmlTexNode(TAG_NAMES[TagType::TEXIMAGE], std::string(i->getBinaryData()));
            layer->addChild(image);

            image->setAttrib(xoj::xml_attrs::TEXT_STR, i->getText().c_str());
            if (isMatrixPositiveScaling(*i)) {
                // Save as file version 4 for backward compatibility
                Range r(i->getBoundingBox());
                image->setAttrib(xoj::xml_attrs::LEFT_POS_STR, r.minX);
                image->setAttrib(xoj::xml_attrs::TOP_POS_STR, r.minY);
                image->setAttrib(xoj::xml_attrs::RIGHT_POS_STR, r.maxX);
                image->setAttrib(xoj::xml_attrs::BOTTOM_POS_STR, r.maxY);
                // Add metadata to allow for lazy loading
                const auto& s = i->getNaturalSize();
                image->setAttrib(xoj::xml_attrs::NATURAL_SIZE_STR, std::vector<double>{s.width, s.height});
            } else {
                image->setAttrib(xoj::xml_attrs::MATRIX_STR, makeMatrixVector(*i));
            }
        } else if (e->getType() == ELEMENT_LINK) {
            auto* l = dynamic_cast<const Link*>(e);
            auto* link = new XmlTextNode(TAG_NAMES[TagType::LINK], l->getText());
            layer->addChild(link);

            const XojFont& f = l->getFont();

            link->setAttrib(xoj::xml_attrs::ALIGN_STR, TextAlignment::NAMES[l->getAlignment()]);
            link->setAttrib(xoj::xml_attrs::FONT_STR, f.getName().c_str());
            link->setAttrib(xoj::xml_attrs::SIZE_STR, f.getSize());
            link->setAttrib(xoj::xml_attrs::MATRIX_STR, makeMatrixVector(*l));
            link->setAttrib(xoj::xml_attrs::COLOR_STR, getColorStr(l->getColor()).c_str());
            link->setAttrib(xoj::xml_attrs::URL_STR, l->getUrl().c_str());
        }
    }
}

void SaveHandler::visitPage(XmlNode* root, ConstPageRef p, const Document* doc, int id, const fs::path& target) {
    auto* page = new XmlNode(TAG_NAMES[TagType::PAGE]);
    root->addChild(page);
    page->setAttrib(xoj::xml_attrs::WIDTH_STR, p->getWidth());
    page->setAttrib(xoj::xml_attrs::HEIGHT_STR, p->getHeight());

    if (this->exportAudience == ExportAudience::Teacher && !p->getUtnPageLabel().empty()) {
        page->setAttrib(xoj::xml_attrs::UTN_PAGE_LABEL_STR, p->getUtnPageLabel().c_str());
    }

    auto* background = new XmlNode(TAG_NAMES[TagType::BACKGROUND]);
    page->addChild(background);

    writeBackgroundName(background, p);

    using xoj::xml_attrs::BackgroundType;
    using xoj::xml_attrs::Domain;

    if (p->getBackgroundType().isPdfPage()) {
        /**
         * ATTENTION! The original xournal can only read the XML if the attributes are in the right order!
         * DO NOT CHANGE THE ORDER OF THE ATTRIBUTES!
         */

        background->setAttrib(xoj::xml_attrs::TYPE_STR, BackgroundType::NAMES[BackgroundType::PDF]);
        if (!firstPdfPageVisited) {
            firstPdfPageVisited = true;

            if (doc->isAttachPdf()) {
                background->setAttrib(xoj::xml_attrs::DOMAIN_STR, Domain::NAMES[Domain::ATTACH]);
                auto filepath = doc->getFilepath();
                Util::clearExtensions(filepath);
                filepath += ".xopp.bg.pdf";
                background->setAttrib(xoj::xml_attrs::FILENAME_STR, "bg.pdf");

                GError* error = nullptr;
                if (!exists(filepath)) {
                    doc->getPdfDocument().save(filepath, &error);
                }

                if (error) {
                    if (!this->errorMessage.empty()) {
                        this->errorMessage += "\n";
                    }
                    this->errorMessage +=
                            FS(_F("Could not write background \"{1}\", {2}") % filepath.u8string() % error->message);

                    g_error_free(error);
                }
            } else {
                // "absolute" just means path. For backward compatibility, it is hard to change the word
                background->setAttrib(xoj::xml_attrs::DOMAIN_STR, Domain::NAMES[Domain::ABSOLUTE]);
                auto normalizedPath = Util::normalizeAssetPath(doc->getPdfFilepath(), target.parent_path(),
                                                               doc->getPathStorageMode());
                background->setAttrib(xoj::xml_attrs::FILENAME_STR, normalizedPath.c_str());
            }
        }
        background->setAttrib(xoj::xml_attrs::PAGE_NUMBER_STR, p->getPdfPageNr() + 1);
    } else if (p->getBackgroundType().isImagePage()) {
        background->setAttrib(xoj::xml_attrs::TYPE_STR, BackgroundType::NAMES[BackgroundType::PIXMAP]);

        int cloneId = p->getBackgroundImage().getCloneId();
        if (cloneId != -1) {
            background->setAttrib(xoj::xml_attrs::DOMAIN_STR, Domain::NAMES[Domain::CLONE]);
            char* filename = g_strdup_printf("%i", cloneId);
            background->setAttrib(xoj::xml_attrs::FILENAME_STR, filename);
            g_free(filename);
        } else if (p->getBackgroundImage().isAttached() && p->getBackgroundImage().getPixbuf()) {
            char* filename = g_strdup_printf("bg_%d.png", this->attachBgId++);
            background->setAttrib(xoj::xml_attrs::DOMAIN_STR, Domain::NAMES[Domain::ATTACH]);
            background->setAttrib(xoj::xml_attrs::FILENAME_STR, filename);

            // Store the changed info to update the document in updateDocumentInfo()
            backgroundImages.emplace_back(ImageInfo{p->getBackgroundImage(), fs::path(filename), id});
            g_free(filename);
        } else {
            // "absolute" just means path. For backward compatibility, it is hard to change the word
            background->setAttrib(xoj::xml_attrs::DOMAIN_STR, Domain::NAMES[Domain::ABSOLUTE]);
            auto normalizedPath = Util::normalizeAssetPath(p->getBackgroundImage().getFilepath(), target.parent_path(),
                                                           doc->getPathStorageMode());
            background->setAttrib(xoj::xml_attrs::FILENAME_STR, normalizedPath.c_str());

            // Store the changed info to update the document in updateDocumentInfo()
            backgroundImages.emplace_back(ImageInfo{p->getBackgroundImage(), std::nullopt, id});
        }
    } else {
        writeSolidBackground(background, p);
    }

    bool wroteLayer = false;
    for (const Layer* l: p->getLayersView()) {
        if (includeLayer(*l)) {
            visitLayer(page, l);
            wroteLayer = true;
        }
    }

    // no layer, but we need to write one layer, else the old Xournal cannot read the file
    if (!wroteLayer) {
        auto* layer = new XmlNode(TAG_NAMES[TagType::LAYER]);
        page->addChild(layer);
    }
}

bool SaveHandler::includeLayer(const Layer& layer) const {
    return this->exportAudience == ExportAudience::Teacher || utn::visibleToStudents(layer);
}

void SaveHandler::writeSolidBackground(XmlNode* background, ConstPageRef p) {
    using xoj::xml_attrs::BackgroundType;
    background->setAttrib(xoj::xml_attrs::TYPE_STR, BackgroundType::NAMES[BackgroundType::SOLID]);
    background->setAttrib(xoj::xml_attrs::COLOR_STR, getColorStr(p->getBackgroundColor()));
    background->setAttrib(xoj::xml_attrs::STYLE_STR,
                          PageTypeHandler::getStringForPageTypeFormat(p->getBackgroundType().format));

    // Not compatible with Xournal, so the background needs
    // to be changed to a basic one!
    if (!p->getBackgroundType().config.empty()) {
        background->setAttrib(xoj::xml_attrs::CONFIG_STR, p->getBackgroundType().config);
    }
}

void SaveHandler::writeBackgroundName(XmlNode* background, ConstPageRef p) {
    if (p->backgroundHasName()) {
        background->setAttrib(xoj::xml_attrs::NAME_STR, p->getBackgroundName());
    }
}

void SaveHandler::saveTo(const fs::path& filepath, ProgressListener* listener) {
    if (!errorMessage.empty()) return;
    resolveDeferredStrokes();  // before the output file is created
    GzOutputStream out(filepath);

    if (!out.getLastError().empty()) {
        this->errorMessage = out.getLastError();
        return;
    }

    saveTo(&out, filepath, listener);

    out.close();

    if (this->errorMessage.empty()) {
        this->errorMessage = out.getLastError();
    }
}

void SaveHandler::saveTo(OutputStream* out, const fs::path& filepath, ProgressListener* listener) {
    if (!errorMessage.empty()) return;
    resolveDeferredStrokes();
    out->write("<?xml version=\"1.0\" standalone=\"no\"?>\n");
    root->writeOut(out, listener);

    for (const auto& info: backgroundImages) {
        if (info.newPath) {
            auto tmpfn = (fs::path(filepath) += ".") += info.newPath.value();
            // Are we certain that does not modify the GdkPixbuf?
            if (!gdk_pixbuf_save(const_cast<GdkPixbuf*>(info.image.getPixbuf()), Util::toGFilename(tmpfn).c_str(),
                                 "png", nullptr, nullptr)) {
                if (!this->errorMessage.empty()) {
                    this->errorMessage += "\n";
                }

                this->errorMessage +=
                        FS(_F("Could not write background \"{1}\". Continuing anyway.") % tmpfn.u8string());
            }
        }
    }
}

void SaveHandler::updateDocumentInfo(Document* doc) {
    for (auto& info: backgroundImages) {
        if (info.newPath) {
            info.image.setFilepath(std::move(info.newPath.value()));
        }
        info.image.setCloneId(info.newId);
    }
    backgroundImages.clear();
}


auto SaveHandler::getErrorMessage() -> const std::string& { return this->errorMessage; }
