/*
 * Ultimate Teacher Notepad
 *
 * Regression tests for list continuation in the UTN text editor: Enter after a list item.
 *
 * Based on Xournal++ GPLv2+
 */

#include <string>

#include <gtest/gtest.h>

#include "control/tools/ListContinuation.h"

using utn::list::listContinuation;

TEST(ListContinuationTest, BulletContinuesWithBullet) {
    auto c = listContinuation("• buy pens");
    EXPECT_EQ(c.prefix, "• ");
    EXPECT_FALSE(c.emptyItem);
}

TEST(ListContinuationTest, EmptyBulletEndsList) {
    auto c = listContinuation("• ");
    EXPECT_EQ(c.prefix, "• ");
    EXPECT_TRUE(c.emptyItem);
}

TEST(ListContinuationTest, NumberedItemCountsUp) {
    auto c = listContinuation("1. first");
    EXPECT_EQ(c.prefix, "2. ");
    EXPECT_FALSE(c.emptyItem);

    c = listContinuation("9. ninth");
    EXPECT_EQ(c.prefix, "10. ");
}

TEST(ListContinuationTest, EmptyNumberedItemEndsList) {
    auto c = listContinuation("3. ");
    EXPECT_EQ(c.prefix, "4. ");
    EXPECT_TRUE(c.emptyItem);
}

TEST(ListContinuationTest, PlainTextIsNotAList) {
    EXPECT_TRUE(listContinuation("hello").prefix.empty());
    EXPECT_TRUE(listContinuation("").prefix.empty());
    EXPECT_FALSE(listContinuation("").emptyItem);
}

TEST(ListContinuationTest, NumberWithoutSpaceIsNotAList) {
    EXPECT_TRUE(listContinuation("3.").prefix.empty());
    EXPECT_TRUE(listContinuation("3.x").prefix.empty());
    EXPECT_TRUE(listContinuation("3 apples").prefix.empty());
    EXPECT_TRUE(listContinuation(". item").prefix.empty());
}

TEST(ListContinuationTest, ZeroIsNotContinued) {
    EXPECT_TRUE(listContinuation("0. zero").prefix.empty());
}

TEST(ListContinuationTest, VeryLongNumbersAreTextNotLists) {
    // Would overflow the counter if parsed; treated as ordinary text instead
    EXPECT_TRUE(listContinuation("99999999999999999999. big").prefix.empty());
}

TEST(ListContinuationTest, MultiByteBulletTextIsRecognised) {
    auto c = listContinuation(std::string("• ") + "café");
    EXPECT_EQ(c.prefix, "• ");
    EXPECT_FALSE(c.emptyItem);
}
