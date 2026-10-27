// SPDX-License-Identifier: GPL-2.0-only
#pragma once
#include "graphics_buffer_bindings.hpp"
namespace ctrport {
// Resource half of public Shader::PushData (resource.h, pinned shadPS4
// e3ce810f3a653f43ac64ebab63023de281a4103a GPL-2.0-or-later).
// Viewport transform is deliberately separate: it must come from MakeUserData.
// Current selected-session buffer descriptors use whole dedicated buffers at
// offset zero, not the public buffer cache's suballocation/AlignDown path.
struct GraphicsPushResources {
 std::array<uint32_t,16> userdata{};std::array<uint8_t,40> offsets{};
 uint32_t userdata_count=0,buffer_count=0;
};
inline int prepare_push_resources(const CtrGraphicsBindingContractV1& ps,const CtrGraphicsBindingContractV1& vs,
 const std::vector<GraphicsBufferWrite>& writes,GraphicsPushResources& output){
 if(ps.version!=1 || vs.version!=1 || ps.size!=sizeof(ps) || vs.size!=sizeof(vs) || ps.userdata_before || ps.buffer_before ||
 vs.userdata_before!=ps.userdata_after || vs.buffer_before!=ps.buffer_after)return 60;
 GraphicsPushResources result;std::array<bool,40> present{};
 for(const auto* c:{&ps,&vs}){
  if(c->userdata_count>16 || c->userdata_before>16 || c->userdata_after>16 ||
   c->userdata_before+c->userdata_count!=c->userdata_after || c->buffer_after>40 || c->buffer_before>c->buffer_after)return 60;
  std::copy_n(c->packed_userdata,c->userdata_count,result.userdata.begin()+c->userdata_before);
 }
 result.userdata_count=vs.userdata_after;result.buffer_count=vs.buffer_after;
 if(writes.size()!=result.buffer_count)return 88;
 for(const auto& w:writes){
  if(w.slot>=result.buffer_count || present[w.slot])return 60;
  if((!w.null_clip_planes && !w.token) || (w.null_clip_planes && w.token))return 88;
  present[w.slot]=true; // Dedicated allocation descriptor offset and effective source both start at zero.
 }
 output=result;return 0;
}
}
