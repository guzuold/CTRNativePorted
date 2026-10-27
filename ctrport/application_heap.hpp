// SPDX-License-Identifier: GPL-2.0-only
// New registry; public layout/provenance in application_heap_api_contract.json.
#pragma once
#include "original_call.hpp"
#include <array>
namespace ctrport {
class ApplicationHeap {
    const void* table = nullptr;
    std::array<void*, 10> read(const void* source) const {
        std::array<void*, 10> values{};
        SIZE_T size = 0;
        if (!source || !ReadProcessMemory(GetCurrentProcess(), source, values.data(), sizeof(values), &size) ||
            size != sizeof(values))
            throw std::runtime_error("Heap callback table unreadable");
        for (std::size_t index = 0; index < values.size(); ++index) {
            auto* callback = values[index];
            if (!callback && index >= 2) continue;
            MEMORY_BASIC_INFORMATION info{};
            if (!callback || !VirtualQuery(callback, &info, sizeof(info)) || info.State != MEM_COMMIT ||
                (info.Protect != PAGE_EXECUTE_READ && info.Protect != PAGE_EXECUTE_READWRITE))
                throw std::runtime_error("Heap callback not executable");
        }
        return values;
    }

public:
    void set(const void* source) {
        (void)read(source);
        table = source;
    }
    void* allocate(std::uint64_t size) const {
        auto values = read(table);
        return reinterpret_cast<void*>(ctr_call_original_integer3(values[0], size, 0, 0));
    }
    void release(void* value) const {
        auto values = read(table);
        (void)ctr_call_original_integer3(values[1], reinterpret_cast<std::uint64_t>(value), 0, 0);
    }
};
inline ApplicationHeap application_heap;
extern "C" void ctr_sysv_heap_malloc_control();
extern "C" void ctr_sysv_heap_free_control();
}
