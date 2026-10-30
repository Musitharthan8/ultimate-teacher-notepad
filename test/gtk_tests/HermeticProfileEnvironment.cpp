/*
 * Ultimate Teacher Notepad
 *
 * Keeps the GTK tests off the profile of whoever runs them.
 *
 * Based on Xournal++ GPLv2+
 */

#include <filesystem>
#include <system_error>

#include <glib.h>
#include <gtest/gtest.h>

namespace {
/// Control loads (and, when the file is missing, writes) settings.xml under the user's config folder, so a test that
/// reads a setting such as the sidebar state would otherwise depend on the real profile of the machine it runs on, and
/// change it. GLib reads the XDG variables on every platform, including Windows, and caches the folders on first use, so
/// they are pointed at a throw-away folder before the first test runs.
class HermeticProfile: public ::testing::Environment {
public:
    void SetUp() override {
        gchar* dir = g_dir_make_tmp("utn-gtk-test-XXXXXX", nullptr);
        ASSERT_NE(dir, nullptr) << "could not create a throw-away profile folder";
        root = dir;
        g_free(dir);
        for (const char* variable: {"XDG_CONFIG_HOME", "XDG_DATA_HOME", "XDG_CACHE_HOME", "XDG_STATE_HOME"}) {
            const auto folder = root / variable;
            std::filesystem::create_directories(folder);
            g_setenv(variable, folder.string().c_str(), TRUE);
        }
    }

    void TearDown() override {
        if (!root.empty()) {
            std::error_code ignored;
            std::filesystem::remove_all(root, ignored);
        }
    }

private:
    std::filesystem::path root;
};

// gtest takes ownership
[[maybe_unused]] const auto* const hermeticProfile = ::testing::AddGlobalTestEnvironment(new HermeticProfile);
}  // namespace
