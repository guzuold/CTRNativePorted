// SPDX-License-Identifier: GPL-2.0-only
#pragma once
#include <map>
#include <mutex>
#include <tuple>
#include <cstdio>
namespace ctrport {
// Own bounded synchronization observer. It never grants tokens or changes scheduling.
class SemaphoreCallTrace {
    std::mutex mutex_;
    std::map<std::tuple<uint32_t, uint64_t, uint32_t, int, int>, uint64_t> seen_;

public:
    template <class Sink>
    bool observe(uint32_t id, uint64_t caller, uint32_t thread, int operation, int result, Sink sink) {
        std::lock_guard<std::mutex> guard(mutex_);
        const auto key = std::make_tuple(id, caller, thread, operation, result);
        const auto found = seen_.find(key);
        if (found != seen_.end()) {
            if (found->second >= 16) return false;
            ++found->second;
            sink();
            return true;
        }
        if (seen_.size() >= 4096) return false;
        seen_.emplace(key, 1);
        sink();
        return true;
    }
    size_t size() {
        std::lock_guard<std::mutex> guard(mutex_);
        return seen_.size();
    }
};
inline SemaphoreCallTrace semaphore_call_trace;
inline void trace_semaphore_call(uint32_t id, uint64_t caller, int operation, int result, int amount) {
    char enabled[4]{};
    if (GetEnvironmentVariableA("CTR_SYNC_TRACE", enabled, sizeof(enabled)) != 1 || enabled[0] != '1') return;
    const auto thread = GetCurrentThreadId();
    semaphore_call_trace.observe(id, caller, thread, operation, result, [&] {
        std::printf(
            "{\"scope\":\"original_Semaphore_call\",\"id\":%u,\"caller\":%llu,\"thread\":%lu,\"operation\":%d,\"result\":%d,\"amount\":%d,\"scheduler_modified\":false}\n",
            id, static_cast<unsigned long long>(caller), thread, operation, result, amount);
        std::fflush(stdout);
    });
}
}
