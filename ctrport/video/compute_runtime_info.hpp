// SPDX-License-Identifier: GPL-2.0-only
#pragma once
#include "compute_shader_state.hpp"
#include "shader_recompiler/runtime_info.h"
namespace CtrPm4 {
// Public PipelineCache::BuildRuntimeInfo compute branch only. No inferred
// graphics stage, hardware profile, descriptor binding or shader execution.
inline Shader::RuntimeInfo BuildComputeRuntimeInfo(const ComputeShaderState& state) {
    Shader::RuntimeInfo info{};
    info.Initialize(Shader::Stage::Compute);
    info.num_user_data = state.userRegisters;
    info.num_allocated_vgprs = state.allocatedVgprs;
    info.cs_info.workgroup_size = state.workgroup;
    info.cs_info.tgid_enable = state.tgid;
    info.cs_info.shared_memory_size = state.sharedBytes;
    return info;
}
}
