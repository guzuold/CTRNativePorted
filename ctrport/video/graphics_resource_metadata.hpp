// SPDX-License-Identifier: GPL-2.0-only
#pragma once
#include "graphics_companion.h"
#include "shader_recompiler/info.h"
#include <bit>
namespace CtrPm4 {
inline CtrGraphicsResourcesV1 CaptureCompilerResourceMetadata(const Shader::Info& info) {
    CtrGraphicsResourcesV1 out{};
    out.version = 1;
    out.size = sizeof(out);
    out.buffers = static_cast<uint32_t>(info.buffers.size());
    out.images = static_cast<uint32_t>(info.images.size());
    out.samplers = static_cast<uint32_t>(info.samplers.size());
    out.fmasks = static_cast<uint32_t>(info.fmasks.size());
    out.readconst_types = static_cast<uint32_t>(info.readconst_types);
    out.userdata_mask = info.ud_mask.mask;
    out.flattened_userdata_words = static_cast<uint32_t>(info.flattened_ud_buf.size());
    out.has_fetch_shader = info.has_fetch_shader;
    out.fetch_shader_sgpr_base = info.fetch_shader_sgpr_base;
    out.mrt_mask = info.mrt_mask;
    for (auto value : info.loads.flags) out.loaded_attribute_components += std::popcount(value);
    for (auto value : info.stores.flags) out.stored_attribute_components += std::popcount(value);
    for (const auto& buffer : info.buffers) {
        auto& item = out.bindings[out.binding_count++];
        item.kind = 0;
        item.sharp_index = buffer.sharp_idx;
        item.flags = uint32_t(buffer.is_written) | (uint32_t(bool(buffer.inline_cbuf)) << 1) |
                     (uint32_t(buffer.is_formatted) << 2);
        item.buffer_type = uint32_t(buffer.buffer_type);
    }
    for (const auto& image : info.images) {
        auto& item = out.bindings[out.binding_count++];
        item.kind = 1;
        item.sharp_index = image.sharp_idx;
        item.flags = uint32_t(image.is_written) | (uint32_t(image.is_depth) << 3) | (uint32_t(image.is_atomic) << 4) |
                     (uint32_t(image.is_array) << 5) | (uint32_t(image.is_r128) << 6);
    }
    for (const auto& sampler : info.samplers) {
        auto& item = out.bindings[out.binding_count++];
        item.kind = 2;
        item.sharp_index = sampler.sharp_idx;
        item.flags = uint32_t(sampler.is_inline_sampler) << 1;
        item.associated_image = sampler.associated_image;
    }
    for (const auto& fmask : info.fmasks) {
        auto& item = out.bindings[out.binding_count++];
        item.kind = 3;
        item.sharp_index = fmask.sharp_idx;
    }
    return out;
}
}
