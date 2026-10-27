// SPDX-License-Identifier: GPL-2.0-only
// New owned storage; public/retail evidence: heap_trace_storage_contract.json.
#pragma once
#include "original_call.hpp"
#include <array>
namespace ctrport {
struct HeapTraceInfo {
    std::uint64_t size;
    std::uint32_t flags, segment_info;
    std::uint64_t* mask;
    std::uint64_t* states;
};
static_assert(sizeof(HeapTraceInfo) == 32);
class HeapTraceStorage {
    alignas(8) std::uint64_t mask = 0;
    std::array<std::uint64_t, 65> states{};

public:
    void query(void* output) {
        HeapTraceInfo info{};
        SIZE_T got = 0;
        if (!output || !ReadProcessMemory(GetCurrentProcess(), output, &info, sizeof(info), &got) ||
            got != sizeof(info) || info.size != sizeof(info))
            throw std::runtime_error("Heap trace output layout unreadable or unsupported");
        MEMORY_BASIC_INFORMATION region{};
        if (!VirtualQuery(output, &region, sizeof(region)) || region.State != MEM_COMMIT ||
            (region.Protect != PAGE_READWRITE && region.Protect != PAGE_EXECUTE_READWRITE) ||
            reinterpret_cast<std::uintptr_t>(output) - reinterpret_cast<std::uintptr_t>(region.BaseAddress) >
                region.RegionSize - sizeof(info))
            throw std::runtime_error("Heap trace output not writable");
        info.segment_info = 0;
        info.mask = &mask;
        info.states = states.data();
        std::memcpy(output, &info, sizeof(info));
    }
};
inline HeapTraceStorage heap_trace;
}
