#include "TeacherPalette.h"

#include <cstdio>

namespace utn::palette {

namespace {
constexpr uint32_t RGB_MASK = 0x00FFFFFFU;
constexpr uint32_t ALPHA_MASK = 0xFF000000U;
}  // namespace

auto count(Kind kind) -> std::size_t { return kind == Kind::Highlighter ? HIGHLIGHTER.size() : INK.size(); }

auto swatch(Kind kind, std::size_t index) -> const Swatch& {
    return kind == Kind::Highlighter ? HIGHLIGHTER.at(index) : INK.at(index);
}

auto kindFor(bool highlighter) -> Kind { return highlighter ? Kind::Highlighter : Kind::Ink; }

auto find(Kind kind, uint32_t argb) -> std::optional<std::size_t> {
    for (std::size_t i = 0; i < count(kind); ++i) {
        if (swatch(kind, i).rgb == (argb & RGB_MASK)) {
            return i;
        }
    }
    return std::nullopt;
}

auto hex(uint32_t argb) -> std::string {
    char text[8];
    std::snprintf(text, sizeof(text), "#%06X", static_cast<unsigned>(argb & RGB_MASK));
    return text;
}

auto nameOf(Kind kind, uint32_t argb) -> const char* {
    const auto index = find(kind, argb);
    return index ? swatch(kind, *index).name : "";
}

auto withOpacityOf(uint32_t chosen, uint32_t current) -> uint32_t {
    return (current & ALPHA_MASK) | (chosen & RGB_MASK);
}

}  // namespace utn::palette
