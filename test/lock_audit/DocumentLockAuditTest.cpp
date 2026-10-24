#include <atomic>
#include <chrono>
#include <mutex>
#include <shared_mutex>
#include <string>
#include <thread>
#include <vector>

#include <glib.h>
#include <gtest/gtest.h>

#include "model/DocumentLockAudit.h"

using namespace std::chrono_literals;
namespace audit = utn::lockaudit;

namespace {
class AuditWarnings {
public:
    AuditWarnings(): handler(g_log_set_handler("xopp", G_LOG_LEVEL_WARNING, +[](const gchar*, GLogLevelFlags,
                                                                            const gchar* message, gpointer data) {
        auto* self = static_cast<AuditWarnings*>(data);
        std::lock_guard guard(self->mutex);
        self->messages.emplace_back(message);
    }, this)) {}
    ~AuditWarnings() { g_log_remove_handler("xopp", handler); }
    std::vector<std::string> received() {
        std::lock_guard guard(mutex);
        return messages;
    }
private:
    std::mutex mutex;
    std::vector<std::string> messages;
    guint handler;
};
}

TEST(LockAuditDiagnostics, ReportsBlockedWaitBeforeAcquisitionAndCountsItOnlyOnce) {
    std::shared_mutex mutex;
    mutex.lock();
    AuditWarnings warnings;
    const unsigned before = audit::slowAcquisitions();
    std::atomic<bool> acquired{false};
    std::thread waiter([&] {
        auto attempt = audit::begin(&mutex, false, true);
        mutex.lock_shared();
        audit::acquired(attempt);
        acquired = true;
        audit::released(&mutex, false);
        mutex.unlock_shared();
    });
    const auto deadline = audit::Clock::now() + 2s;
    while (warnings.received().empty() && audit::Clock::now() < deadline) {
        std::this_thread::sleep_for(10ms);
    }
    EXPECT_FALSE(acquired);
    EXPECT_EQ(audit::slowAcquisitions(), before + 1) << "must report while the native lock is still blocked";
    const auto pendingWarnings = warnings.received();
    EXPECT_EQ(pendingWarnings.size(), 1U) << "must emit the warning before acquisition returns";
    if (!pendingWarnings.empty()) {
        gchar* identity = g_strdup_printf("%p", static_cast<void*>(&mutex));
        EXPECT_NE(pendingWarnings.front().find(identity), std::string::npos) << "identify the blocked mutex";
        g_free(identity);
        EXPECT_NE(pendingWarnings.front().find("shared"), std::string::npos);
        EXPECT_NE(pendingWarnings.front().find("thread"), std::string::npos);
    }
    EXPECT_GE(audit::longestWaitMs(), 50.0);
    mutex.unlock();
    waiter.join();
    EXPECT_TRUE(acquired);
    EXPECT_EQ(audit::slowAcquisitions(), before + 1) << "completion must not count the pending warning twice";
    EXPECT_EQ(warnings.received().size(), 1U);
}

TEST(LockAuditDiagnostics, DistinctMutexesOnOneThreadAreNotRecursion) {
    std::shared_mutex first;
    std::shared_mutex second;
    const unsigned before = audit::nestedViolations();
    auto firstAttempt = audit::begin(&first, false, true);
    first.lock_shared();
    audit::acquired(firstAttempt);
    auto secondAttempt = audit::begin(&second, true, true);
    second.lock();
    audit::acquired(secondAttempt);
    audit::released(&second, true);
    second.unlock();
    audit::released(&first, false);
    first.unlock_shared();
    EXPECT_EQ(audit::nestedViolations(), before);
}

TEST(LockAuditDiagnostics, ReportsSameMutexRecursionBeforeCallingTheNativeLock) {
    std::shared_mutex mutex;
    auto held = audit::begin(&mutex, false, true);
    mutex.lock_shared();
    audit::acquired(held);
    const unsigned before = audit::nestedViolations();
    {
        // Diagnose the request without entering undefined native recursive-lock behavior.
        auto recursiveShared = audit::begin(&mutex, false, true);
    }
    {
        auto recursiveExclusive = audit::begin(&mutex, true, true);
    }
    EXPECT_EQ(audit::nestedViolations(), before + 2);
    audit::released(&mutex, false);
    mutex.unlock_shared();
}

TEST(LockAuditDiagnostics, CancelledAcquisitionDoesNotLeaveAPendingWaitOrHeldDepth) {
    std::shared_mutex mutex;
    const unsigned slowBefore = audit::slowAcquisitions();
    const unsigned nestedBefore = audit::nestedViolations();
    {
        // Native lock() can throw; unwinding the attempt must cancel its registration.
        auto cancelled = audit::begin(&mutex, true, true);
    }
    std::this_thread::sleep_for(100ms);
    EXPECT_EQ(audit::slowAcquisitions(), slowBefore);
    auto next = audit::begin(&mutex, true, true);
    mutex.lock();
    audit::acquired(next);
    audit::released(&mutex, true);
    mutex.unlock();
    EXPECT_EQ(audit::nestedViolations(), nestedBefore);
}

TEST(LockAuditDiagnostics, FailedTryDoesNotRegisterAWaitOrMarkTheMutexHeld) {
    std::shared_mutex mutex;
    mutex.lock();
    const unsigned slowBefore = audit::slowAcquisitions();
    const unsigned nestedBefore = audit::nestedViolations();
    std::atomic<bool> tried{false};
    std::atomic<bool> released{false};
    std::thread waiter([&] {
        {
            auto failed = audit::begin(&mutex, false, false);
            const bool locked = mutex.try_lock_shared();
            EXPECT_FALSE(locked);
            if (locked) {
                audit::acquired(failed);
                audit::released(&mutex, false);
                mutex.unlock_shared();
            }
            // A failed nonblocking attempt must not become a watchdog warning.
            std::this_thread::sleep_for(100ms);
        }
        tried = true;
        while (!released) std::this_thread::yield();
        auto next = audit::begin(&mutex, false, true);
        mutex.lock_shared();
        audit::acquired(next);
        audit::released(&mutex, false);
        mutex.unlock_shared();
    });
    while (!tried) std::this_thread::yield();
    EXPECT_EQ(audit::slowAcquisitions(), slowBefore);
    mutex.unlock();
    released = true;
    waiter.join();
    EXPECT_EQ(audit::nestedViolations(), nestedBefore);
}

TEST(LockAuditDiagnostics, SuccessfulTryRecordsAndReleasesHeldDepth) {
    std::shared_mutex mutex;
    const unsigned before = audit::nestedViolations();
    {
        auto attempt = audit::begin(&mutex, true, false);
        ASSERT_TRUE(mutex.try_lock());
        audit::acquired(attempt);
        { auto recursive = audit::begin(&mutex, false, true); }
        EXPECT_EQ(audit::nestedViolations(), before + 1);
        audit::released(&mutex, true);
        mutex.unlock();
    }
    {
        auto next = audit::begin(&mutex, false, true);
        mutex.lock_shared();
        audit::acquired(next);
        audit::released(&mutex, false);
        mutex.unlock_shared();
    }
    EXPECT_EQ(audit::nestedViolations(), before + 1) << "released try-lock depth must not survive";
}
