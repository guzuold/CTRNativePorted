// SPDX-License-Identifier: GPL-2.0-only
#pragma once
#include <array>
#include <cstddef>
#include <cstring>
#include <stdexcept>
#include <string>

namespace ctrport::settings::retail_graphics {
inline constexpr const char* retail_sha256 = "39d716b70a299b2546bd39996c8592f9375d3f18f9bb8bf11f9139e9af1eb3f5";
struct Descriptor {
    const char* key;
    std::size_t function_rva;
    std::size_t site_rva;
    std::size_t length;
    std::array<unsigned char, 8> original;
    std::array<unsigned char, 8> disabled;
};
// Override the published value, never the scene-owned source object. The
// original constant publisher still performs cache comparison/dirty tracking.
// These controls do not claim to skip GPU passes or reduce their allocations.
inline constexpr std::array<Descriptor, 5> descriptors{{
    {"screen_space_reflections", 0x36d430, 0x36d470, 4,
     {0x0f,0xb6,0x53,0x40}, {0x31,0xd2,0x90,0x90}},
    {"bloom", 0x963ce0, 0x963d00, 7,
     {0xc4,0xe3,0x79,0x21,0x4f,0x20,0x28}, {0xc4,0xe3,0x79,0x21,0x4f,0x20,0x2c}},
    // Follow the existing camera-cut zero-exposure path on every update;
    // retain previous matrices, bundle rotation and reference accounting.
    {"motion_blur", 0xaf23d0, 0xaf2540, 6,
     {0x0f,0x85,0xff,0x02,0x00,0x00}, {0xe9,0x00,0x03,0x00,0x00,0x90}},
    {"vignette", 0xaf1f90, 0xaf204a, 8,
     {0xc5,0xfa,0x10,0x8b,0x68,0x04,0x00,0x00}, {0xc5,0xf0,0x57,0xc9,0x90,0x90,0x90,0x90}},
    // Zero fade before both the original enable comparison and publication.
    // Applies at the common join for scene and camera-override parameters.
    {"depth_of_field", 0xaf8da0, 0xaf8dfb, 6,
     {0x81,0xe1,0xff,0xff,0xff,0x7f}, {0x45,0x31,0xff,0x31,0xc9,0x90}}
}};
using Options = std::array<bool, descriptors.size()>; // true = off; false = original
struct Readback { std::size_t controls_applied = 0; std::size_t changed_bytes = 0; };

inline void verify(const unsigned char* base, std::size_t extent, const std::string& sha,
                   const Options& selected, bool disabled) {
    if (sha != retail_sha256) throw std::invalid_argument("retail graphics executable identity is unsupported");
    for (std::size_t i = 0; i != descriptors.size(); ++i) {
        if (!selected[i]) continue;
        const auto& d = descriptors[i];
        if (!base || d.site_rva > extent || d.length > extent - d.site_rva)
            throw std::invalid_argument(std::string(d.key) + ": instruction outside image");
        const auto& bytes = disabled ? d.disabled : d.original;
        if (std::memcmp(base + d.site_rva, bytes.data(), d.length))
            throw std::invalid_argument(std::string(d.key) + ": instruction signature mismatch");
    }
}
// Bootstrap only, while the loader image is writable and before guest workers.
// Preflight ALL selected controls before any write. No page protection changes,
// live mutation, retained object pointer, or process-global preference cache.
inline Readback apply(unsigned char* base, std::size_t extent, const std::string& sha,
                      const Options& selected) {
    if (selected == Options{}) return {};
    verify(base, extent, sha, selected, false);
    Readback result;
    for (std::size_t i = 0; i != descriptors.size(); ++i) {
        if (!selected[i]) continue;
        const auto& d = descriptors[i];
        for (std::size_t j = 0; j != d.length; ++j)
            result.changed_bytes += d.original[j] != d.disabled[j];
        std::memcpy(base + d.site_rva, d.disabled.data(), d.length);
        ++result.controls_applied;
    }
    verify(base, extent, sha, selected, true);
    return result;
}
inline void rollback(unsigned char* base, std::size_t extent, const std::string& sha,
                     const Options& selected) {
    if (selected == Options{}) return;
    verify(base, extent, sha, selected, true);
    for (std::size_t i = 0; i != descriptors.size(); ++i) {
        if (!selected[i]) continue;
        const auto& d = descriptors[i];
        std::memcpy(base + d.site_rva, d.original.data(), d.length);
    }
    verify(base, extent, sha, selected, false);
}
} // namespace ctrport::settings::retail_graphics
