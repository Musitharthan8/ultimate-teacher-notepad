/* Ultimate Teacher Notepad - optional mutex diagnostics. Based on Xournal++ GPLv2+ */
#pragma once
#ifdef UTN_LOCK_AUDIT
#include <chrono>
#include <cstdint>
namespace utn::lockaudit {
using Clock = std::chrono::steady_clock;
unsigned nestedViolations();
// Includes pending overdue waits, counted once per attempt.
unsigned slowAcquisitions();
// Process-wide maximum across threads, including pending waits.
double longestWaitMs();
class Attempt {
public:
    Attempt(const void* mutex, bool exclusive, bool blocking);
    ~Attempt();
    Attempt(const Attempt&) = delete;
    Attempt& operator=(const Attempt&) = delete;

private:
    friend void acquired(Attempt&);
    const void* mutex;
    bool exclusive;
    Clock::time_point start;
    uint64_t pending = 0;
};
inline Attempt begin(const void* mutex, bool exclusive, bool blocking) { return Attempt(mutex, exclusive, blocking); }
void acquired(Attempt& attempt);
void released(const void* mutex, bool exclusive);
}  // namespace utn::lockaudit
#else
namespace utn::lockaudit {
struct Attempt {};
inline Attempt begin(const void*, bool, bool) { return {}; }
inline void acquired(Attempt&) {}
inline void released(const void*, bool) {}
}  // namespace utn::lockaudit
#endif
