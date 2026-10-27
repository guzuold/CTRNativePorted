// SPDX-License-Identifier: GPL-2.0-only
#pragma once
#include "draw_resource_inputs.hpp"
#include "guest_copy_api.h"
namespace ctrport {
// Host transport for the public DrawIndex2 capture. No index generation, draw,
// descriptor substitution or CPU refresh after an overlapping GPU writer.
struct IndexedDrawTransport {
 uint64_t token=0,address=0,bytes=0;
 uint32_t count=0,type=0,minimum=0,maximum=0;
};
inline int prepare_indexed_transport(const DrawPacketInputs& packet,const GraphicsBufferCapture& source,
 uint64_t token,uint32_t stride,uint64_t token_bytes,const CtrGuestCopyApi& memory,
 const std::vector<std::pair<uint64_t,uint64_t>>& color_writers,
 const std::vector<std::pair<uint64_t,uint64_t>>& compute_writers,IndexedDrawTransport& output){
 if(!packet.indexed || packet.opcode!=0x27 || !packet.count || packet.count>packet.max_indices ||
    (stride!=2 && stride!=4) || memory.version!=1 || memory.size!=sizeof(memory) || !memory.read)return 60;
 const uint64_t address=uint64_t(packet.index_base_low&~1u)|(uint64_t(packet.index_base_high&255u)<<32);
 const uint64_t bytes=uint64_t(packet.count)*stride;
 if(!token || !address || bytes>16*1024*1024 || source.address!=address || source.bytes!=bytes ||
    token_bytes!=bytes || source.stride!=stride || source.records!=packet.count)return 88;
 for(const int status:{source.requirement_status,source.lease_status,source.capture_status,source.release_status,source.lease_end_status})if(status)return status;
 if(source.snapshot.size()!=bytes)return 112;
 if(!draw_snapshot_disjoint(address,bytes,color_writers) || !draw_snapshot_disjoint(address,bytes,compute_writers))return 88;
 std::vector<unsigned char> current(static_cast<size_t>(bytes));
 const int copied=memory.read(memory.context,address,current.data(),bytes);if(copied)return copied;
 if(current!=source.snapshot)return 88;
 IndexedDrawTransport result;result.token=token;result.address=address;result.bytes=bytes;result.count=packet.count;result.type=stride==4?1u:0u;
 result.minimum=UINT32_MAX;
 for(uint32_t i=0;i<packet.count;++i){uint32_t value=0;std::memcpy(&value,current.data()+size_t(i)*stride,stride);result.minimum=std::min(result.minimum,value);result.maximum=std::max(result.maximum,value);}
 output=result;return 0;
}
// Whole public Rasterizer::Draw indexed argument mapping, pinned shadPS4
// e3ce810f3a653f43ac64ebab63023de281a4103a, vk_rasterizer.cpp:227.
// Binding offset is zero for the exact DrawIndex2 source snapshot retained here.
struct IndexedCommandParameters {
 uint64_t index_token=0,index_buffer_offset=0;
 uint32_t index_type=0,index_count=0,instance_count=0,first_index=0,first_instance=0;
 int32_t vertex_offset=0;
};
inline int prepare_indexed_command(const IndexedDrawTransport& index,const CtrGraphicsDrawParametersV1& offsets,IndexedCommandParameters& output){
 if(offsets.version!=1 || offsets.size!=sizeof(offsets) || !offsets.instance_count)return 60;
 if(!index.token || !index.count || index.type>1 || index.bytes!=uint64_t(index.count)*(index.type?4u:2u))return 88;
 IndexedCommandParameters result;result.index_token=index.token;result.index_type=index.type;result.index_count=index.count;
 result.instance_count=offsets.instance_count;result.first_instance=offsets.instance_offset;
 std::memcpy(&result.vertex_offset,&offsets.vertex_offset,sizeof(result.vertex_offset));
 output=result;return 0;
}
// Strict public indexed C ABI transport. Output remains unchanged on rejection.
inline int prepare_indexed_call(const IndexedCommandParameters& command,const CtrGraphicsDrawV1& resources,CtrGraphicsIndexedDrawV1& output){
 if(resources.version!=1 || resources.size!=sizeof(resources))return 60;
 if(!command.index_token || !command.index_count || command.index_type>1 || command.instance_count!=1 || command.first_instance || command.first_index || command.index_buffer_offset || resources.vertex_buffer_count>32)return 88;
 CtrGraphicsIndexedDrawV1 result{};result.version=1;result.size=sizeof(result);
 result.index_count=command.index_count;result.instance_count=command.instance_count;result.first_index=command.first_index;result.first_instance=command.first_instance;
 result.vertex_offset=command.vertex_offset;result.index_bytes=command.index_type?4u:2u;result.index_token=command.index_token;result.index_offset=command.index_buffer_offset;
 result.vertex_buffer_count=resources.vertex_buffer_count;result.written_buffers=resources.written_buffers;result.written_images=resources.written_images;result.atomic_resources=resources.atomic_resources;
 for(uint32_t i=0;i<result.vertex_buffer_count;++i){if(!resources.vertex_buffers[i])return 88;result.vertex_buffers[i]=resources.vertex_buffers[i];}
 output=result;return 0;
}
// Range admission for the bounded vertex formats already admitted by the
// companion. Signed baseVertex is applied before bounds checks, including
// negative bases; instance-rate inputs use firstInstance and divisor one.
inline int prepare_indexed_vertex_ranges(const IndexedDrawTransport& index,const IndexedCommandParameters& command,
 const GraphicsVertexFetchOwner& fetch,uint64_t& checked){
 if(fetch.status || fetch.format_status || fetch.physical_status || fetch.value.count>32)return 88;
 if(fetch.value.version!=1 || fetch.value.size!=sizeof(fetch.value))return 60;
 if(!index.token || !index.count || command.first_index || command.index_buffer_offset || command.index_token!=index.token || command.index_count!=index.count || command.index_type!=index.type || !command.instance_count || index.minimum>index.maximum)return 88;
 const int64_t low=int64_t(index.minimum)+command.vertex_offset,high=int64_t(index.maximum)+command.vertex_offset;
 if(low<0 || high>UINT32_MAX)return 88;
 for(uint32_t i=0;i<fetch.value.count;++i){const auto& v=fetch.value.inputs[i];
  uint64_t element=0;switch(fetch.formats[i]){case 37:case 83:element=4;break;case 103:element=8;break;case 106:element=12;break;case 109:element=16;break;default:return 88;}
  if(v.input_rate>1 || (v.input_rate && v.divisor!=1))return 88;
  const uint64_t last=v.input_rate?uint64_t(command.first_instance)+command.instance_count-1:uint64_t(high);
  // Division avoids overflowing last*stride on hostile metadata.
  if(v.offset>v.buffer_size || element>uint64_t(v.buffer_size)-v.offset)return 88;
  if(v.stride && last>(uint64_t(v.buffer_size)-v.offset-element)/v.stride)return 88;
 }
 checked=fetch.value.count;return 0;
}
// Indexed submission must also retain the exact vertex generation observed by
// its fetch shader. A GPU token alone cannot authorize stale CPU input bytes.
inline int prepare_indexed_vertices(uint32_t word,const GraphicsVertexFetchOwner& fetch,
 const std::vector<LiveVertexBuffer>& live,const CtrGuestCopyApi& memory,
 const std::vector<std::pair<uint64_t,uint64_t>>& colors,
 const std::vector<std::pair<uint64_t,uint64_t>>& computes,uint64_t& compared){
 if(memory.version!=1 || memory.size!=sizeof(memory) || !memory.read)return 60;
 if(fetch.status || fetch.format_status || fetch.physical_status)return 88;
 if(fetch.value.count!=fetch.buffers.size() || fetch.value.count>fetch.formats.size() || fetch.value.count>32)return 112;
 uint64_t total=0;
 for(uint32_t i=0;i<fetch.value.count;++i){const auto& input=fetch.value.inputs[i];const auto& source=fetch.buffers[i];
  const LiveVertexBuffer* match=nullptr;
  for(const auto& candidate:live)if(candidate.word==word && candidate.binding==input.binding){if(match)return 112;match=&candidate;}
  if(!match || !match->token || match->guest_address!=input.guest_address || match->bytes!=input.buffer_size ||
     match->stride!=input.stride || match->location!=input.location || match->input_rate!=input.input_rate ||
     match->divisor!=input.divisor || match->format!=fetch.formats[i])return 88;
  if(source.address!=input.guest_address || source.bytes!=input.buffer_size || source.snapshot.size()!=source.bytes || source.bytes>16*1024*1024)return 112;
  for(const int status:{source.requirement_status,source.lease_status,source.capture_status,source.release_status,source.lease_end_status})if(status)return status;
  if(!draw_snapshot_disjoint(source.address,source.bytes,colors) || !draw_snapshot_disjoint(source.address,source.bytes,computes))return 88;
  std::vector<unsigned char> current(source.snapshot.size());
  const int copied=memory.read(memory.context,source.address,current.data(),source.bytes);if(copied)return copied;
  if(current!=source.snapshot)return 88;total+=source.bytes;
 }
 compared=total;return 0;
}
}
