/*
 * Ultimate Teacher Notepad
 *
 * Every model change that can reduce what students see must be announced, whichever user interface path made it.
 *
 * Based on Xournal++ GPLv2+
 */
#include <vector>

#include <gtest/gtest.h>

#include "model/Layer.h"
#include "model/StudentVisibility.h"

namespace {
struct Recorder {
    std::vector<bool> calls;
    size_t id;
    Recorder(): id(utn::addStudentVisibilityListener([this](bool mayCheckNow) { calls.push_back(mayCheckNow); })) {}
    ~Recorder() { utn::removeStudentVisibilityListener(id); }
};
}  // namespace

TEST(StudentVisibility, layerSettersAnnounceWithoutPermissionToCheckNow) {
    Recorder recorder;
    Layer layer;
    layer.setVisible(false);
    layer.setAudience(LayerAudience::TeacherOnly);
    ASSERT_EQ(recorder.calls.size(), 2U);
    EXPECT_FALSE(recorder.calls[0]) << "a model setter may run under the Document lock; the listener must defer";
    EXPECT_FALSE(recorder.calls[1]);
}

TEST(StudentVisibility, explicitNotificationCarriesTheCallersPermission) {
    Recorder recorder;
    utn::notifyStudentVisibilityChanged(true);
    ASSERT_EQ(recorder.calls.size(), 1U);
    EXPECT_TRUE(recorder.calls[0]);
}

TEST(StudentVisibility, removedListenerHearsNothingMore) {
    size_t before = 0;
    {
        Recorder recorder;
        utn::notifyStudentVisibilityChanged(true);
        before = recorder.calls.size();
        EXPECT_EQ(before, 1U);
    }
    EXPECT_NO_THROW(utn::notifyStudentVisibilityChanged(true));
}
