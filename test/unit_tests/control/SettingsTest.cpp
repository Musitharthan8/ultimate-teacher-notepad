/*
 * Xournal++
 *
 * This file is part of the Xournal UnitTests
 *
 * @author Xournal++ Team
 * https://github.com/xournalpp/xournalpp
 *
 * @license GNU GPLv2 or later
 */

#include <gtest/gtest.h>
#include <chrono>
#include <fstream>
#include <sstream>
#include <string>

#include "control/settings/Settings.h"

TEST(SettingsTest, testLoadDoesNotThrowForNonExistingFilePath) {
    Settings settings{"non-existing-file-path"};
    EXPECT_NO_THROW(settings.load());
}

// Rudimentary test for Settings save/load - very crude
TEST(SettingsTest, testReadWrite) {
    auto saveReloadTest = [&](const fs::path& dir) {
        std::cout << "Test saving in " << dir << std::endl;
        const fs::path outPath = dir / "xournalpp-test-units_Settings_testReadWrite.xml";
        if (fs::exists(outPath)) {
            std::cout << "Removing file (already exist): " << dir << std::endl;
            fs::remove(outPath);
        };

        Settings settings(outPath);
        settings.transactionStart();
        settings.setAudioDisabled(true);               // bool
        settings.setDefaultSaveName(u8"foo/bar€_%H");  // u8string
        settings.setPreferredLocale("es");             // string
        PageTemplateSettings tp;
        tp.parse("xoj/"
                 "template\ncopyLastPageSettings=false\ncopyLastPageSize=true\nsize=5.123x8.764\nbackgroundType="
                 "cµßtom\nbackgroundTypeConfig=m1=3,®ændomString=↓↓↓\nbackgroundColor=#abcdef\n");
        settings.setPageTemplateSettings(tp);                          // string
        settings.setDisplayDpi(123);                                   // int
        settings.setStabilizerDrag(3.1415);                            // double
        settings.setBackgroundColor(Color(123, 45, 67));               // Color
        settings.setColorPaletteSetting("foo/bar€_palette");           // path
        settings.setEraserVisibility(ERASER_VISIBILITY_HOVER);         // enum
        settings.setFont(XojFont{"myfontname italic 34"});             // Font
        settings.latexSettings.editorFont = XojFont{"myfonttest 52"};  // Font
        settings.setPreloadPagesAfter(145);                            // unsigned int
        settings.transactionEnd();                                     // calls save()

        Settings loaded(outPath);
        loaded.load();

        // For each type, we test one that has been changed and one that should be default
        EXPECT_EQ(settings.isAudioDisabled(), loaded.isAudioDisabled());                                    // bool
        EXPECT_EQ(settings.isAutoloadPdfXoj(), loaded.isAutoloadPdfXoj());                                  // bool
        EXPECT_EQ(settings.getDefaultSaveName(), loaded.getDefaultSaveName());                              // u8string
        EXPECT_EQ(settings.getDefaultPdfExportName(), loaded.getDefaultPdfExportName());                    // u8string
        EXPECT_EQ(settings.getPreferredLocale(), loaded.getPreferredLocale());                              // string
        EXPECT_EQ(settings.getPageTemplateSettings(), loaded.getPageTemplateSettings());                    // string
        EXPECT_EQ(settings.getDisplayDpi(), loaded.getDisplayDpi());                                        // int
        EXPECT_EQ(settings.getAddHorizontalSpaceAmountLeft(), loaded.getAddHorizontalSpaceAmountLeft());    // int
        EXPECT_EQ(settings.getStabilizerDrag(), loaded.getStabilizerDrag());                                // double
        EXPECT_EQ(settings.getCursorHighlightBorderWidth(), loaded.getCursorHighlightBorderWidth());        // double
        EXPECT_EQ(settings.getBackgroundColor(), loaded.getBackgroundColor());                              // Color
        EXPECT_EQ(settings.getActiveSelectionColor(), loaded.getActiveSelectionColor());                    // Color
        EXPECT_EQ(settings.getColorPaletteSetting(), loaded.getColorPaletteSetting());                      // path
        EXPECT_EQ(settings.getLastOpenPath(), loaded.getLastOpenPath());                                    // path
        EXPECT_EQ(settings.getEraserVisibility(), loaded.getEraserVisibility());                            // enum
        EXPECT_EQ(settings.getActiveViewMode(), loaded.getActiveViewMode());                                // enum
        EXPECT_EQ(settings.getFont().getName(), loaded.getFont().getName());                                // Font
        EXPECT_EQ(settings.getFont().getSize(), loaded.getFont().getSize());                                // Font
        EXPECT_EQ(settings.latexSettings.editorFont.getName(), loaded.latexSettings.editorFont.getName());  // Font
        EXPECT_EQ(settings.latexSettings.editorFont.getSize(), loaded.latexSettings.editorFont.getSize());  // Font
        EXPECT_EQ(settings.getPreloadPagesAfter(), loaded.getPreloadPagesAfter());    // unsigned int
        EXPECT_EQ(settings.getPreloadPagesBefore(), loaded.getPreloadPagesBefore());  // unsigned int

        fs::remove(outPath);
    };
    saveReloadTest(fs::temp_directory_path());
}

