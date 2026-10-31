#include "CommandPalette.h"

#include <algorithm>
#include <cctype>

#include "PageNavigation.h"

namespace utn {

namespace {
std::string lower(std::string_view s) {
    std::string out(s);
    std::transform(out.begin(), out.end(), out.begin(), [](unsigned char c) { return std::tolower(c); });
    return out;
}

std::vector<std::string> words(std::string_view s) {
    std::vector<std::string> out;
    std::string cur;
    for (char c: lower(s)) {
        if (std::isspace(static_cast<unsigned char>(c))) {
            if (!cur.empty()) {
                out.push_back(std::move(cur));
                cur.clear();
            }
        } else {
            cur.push_back(c);
        }
    }
    if (!cur.empty()) {
        out.push_back(std::move(cur));
    }
    return out;
}

bool startsAWord(const std::vector<std::string>& haystack, const std::string& token) {
    return std::any_of(haystack.begin(), haystack.end(),
                       [&](const std::string& w) { return w.compare(0, token.size(), token) == 0; });
}

/// 0 = no match. Title word start 4, keyword or category word start 3, inside the title 2, inside keywords or category 1
int scoreToken(const PaletteCommand& c, const std::string& token) {
    if (startsAWord(words(c.title), token)) {
        return 4;
    }
    if (startsAWord(words(c.keywords), token) || startsAWord(words(c.category), token)) {
        return 3;
    }
    if (lower(c.title).find(token) != std::string::npos) {
        return 2;
    }
    if (lower(c.keywords).find(token) != std::string::npos || lower(c.category).find(token) != std::string::npos) {
        return 1;
    }
    return 0;
}
}  // namespace

std::vector<size_t> rankCommands(const std::vector<PaletteCommand>& commands, std::string_view query) {
    const auto tokens = words(query);
    struct Hit {
        size_t index;
        int score;
    };
    std::vector<Hit> hits;
    for (size_t i = 0; i < commands.size(); ++i) {
        int total = 0;
        bool all = true;
        for (const auto& token: tokens) {
            const int s = scoreToken(commands[i], token);
            if (s == 0) {
                all = false;
                break;
            }
            total += s;
        }
        if (all) {
            hits.push_back({i, total});
        }
    }
    std::stable_sort(hits.begin(), hits.end(), [&](const Hit& a, const Hit& b) {
        const auto& ca = commands[a.index];
        const auto& cb = commands[b.index];
        if (ca.enabled != cb.enabled) {
            return ca.enabled;
        }
        if (a.score != b.score) {
            return a.score > b.score;
        }
        return ca.priority > cb.priority;
    });
    std::vector<size_t> out;
    out.reserve(hits.size());
    for (const auto& h: hits) {
        out.push_back(h.index);
    }
    return out;
}

std::string accessibleLabel(const PaletteCommand& command) {
    std::string out = command.title;
    if (!command.category.empty()) {
        out += ", " + command.category;
    }
    if (!command.shortcut.empty()) {
        out += ", shortcut " + command.shortcut;
    }
    if (!command.enabled) {
        out += ", unavailable";
        if (!command.unavailableReason.empty()) {
            out += ": " + command.unavailableReason;
        }
    }
    return out;
}

std::optional<size_t> pageNumberFromQuery(std::string_view query, size_t pageCount) {
    auto tokens = words(query);
    size_t first = 0;
    while (first < tokens.size() &&
           (tokens[first] == "go" || tokens[first] == "to" || tokens[first] == "page" || tokens[first] == "pg" ||
            tokens[first] == "p")) {
        ++first;
    }
    if (first + 1 != tokens.size()) {
        return std::nullopt;
    }
    return parsePageNumber(tokens[first], pageCount);
}

}  // namespace utn
