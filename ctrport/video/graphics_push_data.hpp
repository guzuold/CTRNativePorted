// SPDX-FileCopyrightText: Copyright 2024-2026 shadPS4 Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "graphics_binding_metadata.hpp"
#include "graphics_companion.h"
#include "pm4_registers.hpp"
#include "video_core/amdgpu/regs.h"
#include <cstddef>
namespace CtrPm4 {
// Whole public MakeUserData, vk_rasterizer.cpp, pinned
// e3ce810f3a653f43ac64ebab63023de281a4103a.
inline Shader::PushData MakeGraphicsUserData(const AmdGpu::Regs& regs) {
    Shader::PushData push_data{};
    push_data.xoffset = regs.viewport_control.xoffset_enable ? regs.viewports[0].xoffset : 0.f;
    push_data.xscale = regs.viewport_control.xscale_enable ? regs.viewports[0].xscale : 1.f;
    push_data.yoffset = regs.viewport_control.yoffset_enable ? regs.viewports[0].yoffset : 0.f;
    push_data.yscale = regs.viewport_control.yscale_enable ? regs.viewports[0].yscale : 1.f;
    return push_data;
}
inline int DecodePushData(const Registers& captured,const CtrGraphicsBindingContractV1& ps,
 const CtrGraphicsBindingContractV1& vs,const uint32_t* offsets,uint32_t count,CtrGraphicsPushDataV1& output){
 static_assert(sizeof(Shader::PushData)==120);
 static_assert(offsetof(Shader::PushData,ud_regs)==16 && offsetof(Shader::PushData,buf_offsets)==80);
 if(ps.version!=1 || vs.version!=1 || ps.size!=sizeof(ps) || vs.size!=sizeof(vs) ||
    ps.unified_before || ps.buffer_before || ps.userdata_before ||
    vs.unified_before!=ps.unified_after || vs.buffer_before!=ps.buffer_after ||
    vs.userdata_before!=ps.userdata_after || count!=vs.buffer_after || count>Shader::NUM_BUFFERS || (count && !offsets))return 60;
 AmdGpu::Regs regs{};std::memcpy(regs.reg_array.data(),captured.words.data(),sizeof(captured.words));
 auto push=MakeGraphicsUserData(regs);
 // Whole public PushUd packing already captured by compiler in these immutable contracts.
 for(const auto* stage:{&ps,&vs}){
  if(stage->userdata_count>16 || stage->userdata_before>16 || stage->userdata_after>16 ||
     stage->userdata_before+stage->userdata_count!=stage->userdata_after ||
     stage->buffer_before>stage->buffer_after || stage->buffer_after>Shader::NUM_BUFFERS)return 60;
  for(uint32_t i=0;i<stage->userdata_count;++i)push.ud_regs[stage->userdata_before+i]=stage->packed_userdata[i];
 }
 // Whole public AddOffset contract: actual byte offset per shared buffer binding, <256.
 for(uint32_t i=0;i<count;++i){if(offsets[i]>=256)return 88;push.buf_offsets[i]=static_cast<uint8_t>(offsets[i]);}
 CtrGraphicsPushDataV1 result{};result.version=1;result.size=sizeof(result);
 std::memcpy(result.bytes,&push,sizeof(push));output=result;return 0;
}
}
