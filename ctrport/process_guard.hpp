// SPDX-License-Identifier: GPL-2.0-only
#pragma once
#include "flat_loader.hpp"
namespace ctrport {
// Our implementation of the observed stable 64-bit canary object contract.
// No upstream dummy constant is used.
inline const std::uint64_t* process_guard() {
    static const MappedImage storage = [] {
        MappedImage page(
            static_cast<unsigned char*>(VirtualAlloc(nullptr, 4096, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE)));
        if (!page) throw std::runtime_error("Process guard allocation failed");
        if (BCryptGenRandom(nullptr, page.get(), 8, BCRYPT_USE_SYSTEM_PREFERRED_RNG) < 0)
            throw std::runtime_error("Process guard entropy failed");
        DWORD previous = 0;
        if (!VirtualProtect(page.get(), 4096, PAGE_READONLY, &previous))
            throw std::runtime_error("Process guard protection failed");
        return page;
    }();
    return reinterpret_cast<const std::uint64_t*>(storage.get());
}
}
