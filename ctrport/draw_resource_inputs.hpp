// SPDX-License-Identifier: GPL-2.0-only
#pragma once
#include "vertex_buffer_owners.hpp"
#include "draw_packet_inputs.hpp"
namespace ctrport {
// Session-local handoff to a future whole graphics pipeline. Tokens are only
// joined here; readiness never means binding, GPU execution or writer completion.
struct DrawResourceInputs {
 uint32_t word=0;uint64_t layout=0,depth=0,index=0;std::array<uint64_t,8> colors{};
 std::vector<LiveVertexBuffer> vertices;
};
inline int prepare_draw_resources(uint32_t word,const DrawPacketInputs& packet,const CtrGraphicsKeyV1& key,
 const GraphicsVertexFetchOwner& fetch,uint64_t layout,bool descriptors_complete,
 const std::array<uint64_t,8>& colors,uint64_t depth,uint64_t index,
 const std::vector<LiveVertexBuffer>& live,DrawResourceInputs& output,const CtrGraphicsDynamicStateV1* dynamic=nullptr){
 if(key.version!=1 || key.size!=sizeof(key) || key.num_color_attachments>8 || fetch.value.count>fetch.formats.size())return 60;
 if(fetch.status || fetch.format_status || fetch.physical_status || !layout || !descriptors_complete)return 88;
 if(packet.indexed && !index)return 88;
 if(dynamic && (dynamic->version!=1 || dynamic->size!=sizeof(*dynamic)))return 60;
 // A valid but stale depth format does not enable depth processing. Preserve
 // the original key; only exact decoded disabled tests/writes remove the need
 // for a depth owner. Missing dynamic state retains the conservative policy.
 const bool depth_required=!dynamic || dynamic->depth_test || dynamic->depth_write || dynamic->stencil_test;
 if((key.z_format || key.stencil_format) && depth_required && !depth)return 88;
 for(uint32_t slot=0;slot<key.num_color_attachments;++slot)if((key.mrt_mask&(1u<<slot)) && key.colors[slot].write_mask && !colors[slot])return 88;
 DrawResourceInputs result;result.word=word;result.layout=layout;result.depth=depth;result.index=index;result.colors=colors;
 for(uint32_t i=0;i<fetch.value.count;++i){const auto& input=fetch.value.inputs[i];const LiveVertexBuffer* match=nullptr;
  for(const auto& vertex:live)if(vertex.word==word && vertex.binding==input.binding){if(match)return 60;match=&vertex;}
  if(!match || !match->token || match->location!=input.location || match->stride!=input.stride || match->input_rate!=input.input_rate || match->divisor!=input.divisor || match->format!=fetch.formats[i] || match->bytes!=input.buffer_size || match->guest_address!=input.guest_address)return 88;
  result.vertices.push_back(*match);
 }
 output=std::move(result);return 0;
}
// Subtraction avoids address-end overflow. Invalid/unknown ranges never admit
// reuse of a prefetched CPU snapshot after an original GPU writer.
inline bool draw_snapshot_disjoint(uint64_t address,uint64_t bytes,const std::vector<std::pair<uint64_t,uint64_t>>& writers){
 if(!address || !bytes || address>UINT64_MAX-bytes)return false;
 for(const auto& writer:writers){const auto base=writer.first,size=writer.second;
  if(!base || !size || base>UINT64_MAX-size)return false;
  if(address<base+size && base<address+bytes)return false;
 }
 return true;
}
inline int draw_resource_inputs_control(){
 const std::vector<std::pair<uint64_t,uint64_t>> writers{{0x2000,32}};
 if(!draw_snapshot_disjoint(0x1000,32,writers) || !draw_snapshot_disjoint(0x2020,32,writers) || draw_snapshot_disjoint(0x201f,2,writers) || draw_snapshot_disjoint(0x1fff,2,writers) || draw_snapshot_disjoint(UINT64_MAX,2,writers) || draw_snapshot_disjoint(0,32,writers))return 20;
 DrawPacketInputs packet;CtrGraphicsKeyV1 key{};key.version=1;key.size=sizeof(key);GraphicsVertexFetchOwner fetch;fetch.status=fetch.format_status=fetch.physical_status=0;fetch.value.count=1;
 fetch.value.inputs[0].binding=2;fetch.value.inputs[0].location=2;fetch.value.inputs[0].buffer_size=32;fetch.formats[0]=37;
 std::vector<LiveVertexBuffer> live{{255,2,2,0,0,0,37,99,32}};DrawResourceInputs output;std::array<uint64_t,8> colors{};
 if(prepare_draw_resources(255,packet,key,fetch,9,true,colors,0,0,live,output) || output.vertices.size()!=1 || output.vertices[0].token!=99)return 1;
 if(prepare_draw_resources(255,packet,key,fetch,9,false,colors,0,0,live,output)!=88 || output.layout!=9)return 2;
 live.push_back(live[0]);if(prepare_draw_resources(255,packet,key,fetch,9,true,colors,0,0,live,output)!=60 || output.vertices.size()!=1)return 3;live.pop_back();
 key.z_format=3;if(prepare_draw_resources(255,packet,key,fetch,9,true,colors,0,0,live,output)!=88)return 4;
 CtrGraphicsDynamicStateV1 dynamic{};dynamic.version=1;dynamic.size=sizeof(dynamic);
 if(prepare_draw_resources(255,packet,key,fetch,9,true,colors,0,0,live,output,&dynamic) || key.z_format!=3 || output.depth)return 21;
 for(uint32_t* enabled:{&dynamic.depth_test,&dynamic.depth_write,&dynamic.stencil_test}){*enabled=1;if(prepare_draw_resources(255,packet,key,fetch,9,true,colors,0,0,live,output,&dynamic)!=88)return 22;*enabled=0;}
 dynamic.version=0;if(prepare_draw_resources(255,packet,key,fetch,9,true,colors,0,0,live,output,&dynamic)!=60)return 23;key.z_format=0;
 packet.indexed=true;if(prepare_draw_resources(255,packet,key,fetch,9,true,colors,0,0,live,output)!=88)return 5;
 if(prepare_draw_resources(255,packet,key,fetch,9,true,colors,0,77,live,output) || output.index!=77)return 6;
 fetch.value.count=33;if(prepare_draw_resources(255,packet,key,fetch,9,true,colors,0,77,live,output)!=60 || output.index!=77 || output.vertices.size()!=1)return 7;
 fetch.value.count=1;fetch.value.inputs[0].guest_address=1;if(prepare_draw_resources(255,packet,key,fetch,9,true,colors,0,77,live,output)!=88 || output.index!=77)return 8;
 std::cout<<"{\"scope\":\"draw_resource_inputs_control\",\"passed\":true,\"missing_dependencies_rejected\":true,\"duplicate_semantics_rejected\":true,\"failure_atomic\":true,\"synthetic\":true,\"GPU_execution\":false}\n";return 0;
}
}
