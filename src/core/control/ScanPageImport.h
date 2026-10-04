/*
 * Ultimate Teacher Notepad
 * Scan/photo worksheet import, based on Xournal++ GPLv2+.
 */
#pragma once

#include <string>
#include <variant>

#include "model/PageRef.h"
#include "filesystem.h"

namespace xoj::utn {
using ScanPageResult = std::variant<PageRef, std::string>;
ScanPageResult createScanPage(const fs::path& path, double pageWidth);
}  // namespace xoj::utn
