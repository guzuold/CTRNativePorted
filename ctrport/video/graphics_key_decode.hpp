// SPDX-FileCopyrightText: Copyright 2024-2026 shadPS4 Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "fragment_runtime_info.hpp"
#include <bit>
namespace CtrPm4 {
// Public RefreshGraphicsKey register passes, pinned e3ce810f3a653f43ac64ebab63023de281a4103a.
// Stage compilation is supplied by the same-pass MRT mask; this is not resource admission.
struct DecodedGraphicsKey {
    uint32_t patch_control_points{}, num_color_attachments{}, mrt_mask{};
    std::array<Shader::PsColorBuffer, 8> color_buffers{};
    std::array<AmdGpu::BlendControl, 8> blend_controls{};
    std::array<uint32_t, 8> write_masks{};
    AmdGpu::ColorBufferMask cb_shader_mask{};
    AmdGpu::ColorControl::LogicOp logic_op{};
    uint8_t num_samples{}, depth_samples{};
    std::array<uint8_t, 8> color_samples{};
    AmdGpu::DepthBuffer::ZFormat z_format{};
    AmdGpu::DepthBuffer::StencilFormat stencil_format{};
    bool depth_clamp_enable{}, depth_clip_enable{};
    AmdGpu::PrimitiveType prim_type{};
    AmdGpu::PolygonMode polygon_mode{};
    AmdGpu::ClipSpace clip_space{};
    AmdGpu::ProvokingVtxLast provoking_vtx_last{};
};
inline int DecodeGraphicsKey(const Registers& captured, uint32_t mrt_mask, bool mixed_any_samples,
                             bool mixed_depth_samples, DecodedGraphicsKey& output) {
    if (mrt_mask & ~255u) return 60;
    AmdGpu::Regs regs{};
    static_assert(sizeof(regs) == sizeof(captured.words));
    std::memcpy(regs.reg_array.data(), captured.words.data(), sizeof(regs));
    if (regs.stage_enable.raw != AmdGpu::ShaderStageEnable::Vs) return 88;
    DecodedGraphicsKey key{};
    const bool db_enabled = regs.depth_buffer.DepthValid() || regs.depth_buffer.StencilValid();
    key.z_format =
        regs.depth_buffer.DepthValid() ? regs.depth_buffer.z_info.format : AmdGpu::DepthBuffer::ZFormat::Invalid;
    key.stencil_format = regs.depth_buffer.StencilValid() ? regs.depth_buffer.stencil_info.format
                                                          : AmdGpu::DepthBuffer::StencilFormat::Invalid;
    key.depth_clamp_enable = !regs.depth_render_override.disable_viewport_clamp;
    key.depth_clip_enable = regs.clipper_control.ZclipEnable();
    key.clip_space = regs.clipper_control.clip_space;
    key.provoking_vtx_last = regs.polygon_control.provoking_vtx_last;
    key.prim_type = regs.primitive_type;
    key.polygon_mode = regs.polygon_control.PolyMode();
    key.patch_control_points = regs.stage_enable.hs_en ? regs.ls_hs_config.hs_input_control_points : 0;
    key.logic_op = regs.color_control.rop3;
    key.depth_samples = db_enabled ? regs.depth_buffer.NumSamples() : 1;
    key.num_samples = key.depth_samples;
    key.cb_shader_mask = regs.color_shader_mask;
    const bool skip = regs.color_control.mode == AmdGpu::ColorControl::OperationMode::Disable;
    for (int cb = 0; cb < 8 && !skip; ++cb) {
        const auto& color = regs.color_buffers[cb];
        if (!color || !regs.color_target_mask.GetMask(cb)) continue;
        auto& target = key.color_buffers[cb];
        target.data_format = color.GetDataFmt();
        target.num_format = color.GetNumberFmt();
        target.num_conversion = color.GetNumberConversion();
        target.export_format = regs.color_export_format.GetFormat(cb);
        target.swizzle = color.Swizzle();
    }
    key.mrt_mask = mrt_mask;
    key.num_color_attachments = std::bit_width(mrt_mask);
    uint8_t samples = 0;
    bool same = true;
    for (uint32_t cb = 0; cb < key.num_color_attachments && !skip; ++cb) {
        const auto& color = regs.color_buffers[cb];
        const auto mask = regs.color_target_mask.GetMask(cb);
        if (!color || !mask) continue;
        if (!(mrt_mask & (1u << cb))) {
            key.color_buffers[cb] = {};
            continue;
        }
        if (regs.blend_control[cb].enable && !color.info.blend_bypass) key.blend_controls[cb] = regs.blend_control[cb];
        key.write_masks[cb] = key.color_buffers[cb].swizzle.ApplyMask(mask);
        const auto previous = std::exchange(samples, color.NumSamples());
        same &= samples == previous || previous == 0;
        key.color_samples[cb] = samples;
        key.num_samples = std::max(key.num_samples, samples);
    }
    if (samples != 0) {
        const bool depth_mismatch = db_enabled && samples != key.depth_samples;
        if ((!same && !mixed_any_samples) || (same && depth_mismatch && !mixed_depth_samples)) {
            key.color_samples.fill(key.depth_samples);
            key.num_samples = key.depth_samples;
        }
    }
    output = key;
    return 0;
}
}
