/*
 * Ultimate Teacher Notepad
 *
 * Regression tests for where a new layer is inserted. A page with the background selected used to insert the
 * answers layer underneath every existing layer.
 *
 * Based on Xournal++ GPLv2+
 */

#include <gtest/gtest.h>

#include "control/layer/LayerInsertion.h"

using xoj::layer::insertIndexForNewLayer;

TEST(LayerInsertionTest, AboveSelectedLayerKeepsUpstreamPosition) {
    // Layer 2 of 3 is selected: the new layer goes at vector index 2, directly above it
    EXPECT_EQ(insertIndexForNewLayer(2, 3, false), 2u);
    EXPECT_EQ(insertIndexForNewLayer(1, 3, false), 1u);
}

TEST(LayerInsertionTest, BelowSelectedLayerGoesOneDown) {
    EXPECT_EQ(insertIndexForNewLayer(2, 3, true), 1u);
    EXPECT_EQ(insertIndexForNewLayer(1, 3, true), 0u);
}

TEST(LayerInsertionTest, BackgroundSelectedGoesOnTop) {
    // Background selected with three layers: the answers layer must sit above them, not under them
    EXPECT_EQ(insertIndexForNewLayer(0, 3, false), 3u);
}

TEST(LayerInsertionTest, PageWithoutLayersGoesOnTop) {
    EXPECT_EQ(insertIndexForNewLayer(0, 0, false), 0u);
}

TEST(LayerInsertionTest, BelowBackgroundDoesNotUnderflow) {
    EXPECT_EQ(insertIndexForNewLayer(0, 3, true), 0u);
}
