#include "Image.h"

#include <algorithm>  // for min
#include <array>      // for array
#include <memory>
#include <utility>  // for move, pair

#include <cairo.h>    // for cairo_surface_destroy
#include <gdk/gdk.h>  // for gdk_cairo_set_sourc...
#include <glib.h>     // for guchar

#include "model/Element.h"   // for Element, ELEMENT_IMAGE
#include "util/Assert.h"     // for xoj_assert
#include "util/BoundedPixbuf.h"  // for limitGdkPixbufLoaderPixels
#include "util/Rectangle.h"  // for Rectangle
#include "util/i18n.h"
#include "util/matrix/RectangleMultiply.h"
#include "util/raii/GObjectSPtr.h"  // for GObjectSPtr
#include "util/safe_casts.h"
#include "util/serializing/ObjectInputStream.h"   // for ObjectInputStream
#include "util/serializing/ObjectOutputStream.h"  // for ObjectOutputStream

using xoj::util::Rectangle;

Image::Image(): RectangularElement(ELEMENT_IMAGE) {}

Image::~Image() {
    if (this->format) {
        gdk_pixbuf_format_free(this->format);
        this->format = nullptr;
    }
}

auto Image::clone() const -> ElementPtr {
    auto img = std::make_unique<Image>();
    static_cast<RectangularElement&>(*img) = *this;

    img->data = this->data;
    img->image = this->image;

    return img;
}

void Image::setImage(std::string_view data) { setImage(std::string(data)); }

void Image::setImage(std::string&& data) { setImage(std::make_shared<std::string>(std::move(data))); }

void Image::setImage(std::shared_ptr<std::string> data) {
    if (!data) {
        data = std::make_shared<std::string>();
    }
    this->image.reset();
    this->data = std::move(data);

    if (this->format) {
        gdk_pixbuf_format_free(this->format);
        this->format = nullptr;
    }

    // FIXME: awful hack to try to parse the format
    std::array<char*, 4096> buffer{};
    xoj::util::GObjectSPtr<GdkPixbufLoader> loader(gdk_pixbuf_loader_new(), xoj::util::adopt);
    xoj::util::limitGdkPixbufLoaderPixels(loader.get());
    size_t remaining = this->data->size();
    while (remaining > 0) {
        size_t readLen = std::min(remaining, buffer.size());
        if (!gdk_pixbuf_loader_write(loader.get(), reinterpret_cast<const guchar*>(this->data->c_str()), readLen,
                                     nullptr))
            break;
        remaining -= readLen;

        // Try to determine the format early, if possible
        this->format = gdk_pixbuf_loader_get_format(loader.get());
        if (this->format) {
            break;
        }
    }
    gdk_pixbuf_loader_close(loader.get(), nullptr);
    // if the format was not determined early, it can probably be determined now
    if (!this->format) {
        this->format = gdk_pixbuf_loader_get_format(loader.get());
    }
    xoj_assert_message(this->format != nullptr, "could not parse the image format!");

    // the format is owned by the pixbuf, so create a copy
    this->format = gdk_pixbuf_format_copy(this->format);
}

static xoj::util::CairoSurfaceSPtr bufferFromPixbuf(GdkPixbuf* img) {
    xoj::util::CairoSurfaceSPtr res(
            cairo_image_surface_create(CAIRO_FORMAT_ARGB32, gdk_pixbuf_get_width(img), gdk_pixbuf_get_height(img)),
            xoj::util::adopt);
    xoj_assert(res);

    // Paint the pixbuf on to the surface
    // NOTE: we do this manually instead of using gdk_cairo_surface_create_from_pixbuf
    // since this does not work in CLI mode.
    cairo_t* cr = cairo_create(res.get());
    gdk_cairo_set_source_pixbuf(cr, img, 0, 0);
    cairo_paint(cr);
    cairo_destroy(cr);
    return res;
}

