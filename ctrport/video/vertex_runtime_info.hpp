// SPDX-FileCopyrightText: Copyright 2024-2026 shadPS4 Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "pm4_registers.hpp"
#include "shader_recompiler/runtime_info.h"
#include "video_core/amdgpu/regs.h"
namespace CtrPm4 {
using Shader::Output;
// Whole public MapOutputs and ordinary Vertex branch, pinned revision
// e3ce810f3a653f43ac64ebab63023de281a4103a. Caller supplies the actual
// enabled depth-clip-control capability. Other routing is explicitly missing.
inline u32 MapVertexOutputs(std::span<Shader::OutputMap, 3> outputs, const AmdGpu::VsOutputControl& ctl) {
    u32 num_outputs = 0;

    if (ctl.vs_out_misc_enable) {
        auto& misc_vec = outputs[num_outputs++];
        misc_vec[0] = ctl.use_vtx_point_size ? Output::PointSize : Output::None;
        misc_vec[1] = ctl.use_vtx_edge_flag
                          ? Output::EdgeFlag
                          : (ctl.use_vtx_gs_cut_flag ? Output::GsCutFlag : Output::None);
        misc_vec[2] =
            ctl.use_vtx_kill_flag
                ? Output::KillFlag
                : (ctl.use_vtx_render_target_idx ? Output::RenderTargetIndex : Output::None);
        misc_vec[3] = ctl.use_vtx_viewport_idx ? Output::ViewportIndex : Output::None;
    }

    if (ctl.vs_out_ccdist0_enable) {
        auto& ccdist0 = outputs[num_outputs++];
        ccdist0[0] = ctl.IsClipDistEnabled(0)
                         ? Output::ClipDist0
                         : (ctl.IsCullDistEnabled(0) ? Output::CullDist0 : Output::None);
        ccdist0[1] = ctl.IsClipDistEnabled(1)
                         ? Output::ClipDist1
                         : (ctl.IsCullDistEnabled(1) ? Output::CullDist1 : Output::None);
        ccdist0[2] = ctl.IsClipDistEnabled(2)
                         ? Output::ClipDist2
                         : (ctl.IsCullDistEnabled(2) ? Output::CullDist2 : Output::None);
        ccdist0[3] = ctl.IsClipDistEnabled(3)
                         ? Output::ClipDist3
                         : (ctl.IsCullDistEnabled(3) ? Output::CullDist3 : Output::None);
    }

    if (ctl.vs_out_ccdist1_enable) {
        auto& ccdist1 = outputs[num_outputs++];
        ccdist1[0] = ctl.IsClipDistEnabled(4)
                         ? Output::ClipDist4
                         : (ctl.IsCullDistEnabled(4) ? Output::CullDist4 : Output::None);
        ccdist1[1] = ctl.IsClipDistEnabled(5)
                         ? Output::ClipDist5
                         : (ctl.IsCullDistEnabled(5) ? Output::CullDist5 : Output::None);
        ccdist1[2] = ctl.IsClipDistEnabled(6)
                         ? Output::ClipDist6
                         : (ctl.IsCullDistEnabled(6) ? Output::CullDist6 : Output::None);
        ccdist1[3] = ctl.IsClipDistEnabled(7)
                         ? Output::ClipDist7
                         : (ctl.IsCullDistEnabled(7) ? Output::CullDist7 : Output::None);
    }

    return num_outputs;
}


inline int BuildVertexRuntimeInfo(const Registers& captured,bool depthClipControl,
                                  Shader::RuntimeInfo& output) {
    static_assert(sizeof(AmdGpu::Regs)==sizeof(captured.words));
    AmdGpu::Regs regs{};
    std::memcpy(regs.reg_array.data(),captured.words.data(),sizeof(regs));
    if(regs.stage_enable.raw!=AmdGpu::ShaderStageEnable::Vs) return 88;
    Shader::RuntimeInfo info{};info.Initialize(Shader::Stage::Vertex);
    const auto& program=regs.vs_program;
    info.num_user_data=program.settings.num_user_regs;
    if(info.num_user_data>16) return 88;
    info.num_input_vgprs=program.settings.vgpr_comp_cnt;
    info.num_allocated_vgprs=program.NumVgprs();
    info.fp_denorm_mode32=program.settings.fp_denorm_mode32;
    info.fp_denorm_mode16_64=program.settings.fp_denorm_mode64;
    info.fp_round_mode32=program.settings.fp_round_mode32;
    info.fp_round_mode16_64=program.settings.fp_round_mode64;
    info.vs_info.user_clip_plane_mask=regs.clipper_control.user_clip_plane_enable;
    info.vs_info.step_rate_0=regs.vgt_instance_step_rate_0;
    info.vs_info.step_rate_1=regs.vgt_instance_step_rate_1;
    info.vs_info.num_outputs=MapVertexOutputs(info.vs_info.outputs,regs.vs_output_control);
    info.vs_info.emulate_depth_negative_one_to_one=!depthClipControl &&
        regs.clipper_control.clip_space==AmdGpu::ClipSpace::MinusWToW;
    info.vs_info.tess_emulated_primitive=regs.primitive_type==AmdGpu::PrimitiveType::RectList ||
        regs.primitive_type==AmdGpu::PrimitiveType::QuadList;
    info.vs_info.clip_disable=regs.IsClipDisabled();
    output=info;return 0;
}
}
