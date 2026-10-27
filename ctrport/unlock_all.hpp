// SPDX-License-Identifier: GPL-2.0-only
// CTR_UNLOCK_ALL=1: test-only switch (ctrport/GUEST_PATCHES.md). Every retail unlockable
// check (COctaneUnlockable subclasses, CProgressionChallenge subclasses) first calls the
// same singleton getter, follows [singleton+0x10] (handle) -> [handle+0x28] (record) and
// reports "unlocked" when the byte at record+0x81 is non-zero. Retail reflection names
// the record type CBuildData; in its field order _unlockAll follows _saveLoadEnabled
// (+0x80). Retail data leaves the byte at zero. This host thread sets it to one in
// memory; the eboot and save data are not modified, and without the variable nothing runs.
#pragma once
#include <windows.h>
#include <chrono>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <stdexcept>
#include <string>
#include <thread>
namespace ctrport {
// getter: "mov rax,[rip+global]; test rax,rax; je +1; ret" (lazy singleton);
// reader: one of the unlockable checks, "cmp byte ptr [rbx+0x81],0".
struct UnlockAllBinding {
    const char* sha256;
    std::uint64_t getter, global, reader;
    unsigned readers;
};
inline constexpr UnlockAllBinding unlock_all_bindings[] = {
    {"ca9c94411375ebec52de70fbc62f605ac8e2cea95595c627029969b5b9d9b5b1", 0x9de850, 0x26b1320, 0xf3cba8, 10},
    {"39d716b70a299b2546bd39996c8592f9375d3f18f9bb8bf11f9139e9af1eb3f5", 0x9f0cf0, 0x2ab1bd0, 0xe6b342, 25},
    // US CUSA13795, derived from the user's dump (retail_profiles.hpp); reader counts as EU.
    {"23ddf3909d87a42a4c74be395865b130e4128b892de59d71082d8620cd8cd858", 0x9de850, 0x26b1320, 0xf3cb98, 10},
    {"e3b7a068b5e7be0f976102aa223a74425e7817c67e26e21416163291750c1b83", 0x9f0cf0, 0x2ab1bd0, 0xe6b342, 25},
};
inline bool unlock_all_read(std::uint64_t address, void* out, std::size_t size) {
    SIZE_T got = 0;
    return address && ReadProcessMemory(GetCurrentProcess(), reinterpret_cast<LPCVOID>(address), out, size, &got) &&
           got == size;
}
inline void start_unlock_all(unsigned char* image, const std::string& pin) {
    const UnlockAllBinding* binding = nullptr;
    for (const auto& candidate : unlock_all_bindings)
        if (pin == candidate.sha256) binding = &candidate;
    if (!binding) throw std::runtime_error("CTR_UNLOCK_ALL has no binding for this eboot");
    const unsigned char getter_head[] = {0x48, 0x8b, 0x05}, getter_tail[] = {0x48, 0x85, 0xc0, 0x74, 0x01, 0xc3},
                        reader[] = {0x80, 0xbb, 0x81, 0x00, 0x00, 0x00, 0x00};
    const unsigned char* getter = image + binding->getter;
    std::int32_t displacement = 0;
    std::memcpy(&displacement, getter + 3, 4);
    if (std::memcmp(getter, getter_head, 3) || std::memcmp(getter + 7, getter_tail, 6) ||
        binding->getter + 7 + displacement != binding->global)
        throw std::runtime_error("CTR_UNLOCK_ALL getter differs from the pinned original bytes");
    if (std::memcmp(image + binding->reader, reader, sizeof(reader)))
        throw std::runtime_error("CTR_UNLOCK_ALL reader differs from the pinned original bytes");
    const auto global = reinterpret_cast<std::uint64_t>(image) + binding->global;
    std::cout << "{\"scope\":\"guest_unlock_all_armed\",\"getter\":" << binding->getter
              << ",\"global\":" << binding->global << ",\"readers\":" << binding->readers << "}\n"
              << std::flush;
    std::thread([global] {
        std::uint64_t logged_record = 0;
        unsigned long long writes = 0;
        bool logged_indirect = false;
        for (;;) {
            std::uint64_t setup = 0, handle = 0, record = 0;
            std::uint32_t properties = 0;
            unsigned char flags[2]{};
            if (unlock_all_read(global, &setup, 8) && unlock_all_read(setup + 0x10, &handle, 8) &&
                unlock_all_read(handle, &properties, 4)) {
                // Redirected (0x10000000) and aliased (0x20000000) handles resolve through
                // engine calls; only the direct record pointer is followed here.
                if (properties & 0x30000000) {
                    if (!logged_indirect) {
                        logged_indirect = true;
                        std::cout << "{\"scope\":\"guest_unlock_all_handle_indirect\",\"properties\":" << properties
                                  << "}\n"
                                  << std::flush;
                    }
                } else if (unlock_all_read(handle + 0x28, &record, 8) && unlock_all_read(record + 0x80, flags, 2) &&
                           !flags[1]) {
                    const unsigned char one = 1;
                    SIZE_T put = 0;
                    if (WriteProcessMemory(GetCurrentProcess(), reinterpret_cast<LPVOID>(record + 0x81), &one, 1,
                                           &put) &&
                        put == 1) {
                        ++writes;
                        if (record != logged_record) {
                            logged_record = record;
                            std::cout << "{\"scope\":\"guest_unlock_all_set\",\"setup\":" << setup
                                      << ",\"record\":" << record << ",\"byte_0x80\":" << unsigned(flags[0])
                                      << ",\"writes\":" << writes << "}\n"
                                      << std::flush;
                        }
                    }
                }
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(250));
        }
    }).detach();
}
}
