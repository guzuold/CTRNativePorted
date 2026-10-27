// SPDX-License-Identifier: GPL-2.0-only
#pragma once
#include <atomic>
#include <cstdlib>
#include <cstring>
#include <cstdio>
#include <stdexcept>
namespace ctrport::settings::display {
inline const char* env(const char* key) {
    const auto p = std::getenv(key);
    return p ? p : "original";
}
inline bool native_hdr() {
    const auto p = env("CTR_SETTINGS_HDR");
    if (!std::strcmp(p, "original") || !std::strcmp(p, "off")) return false;
    if (!std::strcmp(p, "native")) {
        const auto up = std::getenv("CTR_UPSCALER");
        if (up && up[0] && std::strcmp(up, "off")) throw std::runtime_error("native HDR requires scene upscaler off");
        return true;
    }
    throw std::runtime_error("invalid native HDR setting");
}
inline float numeric(const char* key, const char* const* allowed, unsigned count, float original) {
    const auto p = env(key);
    if (!std::strcmp(p, "original")) return original;
    for (unsigned i = 0; i < count; ++i)
        if (!std::strcmp(p, allowed[i])) return std::strtof(p, nullptr);
    throw std::runtime_error("invalid display calibration value");
}
struct Calibration {
    float gamma, peak, exposure;
};
inline Calibration calibration() {
    const char* gamma[] = {"0.8", "0.9", "1.1", "1.2"};
    const char* peak[] = {"400", "600", "1000", "1500"};
    const char* exposure[] = {"0.75", "1.25", "1.5"};
    return {numeric("CTR_SETTINGS_GAMMA", gamma, 4, 0), numeric("CTR_SETTINGS_HDR_PEAK", peak, 4, 0),
            numeric("CTR_SETTINGS_HDR_EXPOSURE", exposure, 3, 1)};
}
// 0 unrequested/unobserved; 1 successful native dispatch; 2 FSR1 fallback;
// 3 no suitable scene for either path. This is runtime evidence, not a feature bitmap.
inline std::atomic<unsigned> temporal_state{0};
// bits 0 FSR3 context initialized, 1 NVIDIA NGX device initialized. Neither means
// a frame dispatched; see temporal_state for observed output.
inline std::atomic<unsigned> temporal_capabilities{0};
inline void note_temporal(unsigned state, int requested) {
    const auto previous = temporal_state.exchange(state, std::memory_order_relaxed);
    if (previous != state) {
        std::printf("CTR_SETTINGS_TEMPORAL requested=%d state=%u\n", requested, state);
        std::fflush(stdout);
    }
}
inline bool hdr_allowed(bool requested, bool surface, bool title) {
    return requested && surface && title;
}
}
