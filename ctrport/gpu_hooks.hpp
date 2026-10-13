// SPDX-License-Identifier: GPL-2.0-only
// GPU library hooks (gpu_library.hpp): GPU-visible guest mappings and CPU writes by
// ctrport services that must reach the GPU caches first. Null without the library.
#pragma once
#include <cstdint>
namespace ctrport {
inline void (*direct_memory_map_hook)(std::uint64_t,std::uint64_t)=nullptr;
inline void (*guest_cpu_write_hook)(const void*,std::uint64_t)=nullptr;
// True once the GPU library owns presentation (VideoOut, GnmDriver, event queues).
inline bool gpu_library_present=false;
// GPU page-tracking faults: ctrport's exception handlers ask the library first
// (argument: EXCEPTION_POINTERS*). Returns nonzero when the fault was handled.
inline int (*gpu_fault_hook)(void*)=nullptr;
} // namespace ctrport
