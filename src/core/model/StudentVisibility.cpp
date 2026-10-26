/*
 * Ultimate Teacher Notepad
 *
 * Based on Xournal++ GPLv2+
 */

#include "StudentVisibility.h"

#include <mutex>
#include <utility>
#include <vector>

namespace utn {
namespace {
std::recursive_mutex listenersMutex;
size_t nextId = 1;
std::vector<std::pair<size_t, StudentVisibilityListener>> listeners;
}  // namespace

size_t addStudentVisibilityListener(StudentVisibilityListener listener) {
    std::lock_guard lock(listenersMutex);
    listeners.emplace_back(nextId, std::move(listener));
    return nextId++;
}

void removeStudentVisibilityListener(size_t id) {
    std::lock_guard lock(listenersMutex);
    for (auto it = listeners.begin(); it != listeners.end(); ++it) {
        if (it->first == id) {
            listeners.erase(it);
            return;
        }
    }
}

void notifyStudentVisibilityChanged(bool mayCheckNow) {
    std::lock_guard lock(listenersMutex);
    for (const auto& entry: listeners) {
        entry.second(mayCheckNow);
    }
}

}  // namespace utn
