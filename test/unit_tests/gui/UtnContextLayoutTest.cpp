/*
 * Ultimate Teacher Notepad
 *
 * Guards the teacher layout: the contextual tool panel must sit beside the tool rail (toolbarLeft2), not in the
 * top strip, and each teacher profile must keep it. Reads the shipped template, so a future edit cannot silently
 * move the panel back or drop it.
 *
 * Based on Xournal++ GPLv2+
 */

#include <string>

#include <glib.h>
#include <gtest/gtest.h>

#include "config-test.h"  // for PROJECT_SOURCE_DIR

namespace {
struct KeyFileGuard {
    GKeyFile* file = nullptr;
    ~KeyFileGuard() {
        if (file) {
            g_key_file_free(file);
        }
    }
};

std::string readKey(GKeyFile* file, const char* group, const char* key) {
    gchar* value = g_key_file_get_string(file, group, key, nullptr);
    if (!value) {
        return {};
    }
    std::string out(value);
    g_free(value);
    return out;
}

GKeyFile* loadTemplate(KeyFileGuard& guard) {
    guard.file = g_key_file_new();
    g_key_file_set_list_separator(guard.file, ',');
    const std::string path = std::string(PROJECT_SOURCE_DIR) + "/resources-templates/toolbar.ini.in";
    EXPECT_TRUE(g_key_file_load_from_file(guard.file, path.c_str(), G_KEY_FILE_NONE, nullptr)) << path;
    return guard.file;
}
}  // namespace

TEST(UtnContextLayout, teacherProfilesPlaceContextPanelBesideToolRail) {
    for (const char* group: {"UTN Teacher", "UTN Teacher Tablet", "UTN Marking"}) {
        KeyFileGuard guard;
        GKeyFile* file = loadTemplate(guard);
        ASSERT_NE(file, nullptr);
        EXPECT_NE(readKey(file, group, "toolbarLeft2").find("UTN_CONTEXT"), std::string::npos) << group;
        EXPECT_EQ(readKey(file, group, "toolbarTop2").find("UTN_CONTEXT"), std::string::npos) << group;
    }
}
