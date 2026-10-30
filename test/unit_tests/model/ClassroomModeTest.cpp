/*
 * Ultimate Teacher Notepad
 *
 * What the Classroom controls say is on: the one place that names every screen effect that is active, so the control
 * surface, its status line and the "Back to normal" button agree.
 *
 * Based on Xournal++ GPLv2+
 */

#include <algorithm>

#include <gtest/gtest.h>

#include "model/ClassroomMode.h"

using namespace utn;

namespace {
bool has(const ClassroomSummary& s, ClassroomEffect e) {
    return std::find(s.effects.begin(), s.effects.end(), e) != s.effects.end();
}
}  // namespace

TEST(ClassroomMode, nothingOnIsNormalAndOffersNoWayBack) {
    const auto s = summariseClassroom({});
    EXPECT_TRUE(s.effects.empty());
    EXPECT_FALSE(s.anyActive());
    EXPECT_EQ(s.label, "Classroom is normal");
}

TEST(ClassroomMode, everyEffectIsNamedInWords) {
    EXPECT_EQ(summariseClassroom({.spotlight = true}).label, "On: Spotlight");
    EXPECT_EQ(summariseClassroom({.curtain = true}).label, "On: Curtain");
    EXPECT_EQ(summariseClassroom({.frozen = true}).label, "On: Freeze");
    EXPECT_EQ(summariseClassroom({.blanked = true}).label, "On: Blank");
}

TEST(ClassroomMode, severalEffectsAreAllListedInAFixedOrder) {
    const auto s = summariseClassroom({.spotlight = true, .frozen = true, .blanked = true});
    EXPECT_TRUE(s.anyActive());
    EXPECT_TRUE(has(s, ClassroomEffect::Spotlight));
    EXPECT_TRUE(has(s, ClassroomEffect::Freeze));
    EXPECT_TRUE(has(s, ClassroomEffect::Blank));
    EXPECT_FALSE(has(s, ClassroomEffect::Curtain));
    EXPECT_EQ(s.label, "On: Spotlight, Freeze, Blank");
}

TEST(ClassroomMode, spotlightAndCurtainNeverShowTogether) {
    // They are one overlay in the tool handler; if both flags ever arrive the Curtain, the stronger cover, is reported
    const auto s = summariseClassroom({.spotlight = true, .curtain = true});
    EXPECT_FALSE(has(s, ClassroomEffect::Spotlight));
    EXPECT_TRUE(has(s, ClassroomEffect::Curtain));
}

TEST(ClassroomMode, blankIsWhatStudentsSeeWhateverElseIsOn) {
    EXPECT_FALSE(studentsSeeThePage(summariseClassroom({.frozen = true, .blanked = true})));
    EXPECT_FALSE(studentsSeeThePage(summariseClassroom({.curtain = true, .blanked = true})));
    EXPECT_TRUE(studentsSeeThePage(summariseClassroom({})));
    EXPECT_TRUE(studentsSeeThePage(summariseClassroom({.spotlight = true})));
}
