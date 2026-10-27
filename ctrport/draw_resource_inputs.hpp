// SPDX-License-Identifier: GPL-2.0-only
#pragma once
#include "vertex_buffer_owners.hpp"
#include "draw_packet_inputs.hpp"
namespace ctrport {
// Session-local handoff to a future whole graphics pipeline. Tokens are only
// joined here; readiness never means binding, GPU execution or writer completion.
struct DrawResourceInputs {
    uint32_t word = 0;
    uint64_t layout = 0, depth = 0, index = 0;
    std::array<uint64_t, 8> colors{};
    std::vector<LiveVertexBuffer> vertices;
};
inline int prepare_draw_resources(uint32_t word, const DrawPacketInputs& packet, const CtrGraphicsKeyV1& key,
                                  const GraphicsVertexFetchOwner& fetch, uint64_t layout, bool descriptors_complete,
                                  const std::array<uint64_t, 8>& colors, uint64_t depth, uint64_t index,
                                  const std::vector<LiveVertexBuffer>& live, DrawResourceInputs& output,
                                  const CtrGraphicsDynamicStateV1* dynamic = nullptr) {
    if (key.version != 1 || key.size != sizeof(key) || key.num_color_attachments > 8 ||
        fetch.value.count > fetch.formats.size())
        return 60;
    if (fetch.status || fetch.format_status || fetch.physical_status || !layout || !descriptors_complete) return 88;
    if (packet.indexed && !index) return 88;
    if (dynamic && (dynamic->version != 1 || dynamic->size != sizeof(*dynamic))) return 60;
    // A valid but stale depth format does not enable depth processing. Preserve
    // the original key; only exact decoded disabled tests/writes remove the need
    // for a depth owner. Missing dynamic state retains the conservative policy.
    const bool depth_required = !dynamic || dynamic->depth_test || dynamic->depth_write || dynamic->stencil_test;
    if ((key.z_format || key.stencil_format) && depth_required && !depth) return 88;
    for (uint32_t slot = 0; slot < key.num_color_attachments; ++slot)
        if ((key.mrt_mask & (1u << slot)) && key.colors[slot].write_mask && !colors[slot]) return 88;
    DrawResourceInputs result;
    result.word = word;
    result.layout = layout;
    result.depth = depth;
    result.index = index;
    result.colors = colors;
    for (uint32_t i = 0; i < fetch.value.count; ++i) {
        const auto& input = fetch.value.inputs[i];
        const LiveVertexBuffer* match = nullptr;
        for (const auto& vertex : live)
            if (vertex.word == word && vertex.binding == input.binding) {
                if (match) return 60;
                match = &vertex;
            }
        if (!match || !match->token || match->location != input.location || match->stride != input.stride ||
            match->input_rate != input.input_rate || match->divisor != input.divisor ||
            match->format != fetch.formats[i] || match->bytes != input.buffer_size ||
            match->guest_address != input.guest_address)
            return 88;
        result.vertices.push_back(*match);
    }
    output = std::move(result);
    return 0;
}
// Subtraction avoids address-end overflow. Invalid/unknown ranges never admit
// reuse of a prefetched CPU snapshot after an original GPU writer.
inline bool draw_snapshot_disjoint(uint64_t address, uint64_t bytes,
                                   const std::vector<std::pair<uint64_t, uint64_t>>& writers) {
    if (!address || !bytes || address > UINT64_MAX - bytes) return false;
    for (const auto& writer : writers) {
        const auto base = writer.first, size = writer.second;
        if (!base || !size || base > UINT64_MAX - size) return false;
        if (address < base + size && base < address + bytes) return false;
    }
    return true;
}
}
