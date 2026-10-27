// SPDX-License-Identifier: GPL-2.0-only
// New implementation; reference: shadPS4 pthread_spec.cpp, pinned
// e3ce810f3a653f43ac64ebab63023de281a4103a (GPL-2.0-or-later).
#pragma once
#include "kernel_time.hpp"
#include <array>
#include <mutex>
#include <map>
#include <atomic>
namespace ctrport {
class ThreadKeys {
    std::array<std::uint64_t, 256> allocated{};
    inline static std::atomic<std::uint64_t> next_generation{1};
    inline static thread_local std::map<std::uint64_t, const void*> values;
    std::mutex lock;

public:
    bool owns(std::uint32_t key) {
        std::lock_guard<std::mutex> guard(lock);
        return key < allocated.size() && allocated[key] != 0;
    }
    int create(std::uint32_t* output, std::uint64_t destructor) {
        if (destructor) throw std::runtime_error("Thread key destructor lifecycle not implemented");
        if (!output) return static_cast<int>(0x80020016U);
        require_time_output(output, 4);
        std::lock_guard<std::mutex> guard(lock);
        for (std::uint32_t i = 0; i < allocated.size(); ++i)
            if (!allocated[i]) {
                allocated[i] = next_generation.fetch_add(1);
                *output = i;
                return 0;
            }
        return static_cast<int>(0x80020023U);
    }
    int erase(std::uint32_t key) {
        std::lock_guard<std::mutex> guard(lock);
        if (key >= allocated.size() || !allocated[key]) return static_cast<int>(0x80020016U);
        values.erase(allocated[key]);
        allocated[key] = 0;
        return 0;
    }
    int set(std::uint32_t key, const void* value) {
        std::lock_guard<std::mutex> guard(lock);
        if (key >= allocated.size() || !allocated[key]) return static_cast<int>(0x80020016U);
        if (value)
            values[allocated[key]] = value;
        else
            values.erase(allocated[key]);
        return 0;
    }
    const void* get(std::uint32_t key) {
        std::lock_guard<std::mutex> guard(lock);
        if (key >= allocated.size() || !allocated[key]) return nullptr;
        const auto at = values.find(allocated[key]);
        return at == values.end() ? nullptr : at->second;
    }
};
inline ThreadKeys thread_keys;
}
