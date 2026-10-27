// SPDX-License-Identifier: GPL-2.0-only
// New Windows CNG implementation; public PS4 size/error contract from pinned
// shadPS4 random.h/random_error.h, hashes in random_service_contract.json.
#pragma once
#include "process_guard.hpp"
#include "kernel_time.hpp"
#include <array>
namespace ctrport {
inline int random_bytes(unsigned char* output, std::size_t size) {
    if (size > 64) return static_cast<int>(0x817c0016U);
    if (!size) return 0;
    if (!output) throw std::runtime_error("Random output null unsupported");
    require_time_output(output, size);
    if (BCryptGenRandom(nullptr, output, static_cast<ULONG>(size), BCRYPT_USE_SYSTEM_PREFERRED_RNG) < 0)
        return static_cast<int>(0x817c00ffU);
    return 0;
}
}
