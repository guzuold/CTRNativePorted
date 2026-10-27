// SPDX-License-Identifier: GPL-2.0-only
#pragma once
#include <array>
#include <iostream>
#include <cstdint>
namespace ctrport {
// Public packet markers: shadPS4 e3ce810f3a653f43ac64ebab63023de281a4103a,
// amdgpu/pm4_cmds.h and gnmdriver.cpp PatchFlipRequest, GPL-2.0-or-later.
// Validate only the reached plain PrepareFlip. No label/IRQ or completion effect.
struct PrepareFlipRequest {uint32_t slot=0,mode=0;uint64_t argument=0;};
inline int prepare_plain_flip(const uint32_t* words,size_t count,uint64_t handle,bool owned,const std::array<uint64_t,3>& stack,PrepareFlipRequest& output){
 if(!words || count!=64 || !owned || handle!=1)return 60;
 if(words[0]!=0xc03e1000u || words[1]!=0x68750777u)return 88;
 if(stack[0]>15 || stack[1]!=1)return 88;
 // The remaining marker payload belongs to the original command buffer and
 // remains unchanged. Slot/mode/argument come from the SysV call, not tail data.
 output={static_cast<uint32_t>(stack[0]),static_cast<uint32_t>(stack[1]),stack[2]};return 0;
}
}
