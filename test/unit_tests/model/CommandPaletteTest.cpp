/*
 * Ultimate Teacher Notepad
 *
 * Command Palette search: which commands match what was typed and in what order. Free of GTK.
 *
 * Based on Xournal++ GPLv2+
 */

#include <cctype>
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

namespace {
std::string lowered(std::string s) {
    for (auto& c: s) {
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    }
    return s;
}

bool nameContains(const PaletteCommand& c, const std::string& visible) {
    return lowered(accessibleLabel(c)).find(lowered(visible)) != std::string::npos;
}
}  // namespace

TEST(CommandPalette, theVisibleDetailIsCategoryAndShortcutOrTheUnavailableReason) {
    auto on = cmd("a", "Next page", "Page");
    EXPECT_EQ(visibleDetail(on, "Unavailable"), "Page");
    on.shortcut = "Page Down";
    EXPECT_EQ(visibleDetail(on, "Unavailable"), "Page   Page Down");
    auto off = cmd("b", "Delete page", "Page", "", false);
    off.shortcut = "Ctrl+D";
    off.unavailableReason = "Only one page";
    EXPECT_EQ(visibleDetail(off, "Unavailable"), "Unavailable: Only one page") << "no shortcut offered for a row that cannot run";
    off.unavailableReason.clear();
    EXPECT_EQ(visibleDetail(off, "Unavailable"), "Unavailable: ");
}

// Label in name: whatever a row shows, a screen reader or voice control user can say or hear
TEST(CommandPalette, theAccessibleNameAlwaysContainsTheTitleAndEverythingTheRowShows) {
    auto on = cmd("a", "Next page", "Page");
    on.shortcut = "Page Down";
    auto off = cmd("b", "Delete page", "Page", "", false);
    off.unavailableReason = "Only one page";
    auto busy = cmd("c", "Save", "File", "", false);
    busy.unavailableReason = "Busy saving or exporting";
    for (const auto& c: {on, off, busy}) {
        EXPECT_TRUE(nameContains(c, c.title)) << c.id;
        EXPECT_TRUE(nameContains(c, c.category)) << c.id;
        if (c.enabled) {
            EXPECT_TRUE(nameContains(c, c.shortcut)) << c.id;
        } else {
            EXPECT_TRUE(nameContains(c, visibleDetail(c, "Unavailable"))) << c.id;
        }
    }
}

TEST(CommandPalette, anUnavailableCommandIsFoundByItsNameAndKeepsItsReasonWhateverTheQuery) {
    auto off = cmd("del", "Delete page", "Page", "remove", false);
    off.unavailableReason = "Only one page";
    const std::vector<PaletteCommand> all{off};
    for (const char* query: {"delete", "remove", "PAGE", "del pag"}) {
        const auto hit = rankCommands(all, query);
        ASSERT_EQ(hit.size(), 1u) << query;
        EXPECT_EQ(all[hit[0]].unavailableReason, "Only one page") << query;
    }
}

// The palette lists command names only, never page labels or text, so nothing private is shown on a shared screen
TEST(CommandPalette, aQueryNeverMatchesTextThatIsNotACommandSoPageLabelsCannotAppear) {
    const std::vector<PaletteCommand> all{cmd("next", "Next page", "Page", "forward down"),
                                          cmd("blank", "Blank students' screen", "Classroom", "hide black cover")};
    EXPECT_TRUE(rankCommands(all, "Algebra test answers").empty());
    EXPECT_EQ(rankCommands(all, "hide").size(), 1u);
}

TEST(CommandPalette, aPageNumberThatIsNotInTheDocumentOffersNothing) {
    EXPECT_FALSE(pageNumberFromQuery("0", 20));
    EXPECT_FALSE(pageNumberFromQuery("go to page 99", 20));
    EXPECT_FALSE(pageNumberFromQuery("12", 0));
    EXPECT_FALSE(pageNumberFromQuery("", 20));
}
