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
inline int indexed_call_control(){
 IndexedCommandParameters command{};command.index_token=7;command.index_count=102;command.instance_count=1;command.vertex_offset=-3;
 CtrGraphicsDrawV1 resources{};resources.version=1;resources.size=sizeof(resources);resources.vertex_buffer_count=1;resources.vertex_buffers[0]=9;resources.written_buffers=2;
 CtrGraphicsIndexedDrawV1 result{};
 if(prepare_indexed_call(command,resources,result) || result.index_bytes!=2 || result.vertex_offset!=-3 || result.index_count!=102 || result.index_token!=7 || result.vertex_buffers[0]!=9 || result.written_buffers!=2)return 1;
 command.index_type=1;command.vertex_offset=INT32_MIN;if(prepare_indexed_call(command,resources,result) || result.index_bytes!=4 || result.vertex_offset!=INT32_MIN)return 2;
 const auto saved=result;unsigned rejected=0;
 const auto check=[&](){if(prepare_indexed_call(command,resources,result)==88 && !std::memcmp(&saved,&result,sizeof(result)))++rejected;};
 command.index_token=0;check();command.index_token=7;command.index_count=0;check();command.index_count=102;
 command.index_type=2;check();command.index_type=1;command.instance_count=2;check();command.instance_count=1;
 command.first_instance=1;check();command.first_instance=0;command.first_index=1;check();command.first_index=0;
 command.index_buffer_offset=2;check();command.index_buffer_offset=0;resources.vertex_buffer_count=33;check();resources.vertex_buffer_count=1;
 resources.vertex_buffers[0]=0;check();if(rejected!=9)return 3;
 std::cout<<"{\"scope\":\"indexed_call_control\",\"passed\":true,\"atomic_rejections\":9,\"U16_U32_signed_base\":true,\"synthetic\":true,\"GPU_execution\":false}\n";return 0;
}
inline int indexed_command_parameters_control(){
 IndexedDrawTransport index;index.token=9;index.type=0;index.count=102;index.bytes=204;
 CtrGraphicsDrawParametersV1 offsets{1,sizeof(offsets),0xfffffffeu,7,3};IndexedCommandParameters result;
 if(prepare_indexed_command(index,offsets,result) || result.vertex_offset!=-2 || result.first_instance!=7 || result.instance_count!=3 || result.first_index || result.index_buffer_offset || result.index_count!=102)return 1;
 unsigned rejected=0;const auto saved=result;const auto unchanged=[&](int status,int expected){return status==expected && !std::memcmp(&saved,&result,sizeof(result));};
 offsets.instance_count=0;if(unchanged(prepare_indexed_command(index,offsets,result),60))++rejected;offsets.instance_count=3;
 index.bytes=203;if(unchanged(prepare_indexed_command(index,offsets,result),88))++rejected;index.bytes=204;
 index.token=0;if(unchanged(prepare_indexed_command(index,offsets,result),88))++rejected;index.token=9;
 index.type=2;if(unchanged(prepare_indexed_command(index,offsets,result),88))++rejected;
 index.type=1;index.bytes=408;offsets.vertex_offset=0x7fffffffu;if(prepare_indexed_command(index,offsets,result) || result.vertex_offset!=INT32_MAX || result.index_type!=1 || result.index_count!=102)return 2;
 offsets.vertex_offset=0x80000000u;if(prepare_indexed_command(index,offsets,result) || result.vertex_offset!=INT32_MIN)return 3;
 if(rejected!=4)return 4;
 std::cout<<"{\"scope\":\"indexed_command_parameters_control\",\"passed\":true,\"signed_base_vertex\":true,\"atomic_rejections\":4,\"synthetic\":true,\"GPU_execution\":false}\n";return 0;
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
inline int indexed_vertex_ranges_control(){
 IndexedDrawTransport index;index.token=7;index.count=3;index.minimum=1;index.maximum=3;
 IndexedCommandParameters cmd;cmd.index_token=7;cmd.index_count=3;cmd.instance_count=1;cmd.vertex_offset=-1;
 GraphicsVertexFetchOwner fetch;fetch.status=fetch.format_status=fetch.physical_status=0;fetch.value.version=1;fetch.value.size=sizeof(fetch.value);fetch.value.count=1;
 auto& v=fetch.value.inputs[0];v.stride=4;v.buffer_size=12;fetch.formats[0]=37;uint64_t checked=99;
 if(prepare_indexed_vertex_ranges(index,cmd,fetch,checked) || checked!=1)return 1;
 unsigned rejected=0;auto reject=[&](){checked=99;const int rc=prepare_indexed_vertex_ranges(index,cmd,fetch,checked);return rc==88 && checked==99;};
 cmd.vertex_offset=0;if(reject())++rejected;cmd.vertex_offset=INT32_MIN;if(reject())++rejected;cmd.vertex_offset=INT32_MAX;if(reject())++rejected;cmd.vertex_offset=-1;
 v.offset=1;if(reject())++rejected;v.offset=0;v.buffer_size=3;if(reject())++rejected;v.buffer_size=12;
 v.input_rate=1;v.divisor=1;cmd.first_instance=2;cmd.instance_count=1;if(prepare_indexed_vertex_ranges(index,cmd,fetch,checked))return 2;
 cmd.instance_count=2;if(reject())++rejected;cmd.first_instance=UINT32_MAX;cmd.instance_count=UINT32_MAX;if(reject())++rejected;
 cmd.first_instance=0;cmd.instance_count=1;v.divisor=2;if(reject())++rejected;v.divisor=1;
 fetch.formats[0]=0;if(reject())++rejected;fetch.formats[0]=37;cmd.index_token=8;if(reject())++rejected;cmd.index_token=7;
 v.input_rate=0;v.stride=0;if(prepare_indexed_vertex_ranges(index,cmd,fetch,checked))return 3;
 cmd.first_index=1;if(reject())++rejected;cmd.first_index=0;cmd.index_buffer_offset=2;if(reject())++rejected;cmd.index_buffer_offset=0;
 // Retail draw10451 uses R16G16_SFLOAT (Vulkan format83): two 16-bit
 // components, four bytes. Exercise the exact last-element boundary and one
 // byte short; admission must not mutate checked on rejection.
 v.stride=40;v.buffer_size=124;fetch.formats[0]=83;index.minimum=0;index.maximum=3;cmd.vertex_offset=0;
 if(prepare_indexed_vertex_ranges(index,cmd,fetch,checked) || checked!=1)return 5;
 v.buffer_size=123;if(reject())++rejected;
 v.buffer_size=124;v.offset=1;if(reject())++rejected;
 if(rejected!=14)return 4;
 std::cout<<"{\"scope\":\"indexed_vertex_ranges_control\",\"passed\":true,\"atomic_rejections\":14,\"signed_base_before_bounds\":true,\"synthetic\":true,\"GPU_execution\":false}\n";return 0;
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
inline int indexed_vertex_generation_control(){
 GraphicsVertexFetchOwner fetch;fetch.status=fetch.format_status=fetch.physical_status=0;fetch.value.count=1;
 auto& input=fetch.value.inputs[0];input.guest_address=0x1000;input.buffer_size=4;input.stride=4;
 fetch.formats[0]=37;GraphicsBufferCapture source;source.address=0x1000;source.bytes=4;source.snapshot={1,2,3,4};
 source.requirement_status=source.lease_status=source.capture_status=source.release_status=source.lease_end_status=0;fetch.buffers.push_back(source);
 std::vector<LiveVertexBuffer> live{{1,0,0,4,0,0,37,9,4,0x1000}};auto bytes=source.snapshot;
 CtrGuestCopyApi memory{};memory.version=1;memory.size=sizeof(memory);memory.context=&bytes;
 memory.read=+[](void* context,uint64_t address,void* output,uint64_t size)->int{auto& value=*static_cast<std::vector<unsigned char>*>(context);if(address!=0x1000 || size!=value.size())return 62;std::memcpy(output,value.data(),value.size());return 0;};
 uint64_t compared=99;if(prepare_indexed_vertices(1,fetch,live,memory,{}, {},compared) || compared!=4)return 1;
 unsigned rejected=0;compared=99;bytes[0]=8;
 if(prepare_indexed_vertices(1,fetch,live,memory,{}, {},compared)==88 && compared==99)++rejected;bytes=source.snapshot;
 if(prepare_indexed_vertices(1,fetch,live,memory,{{0x1003,1}}, {},compared)==88 && compared==99)++rejected;
 if(prepare_indexed_vertices(1,fetch,live,memory,{}, {{0x1000,4}},compared)==88 && compared==99)++rejected;
 live[0].token=0;if(prepare_indexed_vertices(1,fetch,live,memory,{}, {},compared)==88 && compared==99)++rejected;live[0].token=9;
 live.push_back(live[0]);if(prepare_indexed_vertices(1,fetch,live,memory,{}, {},compared)==112 && compared==99)++rejected;live.pop_back();
 if(rejected!=5)return 2;
 std::cout<<"{\"scope\":\"indexed_vertex_generation_control\",\"passed\":true,\"atomic_rejections\":5,\"synthetic\":true,\"GPU_execution\":false}\n";return 0;
}
inline int indexed_draw_transport_control(){
 DrawPacketInputs packet;packet.opcode=0x27;packet.indexed=true;packet.count=packet.max_indices=3;packet.index_base_low=0x1001;
 GraphicsBufferCapture source;source.address=0x1000;source.bytes=6;source.stride=2;source.records=3;source.snapshot={2,0,9,0,1,0};
 source.requirement_status=source.lease_status=source.capture_status=source.release_status=source.lease_end_status=0;
 auto guest=source.snapshot;
 CtrGuestCopyApi memory{};memory.version=1;memory.size=sizeof(memory);memory.context=&guest;
 memory.read=+[](void* raw,uint64_t address,void* output,uint64_t bytes)->int{const auto& data=*static_cast<std::vector<unsigned char>*>(raw);if(address!=0x1000 || bytes!=data.size())return 62;std::memcpy(output,data.data(),data.size());return 0;};
 IndexedDrawTransport result;
 if(prepare_indexed_transport(packet,source,7,2,6,memory,{}, {},result) || result.minimum!=1 || result.maximum!=9 || result.type || result.count!=3)return 1;
 const auto saved=result;unsigned rejected=0;
 const auto unchanged=[&](int status){return status==88 && !std::memcmp(&saved,&result,sizeof(result));};
 if(unchanged(prepare_indexed_transport(packet,source,7,2,6,memory,{{0x1005,1}}, {},result)))++rejected;
 if(unchanged(prepare_indexed_transport(packet,source,7,2,6,memory,{}, {{0x0fff,2}},result)))++rejected;
 guest[0]=3;if(unchanged(prepare_indexed_transport(packet,source,7,2,6,memory,{}, {},result)))++rejected;guest=source.snapshot;
 if(unchanged(prepare_indexed_transport(packet,source,0,2,6,memory,{}, {},result)))++rejected;
 if(unchanged(prepare_indexed_transport(packet,source,7,2,4,memory,{}, {},result)))++rejected;
 if(rejected!=5)return 2;
 source.snapshot={2,0,0,0,0,0,1,0,1,0,0,0};source.bytes=12;source.stride=4;guest=source.snapshot;
 if(prepare_indexed_transport(packet,source,8,4,12,memory,{}, {},result) || result.type!=1 || result.maximum!=65536 || result.minimum!=1)return 3;
 std::cout<<"{\"scope\":\"indexed_draw_transport_control\",\"passed\":true,\"U16_U32\":true,\"stale_CPU_or_GPU_writers_rejected\":true,\"atomic_rejections\":5,\"synthetic\":true,\"GPU_execution\":false}\n";return 0;
}
}
