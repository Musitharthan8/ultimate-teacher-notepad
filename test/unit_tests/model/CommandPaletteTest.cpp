/*
 * Ultimate Teacher Notepad
 *
 * Command Palette search: which commands match what was typed and in what order. Free of GTK.
 *
 * Based on Xournal++ GPLv2+
 */

#include <string>
#include <vector>

#include <gtest/gtest.h>

#include "model/CommandPalette.h"

using namespace utn;

namespace {
PaletteCommand cmd(std::string id, std::string title, std::string category, std::string keywords = "",
                   bool enabled = true, int priority = 0) {
    PaletteCommand c;
    c.id = std::move(id);
    c.title = std::move(title);
    c.category = std::move(category);
    c.keywords = std::move(keywords);
    c.enabled = enabled;
    c.priority = priority;
    return c;
}

std::vector<std::string> ids(const std::vector<PaletteCommand>& all, const std::vector<size_t>& order) {
    std::vector<std::string> out;
    for (size_t i: order) {
        out.push_back(all[i].id);
    }
    return out;
}
}  // namespace

TEST(CommandPalette, anEmptyQueryListsEverythingAvailableFirstThenByPriority) {
    const std::vector<PaletteCommand> all{cmd("a", "Alpha", "Page", "", false), cmd("b", "Beta", "Page", "", true, 1),
                                          cmd("c", "Gamma", "Page", "", true, 5)};
    EXPECT_EQ(ids(all, rankCommands(all, "")), (std::vector<std::string>{"c", "b", "a"}));
    EXPECT_EQ(ids(all, rankCommands(all, "   ")), (std::vector<std::string>{"c", "b", "a"}));
}

TEST(CommandPalette, everyWordTypedMustMatchTitleKeywordsOrCategory) {
    const std::vector<PaletteCommand> all{cmd("pen", "Pen", "Tool", "write draw ink"),
                                          cmd("hl", "Highlighter", "Tool", "mark"),
                                          cmd("next", "Next page", "Page", "forward")};
    EXPECT_EQ(ids(all, rankCommands(all, "ink")), (std::vector<std::string>{"pen"}));
    EXPECT_EQ(ids(all, rankCommands(all, "tool mark")), (std::vector<std::string>{"hl"}));
    EXPECT_TRUE(rankCommands(all, "zebra").empty());
    EXPECT_TRUE(rankCommands(all, "pen zebra").empty());
}

TEST(CommandPalette, matchingIsCaseInsensitive) {
    const std::vector<PaletteCommand> all{cmd("pen", "Pen", "Tool")};
    EXPECT_EQ(rankCommands(all, "PEN").size(), 1u);
}

TEST(CommandPalette, aTitleWordStartBeatsALaterSubstringAndAKeyword) {
    const std::vector<PaletteCommand> all{cmd("kw", "Zoom to fit", "View", "page width"),
                                          cmd("sub", "Compagnon", "View"),
                                          cmd("title", "Page setup", "Page")};
    EXPECT_EQ(ids(all, rankCommands(all, "pag")), (std::vector<std::string>{"title", "kw", "sub"}));
}

TEST(CommandPalette, unavailableCommandsStillMatchButComeAfterAvailableOnes) {
    const std::vector<PaletteCommand> all{cmd("off", "Delete page", "Page", "", false, 9),
                                          cmd("on", "Delete layer", "Page", "", true, 0)};
    EXPECT_EQ(ids(all, rankCommands(all, "delete")), (std::vector<std::string>{"on", "off"}));
}

TEST(CommandPalette, equalMatchesKeepPriorityThenTheOrderTheyWereGiven) {
    const std::vector<PaletteCommand> all{cmd("a", "Page one", "Page"), cmd("b", "Page two", "Page", "", true, 2),
                                          cmd("c", "Page three", "Page")};
    EXPECT_EQ(ids(all, rankCommands(all, "page")), (std::vector<std::string>{"b", "a", "c"}));
}

TEST(CommandPalette, theAccessibleLabelSaysWhatTheCommandIsAndWhetherItWorks) {
    auto on = cmd("a", "Next page", "Page");
    on.shortcut = "Page Down";
    EXPECT_EQ(accessibleLabel(on), "Next page, Page, shortcut Page Down");
    auto off = cmd("b", "Delete page", "Page", "", false);
    off.unavailableReason = "Only one page";
    EXPECT_EQ(accessibleLabel(off), "Delete page, Page, unavailable: Only one page");
    auto offNoReason = cmd("c", "Undo", "Edit", "", false);
    EXPECT_EQ(accessibleLabel(offNoReason), "Undo, Edit, unavailable");
}

TEST(CommandPalette, aTypedPageNumberOffersToGoThere) {
    EXPECT_EQ(pageNumberFromQuery("12", 20), std::optional<size_t>(11));
    EXPECT_EQ(pageNumberFromQuery("page 12", 20), std::optional<size_t>(11));
    EXPECT_EQ(pageNumberFromQuery("Go to page 3", 20), std::optional<size_t>(2));
    EXPECT_EQ(pageNumberFromQuery("p 3", 20), std::optional<size_t>(2));
    EXPECT_FALSE(pageNumberFromQuery("page 21", 20));
    EXPECT_FALSE(pageNumberFromQuery("pen 3", 20));
    EXPECT_FALSE(pageNumberFromQuery("page", 20));
}
