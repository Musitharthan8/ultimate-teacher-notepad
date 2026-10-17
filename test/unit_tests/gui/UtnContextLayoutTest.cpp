/*
 * Ultimate Teacher Notepad
 *
 * Guards the teacher layout: the contextual tool panel must sit beside the tool rail (toolbarLeft2), not in the
 * top strip, and each teacher profile must keep it. Reads the shipped template, so a future edit cannot silently
 * move the panel back or drop it.
 *
 * Based on Xournal++ GPLv2+
 */

#include <algorithm>
#include <string>
#include <vector>

#include <glib.h>
#include <gtest/gtest.h>

#include "config-test.h"  // for PROJECT_SOURCE_DIR
#include "gui/toolbarMenubar/EraserModes.h"

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

TEST(UtnContextLayout, teachingToolsKeepOneOrderInEveryTeacherProfile) {
    // Select comes first, then the drawing tools, so a teacher finds the same tool in the same place in Teach and Mark.
    const std::vector<std::string> expected = {"UTN_SELECT", "SELECT",   "PEN",    "TEACHING_KIT", "SMART_HIGHLIGHTER",
                                               "UTN_COLOURS", "ERASER", "UTN_TEXT", "ANSWER_BOX",   "TEACHER_STAMP"};
    for (const char* group: {"UTN Teacher", "UTN Teacher Tablet", "UTN Marking"}) {
        KeyFileGuard guard;
        GKeyFile* file = loadTemplate(guard);
        ASSERT_NE(file, nullptr);
        std::vector<std::string> seen;
        std::string rail = readKey(file, group, "toolbarLeft1");
        size_t start = 0;
        while (start <= rail.size()) {
            size_t end = rail.find(',', start);
            if (end == std::string::npos) {
                end = rail.size();
            }
            std::string item = rail.substr(start, end - start);
            item.erase(0, item.find_first_not_of(" 	"));
            item.erase(item.find_last_not_of(" 	") + 1);
            if (std::find(expected.begin(), expected.end(), item) != expected.end()) {
                seen.push_back(item);
            }
            start = end + 1;
        }
        EXPECT_EQ(seen, expected) << group;
    }
}

TEST(UtnContextLayout, eraserModesExposeObjectEraseAndKeepSavedEnumValues) {
    ASSERT_EQ(utn::ERASER_MODES.size(), 4U);
    EXPECT_EQ(utn::eraserModeIndex(ERASER_TYPE_DEFAULT), 0);
    EXPECT_EQ(utn::eraserModeIndex(ERASER_TYPE_DELETE_STROKE), 1);
    EXPECT_EQ(utn::eraserModeIndex(ERASER_TYPE_DELETE_OBJECT), 2);
    EXPECT_EQ(utn::eraserModeIndex(ERASER_TYPE_WHITEOUT), 3);
    EXPECT_EQ(utn::eraserModeIndex(ERASER_TYPE_NONE), 0);

    for (const auto& mode: utn::ERASER_MODES) {
        const auto saved = std::string(eraserTypeToString(mode.type));
        EXPECT_EQ(eraserTypeFromString(saved), mode.type);
    }
    EXPECT_EQ(eraserTypeToString(ERASER_TYPE_DELETE_OBJECT), "deleteObject");
}
