// SPDX-License-Identifier: GPL-2.0-only
// Owned atomic tokens and FIFO/priority host waits. Public reference: shadPS4
// threads/semaphore.cpp316-329, e3ce810f3a653f43ac64ebab63023de281a4103a
// GPL-2.0-or-later. Own implementation; delete/cancel remain missing.
#pragma once
#include "settings/hot_trace.hpp"
#include "kernel_time.hpp"
#include "thread_identity.hpp"
#include <map>
#include <memory>
#include <condition_variable>
#include <list>
namespace ctrport {
struct KernelSemaphore {
    unsigned attribute = 0;
    int maximum = 0, tokens = 0;
    struct Waiter {
        int need;
        int priority;
        bool granted = false;
        std::condition_variable changed;
    };
    std::mutex operation;
    std::list<Waiter*> waiters;
};
class KernelSemaphores {
    std::mutex lock;
    std::uint32_t next = 1;
    std::map<std::uint32_t, std::shared_ptr<KernelSemaphore>> owned;

public:
    int create(std::uint32_t* output, const char* name, unsigned attr, int initial, int maximum, const void* options) {
        if (options) throw std::runtime_error("Semaphore options unsupported");
        if (!output || !name || attr > 2 || initial < 0 || maximum <= 0 || initial > maximum)
            return static_cast<int>(0x80020016U);
        require_time_output(output, 4);
        bool terminated = false;
        for (unsigned i = 0; i < 32; ++i) {
            char ch = 0;
            SIZE_T read = 0;
            if (!ReadProcessMemory(GetCurrentProcess(), name + i, &ch, 1, &read) || read != 1)
                throw std::runtime_error("Semaphore name unreadable");
            if (!ch) {
                terminated = true;
                break;
            }
        }
        if (!terminated) throw std::runtime_error("Semaphore name exceeds supported32bytes");
        auto object = std::make_shared<KernelSemaphore>();
        object->attribute = attr;
        object->maximum = maximum;
        object->tokens = initial;
        std::lock_guard<std::mutex> guard(lock);
        if (!next) throw std::runtime_error("Semaphore handle space exhausted");
        const auto id = next++;
        owned.emplace(id, object);
        *output = id;
        return 0;
    }
    int wait(std::uint32_t id, int need, std::uint32_t* timeout) {
        auto object = find(id);
        if (!object) return static_cast<int>(0x80020003U);
        if (need <= 0 || need > object->maximum) return static_cast<int>(0x80020016U);
        if (timeout) require_time_output(timeout, 4);
        std::unique_lock<std::mutex> guard(object->operation);
        if (object->tokens >= need) {
            object->tokens -= need;
            return 0;
        }
        if (timeout && !*timeout) return static_cast<int>(0x8002003cU);
        if (ctrport::settings::hot_trace()) {
            std::fprintf(stderr, "semaphore blocking id=%u need=%d attr=%u priority=%d thread=%lu\n", id, need,
                         object->attribute, effective_guest_priority(), GetCurrentThreadId());
            std::fflush(stderr);
        }
        KernelSemaphore::Waiter waiter{need, effective_guest_priority()};
        auto position = object->waiters.end();
        if (object->attribute != 1) {
            position = object->waiters.begin();
            while (position != object->waiters.end() && (*position)->priority <= waiter.priority) ++position;
        }
        object->waiters.insert(position, &waiter);
        const auto remove = [&] { object->waiters.remove(&waiter); };
        try {
            if (!timeout) {
                waiter.changed.wait(guard, [&] { return waiter.granted; });
                return 0;
            }
            const auto budget = std::chrono::microseconds(*timeout),
                       start = std::chrono::duration_cast<std::chrono::microseconds>(
                           std::chrono::steady_clock::now().time_since_epoch());
            const bool granted = waiter.changed.wait_for(guard, budget, [&] { return waiter.granted; });
            const auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(
                                     std::chrono::steady_clock::now().time_since_epoch()) -
                                 start;
            *timeout = granted && elapsed < budget ? static_cast<std::uint32_t>((budget - elapsed).count()) : 0;
            if (!granted) {
                remove();
                return static_cast<int>(0x8002003cU);
            }
            return 0;
        } catch (...) {
            remove();
            throw;
        }
    }
    int poll(std::uint32_t id, int need) {
        auto object = find(id);
        if (!object) return static_cast<int>(0x80020003U);
        if (need <= 0 || need > object->maximum) return static_cast<int>(0x80020016U);
        std::lock_guard<std::mutex> guard(object->operation);
        if (object->tokens < need) return static_cast<int>(0x80020010U);
        object->tokens -= need;
        return 0;
    }
    int signal(std::uint32_t id, int count) {
        auto object = find(id);
        if (!object) return static_cast<int>(0x80020003U);
        if (count <= 0) return static_cast<int>(0x80020016U);
        std::lock_guard<std::mutex> guard(object->operation);
        if (count > object->maximum - object->tokens) return static_cast<int>(0x80020016U);
        object->tokens += count;
        for (auto it = object->waiters.begin(); it != object->waiters.end();) {
            auto* waiter = *it;
            if (waiter->need > object->tokens) {
                ++it;
                continue;
            }
            object->tokens -= waiter->need;
            waiter->granted = true;
            it = object->waiters.erase(it);
            waiter->changed.notify_one();
        }
        return 0;
    }
    std::shared_ptr<KernelSemaphore> find(std::uint32_t id) {
        std::lock_guard<std::mutex> guard(lock);
        const auto at = owned.find(id);
        return at == owned.end() ? nullptr : at->second;
    }
};
inline KernelSemaphores kernel_semaphores;
}
