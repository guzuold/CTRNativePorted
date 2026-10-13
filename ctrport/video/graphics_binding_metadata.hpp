// SPDX-License-Identifier: GPL-2.0-only
#pragma once
#include "shader_recompiler/info.h"
#include "shader_recompiler/backend/bindings.h"
#include <array>
#include <vector>
#include <stdexcept>
namespace CtrPm4 {
// Host compiler contract only. Values come from the same Shader::Info used by
// whole public EmitSPIRV; no GPU handles, guest resource ownership or admission.
struct GraphicsDescriptorBinding {
    uint32_t kind,sharpIndex,binding,count,bufferSlot;
    bool storage;
};
struct GraphicsBindingMetadata {
    Shader::Backend::Bindings before{},after{};
    std::array<uint32_t,16> packedUserdata{};
    uint32_t userdataCount{};
    std::vector<GraphicsDescriptorBinding> descriptors;
};
inline GraphicsBindingMetadata CaptureGraphicsBindingMetadata(const Shader::Info& info,
    Shader::Backend::Bindings before,Shader::Backend::Bindings after) {
    GraphicsBindingMetadata out;out.before=before;out.after=after;
    auto unified=before.unified,buffer=before.buffer;
    for(const auto& item:info.buffers)
        out.descriptors.push_back({0,item.sharp_idx,unified++,1,buffer++,item.IsStorage(item.GetSharp(info))});
    for(const auto& item:info.images) {
        const auto count=item.NumBindings(info);
        out.descriptors.push_back({1,item.sharp_idx,unified,count,UINT32_MAX,item.is_written});
        unified+=count;
    }
    for(const auto& item:info.samplers)
        out.descriptors.push_back({2,item.sharp_idx,unified++,1,UINT32_MAX,false});
    // Fmask identities have no independent descriptor in public layout/emitter.
    for(uint32_t bit=0;bit<16;++bit)
        if(info.ud_mask.mask&(1U<<bit))out.packedUserdata[out.userdataCount++]=info.user_data[bit];
    if(unified!=after.unified || buffer!=after.buffer ||
       before.user_data+out.userdataCount!=after.user_data)
        throw std::runtime_error("Compiler binding contract counter mismatch");
    return out;
}
}
