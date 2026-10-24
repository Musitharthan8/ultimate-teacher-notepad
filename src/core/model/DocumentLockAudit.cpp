#include "model/DocumentLockAudit.h"

#ifdef UTN_LOCK_AUDIT

#include <algorithm>
#include <cstdio>

#include <glib.h>

#include "util/Stacktrace.h"

namespace utn::lockaudit {
namespace {
constexpr auto SLOW_WAIT = std::chrono::milliseconds(50);
constexpr unsigned MAX_REPORTS = 25;  // stack traces are costly and one site repeats thousands of times

struct Depth {
    int shared = 0;
    int exclusive = 0;
};
thread_local Depth depth;
std::atomic<unsigned> nested{0};
std::atomic<unsigned> slow{0};
std::atomic<long long> longestWaitUs{0};

void report(const char* what) {
    const unsigned n = ++nested;
    if (n <= MAX_REPORTS) {
        g_warning("UTN lock audit: %s (#%u) on a thread that already holds the document lock", what, n);
        Stacktrace::printStacktrace();
    }
}
}  // namespace

unsigned nestedViolations() { return nested.load(); }
unsigned slowAcquisitions() { return slow.load(); }
double longestWaitMs() { return static_cast<double>(longestWaitUs.load()) / 1000.0; }

Clock::time_point begin(bool exclusive, bool blocking) {
    // A blocking acquisition while this thread already holds the lock can never be satisfied once a writer is queued
    if (blocking && (depth.shared > 0 || depth.exclusive > 0)) {
        report(exclusive ? "exclusive lock requested" : "shared lock requested");
    }
    return Clock::now();
}

void acquired(bool exclusive, Clock::time_point start) {
    const auto waited = Clock::now() - start;
    const auto us = std::chrono::duration_cast<std::chrono::microseconds>(waited).count();
    long long previous = longestWaitUs.load();
    while (us > previous && !longestWaitUs.compare_exchange_weak(previous, us)) {}
    if (waited > SLOW_WAIT) {
        if (++slow <= MAX_REPORTS) {
            g_warning("UTN lock audit: waited %.1f ms for the %s document lock", static_cast<double>(us) / 1000.0,
                      exclusive ? "exclusive" : "shared");
            Stacktrace::printStacktrace();
        }
    }
    (exclusive ? depth.exclusive : depth.shared)++;
}

void released(bool exclusive) { (exclusive ? depth.exclusive : depth.shared)--; }

}  // namespace utn::lockaudit

#endif
