// SPDX-License-Identifier: GPL-2.0-only
#pragma once
#include "live.hpp"
#include <cstdio>
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <string>
#include <stdexcept>

namespace ctrport::settings::renderer {
inline int anisotropy_request(const char* value) {
    if (!value || std::string(value) == "original") return 0;
    for (const int n : {2, 4, 8, 16})
        if (std::string(value) == std::to_string(n)) return n;
    throw std::invalid_argument("invalid anisotropy request");
}
struct SamplerPolicy {
    bool enabled;
    float maximum;
};
// Preserve non-anisotropic samplers (UI, nearest filtering and comparison data).
// Override only the maximum of samplers the game already marks anisotropic.
inline SamplerPolicy sampler_policy(bool supported, bool original_anisotropic, float original_maximum,
                                    float device_maximum, int requested) {
    if (requested != 0 && requested != 2 && requested != 4 && requested != 8 && requested != 16)
        throw std::invalid_argument("invalid anisotropy level");
    if (!supported || !original_anisotropic) return {false, 1.f};
    if (!std::isfinite(original_maximum) || !std::isfinite(device_maximum) || device_maximum < 1.f)
        throw std::invalid_argument("invalid sampler capability");
    return {true, std::clamp(requested ? float(requested) : original_maximum, 1.f, device_maximum)};
}
inline int environment_anisotropy() {
    static const int value = anisotropy_request(std::getenv("CTR_SETTINGS_ANISOTROPY"));
    return value;
}
struct SharpnessPolicy {
    bool enabled;
    float attenuation;
};
// Sharpness in percent (0 turns RCAS off, 100 is attenuation 0), or the old named levels.
inline SharpnessPolicy sharpness_policy(const char* value) {
    const std::string request = value && value[0] ? value : "original";
    if (request == "original") return {true, 0.25f};
    if (request == "off") return {false, 0.25f};
    if (request == "low") return {true, 2.f};
    if (request == "medium") return {true, 1.f};
    if (request == "high") return {true, 0.f};
    for (int percent = 0; percent <= 100; percent += 10)
        if (request == std::to_string(percent)) return {percent != 0, 2.5f * (1.f - percent / 100.f)};
    throw std::invalid_argument("invalid FSR1 sharpness");
}
inline SharpnessPolicy environment_sharpness() {
    static const auto value = sharpness_policy(std::getenv("CTR_SETTINGS_FSR1_SHARPEN"));
    const auto word = live_gpu::sharpness.word.load(std::memory_order_acquire);
    const auto selected = word >> 32 ? sharpness_policy(live_gpu::sharp(static_cast<unsigned>(word))) : value;
    static std::atomic<std::uint64_t> observed{0};
    if (word && observed.exchange(word) != word) {
        std::printf("settings_live_consumed key=fsr1_sharpen revision=%llu enabled=%u attenuation=%.2f\n",
                    static_cast<unsigned long long>(word >> 32), unsigned(selected.enabled),
                    double(selected.attenuation));
        std::fflush(stdout);
    }
    return selected;
}
} // namespace ctrport::settings::renderer
