// SPDX-License-Identifier: GPL-2.0-only
#pragma once
#include "compute_shader_state.hpp"
#include "pm4_decode.hpp"
#include <vulkan/vulkan.h>
namespace CtrPm4 {
// Selected physical device limits only; this does not admit shader features,
// descriptors, SRT, predicate or ordered append, or authorize queue execution.
inline int AdmitComputeLimits(const ComputeShaderState& state,const DispatchDirect& dispatch,
    const VkPhysicalDeviceLimits& limits) {
    const uint32_t groups[]={dispatch.x,dispatch.y,dispatch.z};
    uint64_t invocations=1;
    for(size_t i=0;i<3;++i) {
        if(!state.workgroup[i] || state.workgroup[i]>limits.maxComputeWorkGroupSize[i] ||
           groups[i]>limits.maxComputeWorkGroupCount[i])return 88;
        // Divide before multiplication, so hostile metadata cannot overflow.
        if(invocations>uint64_t(limits.maxComputeWorkGroupInvocations)/state.workgroup[i])return 88;
        invocations*=state.workgroup[i];
    }
    if(state.sharedBytes>limits.maxComputeSharedMemorySize)return 88;
    return 0;
}
}
