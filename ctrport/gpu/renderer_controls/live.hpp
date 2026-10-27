// SPDX-License-Identifier: GPL-2.0-only
#pragma once
#include <atomic>
#include <cstdint>

namespace ctrport::settings::live_gpu {
// One atomic word per control: the high word is a revision, the low word an
// index. Consumers load once at their existing render/present boundary. No
// environment mutation, resource destruction, or guest code patching is needed.
enum : unsigned { presentation_limit = 1, fsr1_sharpen = 2 };
struct Slot {
    std::atomic<std::uint64_t> word{0}; // revision zero = startup configuration
    bool set(unsigned index, unsigned count) noexcept {
        if (index >= count) return false;
        auto old = word.load(std::memory_order_relaxed);
        for (;;) {
            const auto next = (((old >> 32) + 1) << 32) | index;
            if (word.compare_exchange_weak(old, next, std::memory_order_release, std::memory_order_relaxed))
                return true;
        }
    }
};
inline Slot presentation, sharpness;
inline unsigned set(unsigned key, unsigned index) noexcept {
    if (key == presentation_limit) return presentation.set(index, 9) ? 1u : 0u;
    if (key == fsr1_sharpen) return sharpness.set(index, 12) ? 1u : 0u;
    return 0;
}
// presentation_limit: index 0 "original" keeps the console's flip pacing; any other index
// unlocks it (Direct3D 11) and caps presentation at the value, "unlimited" (8) at nothing.
inline unsigned cap(unsigned index) noexcept {
    constexpr unsigned values[]{0, 30, 60, 90, 120, 144, 165, 240, 0};
    return index < 9 ? values[index] : 0;
}
inline bool console_pacing(unsigned index) noexcept {
    return index == 0 || index >= 9;
}
// index in the upscaling_sharpness domain: original, 0%, 10%, ..., 100%
inline const char* sharp(unsigned index) noexcept {
    constexpr const char* values[]{"original", "0", "10", "20", "30", "40", "50", "60", "70", "80", "90", "100"};
    return index < 12 ? values[index] : "original";
}
}
