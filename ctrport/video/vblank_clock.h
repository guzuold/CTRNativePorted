// SPDX-License-Identifier: GPL-2.0-only
#pragma once
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <mutex>
#include <thread>
namespace CtrVideo {
// Host clock control; runtime worker/event binding remains separate and missing.
class VblankClock {
    std::mutex mutex_;
    std::condition_variable cv_;
    bool stopping_ = false;
    std::uint64_t count_ = 0;
    using TickCallback = int (*)(void*);
    TickCallback callback_;
    void* context_;
    std::atomic<int> callbackError_{0};
    std::thread worker_;

public:
    VblankClock(TickCallback callback = nullptr, void* context = nullptr)
        : callback_(callback), context_(context), worker_([this] {
              using clock = std::chrono::steady_clock;
              const auto origin = clock::now();
              std::unique_lock lock(mutex_);
              std::uint64_t tick = 1;
              while (!stopping_) {
                  // Absolute rational deadlines avoid accumulating truncated 1/60s error.
                  const auto deadline = origin + std::chrono::nanoseconds(tick * 1000000000ULL / 60);
                  if (cv_.wait_until(lock, deadline, [this] { return stopping_; })) break;
                  ++tick;
                  // Callback may enter kernel queue locks; never hold the clock lock there.
                  lock.unlock();
                  const int error = callback_ ? callback_(context_) : 0;
                  lock.lock();
                  if (error) {
                      callbackError_.store(error);
                      stopping_ = true;
                  }
                  ++count_; // Publish attempted tick only after callback/event delivery completes.
                  cv_.notify_all();
              }
          }) {}
    VblankClock(const VblankClock&) = delete;
    VblankClock& operator=(const VblankClock&) = delete;
    void Stop() {
        {
            std::lock_guard lock(mutex_);
            stopping_ = true;
            cv_.notify_all();
        }
        if (worker_.joinable()) worker_.join();
    }
    ~VblankClock() { Stop(); }
    int CallbackError() const { return callbackError_.load(); }
    std::uint64_t Count() {
        std::lock_guard lock(mutex_);
        return count_;
    }
    // false on shutdown/timeout; no fabricated tick on wakeup.
    bool WaitAfter(std::uint64_t previous, std::chrono::milliseconds timeout) {
        std::unique_lock lock(mutex_);
        return cv_.wait_for(lock, timeout, [&] { return stopping_ || count_ > previous; }) && !stopping_ &&
               count_ > previous;
    }
};
}
