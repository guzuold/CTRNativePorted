// SPDX-License-Identifier: GPL-2.0-only
#pragma once
#include "../../retail_map.hpp"
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <stdexcept>
#include <string>
namespace ctrport::settings::quality_controls {
inline constexpr std::size_t mesh_site = 0xeadfce;
inline constexpr unsigned char mesh_original[]{0x0f, 0x85, 0x09, 0x01, 0x00, 0x00};
inline constexpr unsigned char mesh_highest[]{0xe9, 0x0a, 0x01, 0x00, 0x00, 0x90};
// Bootstrap-only: original authored highest-detail branch, including original
// multiplayer camera aggregation and resource-switch scheduling. No asset index
// is invented and no runtime ownership or scene visibility is changed.
inline void verify_mesh(unsigned char* base, std::size_t extent, const std::string& sha, bool patched) {
    if (!retail_map::bound(sha) || !base || retail_map::at(mesh_site) > extent ||
        sizeof(mesh_original) > extent - retail_map::at(mesh_site))
        throw std::invalid_argument("unsupported mesh quality image");
    if (!retail_map::same(base + retail_map::at(mesh_site), mesh_site, patched ? mesh_highest : mesh_original,
                          sizeof(mesh_original)))
        throw std::invalid_argument("mesh quality branch signature mismatch");
}
// 'jne rel32' becomes 'jmp rel32; nop' to the same target (rel32 one larger), and back.
inline void apply_mesh_highest(unsigned char* base, std::size_t extent, const std::string& sha) {
    verify_mesh(base, extent, sha, false);
    auto* p = base + retail_map::at(mesh_site);
    std::int32_t rel;
    std::memcpy(&rel, p + 2, 4);
    ++rel;
    p[0] = 0xe9;
    std::memcpy(p + 1, &rel, 4);
    p[5] = 0x90;
}
inline void rollback_mesh(unsigned char* base, std::size_t extent, const std::string& sha) {
    verify_mesh(base, extent, sha, true);
    auto* p = base + retail_map::at(mesh_site);
    std::int32_t rel;
    std::memcpy(&rel, p + 1, 4);
    --rel;
    p[0] = 0x0f;
    p[1] = 0x85;
    std::memcpy(p + 2, &rel, 4);
}
}
