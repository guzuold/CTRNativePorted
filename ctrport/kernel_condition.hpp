// SPDX-License-Identifier: GPL-2.0-only
// New implementation; behavioral reference and license recorded in
// kernel_condition_original_contract.json (pinned shadPS4 condvar.cpp).
#pragma once
#include "settings/hot_trace.hpp"
#include "kernel_mutex.hpp"
#include "cancellation_control.hpp"
#include <atomic>
#include <set>
namespace ctrport {
inline std::uint64_t condition_observation_base = 0;
inline const char* (*condition_observation_image)(std::uint64_t, std::uint64_t&) = nullptr;
inline std::atomic<unsigned> condition_observation_count{0};
inline void observe_condition(const char* phase, std::uint64_t caller, const void* condition, const void* mutex) {
    if (!condition_observation_base) return;
    const auto sequence = condition_observation_count.fetch_add(1);
    if (sequence >= 128) return;
    std::uint64_t offset = 0;
    const char* sha = condition_observation_image ? condition_observation_image(caller, offset) : "unmapped";
    std::fprintf(
        stderr,
        "{\"scope\":\"condition_observation\",\"sequence\":%u,\"phase\":\"%s\",\"caller_image_sha256\":\"%s\",\"caller_offset\":%llu,\"condition\":%llu,\"mutex\":%llu,\"thread\":%lu}\n",
        sequence, phase, sha, static_cast<unsigned long long>(offset),
        static_cast<unsigned long long>(reinterpret_cast<std::uint64_t>(condition)),
        static_cast<unsigned long long>(reinterpret_cast<std::uint64_t>(mutex)), GetCurrentThreadId());
    std::fflush(stderr);
}
// Logarithmic sampling bounds output to 64 events per operation in a run.
inline std::atomic<std::uint64_t> mutex_lock_observations{0}, mutex_unlock_observations{0};
inline void observe_mutex_call(bool unlock, std::uint64_t caller, const void* cell) {
    if (!condition_observation_image) return;
    const auto count = (unlock ? mutex_unlock_observations : mutex_lock_observations).fetch_add(1) + 1;
    if (!count || (count & (count - 1))) return;
    std::uint64_t offset = 0;
    const char* sha = condition_observation_image(caller, offset);
    std::fprintf(
        stderr,
        "{\"scope\":\"mutex_call_sample\",\"operation\":\"%s\",\"count\":%llu,\"caller_image_sha256\":\"%s\",\"caller_offset\":%llu,\"cell\":%llu,\"thread\":%lu}\n",
        unlock ? "unlock" : "lock", static_cast<unsigned long long>(count), sha,
        static_cast<unsigned long long>(offset), static_cast<unsigned long long>(reinterpret_cast<std::uint64_t>(cell)),
        GetCurrentThreadId());
    std::fflush(stderr);
}
inline std::atomic<unsigned> condition_frame_observations{0};
inline void observe_condition_frames(std::uint64_t frame, bool waiting = false, const char* scope = nullptr) {
    if (!condition_observation_image || (!waiting && condition_frame_observations.fetch_add(1) >= 64)) return;
    for (unsigned depth = 0; depth < 8; ++depth) {
        if (!frame || (frame & 7)) break;
        std::uint64_t pair[2]{};
        SIZE_T read = 0;
        if (!ReadProcessMemory(GetCurrentProcess(), reinterpret_cast<void*>(frame), pair, sizeof(pair), &read) ||
            read != sizeof(pair))
            break;
        std::uint64_t offset = 0;
        const auto sha = condition_observation_image(pair[1], offset);
        std::fprintf(stderr,
                     "{\"scope\":\"%s\",\"thread\":%lu,\"depth\":%u,\"image_sha256\":\"%s\",\"return_offset\":%llu}\n",
                     scope ? scope : (waiting ? "condition_wait_original_frame" : "condition_original_frame"),
                     GetCurrentThreadId(), depth, sha, static_cast<unsigned long long>(offset));
        if (pair[0] <= frame || pair[0] - frame > 16 * 1024 * 1024) break;
        frame = pair[0];
    }
    std::fflush(stderr);
}
struct KernelCondition {
    std::string name; // Diagnostic label only; never a shared-object lookup key.
    std::mutex state;
    std::condition_variable changed;
    std::uint64_t generation = 0;
    unsigned waiters = 0;
};
inline std::mutex condition_registry_lock;
inline std::unordered_map<std::uint64_t, std::shared_ptr<KernelCondition>> condition_registry;
inline std::shared_ptr<KernelCondition> lookup_condition(std::uint64_t* cell) {
    if (!cell) throw std::runtime_error("Condition cell null unsupported");
    require_time_output(cell, 8);
    std::lock_guard<std::mutex> guard(condition_registry_lock);
    if (*cell == 0) {
        auto object = std::make_shared<KernelCondition>();
        const auto handle = reinterpret_cast<std::uint64_t>(object.get());
        condition_registry.emplace(handle, object);
        *cell = handle;
    }
    const auto found = condition_registry.find(*cell);
    if (found == condition_registry.end()) throw std::runtime_error("Unknown/destroyed condition handle unsupported");
    return found->second;
}
// Lifecycle reference: pinned condvar.cpp84-95, GPL-2.0-or-later,
// e3ce810f3a653f43ac64ebab63023de281a4103a. Never delete a live waiter.
inline int kernel_condition_destroy(std::uint64_t* cell) {
    if (!cell) return static_cast<int>(0x80020016U);
    require_time_output(cell, 8);
    std::lock_guard<std::mutex> guard(condition_registry_lock);
    if (*cell == 0) return 0;
    const auto found = condition_registry.find(*cell);
    if (found == condition_registry.end()) return static_cast<int>(0x80020016U);
    if (found->second.use_count() != 1) return static_cast<int>(0x80020010U);
    {
        std::lock_guard<std::mutex> state(found->second->state);
        if (found->second->waiters) return static_cast<int>(0x80020010U);
    }
    condition_registry.erase(found);
    *cell = UINT64_MAX;
    return 0;
}
extern "C" int ctr_condition_destroy(std::uint64_t* cell, std::uint64_t caller) {
    try {
        const int result = kernel_condition_destroy(cell);
        std::printf("{\"scope\":\"original_CondDestroy_return\",\"caller\":%llu,\"result\":%d}\n",
                    static_cast<unsigned long long>(caller), result);
        std::fflush(stdout);
        return result;
    } catch (const std::exception& e) {
        std::fprintf(stderr, "NID=g+PZd2hiacg library=libkernel caller=0x%llx unsupported: %s\n",
                     static_cast<unsigned long long>(caller), e.what());
        ExitProcess(4);
    }
}
extern "C" void ctr_sysv_condition_destroy();
inline int kernel_condition_broadcast(std::uint64_t* cell) {
    auto object = lookup_condition(cell);
    std::lock_guard<std::mutex> guard(object->state);
    ++object->generation;
    object->changed.notify_all();
    return 0;
}
// Public condvar.cpp332-336/CondSignal: wake one actual waiter, not all.
inline int kernel_condition_signal(std::uint64_t* cell) {
    auto object = lookup_condition(cell);
    std::lock_guard<std::mutex> guard(object->state);
    ++object->generation;
    object->changed.notify_one();
    return 0;
}
extern "C" int ctr_condition_signal(std::uint64_t* cell, std::uint64_t caller) {
    try {
        const int rc = kernel_condition_signal(cell);
        if (ctrport::settings::hot_trace()) {
            std::printf("{\"scope\":\"original_CondSignal_return\",\"caller\":%llu,\"result\":%d}\n",
                        static_cast<unsigned long long>(caller), rc);
            std::fflush(stdout);
        }
        return rc;
    } catch (const std::exception& e) {
        std::fprintf(stderr, "NID=kDh-NfxgMtE library=libkernel caller=0x%llx unsupported: %s\n",
                     static_cast<unsigned long long>(caller), e.what());
        ExitProcess(4);
    }
}
extern "C" void ctr_sysv_condition_signal();
// Independent infinite host wait; public shadPS4 condvar.cpp CondWait contract,
// revision e3ce810f3a653f43ac64ebab63023de281a4103a, GPL-2.0-or-later.
inline int kernel_condition_wait(std::uint64_t* condition, std::uint64_t* mutex, std::uint64_t usec = UINT64_MAX) {
    if (usec != UINT64_MAX && usec > 86400000000ULL)
        throw std::runtime_error("Condition relative timeout above24h unsupported");
    auto cv = lookup_condition(condition);
    auto mx = lookup_mutex(mutex, false);
    std::unique_lock<std::mutex> guard(cv->state);
    std::uint32_t depth = 0;
    {
        std::lock_guard<std::mutex> owner(mx->state);
        if (mx->owner != std::this_thread::get_id() || !mx->depth) return int(0x80020001U);
        depth = mx->depth;
        mx->depth = 1;
    }
    const auto generation = cv->generation;
    if (kernel_mutex_unlock(mutex)) throw std::runtime_error("Condition wait release failed");
    ++cv->waiters;
    std::exception_ptr failure;
    bool signaled = true;
    if (ctrport::settings::hot_trace()) {
        std::fprintf(stderr, "condition blocking thread=%lu saved_depth=%u\n", GetCurrentThreadId(), depth);
        std::fflush(stderr);
    }
    try {
        if (usec == UINT64_MAX)
            cv->changed.wait(guard, [&] { return cv->generation != generation; });
        else
            signaled = cv->changed.wait_for(guard, std::chrono::microseconds(usec),
                                            [&] { return cv->generation != generation; });
    } catch (...) {
        failure = std::current_exception();
    }
    --cv->waiters;
    guard.unlock();
    if (kernel_mutex_lock(mutex)) throw std::runtime_error("Condition wait reacquire failed");
    {
        std::lock_guard<std::mutex> owner(mx->state);
        mx->depth = depth;
    }
    if (failure) std::rethrow_exception(failure);
    return signaled ? 0 : int(0x8002003cU);
}
// Own opt-in observer: keep the first original frame chain for each thread/cell.
// No guest writes, wait completion, or verified unwind is inferred.
inline std::mutex condition_wait_frames_lock;
inline std::set<std::pair<DWORD, std::uint64_t>> condition_wait_frames_seen;
inline void observe_wait_frames(std::uint64_t frame, const void* condition) {
    char enabled[4]{};
    if (GetEnvironmentVariableA("CTR_SYNC_TRACE", enabled, sizeof(enabled)) != 1 || enabled[0] != '1') return;
    {
        std::lock_guard<std::mutex> lock(condition_wait_frames_lock);
        if (condition_wait_frames_seen.size() >= 256 ||
            !condition_wait_frames_seen.emplace(GetCurrentThreadId(), reinterpret_cast<std::uint64_t>(condition))
                 .second)
            return;
    }
    std::fprintf(
        stderr,
        "{\"scope\":\"condition_wait_identity\",\"thread\":%lu,\"condition_cell\":%llu,\"guest_modified\":false}\n",
        GetCurrentThreadId(), static_cast<unsigned long long>(reinterpret_cast<std::uint64_t>(condition)));
    observe_condition_frames(frame, true);
}
extern "C" int ctr_condition_wait_owned(std::uint64_t* condition, std::uint64_t* mutex, std::uint64_t caller,
                                        std::uint64_t original_frame) {
    try {
        observe_wait_frames(original_frame, condition);
        observe_condition("wait_enter", caller, condition, mutex);
        const auto result = kernel_condition_wait(condition, mutex);
        observe_condition("wait_return", caller, condition, mutex);
        return result;
    } catch (const std::exception& error) {
        std::fprintf(stderr, "NID=WKAXJ4XBPQ4 library=libkernel caller=0x%llx unsupported: %s\n",
                     static_cast<unsigned long long>(caller), error.what());
        std::fflush(stderr);
        ExitProcess(4);
    }
}
extern "C" void ctr_sysv_condition_wait_owned();
// Public relative-wait contract saturates at the steady clock maximum before
// converting uint64 microseconds to a signed duration. Independent implementation.
inline std::chrono::steady_clock::time_point condition_relative_deadline(std::uint64_t usec) {
    const auto now = std::chrono::steady_clock::now();
    const auto remaining =
        std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::time_point::max() - now)
            .count();
    if (usec > static_cast<std::uint64_t>(remaining)) return std::chrono::steady_clock::time_point::max();
    return now + std::chrono::microseconds(static_cast<std::int64_t>(usec));
}
extern "C" int ctr_condition_relative_missing(std::uint64_t* condition, std::uint64_t* mutex, std::uint64_t usec,
                                              std::uint64_t caller) {
    try {
        auto cv = lookup_condition(condition);
        auto mx = lookup_mutex(mutex, false);
        (void)cv;
        {
            std::lock_guard<std::mutex> guard(mx->state);
            if (mx->owner != std::this_thread::get_id() || !mx->depth) return static_cast<int>(0x80020001U);
        }
        std::cout << "{\"scope\":\"condition_relative_missing_arguments\",\"microseconds\":" << usec
                  << ",\"caller\":\"0x" << std::hex << caller << std::dec << "\",\"mutex_released\":false}\n"
                  << std::flush;
        throw std::runtime_error("Guest condition wait scheduler/cancellation missing");
    } catch (const std::exception& error) {
        std::fprintf(stderr, "NID=BmMjYxmew1w library=libkernel caller=0x%llx unsupported: %s\n",
                     static_cast<unsigned long long>(caller), error.what());
        std::fflush(stderr);
        ExitProcess(4);
    }
}
extern "C" int ctr_condition_wait_missing(std::uint64_t* condition, std::uint64_t* mutex, std::uint64_t caller) {
    try {
        auto cv = lookup_condition(condition);
        auto mx = lookup_mutex(mutex, false);
        (void)cv;
        {
            std::lock_guard<std::mutex> guard(mx->state);
            if (mx->owner != std::this_thread::get_id() || !mx->depth) return static_cast<int>(0x80020001U);
        }
        throw std::runtime_error("Guest condition wait scheduler/cancellation missing");
    } catch (const std::exception& error) {
        std::fprintf(stderr, "NID=WKAXJ4XBPQ4 library=libkernel caller=0x%llx unsupported: %s\n",
                     static_cast<unsigned long long>(caller), error.what());
        std::fflush(stderr);
        ExitProcess(4);
    }
}
extern "C" void ctr_sysv_condition_wait_missing();
extern "C" void ctr_condition_wait_fixture();
extern "C" unsigned char ctr_condition_wait_caller;
extern "C" void ctr_sysv_condition_relative_missing();
extern "C" void ctr_condition_relative_fixture();
extern "C" unsigned char ctr_condition_relative_caller;
// Owned opaque attributes: public shadPS4 condvar.cpp22-25,351-368,
// revision e3ce810f3a653f43ac64ebab63023de281a4103a (GPL-2.0-or-later).
struct ConditionAttributes {
    int process_shared = 0;
    int clock_id = 0;
};
inline std::mutex condition_attributes_lock;
inline std::unordered_map<std::uint64_t, std::unique_ptr<ConditionAttributes>> condition_attributes;
inline int condition_attributes_init(std::uint64_t* cell) {
    if (!cell) return 0x80020016;
    require_time_output(cell, 8);
    auto value = std::make_unique<ConditionAttributes>();
    const auto id = reinterpret_cast<std::uint64_t>(value.get());
    std::lock_guard<std::mutex> guard(condition_attributes_lock);
    condition_attributes.emplace(id, std::move(value));
    *cell = id;
    return 0;
}
inline int condition_attributes_destroy(std::uint64_t* cell) {
    if (!cell) return 0x80020016;
    require_time_output(cell, 8);
    std::lock_guard<std::mutex> guard(condition_attributes_lock);
    if (!*cell || !condition_attributes.erase(*cell)) return 0x80020016;
    *cell = 0;
    return 0;
}
inline int kernel_condition_init(std::uint64_t* cell, const std::uint64_t* attributes, const char* name) {
    if (!cell) return 0x80020016;
    require_time_output(cell, 8);
    std::string label;
    // Public pinned condvar.cpp27-48 stores the optional label on each new object.
    // Read defensively; the bounded unsupported case never publishes a handle.
    if (name) {
        for (std::size_t i = 0; i < 4096; ++i) {
            char ch = 0;
            SIZE_T got = 0;
            if (!ReadProcessMemory(GetCurrentProcess(), name + i, &ch, 1, &got) || got != 1)
                throw std::runtime_error("Unreadable condition name");
            if (!ch) break;
            label.push_back(ch);
            if (i == 4095) throw std::runtime_error("Condition name exceeds characterized bound");
        }
    }
    if (attributes) {
        require_time_output(const_cast<std::uint64_t*>(attributes), 8);
        std::lock_guard<std::mutex> guard(condition_attributes_lock);
        const auto found = condition_attributes.find(*attributes);
        if (found == condition_attributes.end()) throw std::runtime_error("Unknown condition attributes");
        if (found->second->process_shared || found->second->clock_id)
            throw std::runtime_error("Nondefault condition attributes unsupported");
    }
    auto object = std::make_shared<KernelCondition>();
    object->name = std::move(label);
    const auto id = reinterpret_cast<std::uint64_t>(object.get());
    std::lock_guard<std::mutex> guard(condition_registry_lock);
    condition_registry.emplace(id, object);
    *cell = id;
    return 0;
}
}

namespace ctrport {
// Pinned condvar.cpp219-224,453-454: Orbis NID wraps relative microseconds, not timespec.
extern "C" int ctr_condition_timedwait(std::uint64_t* condition, std::uint64_t* mutex, std::uint64_t usec,
                                       std::uint64_t caller) {
    try {
        const int result = kernel_condition_wait(condition, mutex, usec);
        if (ctrport::settings::hot_trace()) {
            std::printf(
                "{\"scope\":\"owned_condition_timedwait\",\"nid\":\"BmMjYxmew1w\",\"library\":\"libkernel\",\"caller\":%llu,\"usec\":%llu,\"result\":%d}\n",
                static_cast<unsigned long long>(caller), static_cast<unsigned long long>(usec), result);
            std::fflush(stdout);
        }
        return result;
    } catch (const std::exception& e) {
        std::fprintf(stderr, "NID=BmMjYxmew1w library=libkernel caller=0x%llx unsupported: %s\n",
                     static_cast<unsigned long long>(caller), e.what());
        std::fflush(stderr);
        ExitProcess(4);
    }
}
extern "C" void ctr_sysv_condition_timedwait();
}
