/*
 * Ultimate Teacher Notepad
 *
 * Eraser mode persistence: every eraser mode must survive saveSettings(), a settings-file write and a fresh
 * loadSettings(). Object Eraser was once silently reset to the default eraser after a restart.
 *
 * Based on Xournal++ GPLv2+
 */

#include <string>

#include <glib.h>
#include <gtest/gtest.h>

#include "control/ToolEnums.h"
#include "control/ToolHandler.h"
#include "control/settings/Settings.h"
#include "filesystem.h"
#include "model/TeacherPalette.h"

namespace {
/// Writes the eraser mode through ToolHandler and the settings file, then reads it back into a fresh ToolHandler
EraserType roundTripEraserType(EraserType written) {
    const fs::path path = fs::temp_directory_path() /
                          ("utn-tool-settings-" + std::to_string(static_cast<int>(written)) + "-" +
                           std::to_string(g_get_monotonic_time()) + ".xml");
    {
        Settings settings{path};
        ToolHandler tools(nullptr, nullptr, &settings);
        tools.setEraserType(written);
        tools.saveSettings();
        settings.customSettingsChanged();
    }
    EraserType loaded = ERASER_TYPE_NONE;
    {
        Settings settings{path};
        settings.load();
        ToolHandler tools(nullptr, nullptr, &settings);
        tools.loadSettings();
        loaded = tools.getEraserType();
    }
    std::error_code ec;
    fs::remove(path, ec);
    return loaded;
}
}  // namespace

TEST(ToolHandlerSettings, deleteObjectEraserSurvivesRestart) {
    EXPECT_EQ(roundTripEraserType(ERASER_TYPE_DELETE_OBJECT), ERASER_TYPE_DELETE_OBJECT);
}

TEST(ToolHandlerSettings, everyEraserModeRoundTrips) {
    EXPECT_EQ(roundTripEraserType(ERASER_TYPE_DEFAULT), ERASER_TYPE_DEFAULT);
    EXPECT_EQ(roundTripEraserType(ERASER_TYPE_DELETE_STROKE), ERASER_TYPE_DELETE_STROKE);
    EXPECT_EQ(roundTripEraserType(ERASER_TYPE_WHITEOUT), ERASER_TYPE_WHITEOUT);
    EXPECT_EQ(roundTripEraserType(ERASER_TYPE_DELETE_OBJECT), ERASER_TYPE_DELETE_OBJECT);
}

// A new pen writes in the palette's Blue, not an indigo that no swatch stands for
TEST(ToolHandlerSettings, theDefaultPenColourIsTheTeacherPaletteBlue) {
    Settings settings{"non-existing-file-path"};
    ToolHandler tools(nullptr, nullptr, &settings);
    const auto blue = utn::palette::swatch(utn::palette::Kind::Ink, 1);
    EXPECT_STREQ(blue.name, "Blue");
    EXPECT_EQ(static_cast<uint32_t>(tools.getTool(TOOL_PEN).getColor()), 0xFF000000U | blue.rgb);
    EXPECT_TRUE(utn::palette::find(utn::palette::Kind::Ink, static_cast<uint32_t>(tools.getTool(TOOL_PEN).getColor())))
            << "the default pen colour is one of the swatches";
}
