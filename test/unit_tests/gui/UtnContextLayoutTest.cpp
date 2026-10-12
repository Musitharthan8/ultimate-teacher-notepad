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
    // PROJECT_SOURCE_DIR is a u8 literal (char8_t in C++20), so build the path as u8string first.
    const std::u8string path = std::u8string(PROJECT_SOURCE_DIR) + u8"/resources-templates/toolbar.ini.in";
    const auto* utf8 = reinterpret_cast<const char*>(path.c_str());
    EXPECT_TRUE(g_key_file_load_from_file(guard.file, utf8, G_KEY_FILE_NONE, nullptr)) << utf8;
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
