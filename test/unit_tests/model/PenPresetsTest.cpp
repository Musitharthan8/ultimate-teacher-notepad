/*
 * Ultimate Teacher Notepad
 *
 * Favourite pen presets: the three built-in presets, custom presets, and a saved form that survives a restart and
 * never trusts what it reads.
 *
 * Based on Xournal++ GPLv2+
 */

#include <algorithm>
#include <string>

#include <gtest/gtest.h>

#include "model/PenPresets.h"

using namespace utn;

namespace {
PenPreset custom(const std::string& name, PresetTool tool = PresetTool::Pen) {
    PenPreset preset;
    preset.name = name;
    preset.tool = tool;
    preset.rgb = 0x1B7F5C;
    preset.size = 3;
    return preset;
}
}  // namespace

TEST(PenPresets, StartsWithTheThreeBuiltInPresets) {
    PenPresetStore store;
    ASSERT_EQ(store.all().size(), 3u);
    const PenPreset* red = store.find(PenPresetStore::RED_MARKING_ID);
    const PenPreset* blue = store.find(PenPresetStore::BLUE_EXPLANATION_ID);
    const PenPreset* yellow = store.find(PenPresetStore::YELLOW_HIGHLIGHTER_ID);
    ASSERT_NE(red, nullptr);
    ASSERT_NE(blue, nullptr);
    ASSERT_NE(yellow, nullptr);
    EXPECT_EQ(red->tool, PresetTool::Pen);
    EXPECT_EQ(blue->tool, PresetTool::Pen);
    EXPECT_EQ(yellow->tool, PresetTool::Highlighter);
    EXPECT_NE(red->rgb, blue->rgb);
    EXPECT_FALSE(red->name.empty());
}

TEST(PenPresets, ACustomPresetGetsItsOwnIdAndAUniqueName) {
    PenPresetStore store;
    const std::string first = store.add(custom("Green ticks"));
    const std::string second = store.add(custom("Green ticks"));
    EXPECT_FALSE(first.empty());
    EXPECT_NE(first, second);
    ASSERT_NE(store.find(first), nullptr);
    ASSERT_NE(store.find(second), nullptr);
    EXPECT_NE(store.find(first)->name, store.find(second)->name) << "two presets never look the same in a list";
    EXPECT_EQ(store.all().size(), 5u);
}

TEST(PenPresets, ANameIsTrimmedAndAnEmptyOneIsRefused) {
    PenPresetStore store;
    EXPECT_TRUE(store.add(custom("   ")).empty());
    const std::string id = store.add(custom("  Spelling  "));
    ASSERT_FALSE(id.empty());
    EXPECT_EQ(store.find(id)->name, "Spelling");
    EXPECT_FALSE(store.rename(id, "   "));
    EXPECT_EQ(store.find(id)->name, "Spelling") << "a refused rename changes nothing";
    EXPECT_TRUE(store.rename(id, "Spelling pen"));
    EXPECT_EQ(store.find(id)->name, "Spelling pen");
}

TEST(PenPresets, ValuesAreKeptInRange) {
    PenPresetStore store;
    PenPreset odd = custom("Odd");
    odd.size = 99;
    odd.opacity = 9999;
    odd.rgb = 0xFF1B7F5Cu;
    const std::string id = store.add(odd);
    ASSERT_FALSE(id.empty());
    EXPECT_LE(store.find(id)->size, 4);
    EXPECT_GE(store.find(id)->size, 0);
    EXPECT_LE(store.find(id)->opacity, 255);
    EXPECT_EQ(store.find(id)->rgb & 0xFF000000u, 0u) << "a colour is three bytes";
}

TEST(PenPresets, ThereIsALimitSoTheListStaysUsable) {
    PenPresetStore store;
    while (store.all().size() < PenPresetStore::MAX_PRESETS) {
        ASSERT_FALSE(store.add(custom("p" + std::to_string(store.all().size()))).empty());
    }
    EXPECT_TRUE(store.add(custom("one too many")).empty());
    EXPECT_EQ(store.all().size(), PenPresetStore::MAX_PRESETS);
}

TEST(PenPresets, UpdateKeepsTheIdAndTheOrder) {
    PenPresetStore store;
    const std::string id = store.add(custom("Mine"));
    const size_t position = std::find_if(store.all().begin(), store.all().end(), [&](const PenPreset& p) {
                                return p.id == id;
                            }) - store.all().begin();
    PenPreset changed = custom("Mine, thicker");
    changed.size = 4;
    EXPECT_TRUE(store.update(id, changed));
    EXPECT_EQ(store.find(id)->size, 4);
    EXPECT_EQ(store.all()[position].id, id);
    EXPECT_FALSE(store.update("no-such-id", changed));
}

TEST(PenPresets, ARemovedPresetIsGoneAndAnUnknownOneIsNotAnError) {
    PenPresetStore store;
    const std::string id = store.add(custom("Temporary"));
    EXPECT_TRUE(store.remove(id));
    EXPECT_EQ(store.find(id), nullptr);
    EXPECT_FALSE(store.remove(id));
}

