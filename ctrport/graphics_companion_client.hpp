// SPDX-License-Identifier: GPL-2.0-only
#pragma once
#include "video/graphics_companion.h"
#include "video/pm4_registers.hpp"
#include "video/graphics_shader_state.hpp"
#include "video/compute_shader_state.hpp"
#include "video/shader_code_capture.hpp"
#include "dispatch_packet_inputs.hpp"
#include <windows.h>
#include <bcrypt.h>
#include <string>
#include <exception>
#include <memory>
#include <functional>
#include <sstream>
#include <vector>
#include <array>
#include <algorithm>
#include <cmath>
#include <fstream>
#include "shader_owned_ranges.h"
#include "shader_snapshot_owner.hpp"
#include "shader_capture_trace.hpp"
namespace ctrport {
inline bool graphics_target_register(uint32_t index){
 if(index==0xa08e || index==0xa08f)return true;
 return index>=0xa318 && index<0xa318+8*15 && (index-0xa318)%15<6;
}
// Metadata only, public register identities: shadPS4 e3ce810f3a653f43ac64ebab63023de281a4103a
// src/video_core/amdgpu/regs.cpp and regs_color.h, GPL-2.0-or-later.
// Preserve raw surface state; dimensions, format admission and ownership belong
// to the renderer. In particular a nonzero base is not proof of a valid surface.
inline std::string graphics_targets_json(const CtrPm4::Registers& registers){
 const auto& flat=registers.words;std::ostringstream s;
 s<<"{\"target_mask_raw\":"<<flat[0xa08e]<<",\"shader_mask_raw\":"<<flat[0xa08f]<<",\"color_buffers\":[";
 for(uint32_t slot=0;slot<8;++slot){if(slot)s<<',';const uint32_t base=0xa318+15*slot;
  s<<"{\"slot\":"<<slot<<",\"base_raw\":"<<flat[base]<<",\"pitch_raw\":"<<flat[base+1]
   <<",\"slice_raw\":"<<flat[base+2]<<",\"view_raw\":"<<flat[base+3]
   <<",\"info_raw\":"<<flat[base+4]<<",\"attrib_raw\":"<<flat[base+5]<<'}';}
 s<<"],\"surface_admission\":false}";return s.str();
}
struct GraphicsBufferCapture {
 uint32_t sharp=0,stride=0,records=0;uint64_t address=0,bytes=0,owned_bytes=0,budget_bytes=0;bool range_attempted=false;
 int requirement_status=88,lease_status=88,capture_status=88,release_status=88,lease_end_status=88;
 std::vector<unsigned char> snapshot;
};
inline GraphicsBufferCapture capture_graphics_buffer(const CtrShaderInputsV1& inputs,
 const CtrShaderOwnedRangesV1& ranges,const CtrGraphicsBufferRequirementV1& needed,uint64_t budget,uint64_t maximum_budget=16*1024*1024){
 GraphicsBufferCapture observed;observed.sharp=needed.sharp_index;observed.budget_bytes=budget;
 if(needed.version!=1 || needed.size!=sizeof(needed) || !needed.guest_address || !needed.bytes || needed.guest_address>UINT64_MAX-needed.bytes){observed.requirement_status=112;return observed;}
 observed.requirement_status=0;observed.address=needed.guest_address;observed.bytes=needed.bytes;observed.stride=needed.stride;observed.records=needed.num_records;
 if(inputs.version!=1 || inputs.byte_size!=sizeof(inputs) || !inputs.context || !inputs.capture || !inputs.release ||
    ranges.version!=1 || ranges.size!=sizeof(ranges) || !ranges.context || !ranges.begin || !ranges.end){observed.lease_status=60;return observed;}
 if(maximum_budget>32*1024*1024 || budget>maximum_budget || needed.bytes>budget)return observed;
 CtrShaderRangeLeaseV1 lease{};observed.range_attempted=true;observed.lease_status=ranges.begin(ranges.context,needed.guest_address,needed.bytes,&lease);
 if(observed.lease_status)return observed;observed.owned_bytes=lease.bytes;
 CtrShaderSnapshot snapshot{};bool captured=false;
 try{
  if(lease.guest_address==needed.guest_address && lease.bytes==needed.bytes && lease.token){
   observed.capture_status=inputs.capture(inputs.context,needed.guest_address,needed.bytes,&snapshot);captured=observed.capture_status==0;
   if(captured){
    if(snapshot.guest_address!=needed.guest_address || snapshot.size!=needed.bytes || !snapshot.snapshot_token || !snapshot.bytes)observed.capture_status=112;
    else{observed.snapshot.resize(static_cast<size_t>(needed.bytes));observed.capture_status=ctr_shader_snapshot_read(&snapshot,needed.guest_address,observed.snapshot.data(),static_cast<size_t>(needed.bytes));}
   }
  }
 }catch(...){observed.capture_status=112;}
 if(captured){observed.release_status=inputs.release(inputs.context,&snapshot);if(observed.release_status)observed.capture_status=observed.release_status;}
 observed.lease_end_status=ranges.end(ranges.context,&lease);if(observed.lease_end_status)observed.capture_status=observed.lease_end_status;
 if(observed.capture_status)observed.snapshot.clear();return observed;
}
// Public DrawIndex2/IndexBufferBase/IndexBufferType semantics from pinned
// shadPS4 pm4_cmds.h, regs_vertex.h and buffer_cache/buffer_cache.cpp (GPL-2.0-or-later),
// revision e3ce810f3a653f43ac64ebab63023de281a4103a. No index generation or GPU use.
inline int graphics_index_requirement(const uint32_t* packet,size_t words,uint32_t type,bool type_observed,CtrGraphicsBufferRequirementV1& output){
 if(!packet || words!=6 || (packet[0]>>30)!=3 || ((packet[0]>>8)&255)!=0x27 || ((packet[0]>>16)&0x3fff)!=4)return 60;
 if(!type_observed || type>1 || !packet[4] || packet[4]>packet[1])return 88;
 const uint64_t address=uint64_t(packet[2]&~1u)|(uint64_t(packet[3]&255)<<32);
 const uint64_t bytes=uint64_t(packet[4])*(type?4u:2u);
 if(!address || bytes>16*1024*1024 || address>UINT64_MAX-bytes)return 88;
 CtrGraphicsBufferRequirementV1 prepared{};prepared.version=1;prepared.size=sizeof(prepared);
 prepared.guest_address=address;prepared.bytes=bytes;prepared.stride=type?4u:2u;prepared.num_records=packet[4];output=prepared;return 0;
}
inline int graphics_index_requirement_control(){
 std::array<uint32_t,6> packet{0xc0042700,12,0x1001,0x20,6,0};
 CtrGraphicsBufferRequirementV1 value{};
 if(graphics_index_requirement(packet.data(),6,0,true,value) || value.guest_address!=0x2000001000ull || value.bytes!=12 || value.stride!=2 || value.num_records!=6)return 4;
 if(graphics_index_requirement(packet.data(),6,1,true,value) || value.bytes!=24 || value.stride!=4)return 4;
 unsigned rejected=0;
 const auto unchanged=[&](int expected,const uint32_t* data,size_t words,uint32_t type,bool seen){
  const auto before=value;const int status=graphics_index_requirement(data,words,type,seen,value);
  return status==expected && std::memcmp(&before,&value,sizeof(value))==0;
 };
 if(unchanged(60,nullptr,6,0,true))++rejected;
 if(unchanged(60,packet.data(),5,0,true))++rejected;
 if(unchanged(88,packet.data(),6,0,false))++rejected;
 if(unchanged(88,packet.data(),6,4,true))++rejected;
 packet[0]=0xc0042d00;if(unchanged(60,packet.data(),6,0,true))++rejected;packet[0]=0xc0042700;
 packet[4]=13;if(unchanged(88,packet.data(),6,0,true))++rejected;
 packet[4]=0;if(unchanged(88,packet.data(),6,0,true))++rejected;
 packet[1]=packet[4]=UINT32_MAX;if(unchanged(88,packet.data(),6,1,true))++rejected;
 if(rejected!=8)return 4;
 std::cout<<"{\"scope\":\"graphics_index_requirement_control\",\"passed\":true,\"negative_rejections\":8,\"synthetic\":true,\"gpu_execution\":false}\n";return 0;
}
struct GraphicsStageOwner {
 std::vector<uint32_t> spirv;std::array<uint32_t,16> userdata{};uint32_t userdata_words=0;
 // Descriptor words are host-only, never guest-authority. Optional research
 // evidence is restricted to the ignored private/ directory.
 std::vector<uint32_t> flattened_userdata;
 CtrGraphicsResourcesV1 resources{};std::vector<GraphicsBufferCapture> buffers;
 struct ClipPlanes {uint32_t sharp;int status;CtrGraphicsClipPlanesV1 value;};std::vector<ClipPlanes> clip_planes;
 struct Flatbuf {uint32_t sharp;int status=88;uint32_t bytes=0;std::vector<uint32_t> words;};std::vector<Flatbuf> flatbufs;
 struct Descriptor {uint32_t kind,sharp;int status=88;uint32_t copied_words=0;std::array<uint32_t,8> words{};int sampler_status=88;CtrGraphicsSamplerParametersV1 sampler{};int image_status=88;CtrGraphicsImageParametersV1 image{};int image_extent_status=88;CtrGraphicsImageExtentV1 image_extent{};GraphicsBufferCapture image_payload{};int image_class_status=88;CtrGraphicsImageClassV1 image_class{};int effective_class_status=88;CtrGraphicsEffectiveImageClassV1 effective_class{};bool source_span_verified=false;mutable std::function<int()> resident_image_authority;mutable std::function<int()> submission_image_authority;};std::vector<Descriptor> descriptors;
 struct Color {uint32_t slot;int status;CtrGraphicsColorRequirementV1 requirement;GraphicsBufferCapture initial;};std::vector<Color> colors;
};
// Ephemeral CPU ownership only. This keeps every observed draw stage alive until
// the ordered submission boundary finishes; it does not admit execution or writes.
struct GraphicsSubmissionOwner {
 struct Entry {uint32_t word;bool vertex;std::shared_ptr<const GraphicsStageOwner> stage;};
 struct Route {uint32_t word,opcode;std::vector<uint32_t> packet;std::shared_ptr<const CtrPm4::Registers> registers;};
 struct Compute {uint32_t word;CtrPm4::ComputeShaderState state;CtrPm4::OwnedShaderBinary binary;DispatchPacketInputs dispatch;std::shared_ptr<const CtrPm4::Registers> registers;};
 std::vector<Compute> computes;
 struct Index {uint32_t word;GraphicsBufferCapture capture;};std::vector<Index> indices;
 int retain_index(uint32_t word,GraphicsBufferCapture&& capture){
  if(status)return status;
  if(routes.empty() || routes.back().word!=word || routes.back().opcode!=0x27 ||
     capture.requirement_status || capture.lease_status || capture.capture_status || capture.release_status || capture.lease_end_status ||
     capture.snapshot.size()!=capture.bytes || !capture.bytes || (capture.stride!=2 && capture.stride!=4) ||
     capture.bytes!=uint64_t(capture.records)*capture.stride)return 112;
  if(!indices.empty() && word<=indices.back().word)return 112;
  const uint64_t amount=sizeof(Index)+capture.snapshot.size();if(amount>limit-bytes)return status=88;
  try{indices.push_back({word,std::move(capture)});}catch(...){return status=112;}
  bytes+=amount;return 0;
 }
 std::vector<Route> routes;
 int retain_compute(uint32_t word,const CtrPm4::ComputeShaderState& state,CtrPm4::OwnedShaderBinary&& binary){
  if(status)return status;
  if(routes.empty() || routes.back().word!=word || routes.back().opcode!=0x15 || state.codeAddress!=binary.guestAddress || binary.words.empty())return status=112;
  const auto& route=routes.back();DispatchPacketInputs dispatch;
  const int packet_status=prepare_dispatch_packet(route.packet.data(),route.packet.size(),dispatch);if(packet_status)return status=packet_status;
  CtrPm4::ComputeShaderState original;
  if(!route.registers || CtrPm4::CaptureComputeShaderState(*route.registers,original) ||
     original.codeAddress!=state.codeAddress || original.workgroup!=state.workgroup || original.partial!=state.partial ||
     original.userRegisters!=state.userRegisters || original.allocatedVgprs!=state.allocatedVgprs || original.sharedBytes!=state.sharedBytes ||
     original.tgid!=state.tgid || original.userData!=state.userData)return status=112;
  if(!computes.empty() && word<=computes.back().word)return status=112;
  const uint64_t amount=sizeof(Compute)+binary.words.size()*uint64_t(4);
  if(computes.size()>=32 || amount>limit-bytes)return status=88;
  try{computes.push_back({word,state,std::move(binary),dispatch,route.registers});}catch(...){return status=112;}
  bytes+=amount;return 0;
 }
 std::vector<Entry> entries;uint64_t bytes=0;int status=0;
 int retain_route(uint32_t word,uint32_t opcode,const uint32_t* packet,size_t count,const CtrPm4::Registers& registers){
  if(status)return status;
  if(!packet || !((opcode==0x2d && count==3) || (opcode==0x27 && count==6) || (opcode==0x15 && count==5)))return status=60;
  if(!routes.empty() && word<=routes.back().word)return status=112;
  const uint64_t amount=sizeof(Route)+sizeof(CtrPm4::Registers)+count*4;
  if(routes.size()>=128 || amount>limit-bytes)return status=88;
  try{Route prepared{word,opcode,std::vector<uint32_t>(packet,packet+count),std::make_shared<CtrPm4::Registers>(registers)};routes.push_back(std::move(prepared));}
  catch(...){return status=112;}
  bytes+=amount;return 0;
 }
 static constexpr uint64_t limit=512ull*1024*1024;
 int retain(uint32_t word,bool vertex,const std::shared_ptr<GraphicsStageOwner>& stage){
  if(status)return status;
  if(!stage)return status=60;
  if(!entries.empty() && (word<entries.back().word ||
     (word==entries.back().word && (!entries.back().vertex || vertex))))return status=112;
  uint64_t amount=sizeof(GraphicsStageOwner);
  const auto add=[&](uint64_t count,uint64_t size){if(count>limit/size || amount>limit-count*size)return false;amount+=count*size;return true;};
  if(!add(stage->spirv.size(),4) || !add(stage->flattened_userdata.size(),4) ||
     !add(stage->buffers.size(),sizeof(GraphicsBufferCapture)) || !add(stage->colors.size(),sizeof(GraphicsStageOwner::Color)) ||
     !add(stage->flatbufs.size(),sizeof(GraphicsStageOwner::Flatbuf)) || !add(stage->descriptors.size(),sizeof(GraphicsStageOwner::Descriptor)) ||
     !add(stage->clip_planes.size(),sizeof(GraphicsStageOwner::ClipPlanes)))return status=88;
  for(const auto& d:stage->descriptors)if(!add(d.image_payload.snapshot.size(),1))return status=88;
  for(const auto& b:stage->buffers)if(!add(b.snapshot.size(),1))return status=88;
  for(const auto& c:stage->colors)if(!add(c.initial.snapshot.size(),1))return status=88;
  for(const auto& b:stage->flatbufs)if(!add(b.words.size(),4))return status=88;
  if(entries.size()>=128 || amount>limit-bytes)return status=88;
  try{entries.push_back({word,vertex,stage});}catch(...){return status=112;}
  bytes+=amount;return 0;
 }
};
inline int graphics_submission_owner_control(){
 GraphicsSubmissionOwner batch;auto stage=std::make_shared<GraphicsStageOwner>();stage->spirv={7,8};
 std::weak_ptr<GraphicsStageOwner> weak=stage;
 if(batch.retain(255,true,stage))return 1;stage.reset();
 if(weak.expired() || batch.entries[0].stage->spirv[1]!=8)return 2;
 unsigned rejected=0;const auto count=batch.entries.size();const auto bytes=batch.bytes;
 if(batch.retain(254,false,std::make_shared<GraphicsStageOwner>())==112 && batch.entries.size()==count && batch.bytes==bytes)++rejected;
 GraphicsSubmissionOwner duplicate;if(duplicate.retain(255,false,weak.lock()))return 3;
 if(duplicate.retain(255,true,weak.lock())==112)++rejected;
 GraphicsSubmissionOwner absent;if(absent.retain(1,true,{})==60 && absent.entries.empty())++rejected;
 GraphicsSubmissionOwner capacity;capacity.bytes=GraphicsSubmissionOwner::limit;
 if(capacity.retain(1,true,weak.lock())==88 && capacity.entries.empty())++rejected;
 duplicate.entries.clear();batch.entries.clear();if(!weak.expired())return 4;
 auto registers=std::make_unique<CtrPm4::Registers>();registers->words[7]=19;registers->compute[12]=16;uint32_t packet[5]={0xc0031500,2,3,4,1};
 GraphicsSubmissionOwner route;
 if(route.retain_route(3,0x15,packet,5,*registers))return 6;
 CtrPm4::ComputeShaderState compute;compute.codeAddress=4096;CtrPm4::OwnedShaderBinary binary;binary.guestAddress=4096;binary.words={7,8};
 if(route.retain_compute(3,compute,std::move(binary)) || route.computes[0].binary.words[1]!=8 || route.computes[0].dispatch.groups[1]!=3 || route.computes[0].registers!=route.routes[0].registers)return 8;
 GraphicsSubmissionOwner no_route;CtrPm4::OwnedShaderBinary missing;
 if(no_route.retain_compute(3,compute,std::move(missing))==112 && no_route.computes.empty())++rejected;
 // Retail submission reaches a sixteenth compute at word12144.
 // Count bound and independent byte budget remain explicit and atomic.
 GraphicsSubmissionOwner compute_capacity;
 for(unsigned i=0;i<32;++i){CtrPm4::OwnedShaderBinary owned;owned.guestAddress=4096;owned.words={7,8};
  if(compute_capacity.retain_route(7*i,0x15,packet,5,*registers) || compute_capacity.retain_compute(7*i,compute,std::move(owned)))return 16;
 }
 if(compute_capacity.computes.size()!=32 || compute_capacity.computes[15].word!=105)return 17;
 if(compute_capacity.retain_route(224,0x15,packet,5,*registers))return 18;
 const auto compute_bytes=compute_capacity.bytes;CtrPm4::OwnedShaderBinary excess;excess.guestAddress=4096;excess.words={7,8};
 if(compute_capacity.retain_compute(224,compute,std::move(excess))!=88 || compute_capacity.computes.size()!=32 || compute_capacity.bytes!=compute_bytes)return 19;
 GraphicsSubmissionOwner compute_budget;if(compute_budget.retain_route(0,0x15,packet,5,*registers))return 20;
 compute_budget.bytes=GraphicsSubmissionOwner::limit;CtrPm4::OwnedShaderBinary bounded;bounded.guestAddress=4096;bounded.words={7,8};
 if(compute_budget.retain_compute(0,compute,std::move(bounded))!=88 || !compute_budget.computes.empty() || compute_budget.bytes!=GraphicsSubmissionOwner::limit)return 21;
 packet[1]=99;registers->words[7]=99;
 if(route.routes[0].packet[1]!=2 || route.routes[0].registers->words[7]!=19)return 7;
 if(route.retain_route(3,0x15,packet,5,*registers)==112 && route.routes.size()==1)++rejected;
 GraphicsSubmissionOwner malformed;if(malformed.retain_route(4,0x15,packet,4,*registers)==60 && malformed.routes.empty())++rejected;
 // Original second submit has64 routes/102 shader stages, exceeding old58/86 caps.
 GraphicsSubmissionOwner larger;
 for(unsigned i=0;i<128;++i){if(larger.retain_route(i*7,0x15,packet,5,*registers))return 9;}
 const auto retained_bytes=larger.bytes;
 if(larger.retain_route(128*7,0x15,packet,5,*registers)!=88 || larger.routes.size()!=128 || larger.bytes!=retained_bytes)return 10;
 GraphicsSubmissionOwner stages;auto empty_stage=std::make_shared<GraphicsStageOwner>();
 for(unsigned i=0;i<128;++i){if(stages.retain(i,true,empty_stage))return 11;}
 if(stages.retain(128,true,empty_stage)!=88 || stages.entries.size()!=128)return 12;
 if(rejected!=7)return 5;
 std::cout<<"{\"scope\":\"graphics_submission_owner_control\",\"passed\":true,\"negative_rejections\":7,\"compute_binary_owned\":true,\"route_deepcopy\":true,\"lifetime_released\":true,\"synthetic\":true,\"gpu_execution\":false}\n";return 0;
}
inline bool graphics_resources_valid(const CtrGraphicsResourcesV1& r);
// CPU input completeness only. Writable resources require a separate output
// and completion contract; an input snapshot cannot admit them for execution.
// Reached read-only image domain: exact public invalid-type GetSharp fallback,
// with all eight source DWORD identities measured before flattening loses them.
// This permits only a semantic null requirement; GPU capability/binding remains separate.
inline bool verified_effective_null_image(const GraphicsStageOwner::Descriptor& d){
 const auto& e=d.effective_class;
 return d.kind==1 && !d.status && d.copied_words==8 && d.source_span_verified && !d.image_class_status && !d.image_class.type_valid && !d.effective_class_status && e.version==1 && e.size==sizeof(e) && !e.raw_type_valid && e.effective_type_valid==1 && e.fallback_reason==1 && e.null_reasons==1;
}
inline int effective_null_requirement_control(){
 GraphicsStageOwner::Descriptor d{1,112};d.status=d.image_class_status=d.effective_class_status=0;d.copied_words=8;d.source_span_verified=true;d.effective_class={1,sizeof(CtrGraphicsEffectiveImageClassV1),0,1,1,1};
 if(!verified_effective_null_image(d))return 1;
 for(unsigned test=0;test<9;++test){auto bad=d;switch(test){case 0:bad.source_span_verified=false;break;case 1:bad.effective_class_status=88;break;case 2:bad.effective_class.null_reasons=0;break;case 3:bad.effective_class.fallback_reason=2;break;case 4:bad.effective_class.raw_type_valid=1;break;case 5:bad.image_class.type_valid=1;break;case 6:bad.effective_class.effective_type_valid=0;break;case 7:bad.copied_words=4;break;case 8:bad.effective_class.size=0;break;}if(verified_effective_null_image(bad))return 2;}
 std::vector<CtrShaderProvenanceV1> span(8);for(uint32_t i=0;i<8;++i)span[i]={0,112+i,0,8,128+i,1};if(!contiguous_image_source_span(span,112))return 3;span[4].source_dword=0;if(contiguous_image_source_span(span,112))return 4;
 std::cout<<"{\"scope\":\"effective_null_requirement_control\",\"passed\":true,\"negative_cases\":10,\"synthetic\":true,\"GPU_execution\":false}\n";return 0;
}
inline int graphics_input_capture_status(const GraphicsStageOwner& stage,uint32_t kinds=15){
 if(kinds&~15u)return 60;
 if(!graphics_resources_valid(stage.resources))return 112;
 if(((kinds&8u) && stage.resources.fmasks) ||
    ((kinds&1u) && stage.buffers.size()+stage.clip_planes.size()+stage.flatbufs.size()!=stage.resources.buffers))return 88;
 for(uint32_t i=0;i<stage.resources.binding_count;++i){const auto& binding=stage.resources.bindings[i];
  if(!(kinds&(1u<<binding.kind)))continue;
  if(binding.flags&1)return 88;
  if(binding.kind==1 || binding.kind==2){
   const auto at=std::find_if(stage.descriptors.begin(),stage.descriptors.end(),[&](const GraphicsStageOwner::Descriptor& d){return d.kind==binding.kind && d.sharp==binding.sharp_index;});
   if(at==stage.descriptors.end())return 88;if(at->status)return at->status;
   if(at->copied_words!=(binding.kind==1?8u:4u))return 112;
   if(binding.kind==2){if(at->sampler_status)return at->sampler_status;if(at->sampler.version!=1 || at->sampler.size!=sizeof(at->sampler))return 112;continue;}
   if(!(binding.flags&(8u|16u|32u|64u)) && verified_effective_null_image(*at))continue;
   if(at->image_status)return at->image_status;if(at->image.version!=1 || at->image.size!=sizeof(at->image))return 112;if(at->image_extent_status)return at->image_extent_status;
   const auto& extent=at->image_extent;const auto& b=at->image_payload;
   if(extent.version!=1 || extent.size!=sizeof(extent) || extent.guest_address!=at->image.guest_address || !extent.bytes || b.address!=extent.guest_address || b.bytes!=extent.bytes)return 112;
   if(at->resident_image_authority){const int resident=at->resident_image_authority();if(resident)return resident;continue;}
   if(at->submission_image_authority){const int current=at->submission_image_authority();if(current)return current;continue;}
   for(const int status:{b.requirement_status,b.lease_status,b.capture_status,b.release_status,b.lease_end_status})if(status)return status;
   if(b.snapshot.size()!=extent.bytes)return 112;continue;
  }
  if(binding.kind!=0)return 88;
  if(binding.buffer_type==1){
   const auto found=std::find_if(stage.flatbufs.begin(),stage.flatbufs.end(),[&](const GraphicsStageOwner::Flatbuf& b){return b.sharp==binding.sharp_index;});
   if(found==stage.flatbufs.end())return 88;if(found->status)return found->status;
   if(found->words.size()!=stage.flattened_userdata.size() || found->words.size()<16 || found->words.size()>65536 ||
      found->bytes!=found->words.size()*sizeof(uint32_t) || found->words!=stage.flattened_userdata)return 112;
   continue;
  }
  if(binding.buffer_type==6 && (binding.flags&2)){
   const auto clip=std::find_if(stage.clip_planes.begin(),stage.clip_planes.end(),[&](const GraphicsStageOwner::ClipPlanes& c){return c.sharp==binding.sharp_index;});
   if(clip==stage.clip_planes.end())return 88;if(clip->status)return clip->status;
   const auto& value=clip->value;if(value.version!=1 || value.size!=sizeof(value) || value.enabled_mask>63 || value.bytes!=(value.enabled_mask?96u:0u))return 112;
   continue;
  }
  const auto found=std::find_if(stage.buffers.begin(),stage.buffers.end(),[&](const GraphicsBufferCapture& b){return b.sharp==binding.sharp_index;});
  if(found==stage.buffers.end())return 88;
  for(const int status:{found->requirement_status,found->lease_status,found->capture_status,found->release_status,found->lease_end_status})if(status)return status;
  if(!found->bytes || found->snapshot.size()!=found->bytes)return 112;
 }
 return 0;
}
struct GraphicsCompileObservation {int status=88;uint32_t words=0;std::string diagnostic;DWORD loader_error=0;std::shared_ptr<CtrGraphicsResourcesV1> resources;std::shared_ptr<GraphicsStageOwner> stage;};
inline bool graphics_resources_valid(const CtrGraphicsResourcesV1& r){
 const uint64_t expected=uint64_t(r.buffers)+r.images+r.samplers+r.fmasks;
 if(r.version!=1 || r.size!=sizeof(r) || r.binding_count>128 || r.binding_count!=expected)return false;
 uint32_t counts[4]{};
 for(uint32_t i=0;i<r.binding_count;++i){const auto& item=r.bindings[i];
  if(item.kind>3 || (item.flags&~127u))return false;++counts[item.kind];}
 return counts[0]==r.buffers && counts[1]==r.images && counts[2]==r.samplers && counts[3]==r.fmasks;
}
struct GraphicsStageSinkContext {uint64_t address;bool vertex;unsigned calls=0;std::shared_ptr<GraphicsStageOwner> output;};
inline int graphics_stage_sink(void* context,const CtrCompiledGraphicsStageV1* stage){
 if(!context || !stage)return 60;auto& target=*static_cast<GraphicsStageSinkContext*>(context);
 if(target.calls++ || stage->version!=1 || stage->size!=sizeof(*stage) || stage->vertex!=(target.vertex?1u:0u) || stage->code_address!=target.address ||
    !stage->spirv || stage->spirv_words<5 || stage->spirv_words>4*1024*1024 || stage->spirv[0]!=0x07230203 ||
    !stage->resources || !graphics_resources_valid(*stage->resources) || (!stage->userdata && stage->userdata_words) || stage->userdata_words>16)return 112;
 try{auto prepared=std::make_shared<GraphicsStageOwner>();
  prepared->spirv.assign(stage->spirv,stage->spirv+stage->spirv_words);
  if(stage->userdata_words)std::copy_n(stage->userdata,stage->userdata_words,prepared->userdata.data());prepared->userdata_words=stage->userdata_words;
  prepared->resources=*stage->resources;target.output=std::move(prepared);return 0;
 }catch(...){return 112;}
}
inline int graphics_stage_sink_control(){
 uint32_t spirv[5]{0x07230203,0x10000,0,1,0},userdata[4]{11,22,33,44};
 CtrGraphicsResourcesV1 resources{};resources.version=1;resources.size=sizeof(resources);
 resources.buffers=1;resources.binding_count=1;resources.bindings[0]={0,0,0,0,0};
 CtrCompiledGraphicsStageV1 stage{1,sizeof(CtrCompiledGraphicsStageV1),0,5,0x1000,spirv,&resources,userdata,4};
 GraphicsStageSinkContext context{0x1000,false,0,{}};
 if(graphics_stage_sink(&context,&stage) || !context.output || context.output->spirv.size()!=5)return 1;
 spirv[1]=0;userdata[0]=99;resources.buffers=0;
 if(context.output->spirv[1]!=0x10000 || context.output->userdata[0]!=11 || context.output->resources.buffers!=1)return 1;
 unsigned rejected=0;if(graphics_stage_sink(&context,&stage)==112)++rejected;
 resources=context.output->resources;
 for(unsigned test=0;test<9;++test){auto bad=stage;auto invalid=resources;GraphicsStageSinkContext fresh{0x1000,false,0,{}};
  switch(test){case 0:bad.version=2;break;case 1:bad.size=0;break;case 2:bad.vertex=1;break;
   case 3:bad.code_address=0x2000;break;case 4:bad.spirv_words=4;break;case 5:bad.spirv=nullptr;break;
   case 6:bad.userdata_words=17;break;case 7:invalid.binding_count=129;bad.resources=&invalid;break;
   case 8:invalid.bindings[0].kind=4;bad.resources=&invalid;break;}
  if(graphics_stage_sink(&fresh,&bad)==112 && !fresh.output)++rejected;
 }
 if(rejected!=10)return 1;
 std::cout<<"{\"scope\":\"graphics_stage_sink_control\",\"passed\":true,\"negative_rejections\":10,\"deep_copy_survives_source_mutation\":true,\"synthetic\":true,\"GPU_execution\":false}\n";return 0;
}
// Explicit extended transport only. Legacy v1 cannot supply extended descriptor
// authority. Prepare locally so failed extended delivery never publishes a stage.
inline int graphics_stage_sink_v2(void* context,const CtrCompiledGraphicsStageV2* extended){
 if(!context || !extended)return 60;
 auto& target=*static_cast<GraphicsStageSinkContext*>(context);
 if(target.calls++ || extended->version!=2 || extended->size!=sizeof(*extended) ||
    !extended->flattened_userdata || extended->flattened_words<16 || extended->flattened_words>65536 ||
    !extended->stage.resources || extended->stage.resources->flattened_userdata_words!=extended->flattened_words)return 112;
 GraphicsStageSinkContext temporary{target.address,target.vertex,0,{}};
 const int status=graphics_stage_sink(&temporary,&extended->stage);if(status)return status;
 try{temporary.output->flattened_userdata.assign(extended->flattened_userdata,extended->flattened_userdata+extended->flattened_words);
  // The direct SGPR prefix is copied by the compiler, never recovered from a
  // raw pointer here. Reject mismatched transport before any resource decoding.
  if(!std::equal(temporary.output->userdata.begin(),temporary.output->userdata.begin()+temporary.output->userdata_words,temporary.output->flattened_userdata.begin()))return 112;
  target.output=std::move(temporary.output);return 0;
 }catch(...){return 112;}
}
inline int graphics_extended_sink_control(){
 uint32_t spirv[5]{0x07230203,0x10000,0,1,0},userdata[4]{11,22,33,44},flat[17]{11,22,33,44};flat[16]=99;
 CtrGraphicsResourcesV1 resources{};resources.version=1;resources.size=sizeof(resources);resources.flattened_userdata_words=17;
 CtrCompiledGraphicsStageV1 stage{1,sizeof(stage),0,5,0x1000,spirv,&resources,userdata,4};
 CtrCompiledGraphicsStageV2 view{2,sizeof(view),stage,flat,17};GraphicsStageSinkContext accepted{0x1000,false,0,{}};
 if(graphics_stage_sink_v2(&accepted,&view) || !accepted.output || accepted.output->flattened_userdata.size()!=17)return 1;
 flat[16]=0;if(accepted.output->flattened_userdata[16]!=99)return 2;
 unsigned rejected=0;
 for(unsigned i=0;i<8;++i){auto bad=view;auto meta=resources;GraphicsStageSinkContext fresh{0x1000,false,0,{}};
  switch(i){case 0:bad.version=1;break;case 1:bad.size=0;break;case 2:bad.flattened_userdata=nullptr;break;
   case 3:bad.flattened_words=15;break;case 4:bad.flattened_words=65537;break;
   case 5:meta.flattened_userdata_words=16;bad.stage.resources=&meta;break;
   case 6:bad.stage.userdata_words=17;break;case 7:flat[0]=0;break;}
  if(graphics_stage_sink_v2(&fresh,&bad)==112 && !fresh.output)++rejected;flat[0]=11;
 }
 if(graphics_stage_sink_v2(&accepted,&view)==112 && accepted.output->flattened_userdata[16]==99)++rejected;
 if(rejected!=9)return 3;
 std::cout<<"{\"scope\":\"graphics_extended_sink_control\",\"passed\":true,\"negative_rejections\":9,\"deep_copy_survives_source_mutation\":true,\"synthetic\":true,\"production_enabled\":false,\"GPU_execution\":false}\n";return 0;
}
inline int graphics_buffer_capture_control(){
 auto backing=std::make_shared<DirectMemoryBacking>(16384);int64_t physical=-1;void* mapped=nullptr;
 if(backing->allocate(0,16384,16384,0,0,&physical)||backing->map(&mapped,16384,51,0,physical,0))return 1;
 const auto address=reinterpret_cast<uint64_t>(mapped);std::memset(mapped,0x5a,960);
 ShaderSnapshotOwner owner(backing);auto inputs=owner.api();auto ranges=owner.ranges();
 CtrGraphicsBufferRequirementV1 needed{1,sizeof(CtrGraphicsBufferRequirementV1),0,0,address,960,16,60};
 const auto good=capture_graphics_buffer(inputs,ranges,needed,960);
 std::memset(mapped,0,960);
 if(good.capture_status || good.release_status || good.lease_end_status || good.snapshot.size()!=960 || good.snapshot[0]!=0x5a || good.snapshot[959]!=0x5a)return 1;
 unsigned rejected=0;
 auto bad=needed;bad.guest_address=address+16384-32;bad.bytes=64;
 auto result=capture_graphics_buffer(inputs,ranges,bad,960);
 if(result.lease_status==0 && result.capture_status==88 && result.lease_end_status==0 && result.snapshot.empty())++rejected;
 bad=needed;bad.guest_address=1;result=capture_graphics_buffer(inputs,ranges,bad,960);if(result.lease_status==88 && result.snapshot.empty())++rejected;
 result=capture_graphics_buffer(inputs,ranges,needed,959);if(result.lease_status==88 && result.snapshot.empty() && !result.range_attempted && result.budget_bytes==959)++rejected;
 auto invalid=inputs;invalid.capture=nullptr;result=capture_graphics_buffer(invalid,ranges,needed,960);if(result.lease_status==60 && result.snapshot.empty())++rejected;
 bad=needed;bad.guest_address=UINT64_MAX-31;result=capture_graphics_buffer(inputs,ranges,bad,960);if(result.requirement_status==112 && result.snapshot.empty())++rejected;
 if(rejected!=5)return 1;
 GraphicsStageOwner stage;stage.resources.version=1;stage.resources.size=sizeof(stage.resources);
 stage.resources.buffers=stage.resources.binding_count=1;stage.resources.bindings[0]={0,0,0,0,0};stage.buffers.push_back(good);
 if(graphics_input_capture_status(stage))return 1;
 stage.resources.bindings[0].flags=1;if(graphics_input_capture_status(stage)!=88)return 1;
 stage.resources.bindings[0].flags=0;stage.resources.images=1;stage.resources.binding_count=2;stage.resources.bindings[1]={1,4,0,0,0};if(graphics_input_capture_status(stage)!=88)return 1;
 stage.resources.images=0;stage.resources.binding_count=1;stage.buffers[0].release_status=112;if(graphics_input_capture_status(stage)!=112)return 1;
 std::cout<<"{\"scope\":\"graphics_buffer_capture_control\",\"passed\":true,\"negative_rejections\":5,\"input_completeness_negative_rejections\":3,\"partial_lease_released\":true,\"immutable_960_bytes\":true,\"synthetic\":true,\"GPU_execution\":false}\n";return 0;
}
inline int graphics_image_owner_control(){
 GraphicsStageOwner stage;stage.resources.version=1;stage.resources.size=sizeof(stage.resources);
 stage.resources.images=1;stage.resources.samplers=1;stage.resources.binding_count=2;
 stage.resources.bindings[0]={1,0,0,0,0};stage.resources.bindings[1]={2,8,0,0,0};
 GraphicsStageOwner::Descriptor image{1,0};image.status=0;image.copied_words=8;image.image_status=0;image.image.version=1;image.image.size=sizeof(image.image);image.image.guest_address=4096;
 image.image_extent_status=0;image.image_extent={1,sizeof(CtrGraphicsImageExtentV1),4,4,4096,64};
 auto& b=image.image_payload;b.address=4096;b.bytes=64;b.snapshot.resize(64);b.requirement_status=b.lease_status=b.capture_status=b.release_status=b.lease_end_status=0;
 GraphicsStageOwner::Descriptor sampler{2,8};sampler.status=0;sampler.copied_words=4;sampler.sampler_status=0;sampler.sampler.version=1;sampler.sampler.size=sizeof(sampler.sampler);
 stage.descriptors={image,sampler};if(graphics_input_capture_status(stage))return 1;
 unsigned rejected=0;
 stage.descriptors[0].image_extent.bytes=65;if(graphics_input_capture_status(stage)==112)++rejected;stage.descriptors[0]=image;
 stage.descriptors[0].image_payload.snapshot.pop_back();if(graphics_input_capture_status(stage)==112)++rejected;stage.descriptors[0]=image;
 stage.descriptors[0].image_payload.release_status=112;if(graphics_input_capture_status(stage)==112)++rejected;stage.descriptors[0]=image;
 stage.descriptors[0].image_extent_status=88;if(graphics_input_capture_status(stage)==88)++rejected;stage.descriptors[0]=image;
 stage.descriptors[1].sampler_status=88;if(graphics_input_capture_status(stage)==88)++rejected;stage.descriptors[1]=sampler;
 stage.resources.bindings[0].flags=1;if(graphics_input_capture_status(stage)==88)++rejected;stage.resources.bindings[0].flags=0;
 stage.descriptors.erase(stage.descriptors.begin());if(graphics_input_capture_status(stage)==88)++rejected;
 if(rejected!=7)return 2;
 stage.descriptors={image,sampler};auto lifetime=std::make_shared<int>(7);std::weak_ptr<int> weak=lifetime;
 auto& guarded=stage.descriptors[0];guarded.image_payload.snapshot.clear();guarded.image_payload.capture_status=88;
 guarded.submission_image_authority=[weak](){const auto owner=weak.lock();return owner && *owner==7?0:88;};
 if(graphics_input_capture_status(stage))return 3;
 *lifetime=8;if(graphics_input_capture_status(stage)!=88)return 4;*lifetime=7;
 guarded.image_extent.bytes=65;if(graphics_input_capture_status(stage)!=112)return 5;guarded.image_extent.bytes=64;
 lifetime.reset();if(graphics_input_capture_status(stage)!=88)return 6;
 if(!guarded.image_payload.snapshot.empty() || guarded.image_payload.capture_status!=88)return 7;
 std::cout<<"{\"scope\":\"graphics_image_owner_control\",\"passed\":true,\"negative_rejections\":7,\"submission_authority_rejections\":3,\"expired_owner_rejected\":true,\"capture_failure_preserved\":true,\"synthetic\":true,\"gpu_execution\":false}\n";return 0;
}
inline int graphics_flatbuf_owner_control(){
 GraphicsStageOwner stage;stage.flattened_userdata.resize(20,7);
 stage.resources.version=1;stage.resources.size=sizeof(stage.resources);stage.resources.buffers=stage.resources.binding_count=1;
 stage.resources.bindings[0]={0,16,0,1,0};
 stage.flatbufs.push_back({16,0,80,stage.flattened_userdata});
 if(graphics_input_capture_status(stage))return 1;
 unsigned rejected=0;
 stage.flatbufs[0].words[19]=8;if(graphics_input_capture_status(stage)==112)++rejected;stage.flatbufs[0].words[19]=7;
 stage.flatbufs[0].bytes=76;if(graphics_input_capture_status(stage)==112)++rejected;stage.flatbufs[0].bytes=80;
 stage.flatbufs[0].status=88;if(graphics_input_capture_status(stage)==88)++rejected;stage.flatbufs[0].status=0;
 stage.resources.bindings[0].flags=1;if(graphics_input_capture_status(stage)==88)++rejected;stage.resources.bindings[0].flags=0;
 stage.flatbufs[0].sharp=17;if(graphics_input_capture_status(stage)==88)++rejected;stage.flatbufs[0].sharp=16;
 stage.flatbufs.clear();if(graphics_input_capture_status(stage)==88)++rejected;
 if(rejected!=6)return 2;
 std::cout<<"{\"scope\":\"graphics_flatbuf_owner_control\",\"passed\":true,\"negative_rejections\":6,\"synthetic\":true,\"GPU_execution\":false}\n";return 0;
}
inline GraphicsCompileObservation compile_graphics_on_host(const CtrShaderInputsV1& inputs,
 uint64_t address,uint64_t extent,const CtrPm4::Registers& registers,bool vertex,
 const CtrGraphicsProfileV1& profile,const CtrShaderOwnedRangesV1& ranges) {
 GraphicsCompileObservation result;
 // v6 requires explicit owned ranges for nested fetch code/descriptors.
 wchar_t path[4096]{};
 const auto length=GetFullPathNameW(L"build/CTRPort-infra/graphics_companions/ctr_graphics_companion_v33.dll",4096,path,nullptr);
 if(!length || length>=4096){result.loader_error=GetLastError();result.diagnostic="Compiler DLL absolute path unavailable";return result;}
 HMODULE module=LoadLibraryExW(path,nullptr,LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR|LOAD_LIBRARY_SEARCH_DEFAULT_DIRS);
 if(!module){result.loader_error=GetLastError();result.diagnostic="Compiler DLL load failed";return result;}
 struct LoadedCompiler{HMODULE value;~LoadedCompiler(){FreeLibrary(value);}} loaded{module};
 const auto call=reinterpret_cast<decltype(&ctr_graphics_compile_stage_v3)>(GetProcAddress(module,"ctr_graphics_compile_stage_v3"));
 const auto requirement=reinterpret_cast<decltype(&ctr_graphics_buffer_requirement_v2)>(GetProcAddress(module,"ctr_graphics_buffer_requirement_v2"));
 const auto clip_planes=reinterpret_cast<decltype(&ctr_graphics_clip_planes_v1)>(GetProcAddress(module,"ctr_graphics_clip_planes_v1"));
 const auto color_requirement=reinterpret_cast<decltype(&ctr_graphics_color_requirement_v1)>(GetProcAddress(module,"ctr_graphics_color_requirement_v1"));
 const auto flatbuf_copy=reinterpret_cast<decltype(&ctr_graphics_flatbuf_copy_v1)>(GetProcAddress(module,"ctr_graphics_flatbuf_copy_v1"));
 const auto descriptor_copy=reinterpret_cast<decltype(&ctr_graphics_descriptor_copy_v1)>(GetProcAddress(module,"ctr_graphics_descriptor_copy_v1"));
 const auto sampler_parameters=reinterpret_cast<decltype(&ctr_graphics_sampler_parameters_v1)>(GetProcAddress(module,"ctr_graphics_sampler_parameters_v1"));
 const auto image_classify=reinterpret_cast<decltype(&ctr_graphics_image_classify_v1)>(GetProcAddress(module,"ctr_graphics_image_classify_v1"));
 const auto image_parameters=reinterpret_cast<decltype(&ctr_graphics_image_parameters_v1)>(GetProcAddress(module,"ctr_graphics_image_parameters_v1"));
 const auto image_extent=reinterpret_cast<decltype(&ctr_graphics_image_extent_v2)>(GetProcAddress(module,"ctr_graphics_image_extent_v2"));
 if(!call || !requirement || !clip_planes || !color_requirement || !flatbuf_copy || !descriptor_copy || !sampler_parameters || !image_classify || !image_parameters || !image_extent){result.loader_error=GetLastError();result.diagnostic="Compiler stage or resource requirement export missing";return result;}
 const auto observe=reinterpret_cast<decltype(&ctr_graphics_provenance_observer_v1)>(GetProcAddress(module,"ctr_graphics_provenance_observer_v1"));
 char reason[2048]{};GraphicsStageSinkContext sink{address,vertex,0,{}};
 char trace_private[2]{};const bool private_trace=GetEnvironmentVariableA("CTR_PRIVATE_DESCRIPTOR_EVIDENCE",trace_private,2)==1 && trace_private[0]=='1';
 ShaderCaptureTrace trace(inputs);const auto traced=trace.api();
 ShaderProvenanceTrace provenance;provenance.span=reinterpret_cast<decltype(&ctr_graphics_provenance_span_v1)>(GetProcAddress(module,"ctr_graphics_provenance_span_v1"));
 const auto effective_classify=reinterpret_cast<decltype(&ctr_graphics_effective_image_classify_v1)>(GetProcAddress(module,"ctr_graphics_effective_image_classify_v1"));
 struct ResetObserver{decltype(observe) setter;bool active=false;~ResetObserver(){if(active)setter(nullptr,nullptr);}} reset_observer{observe};
 {if(!observe || !provenance.span){result.status=88;result.diagnostic="Provenance observer export missing";return result;}const int enabled=observe(ShaderProvenanceTrace::observe,&provenance);if(enabled){result.status=enabled;result.diagnostic="Provenance observer enable failed";return result;}reset_observer.active=true;}
 result.status=call(private_trace?&traced:&inputs,&ranges,address,extent,registers.words.data(),static_cast<uint32_t>(registers.words.size()),vertex?1u:0u,&profile,graphics_stage_sink_v2,&sink,reason,static_cast<uint32_t>(sizeof(reason)));
 if(reset_observer.active){const int cleared=observe(nullptr,nullptr);reset_observer.active=false;if(cleared){result.status=cleared;result.diagnostic="Provenance observer reset failed";return result;}}
 result.diagnostic=reason;
 if(!result.status){
  if(sink.calls!=1 || !sink.output){result.status=112;result.diagnostic="Successful compiler did not deliver exactly one owned stage";return result;}
  result.stage=std::move(sink.output);auto& stage=*result.stage;result.words=static_cast<uint32_t>(stage.spirv.size());
  result.resources=std::make_shared<CtrGraphicsResourcesV1>(stage.resources);
  uint64_t captured_bytes=0,captured_image_bytes=0;
  for(uint32_t i=0;i<stage.resources.binding_count;++i){const auto& binding=stage.resources.bindings[i];
   if(binding.kind==1 || binding.kind==2){
    GraphicsStageOwner::Descriptor descriptor{binding.kind,binding.sharp_index};
    descriptor.status=descriptor_copy(&binding,stage.flattened_userdata.data(),static_cast<uint32_t>(stage.flattened_userdata.size()),descriptor.words.data(),static_cast<uint32_t>(descriptor.words.size()),&descriptor.copied_words);
    if(!descriptor.status && descriptor.copied_words!=(binding.kind==1?8u:4u))descriptor.status=112;
    if(descriptor.status){descriptor.words.fill(0);descriptor.copied_words=0;}
    if(!descriptor.status && binding.kind==2){
     descriptor.sampler_status=sampler_parameters(descriptor.words.data(),descriptor.copied_words,&descriptor.sampler);
     const auto& p=descriptor.sampler;
     if(!descriptor.sampler_status && (p.version!=1 || p.size!=sizeof(p) || !std::isfinite(p.lod_bias) || !std::isfinite(p.min_lod) || !std::isfinite(p.max_lod) || !std::isfinite(p.max_anisotropy)))descriptor.sampler_status=112;
     if(descriptor.sampler_status)descriptor.sampler={};
    }
    if(!descriptor.status && binding.kind==1){
     descriptor.source_span_verified=!provenance.status && !provenance.span_status && contiguous_image_source_span(provenance.selected_span,binding.sharp_index);
     if(effective_classify)descriptor.effective_class_status=effective_classify(descriptor.words.data(),descriptor.copied_words,(binding.flags>>3)&1u,(binding.flags>>6)&1u,&descriptor.effective_class);
     descriptor.image_class_status=image_classify(descriptor.words.data(),descriptor.copied_words,&descriptor.image_class);
     const auto& classification=descriptor.image_class;
     if(!descriptor.image_class_status && (classification.version!=1 || classification.size!=sizeof(classification) || classification.null_reasons>3 || classification.type_valid>1))descriptor.image_class_status=112;
     if(descriptor.image_class_status)descriptor.image_class={};
     descriptor.image_status=image_parameters(descriptor.words.data(),descriptor.copied_words,&descriptor.image);
     const auto& p=descriptor.image;
     if(!descriptor.image_status && (p.version!=1 || p.size!=sizeof(p) || !p.width || !p.height || !p.depth || !p.pitch || !p.layers || !p.levels || !p.samples))descriptor.image_status=112;
     if(descriptor.image_status)descriptor.image={};
     else{
      descriptor.image_extent_status=image_extent(&descriptor.image,&descriptor.image_extent);
      const auto& extent_value=descriptor.image_extent;
      if(!descriptor.image_extent_status && (extent_value.version!=1 || extent_value.size!=sizeof(extent_value) || extent_value.guest_address!=p.guest_address || !extent_value.bytes))descriptor.image_extent_status=112;
      if(!descriptor.image_extent_status){
       CtrGraphicsBufferRequirementV1 needed{};needed.version=1;needed.size=sizeof(needed);needed.sharp_index=binding.sharp_index;needed.guest_address=extent_value.guest_address;needed.bytes=extent_value.bytes;
       descriptor.image_payload=capture_graphics_buffer(inputs,ranges,needed,(std::min)(uint64_t(32*1024*1024),uint64_t(64*1024*1024)-captured_image_bytes),32*1024*1024);
       captured_image_bytes+=descriptor.image_payload.snapshot.size();
      }
     }
    }
    // Descriptor capture alone never admits pointed-to image contents or GPU use.
    stage.descriptors.push_back(std::move(descriptor));continue;
   }
   if(binding.kind!=0)continue;
   if(binding.buffer_type==1){
    GraphicsStageOwner::Flatbuf flat{binding.sharp_index};
    if(stage.flattened_userdata.size()*sizeof(uint32_t)<=16*1024*1024-captured_bytes){
     flat.words.resize(stage.flattened_userdata.size());
     flat.status=flatbuf_copy(&binding,stage.flattened_userdata.data(),static_cast<uint32_t>(stage.flattened_userdata.size()),flat.words.data(),static_cast<uint32_t>(flat.words.size()),&flat.bytes);
     if(!flat.status && (flat.bytes!=flat.words.size()*sizeof(uint32_t) || flat.words!=stage.flattened_userdata))flat.status=112;
     if(flat.status)flat.words.clear();else captured_bytes+=flat.bytes;
    }
    stage.flatbufs.push_back(std::move(flat));continue;
   }
   if(binding.buffer_type==6 && (binding.flags&2)){
    GraphicsStageOwner::ClipPlanes clip{binding.sharp_index,88,{}};clip.status=clip_planes(&binding,registers.words.data(),static_cast<uint32_t>(registers.words.size()),&clip.value);
    stage.clip_planes.push_back(std::move(clip));continue;
   }
   GraphicsBufferCapture observed;observed.sharp=binding.sharp_index;CtrGraphicsBufferRequirementV1 needed{};
   observed.requirement_status=requirement(&binding,stage.flattened_userdata.data(),static_cast<uint32_t>(stage.flattened_userdata.size()),&needed);
   if(!observed.requirement_status){
    if(needed.sharp_index!=binding.sharp_index)observed.requirement_status=112;
    else{observed=capture_graphics_buffer(inputs,ranges,needed,16*1024*1024-captured_bytes);
     captured_bytes+=observed.snapshot.size();}
   }
   stage.buffers.push_back(std::move(observed));
  }
  if(vertex)for(uint32_t slot=0;slot<8;++slot)if(registers.words[0xa318+15*slot]){
   GraphicsStageOwner::Color color{slot,88,{}, {}};
   color.status=color_requirement(registers.words.data(),static_cast<uint32_t>(registers.words.size()),slot,&color.requirement);
   if(!color.status){const auto& r=color.requirement;
    if(r.version!=1 || r.size!=sizeof(r) || r.slot!=slot)color.status=112;
    else{CtrGraphicsBufferRequirementV1 demand{1,sizeof(CtrGraphicsBufferRequirementV1),slot,0,r.guest_address,r.bytes,0,0};
     color.initial=capture_graphics_buffer(inputs,ranges,demand,16*1024*1024-captured_bytes);captured_bytes+=color.initial.snapshot.size();}
   }
   stage.colors.push_back(std::move(color));
  }
 }
 // Opt-in retail research evidence, never part of publishable milestone reports.
 // Keep the exact compiler-selected indices and all descriptor siblings together:
 // a type-invalid image may expose an SRT provenance problem, not a null image.
 char capture_private[2]{};
 if(result.stage && GetEnvironmentVariableA("CTR_PRIVATE_DESCRIPTOR_EVIDENCE",capture_private,2)==1 && capture_private[0]=='1'){
  const auto& owned=*result.stage;
  const bool invalid=std::any_of(owned.descriptors.begin(),owned.descriptors.end(),[](const auto& d){return d.kind==1 && !d.status && !d.image_class_status && !d.image_class.type_valid;});
  if(invalid){
   std::ofstream evidence("private/infra_descriptor_evidence.json",std::ios::trunc);
   if(evidence){
    evidence<<"{\"scope\":\"private_original_retail_descriptor_evidence\",\"shader_address\":"<<address<<",\"process_id\":"<<GetCurrentProcessId()<<",\"vertex\":"<<(vertex?"true":"false")<<",\"flattened_words\":"<<owned.flattened_userdata.size()<<",\"descriptors\":[";
    for(size_t i=0;i<owned.descriptors.size();++i){const auto& d=owned.descriptors[i];if(i)evidence<<',';
     evidence<<"{\"kind\":"<<d.kind<<",\"sharp\":"<<d.sharp<<",\"status\":"<<d.status<<",\"words\":[";
     for(uint32_t j=0;j<d.copied_words;++j){if(j)evidence<<',';evidence<<d.words[j];}evidence<<"]}";
    }
    evidence<<"],\"provenance_status\":"<<provenance.status<<",\"selected_sources\":[";
    for(size_t i=0;i<provenance.records.size();++i){const auto& r=provenance.records[i];if(i)evidence<<',';evidence<<"{\"pc\":"<<r.pc<<",\"flat_index\":"<<r.flat_index<<",\"source_opcode\":"<<r.source_opcode<<",\"root_sgpr\":"<<r.root_sgpr<<",\"source_dword\":"<<r.source_dword<<",\"source_kind\":"<<r.source_kind<<"}";}
    evidence<<"],\"span_status\":"<<provenance.span_status<<",\"selected_span\":[";
    for(size_t i=0;i<provenance.selected_span.size();++i){const auto& r=provenance.selected_span[i];if(i)evidence<<',';evidence<<"{\"flat_index\":"<<r.flat_index<<",\"root_sgpr\":"<<r.root_sgpr<<",\"source_dword\":"<<r.source_dword<<",\"source_kind\":"<<r.source_kind<<"}";}
    evidence<<"],\"selected_instruction_windows\":[";bool first_window=true;
    for(const auto& selected:provenance.records){
     const bool wanted=std::any_of(owned.descriptors.begin(),owned.descriptors.end(),[&](const auto& d){return d.kind==1 && d.sharp==selected.flat_index && !d.status && !d.image_class_status && !d.image_class.type_valid;});
     if(!wanted || selected.pc%4)continue;
     for(const auto& captured:trace.records)if(captured.address==address && !captured.capture_status && !captured.release_status && captured.bytes.size()==captured.size && selected.pc<captured.size){
      const uint64_t start=selected.pc>=16?selected.pc-16:0,end=std::min<uint64_t>(captured.size,uint64_t(selected.pc)+32);
      if(end%4)continue;if(!first_window)evidence<<',';first_window=false;
      evidence<<"{\"pc\":"<<selected.pc<<",\"flat_index\":"<<selected.flat_index<<",\"byte_offset\":"<<start<<",\"words\":[";
      for(uint64_t at=start;at+4<=end;at+=4){uint32_t word=0;std::memcpy(&word,captured.bytes.data()+at,4);if(at!=start)evidence<<',';evidence<<word;}
      evidence<<"]}";
     }
    }
    evidence<<"],\"capture_trace_status\":"<<trace.trace_status<<",\"capture_ranges\":[";
    for(size_t i=0;i<trace.records.size();++i){const auto& record=trace.records[i];if(i)evidence<<',';
     evidence<<"{\"address\":"<<record.address<<",\"bytes\":"<<record.size<<",\"retained_bytes\":"<<record.bytes.size()<<",\"capture_status\":"<<record.capture_status<<",\"release_status\":"<<record.release_status<<",\"userdata_root_candidates\":[";bool first_root=true;
     for(uint32_t root=0;root+1<owned.userdata_words;++root){const uint64_t source=(uint64_t(owned.userdata[root])|(uint64_t(owned.userdata[root+1])<<32))&0xffffffffffffULL;
      if(source==record.address){if(!first_root)evidence<<',';first_root=false;evidence<<root;}}
     evidence<<"]}";
    }
    evidence<<"],\"invalid_source_matches\":[";bool first_match=true;
    for(const auto& d:owned.descriptors)if(d.kind==1 && !d.status && !d.image_class_status && !d.image_class.type_valid){
     for(size_t i=0;i<trace.records.size();++i){const auto& record=trace.records[i];
      for(size_t offset=0;offset+32<=record.bytes.size();offset+=4)if(!std::memcmp(record.bytes.data()+offset,d.words.data(),32)){
       if(!first_match)evidence<<',';first_match=false;
       evidence<<"{\"sharp\":"<<d.sharp<<",\"capture_index\":"<<i<<",\"source_address\":"<<record.address+offset<<",\"byte_offset\":"<<offset<<",\"matched_bytes\":32}";
      }
     }
    }
    evidence<<"]}\n";
   }
  }
 }
 return result;
}
// The original guest stack is intentionally small. Compiler host frames include
// a full register bank and must never consume it. Join before releasing inputs.
inline GraphicsCompileObservation compile_graphics_observed(const CtrShaderInputsV1& inputs,
 uint64_t address,uint64_t extent,const CtrPm4::Registers& registers,bool vertex,
 const CtrGraphicsProfileV1& profile,const CtrShaderOwnedRangesV1& ranges) {
 struct Work {const CtrShaderInputsV1& inputs;uint64_t address,extent;const CtrPm4::Registers& registers;bool vertex;const CtrGraphicsProfileV1& profile;const CtrShaderOwnedRangesV1& ranges;GraphicsCompileObservation result;};
 Work work{inputs,address,extent,registers,vertex,profile,ranges,{}};
 auto entry=[](void* argument)->DWORD {auto& w=*static_cast<Work*>(argument);
  try{w.result=compile_graphics_on_host(w.inputs,w.address,w.extent,w.registers,w.vertex,w.profile,w.ranges);}
  catch(const std::exception& error){w.result.status=112;w.result.diagnostic=error.what();}
  catch(...){w.result.status=112;w.result.diagnostic="Compiler host exception";}return 0;};
 HANDLE thread=CreateThread(nullptr,8*1024*1024,entry,&work,0,nullptr);
 if(!thread){work.result.loader_error=GetLastError();work.result.diagnostic="Compiler host thread creation failed";return work.result;}
 const DWORD waited=WaitForSingleObject(thread,INFINITE);CloseHandle(thread);
 if(waited!=WAIT_OBJECT_0)std::terminate(); // never release inputs under a live compiler
 return work.result;
}
inline std::string graphics_resources_json(const CtrGraphicsResourcesV1& r){
 std::ostringstream s;s<<"{\"version\":"<<r.version<<",\"buffers\":"<<r.buffers<<",\"images\":"<<r.images
 <<",\"samplers\":"<<r.samplers<<",\"fmasks\":"<<r.fmasks<<",\"readconst_types\":"<<r.readconst_types
 <<",\"userdata_mask\":"<<r.userdata_mask<<",\"flattened_userdata_words\":"<<r.flattened_userdata_words
 <<",\"has_fetch_shader\":"<<r.has_fetch_shader<<",\"fetch_shader_sgpr_base\":"<<r.fetch_shader_sgpr_base
 <<",\"mrt_mask\":"<<r.mrt_mask<<",\"loaded_attribute_components\":"<<r.loaded_attribute_components
 <<",\"stored_attribute_components\":"<<r.stored_attribute_components<<",\"binding_count\":"<<r.binding_count<<",\"bindings\":[";
 for(uint32_t i=0;i<r.binding_count;++i){const auto& b=r.bindings[i];if(i)s<<',';
  s<<"{\"kind\":"<<b.kind<<",\"sharp_index\":"<<b.sharp_index<<",\"flags\":"<<b.flags
   <<",\"buffer_type\":"<<b.buffer_type<<",\"associated_image\":"<<b.associated_image<<'}';}
 s<<"]}";return s.str();
}
// Diagnostic identity only; no original shader bytes are serialized.
inline std::string graphics_spirv_sha256(const std::vector<uint32_t>& words,int& status){
 status=60;if(words.empty() || words.size()>UINT32_MAX/sizeof(uint32_t))return {};
 BCRYPT_ALG_HANDLE algorithm=nullptr;status=112;
 if(BCryptOpenAlgorithmProvider(&algorithm,BCRYPT_SHA256_ALGORITHM,nullptr,0)<0)return {};
 unsigned char digest[32]{};const auto result=BCryptHash(algorithm,nullptr,0,reinterpret_cast<PUCHAR>(const_cast<uint32_t*>(words.data())),static_cast<ULONG>(words.size()*sizeof(uint32_t)),digest,sizeof(digest));
 BCryptCloseAlgorithmProvider(algorithm,0);if(result<0)return {};
 constexpr char digits[]="0123456789abcdef";std::string output;output.reserve(64);
 for(const auto byte:digest){output.push_back(digits[byte>>4]);output.push_back(digits[byte&15]);}
 status=0;return output;
}
inline std::string graphics_stage_json(const GraphicsStageOwner& stage){
 int digest_status=0;const auto digest=graphics_spirv_sha256(stage.spirv,digest_status);
 std::ostringstream s;s<<"{\"owned_spirv_sha256\":\""<<digest<<"\",\"spirv_hash_status\":"<<digest_status<<",\"owned_spirv_words\":"<<stage.spirv.size()<<",\"copied_userdata_words\":"<<stage.userdata_words<<",\"owned_flattened_userdata_words\":"<<stage.flattened_userdata.size()<<",\"cpu_input_capture_status\":"<<graphics_input_capture_status(stage)<<",\"buffers\":[";
 for(size_t i=0;i<stage.buffers.size();++i){if(i)s<<',';const auto& b=stage.buffers[i];
  s<<"{\"sharp_index\":"<<b.sharp<<",\"requirement_status\":"<<b.requirement_status<<",\"lease_status\":"<<b.lease_status
   <<",\"budget_bytes\":"<<b.budget_bytes<<",\"range_attempted\":"<<(b.range_attempted?"true":"false")<<",\"capture_status\":"<<b.capture_status<<",\"release_status\":"<<b.release_status<<",\"lease_end_status\":"<<b.lease_end_status<<",\"guest_address\":"<<b.address
   <<",\"bytes\":"<<b.bytes<<",\"owned_bytes\":"<<b.owned_bytes<<",\"copied_bytes\":"<<b.snapshot.size()
   <<",\"stride\":"<<b.stride<<",\"records\":"<<b.records<<'}';}
 s<<"],\"descriptors\":[";
 for(size_t i=0;i<stage.descriptors.size();++i){if(i)s<<',';const auto& d=stage.descriptors[i];s<<"{\"kind\":"<<d.kind<<",\"sharp_index\":"<<d.sharp<<",\"status\":"<<d.status<<",\"copied_words\":"<<d.copied_words<<",\"image_payload_owned\":"<<(!d.image_payload.snapshot.empty()?"true":"false")<<",\"gpu_admission\":false";
  if(d.kind==1){const auto& b=d.image_payload;const auto& x=d.image_extent;
   s<<",\"image_extent_status\":"<<d.image_extent_status<<",\"image_extent\":{\"pitch\":"<<x.pitch<<",\"padded_height\":"<<x.padded_height<<",\"address\":"<<x.guest_address<<",\"bytes\":"<<x.bytes<<"},\"image_copy\":{\"requirement_status\":"<<b.requirement_status<<",\"lease_status\":"<<b.lease_status<<",\"budget_bytes\":"<<b.budget_bytes<<",\"range_attempted\":"<<(b.range_attempted?"true":"false")<<",\"capture_status\":"<<b.capture_status<<",\"release_status\":"<<b.release_status<<",\"lease_end_status\":"<<b.lease_end_status<<",\"copied_bytes\":"<<b.snapshot.size()<<'}';
   s<<",\"descriptor_type_raw\":"<<(d.words[3]>>28)<<",\"descriptor_data_format_raw\":"<<((d.words[1]>>20)&63u)<<",\"descriptor_number_format_raw\":"<<((d.words[1]>>26)&15u)<<",\"image_class_status\":"<<d.image_class_status<<",\"null_image_reasons\":"<<d.image_class.null_reasons<<",\"image_type_valid\":"<<d.image_class.type_valid;
   // Diagnostic interpretation only: public AmdGpu::Buffer Table8.5 layout,
   // resource.h at pinned e3ce810. An invalid image never gains buffer authority.
   if(!d.status && d.copied_words==8 && !(d.words[3]>>28))s<<",\"buffer_interpretation_diagnostic_only\":{\"address\":"<<(uint64_t(d.words[0])|(uint64_t(d.words[1]&255u)<<32))<<",\"stride\":"<<((d.words[1]>>16)&16383u)<<",\"records\":"<<d.words[2]<<",\"data_format\":"<<((d.words[3]>>15)&15u)<<",\"number_format\":"<<((d.words[3]>>12)&7u)<<'}';
   const auto& p=d.image;s<<",\"image_status\":"<<d.image_status<<",\"descriptor_all_zero\":"<<((!d.status && d.copied_words==8 && std::all_of(d.words.begin(),d.words.end(),[](uint32_t word){return word==0;}))?"true":"false");
   s<<",\"effective_class_status\":"<<d.effective_class_status<<",\"effective_type_valid\":"<<d.effective_class.effective_type_valid<<",\"effective_null_reasons\":"<<d.effective_class.null_reasons<<",\"effective_fallback_reason\":"<<d.effective_class.fallback_reason<<",\"source_span_verified\":"<<(d.source_span_verified?"true":"false");
   if(!d.image_status)s<<",\"image_parameters\":{\"type\":"<<p.type<<",\"width\":"<<p.width<<",\"height\":"<<p.height<<",\"depth\":"<<p.depth<<",\"pitch\":"<<p.pitch<<",\"layers\":"<<p.layers<<",\"levels\":"<<p.levels<<",\"samples\":"<<p.samples<<",\"data_format\":"<<p.data_format<<",\"number_format\":"<<p.number_format<<",\"tile_mode\":"<<p.tile_mode<<",\"pow2pad\":"<<p.pow2pad<<",\"base_level\":"<<p.base_level<<",\"base_array\":"<<p.base_array<<",\"last_array\":"<<p.last_array<<",\"compression\":"<<p.compression<<",\"alt_tile\":"<<p.alt_tile<<",\"swizzle\":["<<p.swizzle[0]<<','<<p.swizzle[1]<<','<<p.swizzle[2]<<','<<p.swizzle[3]<<"],\"guest_address\":"<<p.guest_address<<'}';
  }
  if(d.kind==2){const auto& p=d.sampler;s.precision(9);s<<",\"sampler_status\":"<<d.sampler_status;
   if(!d.sampler_status)s<<",\"sampler_parameters\":{\"clamp\":["<<p.clamp_x<<','<<p.clamp_y<<','<<p.clamp_z<<"],\"mag_filter\":"<<p.mag_filter<<",\"min_filter\":"<<p.min_filter<<",\"mip_filter\":"<<p.mip_filter<<",\"compare_func\":"<<p.compare_func<<",\"border_color\":"<<p.border_color<<",\"unnormalized_in_shader\":"<<p.unnormalized_in_shader<<",\"anisotropic\":"<<p.anisotropic<<",\"lod_bias\":"<<p.lod_bias<<",\"min_lod\":"<<p.min_lod<<",\"max_lod\":"<<p.max_lod<<",\"max_anisotropy\":"<<p.max_anisotropy<<'}';
  }s<<'}';}
 s<<"],\"flatbufs\":[";
 for(size_t i=0;i<stage.flatbufs.size();++i){if(i)s<<',';const auto& b=stage.flatbufs[i];s<<"{\"sharp_index\":"<<b.sharp<<",\"status\":"<<b.status<<",\"bytes\":"<<b.bytes<<",\"owned_words\":"<<b.words.size()<<",\"gpu_admission\":false}";}
 s<<"],\"clip_planes\":[";
 for(size_t i=0;i<stage.clip_planes.size();++i){if(i)s<<',';const auto& clip=stage.clip_planes[i];s<<"{\"sharp_index\":"<<clip.sharp<<",\"status\":"<<clip.status<<",\"enabled_mask\":"<<clip.value.enabled_mask<<",\"bytes\":"<<clip.value.bytes<<'}';}
 s<<"],\"colors\":[";
 for(size_t i=0;i<stage.colors.size();++i){if(i)s<<',';const auto& c=stage.colors[i];const auto& r=c.requirement;const auto& b=c.initial;
  s<<"{\"slot\":"<<c.slot<<",\"requirement_status\":"<<c.status<<",\"pitch\":"<<r.pitch<<",\"height\":"<<r.height<<",\"layers\":"<<r.layers<<",\"samples\":"<<r.samples<<",\"tile_mode\":"<<r.tile_mode<<",\"data_format\":"<<r.data_format<<",\"number_format\":"<<r.number_format
   <<",\"address\":"<<r.guest_address<<",\"bytes\":"<<r.bytes<<",\"initial_copy_status\":"<<b.capture_status<<",\"lease_status\":"<<b.lease_status<<",\"release_status\":"<<b.release_status<<",\"lease_end_status\":"<<b.lease_end_status<<",\"copied_bytes\":"<<b.snapshot.size()<<",\"GPU_output_admission\":false}";}
 s<<"],\"source_writer_coherence\":false,\"gpu_admission\":false}";return s.str();
}
inline std::string graphics_json_text(const std::string& text){
 std::string output="\"";const char* hex="0123456789abcdef";
 for(unsigned char c:text){if(c=='"' || c=='\\'){output+='\\';output+=static_cast<char>(c);}
 else if(c<32){output+="\\u00";output+=hex[c>>4];output+=hex[c&15];}else output+=static_cast<char>(c);}
 output+='"';return output;
}
// Whole dispatch metadata/copy observation. No compiler or execution admission.
inline std::string compute_census_stage(uint32_t word,const CtrPm4::Registers& registers,
 ShaderSnapshotOwner& owner,DirectMemoryBacking& backing,const uint32_t* packet,size_t packet_words,GraphicsSubmissionOwner* submission=nullptr){
 CtrPm4::ComputeShaderState state;const int status=CtrPm4::CaptureComputeShaderState(registers,state);
 CtrPm4::OwnedShaderBinary binary;uint64_t bytes=0;int lease_status=88,capture_status=88,end_status=88;
 const auto ranges=owner.ranges();const auto inputs=owner.api();CtrShaderRangeLeaseV1 lease{};
 if(!status){bytes=backing.readable_mapped_extent(state.codeAddress,65564)&~uint64_t(3);
  if(bytes>=8){lease_status=ranges.begin(ranges.context,state.codeAddress,bytes,&lease);
   if(!lease_status){
    if(lease.guest_address==state.codeAddress && lease.bytes==bytes && lease.token){
     try{capture_status=CtrPm4::CaptureShaderBinaryInputs(&inputs,state.codeAddress,static_cast<size_t>(bytes),binary);}
     catch(...){capture_status=112;}
    }else capture_status=112;
    end_status=ranges.end(ranges.context,&lease);if(end_status)capture_status=end_status;
   }
  }
 }
 std::ostringstream s;s<<"{\"scope\":\"original_GnmSubmitFlip_compute_census\",\"word\":"<<word
  <<",\"state_status\":"<<status<<",\"code_address\":"<<state.codeAddress<<",\"capture_extent\":"<<bytes
  <<",\"lease_status\":"<<lease_status<<",\"capture_status\":"<<capture_status<<",\"lease_end_status\":"<<end_status
  <<",\"user_registers\":"<<state.userRegisters<<",\"shared_bytes\":"<<state.sharedBytes
  <<",\"workgroup\":["<<state.workgroup[0]<<','<<state.workgroup[1]<<','<<state.workgroup[2]
  <<"],\"partial\":["<<state.partial[0]<<','<<state.partial[1]<<','<<state.partial[2]<<"]";
 if(!capture_status)s<<",\"validated_code_bytes\":"<<binary.metadata.codeBytes<<",\"footer_offset\":"<<binary.metadata.footer;
 if(packet_words>=5)s<<",\"dispatch_groups\":["<<packet[1]<<','<<packet[2]<<','<<packet[3]<<"],\"dispatch_initiator\":"<<packet[4];
 if(!capture_status && submission){const int retained=submission->retain_compute(word,state,std::move(binary));s<<",\"dispatch_owner_status\":"<<retained<<",\"dispatch_register_identity\":"<<(!retained?"true":"false");}
 const auto counts=owner.lifetime_counts();s<<",\"range_begins\":"<<counts[0]<<",\"range_ends\":"<<counts[1]
  <<",\"active_leases\":"<<counts[2]<<",\"active_snapshots\":"<<counts[3]
  <<",\"source_writer_coherence\":false,\"compiler_admission\":false,\"gpu_execution\":false}\n";return s.str();
}
inline unsigned graphics_census_limit(){
 char value[16]{};const auto count=GetEnvironmentVariableA("CTR_GRAPHICS_CENSUS_STAGES",value,sizeof(value));
 if(!count || count>=sizeof(value))return 0;unsigned limit=0;
 for(DWORD i=0;i<count;++i){if(value[i]<'0' || value[i]>'9')return 0;limit=limit*10+unsigned(value[i]-'0');if(limit>128)return 0;}
 return limit;
}
inline std::string graphics_census_stage(uint32_t word,bool vertex,const CtrPm4::Registers& registers,
 const CtrGraphicsProfileV1& profile,ShaderSnapshotOwner& owner,DirectMemoryBacking& backing,GraphicsSubmissionOwner* submission=nullptr){
 CtrPm4::GraphicsShaderState state;const int status=CtrPm4::CaptureGraphicsShaderState(registers,vertex,state);
 GraphicsCompileObservation result;uint64_t bytes=0;
 if(status)result.diagnostic="No declared graphics shader program in observed registers";
 else {bytes=backing.readable_mapped_extent(state.codeAddress,65564)&~uint64_t(3);
  if(bytes>=8)result=compile_graphics_observed(owner.api(),state.codeAddress,bytes,registers,vertex,profile,owner.ranges());
  else result.diagnostic="No readable owned mapping for graphics shader header";}
 std::ostringstream s;s<<"{\"scope\":\"original_GnmSubmitFlip_graphics_census\",\"word\":"<<word<<",\"vertex\":"<<(vertex?"true":"false")
  <<",\"state_status\":"<<status<<",\"code_address\":"<<state.codeAddress<<",\"capture_extent\":"<<bytes<<",\"compiler_status\":"<<result.status
  <<",\"compiler_words\":"<<result.words<<",\"diagnostic\":"<<graphics_json_text(result.diagnostic);
 if(vertex)s<<",\"render_target_metadata\":"<<graphics_targets_json(registers);
 if(result.stage && vertex){s<<",\"color_output_permissions\":[";
  for(size_t i=0;i<result.stage->colors.size();++i){if(i)s<<',';const auto& color=result.stage->colors[i];
   const auto writable_bytes=color.status?0:backing.writable_mapped_extent(color.requirement.guest_address,color.requirement.bytes);
   s<<"{\"slot\":"<<color.slot<<",\"queried\":"<<(color.status?"false":"true")<<",\"writable_extent\":"<<writable_bytes
    <<",\"output_admission\":false}";}
  s<<']';}
 if(result.resources)s<<",\"resources\":"<<graphics_resources_json(*result.resources);
 if(result.stage){s<<",\"owned_stage\":"<<graphics_stage_json(*result.stage);if(submission)submission->retain(word,vertex,result.stage);}
 const auto counts=owner.lifetime_counts();s<<",\"range_begins\":"<<counts[0]<<",\"range_ends\":"<<counts[1]<<",\"active_leases\":"<<counts[2]<<",\"active_snapshots\":"<<counts[3]
  <<",\"source_writer_coherence\":false,\"gpu_execution\":false}\n";return s.str();
}
}

