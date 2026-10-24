#include "model/DocumentLockAudit.h"

#ifdef UTN_LOCK_AUDIT
#include <atomic>
#include <chrono>
#include <thread>

#include <gtest/gtest.h>

#include "model/Document.h"

using namespace std::chrono_literals;

TEST(DocumentLockAudit, ReportsWaitBeforeTheNativeAcquisitionReturns) {
    Document doc(nullptr);
    doc.lock();
    const unsigned before = utn::lockaudit::slowAcquisitions();
    std::atomic<bool> entered{false};
    std::atomic<bool> acquired{false};
    std::thread waiter([&] {
        entered = true;
        doc.lock_shared();
        acquired = true;
        doc.unlock_shared();
    });
    const auto deadline = std::chrono::steady_clock::now() + 2s;
    while ((!entered || utn::lockaudit::slowAcquisitions() == before) && std::chrono::steady_clock::now() < deadline) {
        std::this_thread::sleep_for(10ms);
    }
    EXPECT_FALSE(acquired);
    EXPECT_GT(utn::lockaudit::slowAcquisitions(), before) << "must warn while the waiter is still blocked";
    doc.unlock();
    waiter.join();
    EXPECT_TRUE(acquired);
}

TEST(DocumentLockAudit, HoldingTwoDifferentDocumentsIsNotRecursion) {
    Document first(nullptr);
    Document second(nullptr);
    const unsigned before = utn::lockaudit::nestedViolations();
    first.lock_shared();
    second.lock();
    second.unlock();
    first.unlock_shared();
    EXPECT_EQ(utn::lockaudit::nestedViolations(), before);
}
#endif