TEST(PenPresets, RestoringDefaultsRepairsTheBuiltInsAndKeepsTheTeachersOwn) {
    PenPresetStore store;
    const std::string mine = store.add(custom("Mine"));
    store.remove(PenPresetStore::RED_MARKING_ID);
    PenPreset edited = *store.find(PenPresetStore::BLUE_EXPLANATION_ID);
    edited.rgb = 0x00FF00;
    store.update(PenPresetStore::BLUE_EXPLANATION_ID, edited);

    store.restoreDefaults();
    ASSERT_NE(store.find(PenPresetStore::RED_MARKING_ID), nullptr) << "a deleted built-in comes back";
    EXPECT_EQ(store.find(PenPresetStore::BLUE_EXPLANATION_ID)->rgb,
              PenPresetStore().find(PenPresetStore::BLUE_EXPLANATION_ID)->rgb)
            << "an edited built-in is reset";
    ASSERT_NE(store.find(mine), nullptr) << "the teacher's own preset is not thrown away";
    EXPECT_EQ(store.all().size(), 4u);
}

TEST(PenPresets, ASavedStoreComesBackExactly) {
    PenPresetStore store;
    PenPreset tricky = custom("Pipes | semi; percent % and \xC3\xBCn\xC3\xAF""code");
    tricky.opacity = 77;
    const std::string id = store.add(tricky);
    store.remove(PenPresetStore::BLUE_EXPLANATION_ID);

    const PenPresetStore back = PenPresetStore::parse(store.serialise());
    ASSERT_EQ(back.all().size(), store.all().size());
    for (size_t i = 0; i < store.all().size(); ++i) {
        const PenPreset& a = store.all()[i];
        const PenPreset& b = back.all()[i];
        EXPECT_EQ(a.id, b.id);
        EXPECT_EQ(a.name, b.name);
        EXPECT_EQ(a.tool, b.tool);
        EXPECT_EQ(a.rgb, b.rgb);
        EXPECT_EQ(a.size, b.size);
        EXPECT_EQ(a.opacity, b.opacity);
    }
    EXPECT_EQ(back.find(PenPresetStore::BLUE_EXPLANATION_ID), nullptr)
            << "a deleted built-in stays deleted until restored";
    EXPECT_EQ(back.find(id)->name, tricky.name);
}

TEST(PenPresets, NothingSavedMeansTheDefaults) { EXPECT_EQ(PenPresetStore::parse("").all().size(), 3u); }

TEST(PenPresets, UnreadableOrUnknownSavedFormsFallBackToTheDefaults) {
    for (const std::string bad: {std::string("garbage"), std::string("v9;a|b|pen|ffffff|1|255"), std::string("v1"),
                                 std::string("v1;;;"), std::string("v1;only|three|fields"),
                                 std::string(10000, 'x')}) {
        const PenPresetStore store = PenPresetStore::parse(bad);
        EXPECT_EQ(store.all().size(), 3u) << "from: " << bad.substr(0, 40);
        EXPECT_NE(store.find(PenPresetStore::RED_MARKING_ID), nullptr);
    }
}

TEST(PenPresets, BadRecordsAreDroppedAndGoodOnesKept) {
    const std::string saved = "v1;ok-one|Good|pen|ff0000|2|255;bad-tool|Bad|crayon|ff0000|2|255;"
                              "bad-colour|Bad|pen|zzzzzz|2|255;ok-two|Also good|highlighter|ffee00|9|999;"
                              "ok-one|Duplicate id|pen|00ff00|1|255";
    const PenPresetStore store = PenPresetStore::parse(saved);
    ASSERT_EQ(store.all().size(), 2u);
    EXPECT_EQ(store.find("ok-one")->name, "Good") << "the first record with an id wins";
    ASSERT_NE(store.find("ok-two"), nullptr);
    EXPECT_EQ(store.find("ok-two")->size, 4) << "out of range is clamped, not trusted";
    EXPECT_EQ(store.find("ok-two")->opacity, 255);
    EXPECT_EQ(store.find("bad-tool"), nullptr);
    EXPECT_EQ(store.find("bad-colour"), nullptr);
}

TEST(PenPresets, APresetIsSelectedOnlyWhenTheToolReallyMatchesIt) {
    PenPreset preset = custom("Selected?");
    preset.opacity = 100;
    EXPECT_TRUE(matchesTool(preset, PresetTool::Pen, 0x1B7F5C, 3, 100));
    EXPECT_FALSE(matchesTool(preset, PresetTool::Highlighter, 0x1B7F5C, 3, 100)) << "another tool";
    EXPECT_FALSE(matchesTool(preset, PresetTool::Pen, 0x1B7F5D, 3, 100)) << "another colour";
    EXPECT_FALSE(matchesTool(preset, PresetTool::Pen, 0x1B7F5C, 2, 100)) << "another size";
    EXPECT_FALSE(matchesTool(preset, PresetTool::Pen, 0x1B7F5C, 3, 99)) << "another opacity";
    EXPECT_TRUE(matchesTool(preset, PresetTool::Pen, 0xFF1B7F5C, 3, 100)) << "the alpha byte of a colour is not the colour";
}
