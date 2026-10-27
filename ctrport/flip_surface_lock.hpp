// SPDX-License-Identifier: GPL-2.0-only
#pragma once
#include "video_label_memory.hpp"
#include "owned_guest_copy.hpp"
namespace ctrport {
// Pinned public PatchFlipRequest writes a 32-bit lock1 at label_base+slot*8.
// This bounded CPU publisher is not a fence or a notification mechanism.
class FlipSurfaceLock {
    CtrGuestCopyApi memory_{};
    uint64_t address_ = 0, argument_ = 0, completion_count_ = 0, completion_token_ = 0;
    bool locked_ = false, completed_ = false;

public:
    int acquire(const CtrGuestCopyApi& memory, uint64_t base, uint32_t slot, uint64_t argument) {
        if (locked_ || completed_ || !base || (base & 7) || slot >= 16 || base > UINT64_MAX - 128 || !memory.read ||
            !memory.write)
            return 60;
        uint64_t old = UINT64_MAX;
        const auto address = base + uint64_t(slot) * 8;
        if (memory.read(memory.context, address, &old, 8)) return 60;
        if (old) return 91;
        const uint32_t one = 1;
        if (memory.write(memory.context, address, &one, 4)) return 60;
        memory_ = memory;
        address_ = address;
        argument_ = argument;
        locked_ = true;
        return 0;
    }
    // Completion identity comes from a successful real present_next on this session.
    // Failed/stale completion must retain lock1, preventing premature reuse.
    int complete(int present_status, uint64_t count, uint64_t token, uint64_t argument) {
        if (!locked_ || completed_) return 91;
        if (present_status || !count || !token || argument != argument_) return 88;
        uint64_t current = 0;
        if (memory_.read(memory_.context, address_, &current, 8)) return 60;
        if (current != 1) return 91;
        // driver.cpp ProcessFlip271: current frame remains locked. A later
        // successful frame retires this PREVIOUS slot, not its own current slot.
        completion_count_ = count;
        completion_token_ = token;
        completed_ = true;
        return 0;
    }
    int retire_previous(int status, uint64_t later_count, uint64_t later_token) {
        if (!locked_ || !completed_) return 91;
        if (status || later_count <= completion_count_ || !later_token || later_token == completion_token_) return 88;
        uint64_t current = 0;
        if (memory_.read(memory_.context, address_, &current, 8)) return 60;
        if (current != 1) return 91;
        const uint64_t zero = 0;
        if (memory_.write(memory_.context, address_, &zero, 8)) return 60;
        locked_ = false;
        return 0;
    }
};
}