void Image::setImage(GdkPixbuf* img) {
    this->naturalSize = xoj::util::Size<double>(gdk_pixbuf_get_width(img), gdk_pixbuf_get_height(img));
    this->image = bufferFromPixbuf(img);

    auto pngData = std::make_shared<std::string>();
    const cairo_write_func_t writeFunc = [](void* bufferPtr, const unsigned char* data,
                                            unsigned int length) -> cairo_status_t {
        reinterpret_cast<std::string*>(bufferPtr)->append(reinterpret_cast<const char*>(data), length);
        return CAIRO_STATUS_SUCCESS;
    };
    cairo_surface_write_to_png_stream(image.get(), writeFunc, pngData.get());
    this->data = std::move(pngData);
}

auto Image::renderBuffer() const -> std::optional<std::string> {
    xoj_assert_message(!data->empty(), "image has no data, cannot render it!");
    if (this->image) {
        // Already rendered
        return std::nullopt;
    }
    xoj::util::GObjectSPtr<GdkPixbufLoader> loader(gdk_pixbuf_loader_new(), xoj::util::adopt);
    xoj::util::limitGdkPixbufLoaderPixels(loader.get());
    GError* err = nullptr;
    bool success = gdk_pixbuf_loader_write(loader.get(), reinterpret_cast<const guchar*>(this->data->c_str()),
                                           this->data->length(), &err);
    if (!success) {
        if (err != nullptr) {
            std::string msg = std::string(_("Failed to load image")) + "\n" + _("Error: ") + err->message;
            g_free(err);
            return msg;
        } else {
            return std::string(_("Failed to load image")) + "\n" + _("Unrecoverable error");
        }
    }
    success = gdk_pixbuf_loader_close(loader.get(), &err);
    if (!success) {
        if (err != nullptr) {
            std::string msg = std::string(_("Failed to close image stream")) + "\n" + _("Error: ") + err->message;
            g_free(err);
            return msg;
        } else {
            return std::string(_("Failed to close image stream")) + "\n" + _("Unrecoverable error");
        }
    }

    GdkPixbuf* tmp = gdk_pixbuf_loader_get_pixbuf(loader.get());  // Owned by the GdkPixbufLoader
    xoj_assert(tmp != nullptr);
    xoj::util::GObjectSPtr<GdkPixbuf> pixbuf(gdk_pixbuf_apply_embedded_orientation(tmp), xoj::util::adopt);

    this->naturalSize =
            xoj::util::Size<double>(gdk_pixbuf_get_width(pixbuf.get()), gdk_pixbuf_get_height(pixbuf.get()));

    this->image = bufferFromPixbuf(pixbuf.get());
    return std::nullopt;
}

auto Image::getImage() const -> cairo_surface_t* {
    if (auto opt = renderBuffer(); opt.has_value()) {
        // An error occurred
        g_warning("%s", opt->c_str());
    }
    return this->image.get();
}

void Image::serialize(ObjectOutputStream& out) const {
    out.writeObject("Image");

    this->RectangularElement::serialize(out);

    out.writeImage(*this->data);

    out.endObject();
}

void Image::readSerialized(ObjectInputStream& in) {
    in.readObject("Image");

    this->RectangularElement::readSerialized(in);

    this->image.reset();
    this->data = std::make_shared<std::string>(in.readImage());

    in.endObject();
    this->calcSize();
}

void Image::calcSize() const {
    if (auto opt = renderBuffer(); opt.has_value()) {
        // An error occurred
        g_warning("%s", opt->c_str());
        return;
    }
    this->snappedBounds = transformationMatrix * xoj::util::Rectangle<double>({0, 0}, naturalSize);
    this->boundingBox = this->snappedBounds;
    this->sizeCalculated = true;
}

bool Image::hasData() const { return !this->data->empty(); }

const unsigned char* Image::getRawData() const {
    return reinterpret_cast<const unsigned char*>(this->data->data());
}

size_t Image::getRawDataLength() const { return this->data->size(); }

GdkPixbufFormat* Image::getImageFormat() const { return this->format; }
