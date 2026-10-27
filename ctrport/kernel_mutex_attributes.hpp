// SPDX-License-Identifier: GPL-2.0-only
// New owned attribute implementation, after the public behaviour of pinned shadPS4 v0.18.0.
#pragma once
#include "settings/hot_trace.hpp"
#include "kernel_mutex.hpp"
namespace ctrport {
struct KernelMutexAttributes {
    std::uint32_t type = 1, protocol = 0;
    std::int32_t ceiling = 0;
};
static_assert(sizeof(KernelMutexAttributes) == 12);
inline std::mutex mutex_attributes_registry_lock;
inline std::unordered_map<std::uint64_t, std::shared_ptr<KernelMutexAttributes>> mutex_attributes_registry;
inline int kernel_mutex_attributes_init(std::uint64_t* cell) {
    if (!cell) throw std::runtime_error("Mutex attributes cell null unsupported");
    require_time_output(cell, 8);
    auto object = std::make_shared<KernelMutexAttributes>();
    const auto handle = reinterpret_cast<std::uint64_t>(object.get());
    std::lock_guard<std::mutex> guard(mutex_attributes_registry_lock);
    mutex_attributes_registry.emplace(handle, object);
    *cell = handle;
    return 0;
}
inline int kernel_mutex_attributes_settype(std::uint64_t* cell, std::uint32_t type) {
    if (!cell) return static_cast<int>(0x80020016U); // Orbis EINVAL.
    require_time_output(cell, 8);
    std::lock_guard<std::mutex> guard(mutex_attributes_registry_lock);
    if (!*cell || type < 1 || type >= 5) return static_cast<int>(0x80020016U);
    const auto found = mutex_attributes_registry.find(*cell);
    if (found == mutex_attributes_registry.end())
        throw std::runtime_error("Unknown mutex attributes handle unsupported");
    found->second->type = type;
    return 0;
}
inline int kernel_mutex_init(std::uint64_t* cell, const std::uint64_t* attributes, const char* name) {
    if (!cell) throw std::runtime_error("Mutex output cell null unsupported");
    require_time_output(cell, 8);
    std::string owned_name;
    if (name) {
        bool terminated = false;
        for (std::size_t i = 0; i < 256; ++i) {
            char ch = 0;
            SIZE_T read = 0;
            if (!ReadProcessMemory(GetCurrentProcess(), name + i, &ch, 1, &read) || read != 1)
                throw std::runtime_error("Mutex name unreadable");
            if (!ch) {
                terminated = true;
                break;
            }
            owned_name.push_back(ch);
        }
        if (!terminated) throw std::runtime_error("Mutex name exceeds supported256B bound");
    }
    KernelMutexAttributes values;
    if (attributes) {
        require_time_output(attributes, 8);
        std::lock_guard<std::mutex> guard(mutex_attributes_registry_lock);
        if (*attributes) {
            const auto found = mutex_attributes_registry.find(*attributes);
            if (found == mutex_attributes_registry.end())
                throw std::runtime_error("Foreign mutex attributes unsupported");
            values = *found->second;
        }
    }
    if ((values.type != 1 && values.type != 2) || values.protocol || values.ceiling)
        throw std::runtime_error("Mutex type/protocol unsupported");
    auto object = std::make_shared<KernelMutex>();
    object->type = values.type;
    object->name = owned_name;
    const auto handle = reinterpret_cast<std::uint64_t>(object.get());
    std::lock_guard<std::mutex> guard(mutex_registry_lock);
    mutex_registry.emplace(handle, object);
    *cell = handle;
    return 0;
}
inline int kernel_mutex_attributes_destroy(std::uint64_t* cell) {
    if (!cell) return static_cast<int>(0x80020016U);
    require_time_output(cell, 8);
    std::lock_guard<std::mutex> guard(mutex_attributes_registry_lock);
    if (!*cell) return static_cast<int>(0x80020016U);
    const auto found = mutex_attributes_registry.find(*cell);
    if (found == mutex_attributes_registry.end())
        throw std::runtime_error("Foreign/stale mutex attributes unsupported");
    mutex_attributes_registry.erase(found);
    *cell = 0;
    return 0;
}
}
