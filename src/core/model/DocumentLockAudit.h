/*
 * Ultimate Teacher Notepad
 *
 * Opt-in diagnostics for Document's reader/writer lock (CMake option UTN_LOCK_AUDIT, off by default).
 *
 * Why: std::shared_mutex in the MSYS2 mingw64 toolchain prefers waiting writers. A thread that takes a shared lock it
 * already holds, while another thread waits for the exclusive lock, blocks forever (measured: a 3-thread probe
 * deadlocks in under 200 ms). The UI thread then waits for the document and the window stops responding. This file finds
 * every place where that can happen without needing the rare timing: any nested acquisition on one thread is reported
 * with a stack trace, as is any acquisition that waits long enough to be felt.
 *
 * With the option off every function is empty and compiles away.
 *
 * Based on Xournal++ GPLv2+
 */

#pragma once

#ifdef UTN_LOCK_AUDIT

#include <atomic>
#include <chrono>

namespace utn::lockaudit {

/// Nested acquisitions on one thread seen since start (shared inside shared/exclusive, exclusive inside either)
unsigned nestedViolations();
/// Acquisitions that waited longer than SLOW_WAIT
unsigned slowAcquisitions();
/// Longest wait for the document lock seen since start, in milliseconds
double longestWaitMs();

using Clock = std::chrono::steady_clock;

Clock::time_point begin(bool exclusive, bool blocking);
void acquired(bool exclusive, Clock::time_point start);
void released(bool exclusive);

}  // namespace utn::lockaudit

#else

#include <chrono>

namespace utn::lockaudit {
using Clock = std::chrono::steady_clock;
inline Clock::time_point begin(bool, bool) { return {}; }
inline void acquired(bool, Clock::time_point) {}
inline void released(bool) {}
}  // namespace utn::lockaudit

#endif
