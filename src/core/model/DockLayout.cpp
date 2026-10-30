#include "DockLayout.h"

#include <algorithm>
#include <cctype>
#include <set>
#include <utility>

namespace utn {

namespace {
constexpr const char* GROUP_SEPARATOR = "SEPARATOR";
constexpr const char* SAVED_VERSION = "v1";
constexpr std::size_t MAX_SAVED_BYTES = 8192;
constexpr std::size_t MAX_IDS_PER_LIST = 128;
constexpr std::size_t MAX_ID_BYTES = 40;

using Ids = std::vector<std::string>;

bool validId(const std::string& id) {
    return !id.empty() && id.size() <= MAX_ID_BYTES && id != GROUP_SEPARATOR &&
           std::all_of(id.begin(), id.end(),
                       [](unsigned char c) { return std::isalnum(c) || c == '_' || c == '(' || c == ')' || c == '-'; });
}

bool has(const Ids& ids, const std::string& id) { return std::find(ids.begin(), ids.end(), id) != ids.end(); }

bool eraseId(Ids& ids, const std::string& id) {
    const auto it = std::remove(ids.begin(), ids.end(), id);
    const bool found = it != ids.end();
    ids.erase(it, ids.end());
    return found;
}

/// The default list split at its separators, each tool once, in default order
std::vector<Ids> groupsOf(const Ids& defaults) {
    std::vector<Ids> groups(1);
    std::set<std::string> seen;
    for (const std::string& id: defaults) {
        if (id == GROUP_SEPARATOR) {
            groups.emplace_back();
        } else if (seen.insert(id).second) {
            groups.back().push_back(id);
        }
    }
    return groups;
}

/// A group in the order the teacher arranged it: listed tools first, in their listed order, then the rest as shipped
Ids ranked(const Ids& group, const Ids& order) {
    Ids result = group;
    const auto key = [&](const std::string& id) {
        const auto it = std::find(order.begin(), order.end(), id);
        if (it != order.end()) {
            return static_cast<std::size_t>(it - order.begin());
        }
        return order.size() + static_cast<std::size_t>(std::find(group.begin(), group.end(), id) - group.begin());
    };
    std::stable_sort(result.begin(), result.end(), [&](const std::string& a, const std::string& b) { return key(a) < key(b); });
    return result;
}

Ids parseList(const std::string& text) {
    Ids ids;
    std::size_t start = 0;
    while (start <= text.size() && ids.size() < MAX_IDS_PER_LIST) {
        const std::size_t end = text.find(',', start);
        const std::string id = text.substr(start, end == std::string::npos ? std::string::npos : end - start);
        if (validId(id) && !has(ids, id)) {
            ids.push_back(id);
        }
        if (end == std::string::npos) {
            break;
        }
        start = end + 1;
    }
    return ids;
}

std::string joined(const Ids& ids) {
    std::string text;
    for (const std::string& id: ids) {
        text += (text.empty() ? "" : ",") + id;
    }
    return text;
}
}  // namespace

bool DockLayout::isEssential(const std::string& id) {
    return id == "PEN" || id == "ERASER" || id == "SELECT" || id == "UTN_SELECT";
}

bool DockLayout::isPinned(const std::string& id) const { return has(pinnedIds, id); }
bool DockLayout::isHidden(const std::string& id) const { return has(hiddenIds, id); }

bool DockLayout::pin(const std::string& id) {
    if (!validId(id)) {
        return false;
    }
    eraseId(hiddenIds, id);
    if (!has(pinnedIds, id)) {
        pinnedIds.push_back(id);
    }
    return true;
}

bool DockLayout::unpin(const std::string& id) { return eraseId(pinnedIds, id); }

bool DockLayout::hide(const std::string& id) {
    if (!validId(id) || isEssential(id)) {
        return false;
    }
    eraseId(pinnedIds, id);
    if (!has(hiddenIds, id)) {
        hiddenIds.push_back(id);
    }
    return true;
}

bool DockLayout::show(const std::string& id) { return eraseId(hiddenIds, id); }

void DockLayout::reset() {
    pinnedIds.clear();
    hiddenIds.clear();
    orderIds.clear();
}

std::vector<std::string> DockLayout::arrange(const std::vector<std::string>& defaults) const {
    if (isDefault()) {
        return defaults;
    }
    const std::vector<Ids> groups = groupsOf(defaults);
    std::set<std::string> known;
    for (const Ids& group: groups) {
        known.insert(group.begin(), group.end());
    }

    Ids favourites;
    for (const std::string& id: pinnedIds) {
        if (known.count(id) && !has(favourites, id)) {
            favourites.push_back(id);
        }
    }

    std::vector<Ids> shown;
    for (const Ids& group: groups) {
        Ids visible;
        for (const std::string& id: ranked(group, orderIds)) {
            if (!has(favourites, id) && !has(hiddenIds, id)) {
                visible.push_back(id);
            }
        }
        if (!visible.empty()) {
            shown.push_back(std::move(visible));
        }
    }

    Ids result = favourites;
    for (const Ids& group: shown) {
        if (!result.empty()) {
            result.push_back(GROUP_SEPARATOR);
        }
        result.insert(result.end(), group.begin(), group.end());
    }
    return result;
}

bool DockLayout::step(const std::string& id, const std::vector<std::string>& defaults, int direction) {
    if (!validId(id) || isHidden(id)) {
        return false;
    }
    const std::vector<Ids> groups = groupsOf(defaults);

    if (isPinned(id)) {
        Ids shownFavourites;
        for (const std::string& pinnedId: pinnedIds) {
            for (const Ids& group: groups) {
                if (has(group, pinnedId)) {
                    shownFavourites.push_back(pinnedId);
                    break;
                }
            }
        }
        const auto position = std::find(shownFavourites.begin(), shownFavourites.end(), id);
        if (position == shownFavourites.end()) {
            return false;
        }
        const long neighbour = static_cast<long>(position - shownFavourites.begin()) + direction;
        if (neighbour < 0 || neighbour >= static_cast<long>(shownFavourites.size())) {
            return false;
        }
        std::iter_swap(std::find(pinnedIds.begin(), pinnedIds.end(), id),
                       std::find(pinnedIds.begin(), pinnedIds.end(), shownFavourites[static_cast<std::size_t>(neighbour)]));
        return true;
    }

    const auto home = std::find_if(groups.begin(), groups.end(), [&](const Ids& group) { return has(group, id); });
    if (home == groups.end()) {
        return false;
    }
    Ids inOrder = ranked(*home, orderIds);
    std::vector<std::size_t> showing;  // where the tools that are on the dock sit in this group
    for (std::size_t i = 0; i < inOrder.size(); ++i) {
        if (!has(pinnedIds, inOrder[i]) && !has(hiddenIds, inOrder[i])) {
            showing.push_back(i);
        }
    }
    const auto here = std::find_if(showing.begin(), showing.end(), [&](std::size_t i) { return inOrder[i] == id; });
    if (here == showing.end()) {
        return false;
    }
    const long neighbour = static_cast<long>(here - showing.begin()) + direction;
    if (neighbour < 0 || neighbour >= static_cast<long>(showing.size())) {
        return false;
    }
    std::swap(inOrder[*here], inOrder[showing[static_cast<std::size_t>(neighbour)]]);

    Ids newOrder;
    Ids shippedOrder;
    for (const Ids& group: groups) {
        const Ids arranged = &group == &*home ? inOrder : ranked(group, orderIds);
        newOrder.insert(newOrder.end(), arranged.begin(), arranged.end());
        shippedOrder.insert(shippedOrder.end(), group.begin(), group.end());
    }
    orderIds = newOrder == shippedOrder ? Ids{} : newOrder;  // back to the shipped order means nothing is customised
    return true;
}

bool DockLayout::moveUp(const std::string& id, const std::vector<std::string>& defaults) { return step(id, defaults, -1); }
bool DockLayout::moveDown(const std::string& id, const std::vector<std::string>& defaults) { return step(id, defaults, 1); }

std::string DockLayout::serialise() const {
    std::string saved = SAVED_VERSION;
    if (!pinnedIds.empty()) {
        saved += ";pinned=" + joined(pinnedIds);
    }
    if (!hiddenIds.empty()) {
        saved += ";hidden=" + joined(hiddenIds);
    }
    if (!orderIds.empty()) {
        saved += ";order=" + joined(orderIds);
    }
    return saved;
}

DockLayout DockLayout::parse(const std::string& saved) {
    DockLayout layout;
    if (saved.empty() || saved.size() > MAX_SAVED_BYTES) {
        return layout;
    }
    std::vector<std::string> parts;
    for (std::size_t start = 0;;) {
        const std::size_t end = saved.find(';', start);
        parts.push_back(saved.substr(start, end == std::string::npos ? std::string::npos : end - start));
        if (end == std::string::npos) {
            break;
        }
        start = end + 1;
    }
    if (parts.empty() || parts[0] != SAVED_VERSION) {
        return layout;
    }
    for (std::size_t i = 1; i < parts.size(); ++i) {
        const std::size_t equals = parts[i].find('=');
        if (equals == std::string::npos) {
            continue;
        }
        const std::string key = parts[i].substr(0, equals);
        const Ids ids = parseList(parts[i].substr(equals + 1));
        if (key == "pinned") {
            layout.pinnedIds = ids;
        } else if (key == "hidden") {
            layout.hiddenIds = ids;
        } else if (key == "order") {
            layout.orderIds = ids;
        }  // an unknown key is ignored: a later version may have added it
    }
    // A file cannot hide what must stay, and a tool is either pinned or hidden, never both (pinned wins)
    layout.hiddenIds.erase(std::remove_if(layout.hiddenIds.begin(), layout.hiddenIds.end(),
                                          [&](const std::string& id) { return isEssential(id) || has(layout.pinnedIds, id); }),
                           layout.hiddenIds.end());
    return layout;
}

}  // namespace utn
