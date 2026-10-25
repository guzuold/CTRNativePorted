// SPDX-License-Identifier: GPL-2.0-only
#pragma once
// Live native graphics options for retail CUSA14876 v1.21. They write the same fields the
// game's own "Options > Graphics" debug page edits (registered by the retail renderer setup
// at 0xb45e80, page body 0xb48b30): render debug data at *(*(0x2a7f420)+0x1d8) and the
// FXAA/SMAA statics. No code is patched. Values marked original are never written, so the
// game keeps its own value; any other value is re-asserted every frame because scene data
// may rewrite render state on load.
#include "control_schema.hpp"
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <atomic>
#include <cstdint>
#include <cstdio>
#include <string>

namespace ctrport::settings::native_graphics {
using ctr_online::Json;
constexpr const char* pinned_sha = "39d716b70a299b2546bd39996c8592f9375d3f18f9bb8bf11f9139e9af1eb3f5";
constexpr std::uintptr_t render_static = 0x2a7f420, render_data = 0x1d8, fxaa_static = 0x2a608b0, smaa_static = 0x2a27865;
constexpr std::uintptr_t defocus = 0x58, motion_blur = 0x70, bloom = 0x71, clip_override = 0x128, far_clip = 0x130, fur = 0x178;
// Retail originals, read from a running 1.21 session: FXAA on, SMAA off, bloom/motion blur/DoF
// on, 7 fur shells, track-defined far clip (override off).
constexpr int original_fur = 7;

// -1 = original (leave the game alone); otherwise the requested value.
struct State {
    std::atomic<int> antialiasing{-1};   // 0 off, 2 smaa
    std::atomic<int> bloom{-1}, motion_blur{-1}, depth_of_field{-1};  // 0 off
    std::atomic<int> fur_detail{-1};     // shell count
    std::atomic<int> view_distance{-1};  // far clip
    std::atomic<bool> restore{false};    // one frame writing originals after a switch back
};
inline State state;
inline unsigned char* image = nullptr;

inline bool owns(const std::string& key) {
    return key == "native_antialiasing" || key == "native_bloom" || key == "native_motion_blur" ||
           key == "native_depth_of_field" || key == "native_fur_detail" || key == "native_view_distance";
}

// Accept a stored value for one native key. Returns false for keys it does not own.
inline bool set(const std::string& key, const Json& value) {
    if (!owns(key) || !value.is_string()) return false;
    const auto& v = value.text;
    const bool original = v == "original";
    if (original) state.restore = true;
    if (key == "native_antialiasing") state.antialiasing = original ? -1 : v == "off" ? 0 : 2;
    else if (key == "native_bloom") state.bloom = original ? -1 : 0;
    else if (key == "native_motion_blur") state.motion_blur = original ? -1 : 0;
    else if (key == "native_depth_of_field") state.depth_of_field = original ? -1 : 0;
    else if (key == "native_fur_detail") state.fur_detail = original ? -1 : std::stoi(v);
    else if (key == "native_view_distance") state.view_distance = original ? -1 : std::stoi(v);
    std::printf("{\"scope\":\"settings_native_graphics\",\"key\":\"%s\",\"value\":\"%s\",\"application\":\"live\"}\n", key.c_str(), v.c_str());
    std::fflush(stdout);
    return true;
}

inline void load(const Json& document) {
    for (const auto& item : document.members) if (owns(item.first)) set(item.first, item.second);
    state.restore = false;
}

// Loader barrier: before guest threads start.
inline void configure(unsigned char* retail_image, const std::string& sha, const Json& document) {
    if (sha != pinned_sha) return;
    image = retail_image;
    load(document);
}

inline std::uint64_t read64(std::uint64_t address) {
    std::uint64_t value = 0; SIZE_T got = 0;
    if (!address || !ReadProcessMemory(GetCurrentProcess(), reinterpret_cast<const void*>(address), &value, 8, &got) || got != 8) return 0;
    return value;
}

// Game thread, once per pad poll (once per frame for the polling pad).
inline void apply() {
    if (!image) return;
    const auto render = read64(reinterpret_cast<std::uint64_t>(image + render_static));
    const auto data = render ? read64(render + render_data) : 0;
    if (!data) return;
    auto byte = [&](std::uintptr_t offset) -> unsigned char& { return *reinterpret_cast<unsigned char*>(data + offset); };
    auto& fxaa = *reinterpret_cast<unsigned char*>(image + fxaa_static);
    auto& smaa = *reinterpret_cast<unsigned char*>(image + smaa_static);
    if (state.restore.exchange(false)) {
        // Switching an option back to original restores the retail value once.
        if (state.antialiasing < 0) { fxaa = 1; smaa = 0; }
        if (state.bloom < 0) byte(bloom) = 1;
        if (state.motion_blur < 0) byte(motion_blur) = 1;
        if (state.depth_of_field < 0) byte(defocus) = 1;
        if (state.fur_detail < 0) *reinterpret_cast<int*>(data + fur) = original_fur;
        if (state.view_distance < 0) byte(clip_override) = 0;
    }
    if (const int aa = state.antialiasing; aa >= 0) { fxaa = 0; smaa = aa == 2; }
    if (state.bloom == 0) byte(bloom) = 0;
    if (state.motion_blur == 0) byte(motion_blur) = 0;
    if (state.depth_of_field == 0) byte(defocus) = 0;
    if (const int shells = state.fur_detail; shells > 0) *reinterpret_cast<int*>(data + fur) = shells;
    if (const int distance = state.view_distance; distance > 0) {
        byte(clip_override) = 1; *reinterpret_cast<float*>(data + far_clip) = static_cast<float>(distance);
    }
}
} // namespace ctrport::settings::native_graphics
