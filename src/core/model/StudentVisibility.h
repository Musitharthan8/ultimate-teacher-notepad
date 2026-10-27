/*
 * Ultimate Teacher Notepad
 *
 * One place that learns about every change that can reduce what students may see: a layer shown or hidden, made
 * private, added or removed, Spotlight or Curtain switched. Student View listens here, so a frozen frame cannot
 * outlive a change just because the user interface path that made it forgot to announce a page change.
 *
 * Based on Xournal++ GPLv2+
 */

#pragma once

#include <cstddef>
#include <cstdint>
#include <functional>

namespace utn {

/// @param mayCheckNow true when the caller holds no Document lock and runs on the thread that owns the user interface.
///        false when the change was made inside a model setter, possibly under the Document lock, possibly on another
///        thread: the listener must then defer its check.
using StudentVisibilityListener = std::function<void(bool mayCheckNow)>;

size_t addStudentVisibilityListener(StudentVisibilityListener listener);
/// Returns only after no call to the listener is running any more
void removeStudentVisibilityListener(size_t id);

/**
 * Count of elements that moved from a layer students could see into one they cannot (made teacher-only, hidden, or
 * answers not yet revealed). A frozen Student View picture drawn before the latest such move may still show that content.
 */
uint64_t privateTransferCount();
/// Called by Layer when such a move happens. Counts it and announces it like any visibility change (deferred).
void noteContentEnteredPrivateLayer();

/// Announce a change that may reduce what students are allowed to see (showing more is announced too)
void notifyStudentVisibilityChanged(bool mayCheckNow);

}  // namespace utn
