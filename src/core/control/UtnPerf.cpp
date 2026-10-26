#include "UtnPerf.h"

#include <algorithm>
#include <array>
#include <atomic>
#include <cstdlib>
#include <mutex>
#include <sstream>
#include <vector>

#include <glib.h>

#ifdef _WIN32
#include <windows.h>
#endif

namespace utn::perf {
namespace {
constexpr size_t RING = 512;
constexpr size_t METRICS = static_cast<size_t>(Metric::COUNT);

struct Slot {
    std::mutex mutex;
    Stats stats;
    std::array<double, RING> ring{};
    size_t next = 0;
    size_t filled = 0;
};

std::array<Slot, METRICS> slots;
std::atomic<bool> on{false};
std::once_flag envOnce;

void readEnvironment() {
    if (const char* value = std::getenv("UTN_PERF"); value && *value && *value != '0') {
        on.store(true, std::memory_order_relaxed);
    }
}

double percentile95(Slot& slot) {
    if (slot.filled == 0) {
        return 0;
    }
    std::vector<double> copy(slot.ring.begin(), slot.ring.begin() + static_cast<std::ptrdiff_t>(slot.filled));
    const size_t index = (copy.size() - 1) * 95 / 100;
    std::nth_element(copy.begin(), copy.begin() + static_cast<std::ptrdiff_t>(index), copy.end());
    return copy[index];
}
}  // namespace

bool enabled() {
    std::call_once(envOnce, readEnvironment);
    return on.load(std::memory_order_relaxed);
}

void setEnabled(bool value) {
    std::call_once(envOnce, readEnvironment);
    on.store(value, std::memory_order_relaxed);
}

void record(Metric metric, double value) {
    if (!enabled()) {
        return;
    }
    Slot& slot = slots[static_cast<size_t>(metric)];
    std::lock_guard lock(slot.mutex);
    slot.stats.count++;
    slot.stats.total += value;
    slot.stats.max = std::max(slot.stats.max, value);
    slot.stats.last = value;
    slot.ring[slot.next] = value;
    slot.next = (slot.next + 1) % RING;
    slot.filled = std::min(slot.filled + 1, RING);
}

Stats stats(Metric metric) {
    Slot& slot = slots[static_cast<size_t>(metric)];
    std::lock_guard lock(slot.mutex);
    Stats copy = slot.stats;
    copy.p95 = percentile95(slot);
    return copy;
}

void reset() {
    for (Slot& slot: slots) {
        std::lock_guard lock(slot.mutex);
        slot.stats = {};
        slot.next = 0;
        slot.filled = 0;
    }
}

const char* name(Metric metric) {
    switch (metric) {
        case Metric::PenUp: return "pen_up_ms";
        case Metric::PenUpLockWait: return "pen_up_lock_wait_ms";
        case Metric::DocLockWaitExclusive: return "doc_lock_wait_exclusive_ms";
        case Metric::DocLockWaitShared: return "doc_lock_wait_shared_ms";
        case Metric::RenderJob: return "render_job_ms";
        case Metric::Autosave: return "autosave_ms";
        case Metric::AutosaveLockHold: return "autosave_lock_hold_ms";
        case Metric::StudentFrame: return "student_frame_ms";
        case Metric::StudentRedrawRequests: return "student_redraw_requests";
        case Metric::StudentRedrawUrgent: return "student_redraw_urgent";
        case Metric::EraseEvent: return "erase_event_ms";
        case Metric::ErasuresPerStroke: return "erasures_per_stroke";
        case Metric::Recognizer: return "recognizer_ms";
        case Metric::UiStall: return "ui_stall_ms";
        case Metric::ProcessPrivateMiB: return "process_private_mib";
        case Metric::GdiObjects: return "gdi_objects";
        case Metric::UserObjects: return "user_objects";
        case Metric::ProcessHandles: return "process_handles";
        case Metric::COUNT: break;
    }
    return "?";
}

std::string report() {
    std::ostringstream out;
    out << "UTN perf summary\n";
    for (size_t i = 0; i < METRICS; ++i) {
        const auto metric = static_cast<Metric>(i);
        const Stats s = stats(metric);
        if (s.count == 0) {
            continue;
        }
        out << "  " << name(metric) << ": n=" << s.count << " mean=" << s.mean() << " p95=" << s.p95 << " max=" << s.max
            << " last=" << s.last << '\n';
    }
    return out.str();
}

void sampleProcess() {
#ifdef _WIN32
    if (!enabled()) {
        return;
    }
    // K32GetProcessMemoryInfo lives in kernel32 since Windows 7; resolve it at run time so no extra import is needed
    struct Counters {
        DWORD cb;
        DWORD PageFaultCount;
        SIZE_T PeakWorkingSetSize;
        SIZE_T WorkingSetSize;
        SIZE_T QuotaPeakPagedPoolUsage;
        SIZE_T QuotaPagedPoolUsage;
        SIZE_T QuotaPeakNonPagedPoolUsage;
        SIZE_T QuotaNonPagedPoolUsage;
        SIZE_T PagefileUsage;
        SIZE_T PeakPagefileUsage;
        SIZE_T PrivateUsage;
    };
    using Fn = BOOL(WINAPI*)(HANDLE, Counters*, DWORD);
    static const auto fn = reinterpret_cast<Fn>(
            reinterpret_cast<void*>(GetProcAddress(GetModuleHandleA("kernel32.dll"), "K32GetProcessMemoryInfo")));
    HANDLE process = GetCurrentProcess();
    if (fn) {
        Counters c{};
        c.cb = sizeof(c);
        if (fn(process, &c, sizeof(c))) {
            record(Metric::ProcessPrivateMiB, static_cast<double>(c.PrivateUsage) / (1024.0 * 1024.0));
        }
    }
    record(Metric::GdiObjects, static_cast<double>(GetGuiResources(process, GR_GDIOBJECTS)));
    record(Metric::UserObjects, static_cast<double>(GetGuiResources(process, GR_USEROBJECTS)));
    DWORD handles = 0;
    if (GetProcessHandleCount(process, &handles)) {
        record(Metric::ProcessHandles, static_cast<double>(handles));
    }
#endif
}

namespace {
constexpr guint HEARTBEAT_MS = 20;
std::atomic<bool> heartbeatRunning{false};
gint64 lastBeat = 0;
gint64 lastSample = 0;
gint64 lastReport = 0;

gboolean beat(gpointer) {
    const gint64 now = g_get_monotonic_time();
    if (lastBeat != 0) {
        const double late = static_cast<double>(now - lastBeat) / 1000.0 - HEARTBEAT_MS;
        if (late > 8.0) {
            record(Metric::UiStall, late);
        }
    }
    lastBeat = now;
    if (now - lastSample > 5 * G_USEC_PER_SEC) {
        lastSample = now;
        sampleProcess();
    }
    if (now - lastReport > 30 * G_USEC_PER_SEC) {
        lastReport = now;
        g_message("%s", report().c_str());
    }
    return G_SOURCE_CONTINUE;
}
}  // namespace

void startHeartbeat() {
    if (!enabled() || heartbeatRunning.exchange(true)) {
        return;
    }
    lastBeat = lastSample = lastReport = g_get_monotonic_time();
    g_timeout_add(HEARTBEAT_MS, beat, nullptr);
}

}  // namespace utn::perf
