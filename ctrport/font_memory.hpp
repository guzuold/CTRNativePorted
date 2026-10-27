// SPDX-FileCopyrightText: Copyright 2024 shadPS4 Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later
// Adapted from engine/shadPS4 src/core/libraries/font/{font.cpp,font.h,font_error.h}
// revision e3ce810f3a653f43ac64ebab63023de281a4103a; exact source hashes in report.
#pragma once
#include <cstdint>
#include <cstring>
#include <cstddef>
#include "original_call.hpp"
namespace ctrport {
struct FontMemInterface {
    void* alloc;
    void* dealloc;
    void* realloc_fn;
    void* calloc_fn;
    void* mspace_create;
    void* mspace_destroy;
};
struct FontMem {
    std::uint16_t mem_kind, attr_bits;
    std::uint32_t region_size;
    void* region_base;
    void* mspace_handle;
    const FontMemInterface* iface;
    void* on_destroy;
    void* destroy_ctx;
    void* some_ctx1;
    void* some_ctx2;
};
static_assert(sizeof(FontMem) == 64 && offsetof(FontMem, iface) == 24 && offsetof(FontMem, some_ctx2) == 56);
inline constexpr int font_invalid_parameter = static_cast<int>(0x80460002U);
inline constexpr int font_invalid_memory = static_cast<int>(0x80460003U);
// Trusted owned buffers only. Guest admission must validate extents/lifetime and callback code.
inline int font_memory_init(FontMem* mem, void* region, std::uint32_t size, const FontMemInterface* iface, void* mspace,
                            void* destroy, void* context) {
    if (!mem) return font_invalid_parameter;
    if (!iface) {
        mem->mem_kind = 0;
        if (!region || !size) return font_invalid_parameter;
    }
    mem->mem_kind = 0x0f00;
    mem->attr_bits = 0;
    mem->region_size = size;
    mem->region_base = region;
    mem->mspace_handle = mspace;
    mem->iface = iface;
    mem->on_destroy = destroy;
    mem->destroy_ctx = context;
    mem->some_ctx1 = nullptr;
    mem->some_ctx2 = mspace;
    return 0;
}
inline int font_memory_term(FontMem* mem) {
    if (!mem) return font_invalid_parameter;
    if (mem->mem_kind != 0x0f00) return font_invalid_memory;
    if ((mem->attr_bits & 0x80) != 0) {
        if (mem->iface && mem->iface->mspace_destroy)
            (void)ctr_call_original_integer3(mem->iface->mspace_destroy,
                                             reinterpret_cast<std::uint64_t>(mem->some_ctx2),
                                             reinterpret_cast<std::uint64_t>(mem->mspace_handle), 0);
        std::memset(mem, 0, sizeof(*mem));
        return 0;
    }
    if (mem->on_destroy) {
        mem->mem_kind = 0;
        (void)ctr_call_original_integer3(mem->on_destroy, reinterpret_cast<std::uint64_t>(mem),
                                         reinterpret_cast<std::uint64_t>(mem->mspace_handle),
                                         reinterpret_cast<std::uint64_t>(mem->destroy_ctx));
        return 0;
    }
    std::memset(mem, 0, sizeof(*mem));
    return 0;
}
extern "C" void ctr_font_memory_callback_control();
}
