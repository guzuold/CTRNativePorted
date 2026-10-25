// SPDX-License-Identifier: GPL-2.0-only
#pragma once
#include <atomic>
#include <cstdint>

namespace ctrport::settings::live_gpu {
// One atomic word per control: the high word is a revision, the low word an
// index. Consumers load once at their existing render/present boundary. No
// environment mutation, resource destruction, or guest code patching is needed.
enum : unsigned { presentation_limit=1, fsr1_sharpen=2 };
struct Slot {
    std::atomic<std::uint64_t> word{0}; // revision zero = startup configuration
    bool set(unsigned index, unsigned count) noexcept {
        if(index>=count)return false;
        auto old=word.load(std::memory_order_relaxed);
        for(;;) {
            const auto next=(((old>>32)+1)<<32)|index;
            if(word.compare_exchange_weak(old,next,std::memory_order_release,std::memory_order_relaxed))return true;
        }
    }
};
inline Slot presentation, sharpness;
inline unsigned set(unsigned key,unsigned index) noexcept {
    if(key==presentation_limit)return presentation.set(index,5)?1u:0u;
    if(key==fsr1_sharpen)return sharpness.set(index,5)?1u:0u;
    return 0;
}
inline unsigned cap(unsigned index) noexcept {
    constexpr unsigned values[]{0,30,60,90,120};return index<5?values[index]:0;
}
inline const char* sharp(unsigned index) noexcept {
    constexpr const char* values[]{"original","off","low","medium","high"};return index<5?values[index]:"original";
}
}
