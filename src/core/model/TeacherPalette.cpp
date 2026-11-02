#include "TeacherPalette.h"

namespace utn::palette {

auto count(Kind) -> std::size_t { return 0; }

auto swatch(Kind, std::size_t) -> const Swatch& { return INK[0]; }

auto kindFor(bool) -> Kind { return Kind::Ink; }

auto find(Kind, uint32_t) -> std::optional<std::size_t> { return std::nullopt; }

auto hex(uint32_t) -> std::string { return {}; }

auto nameOf(Kind, uint32_t) -> const char* { return ""; }

auto withOpacityOf(uint32_t chosen, uint32_t) -> uint32_t { return chosen; }

}  // namespace utn::palette