// A new profile starts on the light paper look; the system theme is no longer the default
TEST(SettingsTest, aNewProfileStartsOnTheLightTheme) {
    Settings settings{"non-existing-file-path"};
    settings.load();
    EXPECT_EQ(settings.getThemeVariant(), THEME_VARIANT_FORCE_LIGHT);
}

// A profile that saved another theme keeps it: only the default for a new profile changed
TEST(SettingsTest, aSavedThemeChoiceIsKept) {
    for (const ThemeVariant saved: {THEME_VARIANT_USE_SYSTEM, THEME_VARIANT_FORCE_DARK, THEME_VARIANT_HIGH_CONTRAST}) {
        const fs::path path = fs::temp_directory_path() / "xournalpp-test-units_Settings_savedTheme.xml";
        fs::remove(path);
        {
            Settings settings(path);
            settings.transactionStart();
            settings.setThemeVariant(saved);
            settings.transactionEnd();
        }
        Settings loaded(path);
        loaded.load();
        EXPECT_EQ(loaded.getThemeVariant(), saved);
        fs::remove(path);
    }
}

// Smart Shape has one fixed minimum size now. A profile saved before that still carries strokeRecognizerMinSize: it
// must load without complaint and the next save must no longer write it.
TEST(SettingsTest, anOldProfilesRemovedStrokeRecognizerMinSizeLoadsAndIsNotWrittenAgain) {
    // A name no concurrent test process can share
    const fs::path path = fs::temp_directory_path() / ("xournalpp-test-units_Settings_oldMinSize_" +
                                                       std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()) + ".xml");
    fs::remove(path);
    {
        std::ofstream out(path);
        ASSERT_TRUE(out.is_open());
        out << "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n<settings>\n"
               "  <property name=\"strokeRecognizerMinSize\" value=\"42\"/>\n"
               "  <property name=\"themeVariant\" value=\"forceDark\"/>\n</settings>\n";
    }
    {
        Settings loaded(path);
        EXPECT_TRUE(loaded.load());
        EXPECT_EQ(loaded.getThemeVariant(), THEME_VARIANT_FORCE_DARK) << "the rest of the profile is kept";
        loaded.transactionStart();
        loaded.setThemeVariant(THEME_VARIANT_HIGH_CONTRAST);  // any change makes it save
        loaded.transactionEnd();
    }
    std::ifstream in(path);
    std::stringstream content;
    content << in.rdbuf();
    in.close();
    EXPECT_EQ(content.str().find("strokeRecognizerMinSize"), std::string::npos);
    EXPECT_NE(content.str().find("highContrast"), std::string::npos) << "and the file really was saved again";
    fs::remove(path);
}
