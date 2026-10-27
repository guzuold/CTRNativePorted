// SPDX-License-Identifier: GPL-2.0-only
// Independent owned attribute storage. Contract: pinned shadPS4 pthread_attr.cpp
// defaults and posix_pthread_attr_init; no thread or scheduler service supplied.
#pragma once
#include "kernel_mutex.hpp"
namespace ctrport {
struct ThreadAttributes {
    int policy = 1, inherit = 4, priority = 700;
    bool suspended = false, system_scope = true, detached = false;
    std::uint64_t stack_address = 0, stack_size = 1048576, guard_size = 0, cpu_set_size = 0, cpu_set = 0;
};
inline std::mutex thread_attributes_lock;
inline std::unordered_map<std::uint64_t, std::shared_ptr<ThreadAttributes>> thread_attributes;
inline int thread_attributes_init(std::uint64_t* cell) {
    require_time_output(cell, 8);
    auto object = std::make_shared<ThreadAttributes>();
    auto handle = reinterpret_cast<std::uint64_t>(object.get());
    std::lock_guard<std::mutex> guard(thread_attributes_lock);
    thread_attributes.emplace(handle, object);
    *cell = handle;
    return 0;
}
inline int thread_attributes_affinity(std::uint64_t* cell, std::uint64_t* output) {
    require_time_output(cell, 8);
    require_time_output(output, 8);
    std::lock_guard<std::mutex> guard(thread_attributes_lock);
    const auto found = thread_attributes.find(*cell);
    if (found == thread_attributes.end()) return static_cast<int>(0x80020016U);

    // Null attribute cpuset means unrestricted, not the Windows processor mask.
    *output = found->second->cpu_set_size ? found->second->cpu_set : ~std::uint64_t{0};
    return 0;
}

// Public pthread_attr.cpp283-319, e3ce810f3a653f43ac64ebab63023de281a4103a
// GPL-2.0-or-later contract; independent mask storage, no upstream code copied.
inline int thread_attributes_set_affinity(std::uint64_t* cell, std::uint64_t mask) {
    if (!cell) return int(0x80020016U);
    require_time_output(cell, 8);
    std::lock_guard<std::mutex> guard(thread_attributes_lock);
    const auto found = thread_attributes.find(*cell);
    if (found == thread_attributes.end()) return int(0x80020016U);
    found->second->cpu_set = mask;
    found->second->cpu_set_size = 8;
    return 0;
}
extern "C" int ctr_attr_set_affinity(std::uint64_t* cell, std::uint64_t mask, std::uint64_t caller) {
    try {
        return thread_attributes_set_affinity(cell, mask);
    } catch (const std::exception& error) {
        std::fprintf(stderr, "NID=3qxgM4ezETA library=libkernel caller=0x%llx unsupported: %s\n",
                     static_cast<unsigned long long>(caller), error.what());
        std::fflush(stderr);
        ExitProcess(4);
    }
}
extern "C" void ctr_sysv_attr_set_affinity();
inline int thread_attributes_destroy(std::uint64_t* cell);
// Independent detach-state storage: public shadPS4 pthread_attr.cpp197-206,
// e3ce810f3a653f43ac64ebab63023de281a4103a (GPL-2.0-or-later).
inline int thread_attributes_set_detach(std::uint64_t* cell, int state) {
    if (!cell || (state != 0 && state != 1)) return int(0x80020016U);
    require_time_output(cell, 8);
    std::lock_guard<std::mutex> guard(thread_attributes_lock);
    const auto found = thread_attributes.find(*cell);
    if (found == thread_attributes.end()) return int(0x80020016U);
    found->second->detached = state != 0;
    return 0;
}
extern "C" int ctr_attr_set_detach(std::uint64_t* cell, int state, std::uint64_t caller) {
    try {
        return thread_attributes_set_detach(cell, state);
    } catch (const std::exception& error) {
        std::fprintf(stderr, "NID=-Wreprtu0Qs library=libkernel caller=0x%llx unsupported: %s\n",
                     static_cast<unsigned long long>(caller), error.what());
        std::fflush(stderr);
        ExitProcess(4);
    }
}
extern "C" void ctr_sysv_attr_set_detach();
// Attribute storage only: pinned pthread_attr.cpp143-151, priorities20-24.
inline int thread_attributes_set_policy(std::uint64_t* cell, int policy) {
    if (!cell) return int(0x80020016U);
    require_time_output(cell, 8);
    std::lock_guard<std::mutex> guard(thread_attributes_lock);
    const auto found = thread_attributes.find(*cell);
    if (found == thread_attributes.end()) return int(0x80020016U);
    if (policy < 1 || policy > 3) return int(0x8002002dU);
    found->second->policy = policy;
    found->second->priority = policy == 2 ? 900 : 700;
    return 0;
}
// Public pthread_attr.cpp209-225; storage, not scheduler execution.
inline int thread_attributes_set_priority(std::uint64_t* cell, const int* parameter) {
    if (!cell) return int(0x80020016U);
    require_time_output(cell, 8);
    std::lock_guard<std::mutex> guard(thread_attributes_lock);
    const auto found = thread_attributes.find(*cell);
    if (found == thread_attributes.end()) return int(0x80020016U);
    if (!parameter) return int(0x8002002dU);
    MEMORY_BASIC_INFORMATION region{};
    if (!VirtualQuery(parameter, &region, sizeof(region)) || region.State != MEM_COMMIT ||
        (region.Protect & (PAGE_GUARD | PAGE_NOACCESS)) ||
        reinterpret_cast<std::uintptr_t>(parameter) + sizeof(int) >
            reinterpret_cast<std::uintptr_t>(region.BaseAddress) + region.RegionSize)
        throw std::runtime_error("Priority parameter unreadable");
    const int priority = *parameter;
    if ((found->second->policy == 1 || found->second->policy == 3) && (priority < 256 || priority > 767))
        return int(0x8002002dU);
    found->second->priority = priority;
    return 0;
}
// Public pthread_attr.cpp228-237: attribute storage only.
inline int thread_attributes_set_inherit(std::uint64_t* cell, int inherit) {
    if (!cell) return int(0x80020016U);
    require_time_output(cell, 8);
    std::lock_guard<std::mutex> guard(thread_attributes_lock);
    const auto found = thread_attributes.find(*cell);
    if (found == thread_attributes.end()) return int(0x80020016U);
    if (inherit != 0 && inherit != 4) return int(0x8002002dU);
    found->second->inherit = inherit;
    return 0;
}
inline int thread_attributes_set_stack_size(std::uint64_t* cell, std::uint64_t size) {
    if (!cell || size < 16384) return static_cast<int>(0x80020016U);
    require_time_output(cell, 8);
    std::lock_guard<std::mutex> guard(thread_attributes_lock);
    const auto found = thread_attributes.find(*cell);
    if (found == thread_attributes.end()) return static_cast<int>(0x80020016U);
    found->second->stack_size = size;
    return 0;
}

inline int thread_attributes_destroy(std::uint64_t* cell) {
    require_time_output(cell, 8);
    std::lock_guard<std::mutex> guard(thread_attributes_lock);
    const auto found = thread_attributes.find(*cell);
    if (found == thread_attributes.end()) return static_cast<int>(0x80020016U);
    thread_attributes.erase(found);
    *cell = 0;
    return 0;
}

// Independent public pthread_attr.cpp99-107 contract; owned attribute snapshot.
inline int thread_attributes_get_stack(std::uint64_t* cell, std::uint64_t* address, std::uint64_t* size) {
    if (!cell || !address || !size) return int(0x80020016U);
    require_time_output(cell, 8);
    std::lock_guard<std::mutex> guard(thread_attributes_lock);
    const auto found = thread_attributes.find(*cell);
    if (found == thread_attributes.end()) return int(0x80020016U);
    require_time_output(address, 8);
    require_time_output(size, 8);
    *address = found->second->stack_address;
    *size = found->second->stack_size;
    return 0;
}
extern "C" int ctr_attr_get_stack(std::uint64_t* cell, std::uint64_t* address, std::uint64_t* size,
                                  std::uint64_t caller) {
    try {
        const int result = thread_attributes_get_stack(cell, address, size);
        std::printf(
            "{\"scope\":\"owned_attribute_get_stack\",\"nid\":\"-quPa4SEJUw\",\"library\":\"libkernel\",\"caller\":%llu,\"result\":%d}\n",
            static_cast<unsigned long long>(caller), result);
        return result;
    } catch (const std::exception& e) {
        std::fprintf(stderr, "NID=-quPa4SEJUw library=libkernel caller=0x%llx unsupported: %s\n",
                     static_cast<unsigned long long>(caller), e.what());
        std::fflush(stderr);
        ExitProcess(4);
    }
}
extern "C" void ctr_sysv_attr_get_stack();

}
