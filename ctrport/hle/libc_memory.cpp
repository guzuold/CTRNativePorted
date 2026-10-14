// SPDX-License-Identifier: GPL-2.0-or-later
// Public NID and memcpy behavior: pinned shadPS4 v0.18.0
// src/core/libraries/libc_internal/libc_internal_memory.cpp.
// Implemented with the host CRT, called directly using the guest SysV ABI.
#include <cstring>
#include "ctr_hle.h"
void* CTR_SYSV sceLibcInternalMemcpy(void* destination, const void* source, std::uint64_t size) {
    void* result = size ? std::memcpy(destination, source, static_cast<std::size_t>(size)) : destination;
    ctr_hle::log_call("libSceLibcInternal", "memcpy", 0);
    return result;
}
CTR_HLE_REGISTER("Q3VBxCXhUHs", "libSceLibcInternal", sceLibcInternalMemcpy);

// NID identity: pinned shadPS4 src/core/aerolib/aerolib.inl (strncat).
char* CTR_SYSV sceLibcInternalStrncat(char* destination, const char* source, std::uint64_t limit) {
    char* end = destination + std::strlen(destination);
    std::uint64_t count = 0;
    while (count < limit && source[count]) { end[count] = source[count]; ++count; }
    end[count] = 0;
    ctr_hle::log_call("libSceLibcInternal", "strncat", 0);
    return destination;
}
CTR_HLE_REGISTER("kHg45qPC6f0", "libSceLibcInternal", sceLibcInternalStrncat);
