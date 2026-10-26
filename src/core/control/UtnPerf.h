/*
 * Ultimate Teacher Notepad
 *
 * Opt-in performance counters for the classroom stability work. Off by default (one relaxed atomic load per call
 * site). Enable with UTN_PERF=1 in the environment; a summary is then written to the log every 30 s.
 * The summary and any UI hang of more than 2 s (reported from a watchdog thread, so it works during the hang) are
 * appended to UTN_PERF_FILE, or utn-perf.log in the temp folder. Tests call setEnabled(true) and read stats() directly.
 *
 * Based on Xournal++ GPLv2+
 */

#pragma once

#include <chrono>
#include <cstddef>
#include <string>

namespace utn::perf {

enum class Metric : unsigned {
    PenUp,               ///< StrokeHandler::onButtonReleaseEvent, whole call (ms)
    PenUpLockWait,       ///< time pen-up waited for the exclusive Document lock (ms)
    DocLockWaitExclusive,  ///< every Document::lock() wait (ms)
    DocLockWaitShared,   ///< every Document::lock_shared() wait (ms)
    RenderJob,           ///< RenderJob::renderToBuffer, including the shared lock it holds (ms)
    Autosave,            ///< AutosaveJob::run, whole job (ms)
    AutosaveLockHold,    ///< time AutosaveJob held the shared Document lock (ms)
    StudentFrame,        ///< one Student View frame render on the UI thread (ms)
    StudentRedrawRequests,  ///< value 1 per ToolStudentView::requestRedraw call
    StudentRedrawUrgent,    ///< value 1 per request that skipped the throttle
    EraseEvent,          ///< EraseHandler::erase for one pointer sample (ms)
    ErasuresPerStroke,   ///< value, not ms: erasure capsules on a stroke after an erase event
    Recognizer,          ///< ShapeRecognizer::recognizePatterns at pen-up (ms)
    UiStall,             ///< how late the main loop ran a 20 ms heartbeat (ms)
    ProcessPrivateMiB,   ///< value: private bytes of the process (MiB)
    GdiObjects,          ///< value: GDI objects of the process
    UserObjects,         ///< value: USER objects of the process
    ProcessHandles,      ///< value: kernel handles of the process
    COUNT
};

/**
 * Two populations, never mixed. "All" (count, total/mean(), max, first, last) covers every sample since reset().
 * "Window" (windowCount, windowMean, p95, windowMax) covers the most recent windowCount samples, at most 65536; for a
 * shorter session it is the same population as "All". Compare mean, p95 and max only within one population.
 */
struct Stats {
    size_t count = 0;
    double total = 0;
    double max = 0;
    double first = 0;
    double last = 0;
    size_t windowCount = 0;
    double windowMean = 0;
    double windowMax = 0;
    double p95 = 0;  ///< of the window
    double mean() const { return count ? total / static_cast<double>(count) : 0.0; }
};

bool enabled();
void setEnabled(bool on);

void record(Metric metric, double value);
Stats stats(Metric metric);
void reset();
const char* name(Metric metric);

/// Multi-line, human readable summary of every metric with samples
std::string report();

/// Reads the process counters (memory, GDI/USER handles) into their metrics. No-op off Windows.
void sampleProcess();

/**
 * Starts (once) a main-loop heartbeat. Every 20 ms it measures how late it ran and records UiStall for lateness above
 * 8 ms, samples the process every 5 s and writes report() every 30 s. Must be called from the GTK main thread.
 */
void startHeartbeat();

class ScopedTimer {
public:
    explicit ScopedTimer(Metric metric): metric(metric), active(enabled()) {
        if (active) {
            start = std::chrono::steady_clock::now();
        }
    }
    ~ScopedTimer() {
        if (active) {
            record(metric, std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count());
        }
    }
    ScopedTimer(const ScopedTimer&) = delete;
    ScopedTimer& operator=(const ScopedTimer&) = delete;

private:
    Metric metric;
    bool active;
    std::chrono::steady_clock::time_point start;
};

}  // namespace utn::perf
