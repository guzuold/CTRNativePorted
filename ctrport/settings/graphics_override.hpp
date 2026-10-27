// SPDX-License-Identifier: GPL-2.0-only
#pragma once
#include "../retail_map.hpp"
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <stdexcept>
#include <string>

namespace ctrport::settings::graphics {
inline constexpr const char* retail_121_sha = "39d716b70a299b2546bd39996c8592f9375d3f18f9bb8bf11f9139e9af1eb3f5";
inline constexpr std::size_t aa_function = 0x961b30;
inline constexpr std::size_t aa_gate = aa_function + 0x21;
inline constexpr std::size_t aa_branch = aa_function + 0x28;
// Retail code compares the pass-enable byte and branches around the complete
// update body. Force only that existing skip branch; never force an inactive
// pass on and never mutate scene-owned enable globals. Original restores the
// original dump on the next process start. Installation is bootstrap-only.
inline constexpr std::array<unsigned char, 9> aa_gate_original{0x80,0x3d,0x58,0xed,0x0f,0x02,0x00,0x74,0x47};
inline constexpr std::array<unsigned char, 9> aa_gate_disabled{0x80,0x3d,0x58,0xed,0x0f,0x02,0x00,0xeb,0x47};

struct AAReadback {
    bool override_applied = false;
    bool original_branch_preserved = true;
    std::size_t changed_bytes = 0;
};

inline void require_gate(const unsigned char* base, std::size_t extent, const std::string& sha,
                         const std::array<unsigned char, 9>& expected) {
    if (!retail_map::bound(sha)) throw std::invalid_argument("AA override has no binding for this retail executable");
    const auto gate = retail_map::at(aa_gate);
    if (!base || extent < gate + expected.size()) throw std::invalid_argument("AA gate outside executable image");
    if (!retail_map::same(base + gate, aa_gate, expected.data(), expected.size()))
        throw std::invalid_argument("AA gate differs from the pinned retail instructions");
}

inline AAReadback apply_aa(unsigned char* base, std::size_t extent, const std::string& sha, bool disable) {
    if (!disable) return {};
    require_gate(base, extent, sha, aa_gate_original);
    // This function deliberately has no page-protection or live-patching path.
    // The loader invokes it while the original image is writable and before
    // original game workers enter the rendering code.
    base[retail_map::at(aa_gate) + (aa_branch - aa_gate)] = 0xeb;
    require_gate(base, extent, sha, aa_gate_disabled);
    return {true, false, 1};
}

inline void rollback_aa(unsigned char* base, std::size_t extent, const std::string& sha) {
    require_gate(base, extent, sha, aa_gate_disabled);
    base[retail_map::at(aa_gate) + (aa_branch - aa_gate)] = 0x74;
    require_gate(base, extent, sha, aa_gate_original);
}
} // namespace ctrport::settings::graphics
