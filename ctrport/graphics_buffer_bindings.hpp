// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "graphics_pair_client.hpp"
namespace ctrport {
struct LiveGraphicsBuffer {
    const GraphicsStageOwner* stage;
    uint32_t sharp, type;
    uint64_t token, bytes;
};
struct GraphicsBufferWrite {
    uint32_t binding, slot, sharp, type;
    bool vertex, null_clip_planes;
    uint64_t token, bytes;
};
// Host input preparation only. Tokens are borrowed from the live owner batch;
// this table must not escape that callback or be consumed after batch close.
// Null ClipPlanes policy: pinned public shadPS4 e3ce810f3a653f43ac64ebab63023de281a4103a,
// renderer_vulkan/vk_rasterizer.cpp BindBuffers. Requires real nullDescriptor
// feature and descriptor writes downstream; never create a fabricated buffer.
inline int prepare_graphics_buffer_writes(const GraphicsPairOwner& pair, const std::vector<LiveGraphicsBuffer>& live,
                                          std::vector<GraphicsBufferWrite>& output) {
    if (!pair.fragment_inputs || !pair.vertex_inputs) return 88;
    auto checked = pair;
    const int joined = graphics_pair_attach_inputs(checked, pair.fragment_inputs, pair.vertex_inputs, 1u);
    if (joined) return joined;
    std::vector<GraphicsBufferWrite> prepared;
    prepared.reserve(48);
    for (bool vertex : {false, true}) {
        const auto& input = *(vertex ? pair.vertex_inputs : pair.fragment_inputs);
        const auto& contract = vertex ? pair.vertex_bindings : pair.fragment_bindings;
        if (contract.version != 1 || contract.size != sizeof(contract) || contract.descriptor_count > 120) return 112;
        for (uint32_t i = 0; i < contract.descriptor_count; ++i) {
            const auto& binding = contract.descriptors[i];
            if (binding.kind != 0) continue;
            if (binding.count != 1 || binding.storage != 1 || binding.buffer_slot < contract.buffer_before ||
                binding.buffer_slot >= contract.buffer_after)
                return 88;
            // Both compiler metadata exports enumerate the same info.buffers in order.
            // Sharp0 alone is ambiguous: inline ClipPlanes and a guest buffer may share it.
            const uint32_t ordinal = binding.buffer_slot - contract.buffer_before;
            if (ordinal >= input.resources.buffers || ordinal >= input.resources.binding_count) return 112;
            const auto* identity = &input.resources.bindings[ordinal];
            if (identity->kind != 0 || identity->sharp_index != binding.sharp_index) return 112;
            GraphicsBufferWrite item{
                binding.binding, binding.buffer_slot, binding.sharp_index, identity->buffer_type, vertex, false, 0, 0};
            if (identity->buffer_type == 6 && (identity->flags & 2)) {
                const auto clip = std::find_if(input.clip_planes.begin(), input.clip_planes.end(),
                                               [&](const auto& value) { return value.sharp == binding.sharp_index; });
                if (clip == input.clip_planes.end() || clip->status || clip->value.enabled_mask || clip->value.bytes)
                    return 88;
                item.null_clip_planes = true;
                item.bytes = UINT64_MAX;
            } else {
                const LiveGraphicsBuffer* found = nullptr;
                for (const auto& resource : live)
                    if (resource.stage == &input && resource.sharp == binding.sharp_index &&
                        resource.type == identity->buffer_type) {
                        if (found) return 112;
                        found = &resource;
                    }
                if (!found || !found->token || !found->bytes) return 88;
                uint64_t expected = 0;
                if (identity->buffer_type == 1) {
                    for (const auto& source : input.flatbufs)
                        if (source.sharp == binding.sharp_index) expected = source.bytes;
                } else if (identity->buffer_type == 0) {
                    for (const auto& source : input.buffers)
                        if (source.sharp == binding.sharp_index) expected = source.bytes;
                } else
                    return 88;
                if (!expected || found->bytes != expected) return 112;
                item.token = found->token;
                item.bytes = found->bytes;
            }
            for (const auto& previous : prepared)
                if (previous.binding == item.binding || previous.slot == item.slot) return 112;
            prepared.push_back(item);
        }
    }
    output = std::move(prepared);
    return 0;
}
struct LiveGraphicsSampler {
    const GraphicsStageOwner* stage;
    uint32_t sharp;
    uint64_t token;
};
struct GraphicsSamplerWrite {
    uint32_t binding, sharp;
    bool vertex;
    uint64_t token;
};
inline int prepare_graphics_sampler_writes(const GraphicsPairOwner& pair, const std::vector<LiveGraphicsSampler>& live,
                                           std::vector<GraphicsSamplerWrite>& output) {
    if (!pair.fragment_inputs || !pair.vertex_inputs) return 88;
    auto checked = pair;
    const int joined = graphics_pair_attach_inputs(checked, pair.fragment_inputs, pair.vertex_inputs, 4u);
    if (joined) return joined;
    std::vector<GraphicsSamplerWrite> prepared;
    for (bool vertex : {false, true}) {
        const auto& input = *(vertex ? pair.vertex_inputs : pair.fragment_inputs);
        const auto& contract = vertex ? pair.vertex_bindings : pair.fragment_bindings;
        if (contract.version != 1 || contract.size != sizeof(contract) || contract.descriptor_count > 120) return 112;
        for (uint32_t index = 0; index < contract.descriptor_count; ++index) {
            const auto& binding = contract.descriptors[index];
            if (binding.kind != 2) continue;
            if (binding.count != 1 || binding.storage || binding.buffer_slot != UINT32_MAX) return 88;
            const LiveGraphicsSampler* found = nullptr;
            for (const auto& item : live)
                if (item.stage == &input && item.sharp == binding.sharp_index) {
                    if (found) return 112;
                    found = &item;
                }
            if (!found || !found->token) return 88;
            for (const auto& item : prepared)
                if (item.binding == binding.binding) return 112;
            prepared.push_back({binding.binding, binding.sharp_index, vertex, found->token});
        }
    }
    output = std::move(prepared);
    return 0;
}
struct LiveGraphicsImage {
    const GraphicsStageOwner* stage;
    uint32_t sharp;
    uint64_t token;
};
struct GraphicsImageWrite {
    uint32_t binding, sharp;
    bool vertex;
    uint64_t token;
    bool semantic_null = false;
};
inline int prepare_graphics_image_writes(const GraphicsPairOwner& pair, const std::vector<LiveGraphicsImage>& live,
                                         std::vector<GraphicsImageWrite>& output) {
    if (!pair.fragment_inputs || !pair.vertex_inputs) return 88;
    auto checked = pair;
    const int joined = graphics_pair_attach_inputs(checked, pair.fragment_inputs, pair.vertex_inputs, 2u);
    if (joined) return joined;
    std::vector<GraphicsImageWrite> prepared;
    for (bool vertex : {false, true}) {
        const auto& input = *(vertex ? pair.vertex_inputs : pair.fragment_inputs);
        const auto& contract = vertex ? pair.vertex_bindings : pair.fragment_bindings;
        if (contract.version != 1 || contract.size != sizeof(contract) || contract.descriptor_count > 120) return 112;
        for (uint32_t index = 0; index < contract.descriptor_count; ++index) {
            const auto& binding = contract.descriptors[index];
            if (binding.kind != 1) continue;
            if (binding.count != 1 || binding.storage || binding.buffer_slot != UINT32_MAX) return 88;
            const LiveGraphicsImage* found = nullptr;
            for (const auto& item : live)
                if (item.stage == &input && item.sharp == binding.sharp_index) {
                    if (found) return 112;
                    found = &item;
                }
            bool semantic_null = false;
            for (const auto& d : input.descriptors)
                if (d.kind == 1 && d.sharp == binding.sharp_index && verified_effective_null_image(d)) {
                    for (uint32_t n = 0; n < input.resources.binding_count; ++n) {
                        const auto& r = input.resources.bindings[n];
                        if (r.kind == 1 && r.sharp_index == binding.sharp_index &&
                            !(r.flags & (1u | 8u | 16u | 32u | 64u)))
                            semantic_null = true;
                    }
                }
            if (semantic_null) {
                if (found) return 112;
            } else if (!found || !found->token)
                return 88;
            for (const auto& item : prepared)
                if (item.binding == binding.binding) return 112;
            prepared.push_back(
                {binding.binding, binding.sharp_index, vertex, semantic_null ? 0 : found->token, semantic_null});
        }
    }
    output = std::move(prepared);
    return 0;
}
}
