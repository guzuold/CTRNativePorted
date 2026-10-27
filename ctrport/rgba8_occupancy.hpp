// SPDX-License-Identifier: GPL-2.0-only
#pragma once
#include <cstdint>
#include <cstring>
#include <ostream>
#include <vector>
namespace ctrport {
// Metadata only, including mapped padding. No image decoding or pixel persistence.
// RGBA8/BGRA8 RGB occupancy is invariant under channel swaps and tiled permutations.
inline void log_rgba8_occupancy(std::ostream& log, uint64_t word, uint64_t address,
                                const std::vector<unsigned char>& bytes, const char* origin) {
    if (bytes.empty() || bytes.size() % 4) return;
    uint64_t rgb = 0, alpha = 0;
    for (size_t offset = 0; offset < bytes.size(); offset += 4) {
        uint32_t value = 0;
        std::memcpy(&value, bytes.data() + offset, 4);
        if (value & 0xffffffU) ++rgb;
        if (value >> 24) ++alpha;
    }
    log << "{\"scope\":\"original_GnmSubmitFlip_RGBA8_occupancy\",\"word\":" << word << ",\"address\":" << address
        << ",\"bytes\":" << bytes.size() << ",\"RGB_nonzero_DWORDs\":" << rgb << ",\"alpha_nonzero_DWORDs\":" << alpha
        << ",\"origin\":\"" << origin << "\",\"mapped_padding_included\":true,\"pixel_payload_persisted\":false}\n";
}
}
