// SPDX-FileCopyrightText: Copyright 2024-2026 shadPS4 Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "pm4_registers.hpp"
#include "shader_recompiler/runtime_info.h"
#include "shader_recompiler/profile.h"
#include "video_core/amdgpu/regs.h"
namespace CtrPm4 {
inline bool IsDiagnosticDualSourceBlendFactor(AmdGpu::BlendControl::BlendFactor factor) {
    using BlendFactor = AmdGpu::BlendControl::BlendFactor;
    switch (factor) {
    case BlendFactor::Src1Color:
    case BlendFactor::Src1Alpha:
    case BlendFactor::InvSrc1Color:
    case BlendFactor::InvSrc1Alpha: return true;
    default: return false;
    }
}
// Whole compiler color-key first pass and Fragment runtime branch from pinned
// PipelineCache; second pass is post-compilation pipeline blending, not context.
// This is metadata only: no attachment resource existence/coherence admission.
inline int BuildFragmentRuntimeInfo(const Registers& captured, const Shader::Profile& profile,
                                    Shader::RuntimeInfo& output) {
    static_assert(sizeof(AmdGpu::Regs) == sizeof(captured.words));
    AmdGpu::Regs regs{};
    std::memcpy(regs.reg_array.data(), captured.words.data(), sizeof(regs));
    if (regs.num_interp > 32 || regs.ps_program.settings.num_user_regs > 16 ||
        regs.stage_enable.raw != AmdGpu::ShaderStageEnable::Vs)
        return 88;
    std::array<Shader::PsColorBuffer, Shader::MaxColorBuffers> colors{};
    const bool skip_cb_binding = regs.color_control.mode == AmdGpu::ColorControl::OperationMode::Disable;
    for (s32 cb = 0; cb < AmdGpu::NUM_COLOR_BUFFERS && !skip_cb_binding; ++cb) {
        const auto& col_buf = regs.color_buffers[cb];
        if (!col_buf || !regs.color_target_mask.GetMask(cb)) continue;
        const auto components = AmdGpu::NumComponents(AmdGpu::DataFormat(col_buf.info.format));
        if (components < 1 || components > 4) return 88;
        auto& color_buffer = colors[cb];
        color_buffer.data_format = col_buf.GetDataFmt();
        color_buffer.num_format = col_buf.GetNumberFmt();
        color_buffer.num_conversion = col_buf.GetNumberConversion();
        color_buffer.export_format = regs.color_export_format.GetFormat(cb);
        color_buffer.swizzle = col_buf.Swizzle();
    }
    Shader::RuntimeInfo info{};
    info.Initialize(Shader::Stage::Fragment);
    const auto& program = regs.ps_program;
    info.num_user_data = program.settings.num_user_regs;
    info.num_input_vgprs = program.settings.vgpr_comp_cnt;
    info.num_allocated_vgprs = program.NumVgprs();
    info.fp_denorm_mode32 = program.settings.fp_denorm_mode32;
    info.fp_denorm_mode16_64 = program.settings.fp_denorm_mode64;
    info.fp_round_mode32 = program.settings.fp_round_mode32;
    info.fp_round_mode16_64 = program.settings.fp_round_mode64;
    info.fs_info.en_flags = regs.ps_input_ena;
    info.fs_info.addr_flags = regs.ps_input_addr;
    info.fs_info.num_inputs = regs.num_interp;
    info.fs_info.z_export_format = regs.z_export_format;
    u8 stencil_ref_export_enable = regs.depth_shader_control.stencil_op_val_export_enable |
                                   regs.depth_shader_control.stencil_test_val_export_enable;
    info.fs_info.mrtz_mask = regs.depth_shader_control.z_export_enable | (stencil_ref_export_enable << 1) |
                             (regs.depth_shader_control.mask_export_enable << 2) |
                             (regs.depth_shader_control.coverage_to_mask_enable << 3);
    const auto& cb0_blend = regs.blend_control[0];
    if (cb0_blend.enable) {
        info.fs_info.dual_source_blending = IsDiagnosticDualSourceBlendFactor(cb0_blend.color_dst_factor) ||
                                            IsDiagnosticDualSourceBlendFactor(cb0_blend.color_src_factor);
        if (cb0_blend.separate_alpha_blend) {
            info.fs_info.dual_source_blending |= IsDiagnosticDualSourceBlendFactor(cb0_blend.alpha_dst_factor) ||
                                                 IsDiagnosticDualSourceBlendFactor(cb0_blend.alpha_src_factor);
        }
    } else {
        info.fs_info.dual_source_blending = false;
    }
    const auto& ps_inputs = regs.ps_inputs;
    for (u32 i = 0; i < regs.num_interp; i++) {
        info.fs_info.inputs[i] = {
            .param_index = u8(ps_inputs[i].input_offset),
            .is_default = bool(ps_inputs[i].use_default),
            .is_flat = bool(ps_inputs[i].flat_shade),
            .default_value = u8(ps_inputs[i].default_value),
        };
    }
    for (u32 i = 0; i < Shader::MaxColorBuffers; i++) {
        info.fs_info.color_buffers[i] = colors[i];
    }
    // Lowered user clip planes ride the same emulation path as guest-exported distances, so
    // the fragment side arms whenever the hardware vertex stage lowers them, keeping its input
    // locations in sync with the shifted vertex outputs.
    const bool lowers_user_clip_planes = regs.clipper_control.user_clip_plane_enable &&
                                         !regs.stage_enable.IsStageEnabled(static_cast<u32>(Shader::Stage::Geometry));
    info.fs_info.clip_distance_emulation =
        ((regs.vs_output_control.clip_distance_enable &&
          !regs.stage_enable.IsStageEnabled(static_cast<u32>(Shader::Stage::Local))) ||
         lowers_user_clip_planes) &&
        profile.needs_clip_distance_emulation;

    output = info;
    return 0;
}
}
