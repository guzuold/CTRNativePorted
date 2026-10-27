// SPDX-License-Identifier: GPL-2.0-only
// GPU library hooks (gpu_library.hpp): GPU-visible guest mappings and CPU writes by
// ctrport services that must reach the GPU caches first. Null without the library.
#pragma once
#include <windows.h>
#include <cstdint>
namespace ctrport {
inline void (*direct_memory_map_hook)(std::uint64_t, std::uint64_t) = nullptr;
inline void (*guest_cpu_write_hook)(const void*, std::uint64_t) = nullptr;
// True once the GPU library owns presentation (VideoOut, GnmDriver, event queues).
inline bool gpu_library_present = false;
// GPU page-tracking faults: ctrport's exception handlers ask the library first
// (argument: EXCEPTION_POINTERS*). Returns nonzero when the fault was handled.
inline int (*gpu_fault_hook)(void*) = nullptr;
// The D3D11 backend (CTR_GPU_BACKEND=d3d11) refreshes its copies of guest buffers and
// textures from Windows write watch: guest mappings are reserved with MEM_WRITE_WATCH then.
inline unsigned long guest_write_watch_flag() {
    static const unsigned long flag = [] {
        char v[8]{};
        return GetEnvironmentVariableA("CTR_GPU_BACKEND", v, sizeof(v)) == 5 && v[0] == 'd' && v[1] == '3' &&
                       v[2] == 'd' && v[3] == '1' && v[4] == '1'
                   ? 0x00200000UL
                   : 0UL;
    }();
    return flag;
}
} // namespace ctrport
