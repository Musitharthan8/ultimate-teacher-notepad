/*
 * Ultimate Teacher Notepad
 *
 * Identifies the UTN teacher toolbar layouts.
 *
 * Based on Xournal++ GPLv2+
 */

#pragma once

#include <string_view>

namespace utn {

/// Teacher layouts use the UTN shell, the teacher tool policy and unfilled handwriting.
/// Every other layout is a Classic Xournal++ layout and keeps upstream behaviour.
inline bool isTeacherLayout(std::string_view layoutId) {
    return layoutId == "UTN Teacher" || layoutId == "UTN Teacher Tablet" || layoutId == "UTN Marking";
}

}  // namespace utn
