#include "IconNameHelper.h"

#include <array>
#include <utility>

#include <gtk/gtk.h>  // for gtk_icon_theme_get_default

#include "control/settings/Settings.h"  // for Settings

IconNameHelper::IconNameHelper(Settings* settings): settings(settings) {}

auto IconNameHelper::iconName(const char* icon) const -> std::string {
    std::string xoppName = std::string("xopp-") + icon;
    auto iconName = this->settings->areStockIconsUsed() && gtk_icon_theme_has_icon(gtk_icon_theme_get_default(), icon) ?
                            std::string(icon) :
                            xoppName;
    // Older resource directories may not contain the UTN icons yet.
    if (!gtk_icon_theme_has_icon(gtk_icon_theme_get_default(), iconName.c_str())) {
        const std::array<std::pair<const char*, const char*>, 7> fallbacks{{
                {"utn-appearance", "preferences-system"}, {"utn-answer-box", "tool-text"},
                {"utn-feedback", "tool-text"}, {"utn-reveal", "sidebar-layerstack"},
                {"utn-presentation", "presentation-mode"}, {"utn-teaching-tools", "draw-rectangle"},
                {"utn-workflow", "document-open"}}};
        for (const auto& [utn, fallback]: fallbacks) {
            if (std::string(icon) == utn) {
                const auto bundled = std::string("xopp-") + fallback;
                return gtk_icon_theme_has_icon(gtk_icon_theme_get_default(), bundled.c_str()) ? bundled : fallback;
            }
        }
    }
    return iconName;
}
