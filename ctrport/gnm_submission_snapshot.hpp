// SPDX-License-Identifier: GPL-2.0-only
#pragma once
#include "owned_guest_copy.hpp"
#include <array>
#include <vector>
namespace ctrport {
struct GnmCommandSnapshot {
    uint64_t guest_address = 0;
    std::vector<uint32_t> words;
};
struct GnmSubmissionSnapshot {
    std::vector<GnmCommandSnapshot> draw, constant;
};
// CPU-owned snapshots only. No GPU execution, coherence or completion assertion.
// Bounds are this host transport's supported scope, never invented PS4 errno.
// Prepare all arrays and buffers before publishing output; failure preserves it.
inline bool capture_gnm_submission(uint32_t count, uint64_t draw_addresses, uint64_t draw_sizes,
                                   uint64_t constant_addresses, uint64_t constant_sizes,
                                   GnmSubmissionSnapshot& output) {
    if (!count || count > 16 || !draw_addresses || !draw_sizes || bool(constant_addresses) != bool(constant_sizes))
        return false;
    const auto memory = make_owned_guest_copy_api();
    std::array<uint64_t, 16> da{}, ca{};
    std::array<uint32_t, 16> ds{}, cs{};
    if (memory.read(memory.context, draw_addresses, da.data(), size_t(count) * 8) ||
        memory.read(memory.context, draw_sizes, ds.data(), size_t(count) * 4))
        return false;
    if (constant_addresses && (memory.read(memory.context, constant_addresses, ca.data(), size_t(count) * 8) ||
                               memory.read(memory.context, constant_sizes, cs.data(), size_t(count) * 4)))
        return false;
    GnmSubmissionSnapshot prepared;
    prepared.draw.resize(count);
    prepared.constant.resize(count);
    size_t total = 0;
    for (uint32_t index = 0; index < count; ++index)
        for (unsigned kind = 0; kind < 2; ++kind) {
            const uint32_t bytes = kind ? cs[index] : ds[index];
            const uint64_t address = kind ? ca[index] : da[index];
            if (bytes > 0x3ffffc || bytes % 4 || total > 16 * 1024 * 1024 - bytes) return false;
            total += bytes;
            auto& buffer = kind ? prepared.constant[index] : prepared.draw[index];
            buffer.guest_address = address;
            buffer.words.resize(bytes / 4);
            if (bytes && ((address & 3) || memory.read(memory.context, address, buffer.words.data(), bytes)))
                return false;
        }
    output = std::move(prepared);
    return true;
}
}
