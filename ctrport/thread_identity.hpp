// SPDX-License-Identifier: GPL-2.0-only
// Independent opaque identity for a Windows thread executing guest code.
// Public behavioral reference: shadPS4 pthread.cpp posix_pthread_self.
#pragma once
#include "kernel_mutex.hpp"
#include "original_call.hpp"
#include "thread_attributes.hpp"
#include <atomic>
#include <limits>
#include <set>
namespace ctrport {
// Public shadPS4 pthread.cpp418-420 e3ce810f3a653f43ac64ebab63023de281a4103a,
// GPL-2.0-or-later: void yield. Independent Windows host scheduling call.
inline std::atomic<std::uint64_t> host_yield_calls{0};
extern "C" void ctr_thread_yield() {
    std::this_thread::yield();
    host_yield_calls.fetch_add(1, std::memory_order_relaxed);
}
extern "C" void ctr_sysv_thread_yield();
// Retail NpToolkit2 offset0x453e1 reads the first int32 of pthread_self.
// Public pthread.h Pthread::tid is first. Other Pthread fields are not supplied.
struct ThreadIdentity {
    std::int32_t guest_tid = 0;
    std::thread::id host;
    std::atomic<int> priority_override{-1};
    std::atomic<int> guest_policy{1};
    std::atomic<std::uint64_t> guest_affinity{~std::uint64_t{0}};
};
inline std::mutex thread_identity_lock;
inline std::unordered_map<std::uint64_t, std::shared_ptr<ThreadIdentity>> thread_identities;
inline std::atomic<std::uint64_t> next_thread_identity{1};
// Preserve retired address identities for this process; never alias a stale handle.
inline std::vector<std::shared_ptr<ThreadIdentity>> retired_thread_identities;
inline thread_local std::uint64_t current_thread_identity = 0;
inline thread_local int current_guest_priority = 700;
inline int effective_guest_priority() {
    std::lock_guard<std::mutex> guard(thread_identity_lock);
    const auto found = thread_identities.find(current_thread_identity);
    if (found != thread_identities.end()) {
        const int value = found->second->priority_override.load();
        if (value >= 0) return value;
    }
    return current_guest_priority;
}
inline std::uint64_t thread_self() {
    if (current_thread_identity) return current_thread_identity;
    auto object = std::make_shared<ThreadIdentity>();
    object->host = std::this_thread::get_id();
    const auto tid = next_thread_identity.fetch_add(1);
    if (!tid || tid > static_cast<std::uint64_t>(std::numeric_limits<int>::max()))
        throw std::runtime_error("Thread numeric identity exhausted");
    object->guest_tid = static_cast<std::int32_t>(tid);
    const auto handle = reinterpret_cast<std::uint64_t>(&object->guest_tid);
    std::lock_guard<std::mutex> guard(thread_identity_lock);
    thread_identities.emplace(handle, object);
    current_thread_identity = handle;
    return handle;
}
// Own positive32-bit ID domain; shadPS4 pthread.cpp314,379-381 uses a monotonic tid.
inline int thread_numeric_id() {
    const auto identity = thread_self();
    std::lock_guard<std::mutex> guard(thread_identity_lock);
    const auto found = thread_identities.find(identity);
    if (found == thread_identities.end() || found->second->host != std::this_thread::get_id())
        throw std::runtime_error("Numeric thread identity ownership mismatch");
    return found->second->guest_tid;
}
extern "C" int ctr_thread_numeric_id(std::uint64_t caller) {
    try {
        return thread_numeric_id();
    } catch (const std::exception& error) {
        std::fprintf(stderr, "NID=EI-5-jlq2dE library=libkernel caller=0x%llx unsupported: %s\n",
                     static_cast<unsigned long long>(caller), error.what());
        std::fflush(stderr);
        ExitProcess(4);
    }
}
extern "C" void ctr_sysv_thread_numeric_id();
inline bool release_thread_identity() {
    if (!current_thread_identity) return false;
    std::lock_guard<std::mutex> guard(thread_identity_lock);
    const auto found = thread_identities.find(current_thread_identity);
    if (found == thread_identities.end() || found->second->host != std::this_thread::get_id())
        throw std::runtime_error("Thread identity ownership differs");
    retired_thread_identities.push_back(found->second);
    thread_identities.erase(found);
    current_thread_identity = 0;
    return true;
}
// Public pthread.cpp879-896 reads retained thread attributes, not current CPU.
inline void record_thread_affinity(std::uint64_t handle, std::uint64_t mask) {
    std::lock_guard<std::mutex> guard(thread_identity_lock);
    const auto found = thread_identities.find(handle);
    if (found == thread_identities.end()) throw std::runtime_error("Affinity identity retired");
    found->second->guest_affinity.store(mask);
}
inline int thread_get_affinity(std::uint64_t handle, std::uint64_t* output) {
    if (!handle || !output) return static_cast<int>(0x80020016U);
    std::lock_guard<std::mutex> guard(thread_identity_lock);
    const auto found = thread_identities.find(handle);
    if (found == thread_identities.end()) return static_cast<int>(0x80020003U);
    require_time_output(output, 8);
    *output = found->second->guest_affinity.load();
    return 0;
}
extern "C" int ctr_thread_get_affinity(std::uint64_t handle, std::uint64_t* output, std::uint64_t caller) {
    try {
        const int rc = thread_get_affinity(handle, output);
        std::printf(
            "{\"scope\":\"owned_thread_affinity_readback\",\"caller\":%llu,\"thread_handle\":%llu,\"result\":%d,\"mask\":%llu}\n",
            static_cast<unsigned long long>(caller), static_cast<unsigned long long>(handle), rc,
            static_cast<unsigned long long>(rc ? 0 : *output));
        std::fflush(stdout);
        return rc;
    } catch (const std::exception& e) {
        std::fprintf(stderr, "NID=rcrVFJsQWRY library=libkernel caller=0x%llx unsupported: %s\n",
                     static_cast<unsigned long long>(caller), e.what());
        ExitProcess(4);
    }
}
extern "C" void ctr_sysv_thread_get_affinity();
inline int thread_set_affinity(std::uint64_t handle, std::uint64_t mask) {
    const auto current = thread_self();
    {
        std::lock_guard<std::mutex> guard(thread_identity_lock);
        const auto found = thread_identities.find(handle);
        if (found == thread_identities.end()) return static_cast<int>(0x80020003U);
        if (handle != current) throw std::runtime_error("Foreign thread affinity unsupported");
    }
    if (!mask || (mask & ~0x7fULL)) return static_cast<int>(0x80020016U);
    if (GetActiveProcessorGroupCount() != 1) throw std::runtime_error("Multiple processor groups unsupported");
    DWORD_PTR process = 0, system = 0;
    if (!GetProcessAffinityMask(GetCurrentProcess(), &process, &system))
        throw std::runtime_error("Process affinity query failed");
    if (mask & ~process) throw std::runtime_error("Guest CPU bit unavailable in identity host mapping");
    if (!SetThreadAffinityMask(GetCurrentThread(), static_cast<DWORD_PTR>(mask)))
        throw std::runtime_error("Windows thread affinity update failed");
    record_thread_affinity(handle, mask);
    return 0;
}
inline int thread_get_priority(std::uint64_t handle, int* output) {
    std::shared_ptr<ThreadIdentity> identity;
    {
        std::lock_guard<std::mutex> guard(thread_identity_lock);
        const auto found = thread_identities.find(handle);
        if (found == thread_identities.end()) return static_cast<int>(0x80020003U);
        identity = found->second;
    }
    if (identity->host != std::this_thread::get_id())
        throw std::runtime_error("Foreign thread priority query unsupported");
    require_time_output(output, sizeof(*output));
    *output = effective_guest_priority();
    return 0;
}
extern "C" int ctr_thread_get_priority(std::uint64_t handle, int* output, std::uint64_t caller) {
    try {
        return thread_get_priority(handle, output);
    } catch (const std::exception& error) {
        std::fprintf(stderr, "NID=1tKyG7RlMJo library=libkernel caller=0x%llx unsupported: %s\n",
                     static_cast<unsigned long long>(caller), error.what());
        std::fflush(stderr);
        ExitProcess(4);
    }
}
extern "C" void ctr_sysv_thread_get_priority();
inline int thread_attributes_current(std::uint64_t handle, std::uint64_t* cell) {
    require_time_output(cell, 8);
    std::shared_ptr<ThreadIdentity> identity;
    {
        std::lock_guard<std::mutex> guard(thread_identity_lock);
        const auto found = thread_identities.find(handle);
        if (found == thread_identities.end()) return static_cast<int>(0x80020003U);
        identity = found->second;
    }
    if (identity->host != std::this_thread::get_id())
        throw std::runtime_error("Foreign thread attribute snapshot unsupported");
    std::lock_guard<std::mutex> guard(thread_attributes_lock);
    const auto found = thread_attributes.find(*cell);
    if (found == thread_attributes.end()) return static_cast<int>(0x80020016U);
    ULONG_PTR low = 0, high = 0;
    GetCurrentThreadStackLimits(&low, &high);
    if (!low || high <= low) throw std::runtime_error("Actual Windows thread stack unavailable");
    ThreadAttributes snapshot;
    snapshot.stack_address = low;
    snapshot.stack_size = high - low;
    *found->second = snapshot;
    return 0;
}

// Public pthread.cpp948: POSIX self uses the same owned thread identity.
extern "C" std::uint64_t ctr_posix_thread_self(std::uint64_t caller) {
    const auto result = thread_self();
    std::printf(
        "{\"scope\":\"owned_posix_thread_self\",\"nid\":\"EotR8a3ASf4\",\"library\":\"libScePosix\",\"caller\":%llu,\"result\":%llu}\n",
        static_cast<unsigned long long>(caller), static_cast<unsigned long long>(result));
    return result;
}
extern "C" void ctr_sysv_posix_thread_self();

}
