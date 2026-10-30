/*
 * Ultimate Teacher Notepad
 *
 * The customisable tool dock. A teacher pins favourites to the top, hides tools they never use and reorders tools
 * inside their group; this applies that to the rail's default list. Free of GTK so every rule is unit tested.
 *
 * Groups come from the default list (its separators), so related tools stay together however they are arranged. The
 * essential tools can never be hidden, and a layout that names tools that no longer exist is harmless.
 *
 * Based on Xournal++ GPLv2+
 */

#pragma once

#include <string>
#include <vector>

namespace utn {

class DockLayout {
public:
    /// Tools that must always be on the dock: the pen, the eraser and both selection tools
    static bool isEssential(const std::string& id);

    /// Nothing customised: the dock is exactly the default list
    bool isDefault() const { return pinnedIds.empty() && hiddenIds.empty() && orderIds.empty(); }
    bool isPinned(const std::string& id) const;
    bool isHidden(const std::string& id) const;

    /// Moves a tool to the favourites group at the top. A hidden tool is shown again.
    bool pin(const std::string& id);
    /// @return false when the tool was not pinned
    bool unpin(const std::string& id);
    /// Takes a tool off the dock (it stays in the menus and on its shortcut). False for an essential tool.
    bool hide(const std::string& id);
    /// @return false when the tool was not hidden
    bool show(const std::string& id);
    /// One step among the tools that are showing, inside the tool's group (or among the favourites). False when it
    /// cannot move that way.
    bool moveUp(const std::string& id, const std::vector<std::string>& defaults);
    bool moveDown(const std::string& id, const std::vector<std::string>& defaults);
    void reset();

    /// The dock for the given default list. Separators between groups stay, favourites get a group of their own, and a
    /// group with nothing left loses its separator.
    std::vector<std::string> arrange(const std::vector<std::string>& defaults) const;

    /// The saved form: versioned, safe to keep in a settings attribute
    std::string serialise() const;
    /// Reads the saved form. Bad entries are dropped, and anything unreadable is the default dock.
    static DockLayout parse(const std::string& saved);

    const std::vector<std::string>& pinned() const { return pinnedIds; }
    const std::vector<std::string>& hidden() const { return hiddenIds; }
    const std::vector<std::string>& order() const { return orderIds; }

private:
    bool step(const std::string& id, const std::vector<std::string>& defaults, int direction);

    std::vector<std::string> pinnedIds;  ///< favourites, in the order shown
    std::vector<std::string> hiddenIds;
    std::vector<std::string> orderIds;   ///< the teacher's order inside groups; tools not listed follow in default order
};

}  // namespace utn
