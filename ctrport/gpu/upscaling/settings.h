// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include <cstdlib>
#include <cstring>
#include <stdexcept>
#include <array>
#include <atomic>
#include <cstdint>
namespace CtrUpscaling {
inline int Mode() {
    static const int value = [] {
        const char* v = std::getenv("CTR_UPSCALER");
        if (!v || !std::strcmp(v, "off")) return 0;
        if (!std::strcmp(v, "fsr1")) return 1;
        if (!std::strcmp(v, "dlss")) return 2;
        if (!std::strcmp(v, "fsr3")) return 3;
        throw std::runtime_error("CTR_UPSCALER must be off, fsr1, dlss or fsr3");
    }();
    return value;
}
// AMD FSR 3.1.6 frame generation (CTR_FRAMEGEN=1).
inline bool FrameGen() {
    static const bool value = [] {
        const char* v = std::getenv("CTR_FRAMEGEN");
        return v && *v && std::strcmp(v, "0") != 0 && std::strcmp(v, "off") != 0;
    }();
    return value;
}
// Populated by successful video-out registration, never inferred from dimensions.
inline std::array<std::atomic<uintptr_t>, 16> display_buffers{};
inline bool IsDisplay(uintptr_t address) {
    if (!address) return false;
    for (auto& item : display_buffers) if (item.load() == address) return true;
    return false;
}
inline void RemoveDisplay(uintptr_t address) {
    for (auto& item : display_buffers) {
        auto expected = address;
        item.compare_exchange_strong(expected, 0);
    }
}
}
