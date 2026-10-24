#include "model/DocumentLockAudit.h"
#ifdef UTN_LOCK_AUDIT
#include <atomic>
#include <condition_variable>
#include <mutex>
#include <thread>
#include <unordered_map>
#include <vector>

#include <glib.h>

#include "util/Stacktrace.h"
namespace utn::lockaudit {
namespace {
constexpr auto SLOW_WAIT = std::chrono::milliseconds(50);
constexpr unsigned MAX_REPORTS = 25;
struct Depth {
    unsigned shared = 0;
    unsigned exclusive = 0;
};
thread_local std::unordered_map<const void*, Depth> depths;
std::atomic<unsigned> nested{0};
std::atomic<unsigned> slow{0};
std::atomic<long long> longestWaitUs{0};
void recordWait(Clock::duration elapsed) {
    const auto us = std::chrono::duration_cast<std::chrono::microseconds>(elapsed).count();
    long long previous = longestWaitUs.load();
    while (us > previous && !longestWaitUs.compare_exchange_weak(previous, us)) {}
}
struct Pending {
    const void* mutex;
    bool exclusive;
    Clock::time_point start;
    std::thread::id thread;
    bool reported = false;
};
// Never acquires a Document mutex; never holds the registry lock while logging.
class Watchdog {
public:
    Watchdog(): worker([this] { run(); }) {}
    ~Watchdog() {
        {
            std::lock_guard guard(mutex);
            stopping = true;
        }
        wake.notify_one();
        worker.join();
    }
    uint64_t add(const void* identity, bool exclusive, Clock::time_point start) {
        std::lock_guard guard(mutex);
        const auto token = ++nextToken;
        pending.emplace(token, Pending{identity, exclusive, start, std::this_thread::get_id()});
        wake.notify_one();
        return token;
    }
    bool remove(uint64_t token) {
        std::lock_guard guard(mutex);
        const auto it = pending.find(token);
        if (it == pending.end())
            return false;
        const bool reported = it->second.reported;
        pending.erase(it);
        return reported;
    }

private:
    void run() {
        std::unique_lock guard(mutex);
        while (!stopping) {
            if (pending.empty())
                wake.wait(guard, [this] { return stopping || !pending.empty(); });
            else
                wake.wait_for(guard, std::chrono::milliseconds(10), [this] { return stopping; });
            if (stopping)
                break;
            std::vector<Pending> reports;
            const auto now = Clock::now();
            for (auto& [token, wait]: pending) {
                recordWait(now - wait.start);
                if (!wait.reported && now - wait.start >= SLOW_WAIT) {
                    wait.reported = true;
                    if (++slow <= MAX_REPORTS)
                        reports.push_back(wait);
                }
            }
            guard.unlock();
            for (const auto& wait: reports) {
                g_warning("UTN lock audit: still waiting for %s document mutex %p on thread %zu (%.1f ms); "
                          "watchdog observation, acquisition may still be blocked",
                          wait.exclusive ? "exclusive" : "shared", wait.mutex,
                          std::hash<std::thread::id>{}(wait.thread),
                          std::chrono::duration<double, std::milli>(now - wait.start).count());
            }
            guard.lock();
        }
    }
    std::mutex mutex;
    std::condition_variable wake;
    std::unordered_map<uint64_t, Pending> pending;
    uint64_t nextToken = 0;
    bool stopping = false;
    std::thread worker;
};
Watchdog& watchdog() {
    static Watchdog instance;
    return instance;
}
}  // namespace
unsigned nestedViolations() { return nested.load(); }
unsigned slowAcquisitions() { return slow.load(); }
double longestWaitMs() { return static_cast<double>(longestWaitUs.load()) / 1000.0; }
Attempt::Attempt(const void* mutex, bool exclusive, bool blocking):
        mutex(mutex), exclusive(exclusive), start(Clock::now()) {
    const auto held = depths.find(mutex);
    if (blocking && held != depths.end() && (held->second.shared || held->second.exclusive)) {
        const unsigned count = ++nested;
        if (count <= MAX_REPORTS) {
            g_warning("UTN lock audit: nested %s acquisition of document mutex %p on thread %zu (#%u)",
                      exclusive ? "exclusive" : "shared", mutex,
                      std::hash<std::thread::id>{}(std::this_thread::get_id()), count);
            Stacktrace::printStacktrace();
        }
    }
    if (blocking)
        pending = watchdog().add(mutex, exclusive, start);
}
Attempt::~Attempt() {
    // Cancels on success or if the native lock operation throws.
    if (pending)
        watchdog().remove(pending);
}
void acquired(Attempt& attempt) {
    const bool reported = attempt.pending && watchdog().remove(attempt.pending);
    attempt.pending = 0;
    const auto waited = Clock::now() - attempt.start;
    recordWait(waited);
    if (!reported && waited >= SLOW_WAIT && ++slow <= MAX_REPORTS) {
        g_warning("UTN lock audit: acquired %s document mutex %p after %.1f ms on thread %zu",
                  attempt.exclusive ? "exclusive" : "shared", attempt.mutex,
                  std::chrono::duration<double, std::milli>(waited).count(),
                  std::hash<std::thread::id>{}(std::this_thread::get_id()));
        Stacktrace::printStacktrace();
    }
    auto& depth = depths[attempt.mutex];
    ++(attempt.exclusive ? depth.exclusive : depth.shared);
}
void released(const void* mutex, bool exclusive) {
    const auto it = depths.find(mutex);
    if (it == depths.end())
        return;
    auto& depth = exclusive ? it->second.exclusive : it->second.shared;
    if (depth)
        --depth;
    if (!it->second.shared && !it->second.exclusive)
        depths.erase(it);
}
}  // namespace utn::lockaudit
#endif
