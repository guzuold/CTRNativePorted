// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "server.hpp"
#include <mutex>
#include <condition_variable>
namespace ctr_online {
// Shared between lobby workers and the one election worker. Network I/O never
// runs under this mutex. A demand wakes the worker; publish wakes all waiters.
class HubGate {
    mutable std::mutex mutex;
    std::condition_variable changed;
    std::optional<MatchHub> hub;
    bool stopped = false, requested = false;
    std::chrono::steady_clock::time_point expires = std::chrono::steady_clock::time_point::max();

public:
    void reset(std::optional<MatchHub> initial) {
        std::lock_guard lock(mutex);
        hub = std::move(initial);
        expires = std::chrono::steady_clock::time_point::max();
        stopped = false;
        requested = false;
    }
    std::optional<MatchHub> current() const {
        std::lock_guard lock(mutex);
        return !stopped && std::chrono::steady_clock::now() < expires ? hub : std::nullopt;
    }
    void publish(std::optional<MatchHub> value, std::chrono::seconds ttl = std::chrono::seconds(300)) {
        {
            std::lock_guard lock(mutex);
            if (stopped) return;
            hub = std::move(value);
            expires = std::chrono::steady_clock::now() + ttl;
        }
        changed.notify_all();
    }
    std::optional<MatchHub> acquire(std::chrono::milliseconds budget) {
        std::unique_lock lock(mutex);
        if (std::chrono::steady_clock::now() >= expires) hub.reset();
        if (stopped) return std::nullopt;
        if (hub) return hub;
        requested = true;
        changed.notify_all();
        changed.wait_for(lock, budget, [&] { return stopped || (hub && std::chrono::steady_clock::now() < expires); });
        return !stopped && std::chrono::steady_clock::now() < expires ? hub : std::nullopt;
    }
    bool wait_until(std::chrono::steady_clock::time_point deadline) {
        std::unique_lock lock(mutex);
        changed.wait_until(lock, deadline, [&] { return stopped || requested; });
        requested = false;
        return !stopped;
    }
    bool stopping() const {
        std::lock_guard lock(mutex);
        return stopped;
    }
    void stop() {
        {
            std::lock_guard lock(mutex);
            stopped = true;
            hub.reset();
        }
        changed.notify_all();
    }
};
}
