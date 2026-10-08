/*
 * Ultimate Teacher Notepad
 * Attached worksheet import, based on Xournal++ GPLv2+.
 */
#include "ScanPageImport.h"

#include <cmath>
#include <memory>
#include <utility>

#include "model/BackgroundImage.h"
#include "model/PageType.h"
#include "model/XojPage.h"
#include "util/StringUtils.h"
#include "util/i18n.h"

namespace xoj::utn {
ScanPageResult createScanPage(const fs::path& path, double pageWidth) {
    if (!std::isfinite(pageWidth) || pageWidth <= 0) {
        return std::string(_("Invalid worksheet page size"));
    }

    BackgroundImage image;
    GError* error = nullptr;
    image.loadFile(path, &error);
    if (error) {
        std::string message = error->message;
        g_error_free(error);
        return message;
    }
    if (!image.getPixbuf()) {
        return std::string(_("The scan could not be read as an image"));
    }

    image.applyEmbeddedOrientation();
    image.setAttach(true);
    const double width = gdk_pixbuf_get_width(image.getPixbuf());
    const double height = gdk_pixbuf_get_height(image.getPixbuf());
    auto page = std::make_shared<XojPage>(pageWidth, pageWidth * height / width);
    page->setBackgroundImage(std::move(image));
    page->setBackgroundType(PageType(PageTypeFormat::Image));
    page->setUtnPageLabel(std::string(char_cast(path.stem().u8string())));
    return page;
}
}  // namespace xoj::utn
