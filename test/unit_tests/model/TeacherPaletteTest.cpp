/*
 * Ultimate Teacher Notepad
 *
 * Teacher Annotation Palette: which named colours each tool offers, which swatch stands for a tool colour, and the
 * words used for a Custom colour.
 *
 * Based on Xournal++ GPLv2+
 */

#include <set>
#include <string>

#include <gtest/gtest.h>

#include "model/PenPresets.h"
#include "model/TeacherPalette.h"

using namespace utn::palette;

TEST(TeacherPalette, PenShapesAndTextShareTheSixInkColoursInClassroomOrder) {
    ASSERT_EQ(count(Kind::Ink), 6U);
    const char* expected[] = {"Black", "Blue", "Red", "Green", "Orange", "Purple"};
    for (std::size_t i = 0; i < 6; ++i) {
        EXPECT_STREQ(swatch(Kind::Ink, i).name, expected[i]);
    }
    EXPECT_EQ(swatch(Kind::Ink, 1).rgb, 0x1E5BD8U) << "the Blue every surface already used";
    EXPECT_EQ(kindFor(false), Kind::Ink);
}

TEST(TeacherPalette, TheHighlighterHasFourSeeThroughClassroomColours) {
    ASSERT_EQ(count(Kind::Highlighter), 4U);
    const char* expected[] = {"Yellow", "Pink", "Cyan", "Green"};
    for (std::size_t i = 0; i < 4; ++i) {
        EXPECT_STREQ(swatch(Kind::Highlighter, i).name, expected[i]);
    }
    EXPECT_EQ(kindFor(true), Kind::Highlighter);
}

TEST(TeacherPalette, EverySwatchHasADistinctNameAndHueWithinItsPalette) {
    for (Kind kind: {Kind::Ink, Kind::Highlighter}) {
        std::set<std::string> names;
        std::set<uint32_t> hues;
        for (std::size_t i = 0; i < count(kind); ++i) {
            const Swatch& entry = swatch(kind, i);
            EXPECT_NE(std::string(entry.name), "");
            EXPECT_EQ(entry.rgb & 0xFF000000U, 0U) << "rgb only, no alpha";
            names.insert(entry.name);
            hues.insert(entry.rgb);
        }
        EXPECT_EQ(names.size(), count(kind));
        EXPECT_EQ(hues.size(), count(kind));
    }
}

TEST(TeacherPalette, FindMatchesTheHueOnlySoOpacityNeverHidesTheSwatch) {
    EXPECT_EQ(find(Kind::Ink, 0xFF1E5BD8U), std::optional<std::size_t>(1));
    EXPECT_EQ(find(Kind::Ink, 0x001E5BD8U), std::optional<std::size_t>(1));
    EXPECT_EQ(find(Kind::Ink, 0xFF000000U), std::optional<std::size_t>(0)) << "opaque black";
    EXPECT_EQ(find(Kind::Highlighter, 0x80FFEB3BU), std::optional<std::size_t>(0)) << "half see-through yellow";
    EXPECT_EQ(find(Kind::Highlighter, 0x8000D5E8U), std::optional<std::size_t>(2));
}

TEST(TeacherPalette, AnythingElseIsCustomAndPalettesDoNotLeakIntoEachOther) {
    EXPECT_FALSE(find(Kind::Ink, 0xFFD93025U).has_value()) << "a near-red the palette does not offer is Custom";
    EXPECT_FALSE(find(Kind::Ink, 0xFF1E5BD9U).has_value()) << "one step off is a different colour";
    EXPECT_FALSE(find(Kind::Ink, 0xFFFFEB3BU).has_value()) << "highlighter yellow is not an ink swatch";
    EXPECT_FALSE(find(Kind::Highlighter, 0xFF2E7D32U).has_value()) << "ink green is not highlighter green";
    EXPECT_STREQ(nameOf(Kind::Ink, 0xFFD32F2FU), "Red");
    EXPECT_STREQ(nameOf(Kind::Highlighter, 0x40FF7EB6U), "Pink");
    EXPECT_STREQ(nameOf(Kind::Ink, 0xFF123456U), "");
}

TEST(TeacherPalette, CustomColoursAreNamedByAStrictUpperCaseHexCode) {
    EXPECT_EQ(hex(0xFF1A5FD0U), "#1A5FD0");
    EXPECT_EQ(hex(0x00000000U), "#000000");
    EXPECT_EQ(hex(0x80ABCDEFU), "#ABCDEF") << "the alpha is not part of the name";
    EXPECT_EQ(hex(0xFFFFFFFFU).size(), 7U);
}

TEST(TeacherPalette, ANewHueKeepsTheToolsOpacity) {
    EXPECT_EQ(withOpacityOf(0xFF00D5E8U, 0x80FFEB3BU), 0x8000D5E8U) << "highlighter stays half see-through";
    EXPECT_EQ(withOpacityOf(0x001E5BD8U, 0xFF000000U), 0xFF1E5BD8U) << "a pen stays opaque";
}

TEST(TeacherPalette, TheBuiltInPenPresetsUseSwatchesSoThePaletteShowsThemSelected) {
    const utn::PenPresetStore store;
    const utn::PenPreset* red = store.find(utn::PenPresetStore::RED_MARKING_ID);
    const utn::PenPreset* blue = store.find(utn::PenPresetStore::BLUE_EXPLANATION_ID);
    const utn::PenPreset* yellow = store.find(utn::PenPresetStore::YELLOW_HIGHLIGHTER_ID);
    ASSERT_TRUE(red && blue && yellow);
    EXPECT_STREQ(nameOf(Kind::Ink, red->rgb), "Red");
    EXPECT_STREQ(nameOf(Kind::Ink, blue->rgb), "Blue");
    EXPECT_STREQ(nameOf(Kind::Highlighter, yellow->rgb), "Yellow");
}
