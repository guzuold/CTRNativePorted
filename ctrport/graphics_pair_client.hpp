// SPDX-License-Identifier: GPL-2.0-only
#pragma once
#include "graphics_companion_client.hpp"
#include "graphics_key_client.hpp"
#include "draw_packet_inputs.hpp"
#include "pair_host_timing.hpp"
namespace ctrport {
struct GraphicsPairOwner {
 uint32_t word=0,command_buffer_index=0,command_buffer_kind=0;
 uint64_t submission_token=0;
 std::shared_ptr<const CtrPm4::Registers> registers;
 CtrGraphicsProfileV1 compiler_profile{};
 CtrGraphicsKeyV1 graphics_key{};int graphics_key_status=88;GraphicsVertexFetchOwner vertex_fetch;DrawPacketInputs draw_inputs;int draw_inputs_status=88;
 CtrGraphicsPipelineStageV1 fragment_pipeline{},vertex_pipeline{};
 std::vector<uint32_t> original_packet;
 uint32_t original_opcode=0;
 std::shared_ptr<GraphicsStageOwner> fragment,vertex;
 std::shared_ptr<const GraphicsStageOwner> fragment_inputs,vertex_inputs;
 CtrGraphicsBindingContractV1 fragment_bindings{},vertex_bindings{};
};
// Immutable full public register bank belonging to this exact paired compilation.
inline int graphics_pair_retain_registers(GraphicsPairOwner& pair,const CtrPm4::Registers& source,
 uint64_t submission,uint32_t buffer_index,uint32_t kind,uint32_t word){
 if(!submission || kind>1 || pair.registers || !pair.fragment || !pair.vertex)return 60;
 try{auto snapshot=std::make_shared<const CtrPm4::Registers>(source);
  if(snapshot->words.size()!=0xD000 || std::memcmp(snapshot->words.data(),source.words.data(),0xD000*4))return 112;
  pair.registers=std::move(snapshot);pair.submission_token=submission;pair.command_buffer_index=buffer_index;
  pair.command_buffer_kind=kind;pair.word=word;return 0;
 }catch(...){return 112;}
}
inline int graphics_pair_attach_command(GraphicsPairOwner& pair,const GraphicsSubmissionOwner::Route& route,
 const CtrGraphicsProfileV1& profile){
 if(!pair.registers || !route.registers || pair.word!=route.word || !pair.original_packet.empty() ||
 profile.version!=1 || profile.size!=sizeof(profile))return 60;
 if(!((route.opcode==0x2d && route.packet.size()==3) || (route.opcode==0x27 && route.packet.size()==6)))return 60;
 if(std::memcmp(pair.registers->words.data(),route.registers->words.data(),0xD000*4))return 112;
 try{auto packet=route.packet;pair.original_packet=std::move(packet);pair.original_opcode=route.opcode;
 pair.compiler_profile=profile;return 0;}catch(...){return 112;}
}
inline int graphics_pair_register_control(){
 auto source=std::make_unique<CtrPm4::Registers>();
 for(size_t i=0;i<source->words.size();++i)source->words[i]=static_cast<uint32_t>(i*17+3);
 GraphicsPairOwner pair;pair.fragment=std::make_shared<GraphicsStageOwner>();pair.vertex=std::make_shared<GraphicsStageOwner>();
 if(graphics_pair_retain_registers(pair,*source,7,2,0,255))return 1;
 source->words.fill(0);
 for(size_t i=0;i<pair.registers->words.size();++i)if(pair.registers->words[i]!=i*17+3)return 2;
 if(pair.submission_token!=7 || pair.command_buffer_index!=2 || pair.word!=255)return 3;
 if(graphics_pair_retain_registers(pair,*source,8,3,1,256)!=60 || pair.submission_token!=7)return 4;
 GraphicsPairOwner invalid;invalid.fragment=pair.fragment;invalid.vertex=pair.vertex;
 if(graphics_pair_retain_registers(invalid,*source,0,0,0,0)!=60 || invalid.registers)return 5;
 if(graphics_pair_retain_registers(invalid,*source,1,0,2,0)!=60 || invalid.registers)return 6;
 GraphicsSubmissionOwner::Route route{255,0x2d,{0xc0012d00,3,0},pair.registers};
 CtrGraphicsProfileV1 profile{};profile.version=1;profile.size=sizeof(profile);profile.max_ubo_size=12345;
 if(graphics_pair_attach_command(pair,route,profile))return 7;
 route.packet[1]=99;profile.max_ubo_size=0;
 if(pair.original_packet[1]!=3 || pair.compiler_profile.max_ubo_size!=12345)return 8;
 if(graphics_pair_attach_command(pair,route,profile)!=60 || pair.original_packet[1]!=3)return 9;
 std::cout<<"{\"scope\":\"authored_pair_register_ownership\",\"words\":53248,\"mutation_isolated\":true,\"negative_cases\":3}\n";return 0;
}
// Join CPU snapshots only after exact compiler-input identity comparison.
// This creates no GPU resource binding or source-writer coherence.
inline int graphics_pair_attach_inputs(GraphicsPairOwner& pair,const std::shared_ptr<const GraphicsStageOwner>& fragment,const std::shared_ptr<const GraphicsStageOwner>& vertex,uint32_t capture_kinds=15){
 if(!pair.fragment || !pair.vertex || !fragment || !vertex)return 60;
 static_assert(sizeof(CtrGraphicsResourcesV1)==(15+128*5)*sizeof(uint32_t));
 const auto matches=[](const GraphicsStageOwner& compiled,const GraphicsStageOwner& input){
  if(compiled.userdata_words!=input.userdata_words || compiled.userdata!=input.userdata || compiled.flattened_userdata!=input.flattened_userdata)return false;
  if(!graphics_resources_valid(compiled.resources) || !graphics_resources_valid(input.resources))return false;
  if(std::memcmp(&compiled.resources,&input.resources,15*sizeof(uint32_t)))return false;
  return std::memcmp(compiled.resources.bindings,input.resources.bindings,compiled.resources.binding_count*sizeof(CtrGraphicsResourceIdentityV1))==0;
 };
 if(!matches(*pair.fragment,*fragment) || !matches(*pair.vertex,*vertex))return 112;
 const int first=graphics_input_capture_status(*fragment,capture_kinds);if(first)return first;
 const int second=graphics_input_capture_status(*vertex,capture_kinds);if(second)return second;
 pair.fragment_inputs=fragment;pair.vertex_inputs=vertex;return 0;
}
// Resolve compiler-exported MRT slots to owned same-draw color captures.
// This prepares CPU inputs only: format support, attachment creation and LOAD
// coherence still belong to the selected video session.
inline int graphics_pair_color_inputs(const GraphicsPairOwner& pair,
 std::vector<const GraphicsStageOwner::Color*>& output){
 if(!pair.registers || !pair.fragment || !pair.vertex_inputs)return 88;
 const uint32_t mask=pair.fragment->resources.mrt_mask;if(mask&~255u)return 88;
 std::vector<const GraphicsStageOwner::Color*> prepared;
 for(uint32_t slot=0;slot<8;++slot)if(mask&(1u<<slot)){
  // Public vk_pipeline_cache.cpp434 skips invalid color targets. A shader
  // export alone does not create an attachment. Require the decoded exact key;
  // any active/unknown target still needs its complete owned snapshot below.
  if(!pair.graphics_key_status && pair.graphics_key.version==1 &&
     pair.graphics_key.size==sizeof(pair.graphics_key) && pair.graphics_key.mrt_mask==mask &&
     slot<pair.graphics_key.num_color_attachments && !pair.graphics_key.colors[slot].data_format &&
     !pair.graphics_key.colors[slot].write_mask && !pair.graphics_key.colors[slot].samples)continue;
  const GraphicsStageOwner::Color* found=nullptr;
  for(const auto& color:pair.vertex_inputs->colors)if(color.slot==slot){if(found)return 112;found=&color;}
  if(!found || found->status)return 88;
  const auto& r=found->requirement;const auto& b=found->initial;
  if(r.version!=1 || r.size!=sizeof(r) || r.slot!=slot || !r.bytes || !r.guest_address ||
   b.requirement_status || b.lease_status || b.capture_status || b.release_status || b.lease_end_status ||
   b.snapshot.size()!=r.bytes || b.bytes!=r.bytes || b.address!=r.guest_address || b.sharp!=slot)return 88;
  prepared.push_back(found);
 }
 output=std::move(prepared);return 0;
}
inline int graphics_pair_color_control(){
 GraphicsPairOwner pair;pair.registers=std::make_shared<CtrPm4::Registers>();pair.fragment=std::make_shared<GraphicsStageOwner>();
 auto inputs=std::make_shared<GraphicsStageOwner>();pair.vertex_inputs=inputs;
 GraphicsStageOwner::Color color{};color.slot=2;color.status=0;
 color.requirement={1,sizeof(CtrGraphicsColorRequirementV1),2,1,1,1,1,14,10,0,0x1000,4};
 color.initial.bytes=4;color.initial.address=0x1000;color.initial.sharp=2;
 color.initial.requirement_status=color.initial.lease_status=color.initial.capture_status=color.initial.release_status=color.initial.lease_end_status=0;
 color.initial.snapshot.resize(4);inputs->colors.push_back(color);
 pair.fragment->resources.mrt_mask=4;std::vector<const GraphicsStageOwner::Color*> out;
 if(graphics_pair_color_inputs(pair,out) || out.size()!=1 || out[0]!=&inputs->colors[0])return 1;
 pair.fragment->resources.mrt_mask=5;
 if(graphics_pair_color_inputs(pair,out)!=88 || out.size()!=1)return 2;
 pair.fragment->resources.mrt_mask=4;inputs->colors[0].initial.snapshot.clear();
 if(graphics_pair_color_inputs(pair,out)!=88 || out.size()!=1)return 3;
 inputs->colors[0]=color;inputs->colors.push_back(color);
 if(graphics_pair_color_inputs(pair,out)!=112 || out.size()!=1)return 4;
 pair.fragment->resources.mrt_mask=0;
 if(graphics_pair_color_inputs(pair,out) || !out.empty())return 5;
 // Authored CPU fixture from the observed12300 slot policy, not pixel proof.
 color.slot=0;color.requirement.slot=0;color.initial.sharp=0;
 inputs->colors={color};pair.fragment->resources.mrt_mask=3;
 pair.graphics_key={};pair.graphics_key.version=1;pair.graphics_key.size=sizeof(pair.graphics_key);
 pair.graphics_key.mrt_mask=3;pair.graphics_key.num_color_attachments=2;
 pair.graphics_key.colors[0].data_format=10;pair.graphics_key.colors[0].write_mask=15;pair.graphics_key.colors[0].samples=1;
 pair.graphics_key_status=0;
 if(graphics_pair_color_inputs(pair,out) || out.size()!=1 || out[0]!=&inputs->colors[0])return 6;
 const auto preserved=out;
 pair.graphics_key_status=88;if(graphics_pair_color_inputs(pair,out)!=88 || out!=preserved)return 7;pair.graphics_key_status=0;
 pair.graphics_key.colors[1].data_format=10;if(graphics_pair_color_inputs(pair,out)!=88 || out!=preserved)return 8;pair.graphics_key.colors[1].data_format=0;
 pair.graphics_key.colors[1].write_mask=1;if(graphics_pair_color_inputs(pair,out)!=88 || out!=preserved)return 9;pair.graphics_key.colors[1].write_mask=0;
 pair.graphics_key.colors[1].samples=1;if(graphics_pair_color_inputs(pair,out)!=88 || out!=preserved)return 10;pair.graphics_key.colors[1].samples=0;
 pair.graphics_key.mrt_mask=1;if(graphics_pair_color_inputs(pair,out)!=88 || out!=preserved)return 11;pair.graphics_key.mrt_mask=3;
 pair.graphics_key.size=0;if(graphics_pair_color_inputs(pair,out)!=88 || out!=preserved)return 12;
 std::cout<<"{\"scope\":\"authored_pair_color_join\",\"negative_cases\":9,\"zero_mrt_supported\":true,\"inactive_slot_skipped\":true,\"atomic_failure\":true,\"GPU_execution\":false}\n";return 0;
}
struct GraphicsPairSink {
 uint64_t ps,vs;unsigned calls=0;std::shared_ptr<GraphicsPairOwner> output;
};
inline int graphics_pair_sink(void* raw,const CtrCompiledGraphicsPairV1* pair){
 if(!raw || !pair)return 60;auto& target=*static_cast<GraphicsPairSink*>(raw);
 if(target.calls++ || pair->version!=1 || pair->size!=sizeof(*pair) || !pair->fragment || !pair->vertex || !pair->fragment_bindings || !pair->vertex_bindings)return 112;
 const auto& ps=*pair->fragment_bindings;const auto& vs=*pair->vertex_bindings;
 for(const auto* binding:{&ps,&vs})if(binding->version!=1 || binding->size!=sizeof(*binding) || binding->descriptor_count>120 || binding->userdata_count>16 ||
 binding->unified_after<binding->unified_before || binding->buffer_after<binding->buffer_before || binding->userdata_after<binding->userdata_before)return 112;
 for(const auto* binding:{&ps,&vs}){
  if(binding->userdata_after-binding->userdata_before!=binding->userdata_count)return 112;
  for(uint32_t index=0;index<binding->descriptor_count;++index){const auto& d=binding->descriptors[index];
   if(d.kind>2 || d.storage>1 || !d.count || d.binding<binding->unified_before || d.binding>=binding->unified_after || d.count>binding->unified_after-d.binding)return 112;
   if(d.kind==0){if(d.count!=1 || d.buffer_slot<binding->buffer_before || d.buffer_slot>=binding->buffer_after)return 112;}
   else if(d.buffer_slot!=UINT32_MAX)return 112;
  }
 }
 if(vs.unified_before!=ps.unified_after || vs.buffer_before!=ps.buffer_after || vs.userdata_before!=ps.userdata_after)return 112;
 GraphicsStageSinkContext fragment{target.ps,false,0,{}},vertex{target.vs,true,0,{}};
 const int first=graphics_stage_sink_v2(&fragment,pair->fragment);if(first)return first;
 const int second=graphics_stage_sink_v2(&vertex,pair->vertex);if(second)return second;
 try{auto prepared=std::make_shared<GraphicsPairOwner>();prepared->fragment=std::move(fragment.output);prepared->vertex=std::move(vertex.output);
 prepared->fragment_bindings=ps;prepared->vertex_bindings=vs;target.output=std::move(prepared);return 0;}catch(...){return 112;}
}
inline int graphics_pair_sink_v2(void* raw,const CtrCompiledGraphicsPairV2* pair){
 if(!raw || !pair || pair->version!=2 || pair->size!=sizeof(*pair) || !pair->pair ||
 !pair->fragment_pipeline || !pair->vertex_pipeline)return 60;
 for(const auto* m:{pair->fragment_pipeline,pair->vertex_pipeline}){
  if(m->version!=1 || m->size!=sizeof(*m) || m->attribute_count>32 || m->has_fetch>1 ||
   (m->has_fetch && (m->vertex_offset_sgpr < -1 || m->vertex_offset_sgpr>=16 || m->instance_offset_sgpr < -1 || m->instance_offset_sgpr>=16)) || m->persp_sample>1 || m->linear_sample>1 || (!m->has_fetch && m->attribute_count))return 112;
 }
 GraphicsPairSink temporary{static_cast<GraphicsPairSink*>(raw)->ps,static_cast<GraphicsPairSink*>(raw)->vs,0,{}};
 const int status=graphics_pair_sink(&temporary,pair->pair);if(status)return status;
 if(temporary.output->fragment->resources.mrt_mask!=pair->fragment_pipeline->mrt_mask ||
 temporary.output->vertex->resources.has_fetch_shader!=pair->vertex_pipeline->has_fetch)return 112;
 auto& target=*static_cast<GraphicsPairSink*>(raw);if(target.calls++)return 112;
 temporary.output->fragment_pipeline=*pair->fragment_pipeline;temporary.output->vertex_pipeline=*pair->vertex_pipeline;
 target.output=std::move(temporary.output);return 0;
}
inline int compile_graphics_pair_on_host(const CtrShaderInputsV1& inputs,const CtrShaderOwnedRangesV1& ranges,
 const CtrPm4::Registers& registers,const CtrGraphicsProfileV1& profile,
 uint64_t ps,uint64_t vs,std::shared_ptr<GraphicsPairOwner>& output,std::string& diagnostic){
 PairHostTiming timing;
 wchar_t path[4096]{};const auto length=GetFullPathNameW(L"build/CTRPort-infra/graphics_companions/ctr_graphics_companion_v33.dll",4096,path,nullptr);
 if(!length || length>=4096)return 88;
 std::cerr<<"paired compiler loading ps="<<ps<<" vs="<<vs<<std::endl;
 timing.Next("load_library");
 HMODULE module=LoadLibraryExW(path,nullptr,LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR|LOAD_LIBRARY_SEARCH_DEFAULT_DIRS);if(!module)return 88;
 timing.Next("resolve_export_ranges");
 struct Loaded{HMODULE value;PairHostTiming& timing;~Loaded(){timing.Next("unload_library");FreeLibrary(value);timing.Next("done");}} loaded{module,timing};
 const auto call=reinterpret_cast<decltype(&ctr_graphics_compile_pair_v2)>(GetProcAddress(module,"ctr_graphics_compile_pair_v2"));if(!call)return 88;
 uint64_t extents[2]{};unsigned i=0;
 for(const auto address:{ps,vs}){CtrShaderRangeLeaseV1 lease{};const int status=ranges.begin(ranges.context,address,65564,&lease);if(status)return status;
 extents[i++]=lease.bytes&~uint64_t(3);const int ended=ranges.end(ranges.context,&lease);if(ended)return ended;}
 GraphicsPairSink sink{ps,vs,0,{}};char reason[2048]{};
 std::cerr<<"paired compiler invoking ps_bytes="<<extents[0]<<" vs_bytes="<<extents[1]<<std::endl;
 timing.Next("compiler_and_sink");
 const int status=call(&inputs,&ranges,ps,extents[0],vs,extents[1],registers.words.data(),static_cast<uint32_t>(registers.words.size()),&profile,graphics_pair_sink_v2,&sink,reason,sizeof(reason));
 timing.Next("publish_output");
 std::cerr<<"paired compiler completed status="<<status<<std::endl;
 diagnostic=reason;if(status)return status;if(sink.calls!=1 || !sink.output)return 112;output=std::move(sink.output);return 0;
}
// Whole compiler frames must run on a Windows host stack, never the guest stack.
inline int compile_graphics_pair(const CtrShaderInputsV1& inputs,const CtrShaderOwnedRangesV1& ranges,
 const CtrPm4::Registers& registers,const CtrGraphicsProfileV1& profile,uint64_t ps,uint64_t vs,
 std::shared_ptr<GraphicsPairOwner>& output,std::string& diagnostic){
 struct Work{const CtrShaderInputsV1& inputs;const CtrShaderOwnedRangesV1& ranges;const CtrPm4::Registers& registers;
 const CtrGraphicsProfileV1& profile;uint64_t ps,vs;std::shared_ptr<GraphicsPairOwner> output;std::string reason;int status=112;};
 Work work{inputs,ranges,registers,profile,ps,vs,{}, {},112};
 auto entry=[](void* raw)->DWORD{auto& w=*static_cast<Work*>(raw);try{w.status=compile_graphics_pair_on_host(w.inputs,w.ranges,w.registers,w.profile,w.ps,w.vs,w.output,w.reason);}
 catch(...){w.status=112;w.reason="Paired compiler host exception";}return 0;};
 HANDLE thread=CreateThread(nullptr,8*1024*1024,entry,&work,0,nullptr);if(!thread){diagnostic="Paired compiler host thread creation failed";return 112;}
 const auto waited=WaitForSingleObject(thread,INFINITE);CloseHandle(thread);if(waited!=WAIT_OBJECT_0)std::terminate();
 diagnostic=std::move(work.reason);if(!work.status)output=std::move(work.output);return work.status;
}
inline std::string graphics_pair_metadata(uint32_t word,int status,const std::shared_ptr<GraphicsPairOwner>& pair,const std::string& reason){
 std::ostringstream out;out<<"{\"scope\":\"original_GnmSubmitFlip_graphics_pair\",\"word\":"<<word<<",\"status\":"<<status<<",\"diagnostic\":"<<graphics_json_text(reason);
 if(pair){const auto& p=pair->fragment_bindings;const auto& v=pair->vertex_bindings;
 out<<",\"fragment_spirv_words\":"<<pair->fragment->spirv.size()<<",\"vertex_spirv_words\":"<<pair->vertex->spirv.size()
 <<",\"fragment_flattened_words\":"<<pair->fragment->flattened_userdata.size()<<",\"vertex_flattened_words\":"<<pair->vertex->flattened_userdata.size()
 <<",\"fragment_descriptors\":"<<p.descriptor_count<<",\"vertex_descriptors\":"<<v.descriptor_count
 <<",\"fragment_unified_begin\":"<<p.unified_before<<",\"fragment_unified_end\":"<<p.unified_after
 <<",\"vertex_unified_begin\":"<<v.unified_before<<",\"vertex_unified_end\":"<<v.unified_after
 <<",\"pipeline_metadata_owned\":"<<(pair->fragment_pipeline.version==1 && pair->vertex_pipeline.version==1?"true":"false")
 <<",\"fragment_mrt_mask\":"<<pair->fragment_pipeline.mrt_mask<<",\"fragment_persp_sample\":"<<pair->fragment_pipeline.persp_sample<<",\"fragment_linear_sample\":"<<pair->fragment_pipeline.linear_sample
 <<",\"vertex_has_fetch\":"<<pair->vertex_pipeline.has_fetch<<",\"vertex_fetch_attributes\":"<<pair->vertex_pipeline.attribute_count
 <<",\"original_packet_words\":"<<pair->original_packet.size()<<",\"original_opcode\":"<<pair->original_opcode<<",\"compiler_profile_owned\":"<<(pair->compiler_profile.version==1?"true":"false")
 <<",\"register_words\":"<<(pair->registers?pair->registers->words.size():0)<<",\"submission_token\":"<<pair->submission_token<<",\"command_buffer_index\":"<<pair->command_buffer_index<<",\"command_buffer_kind\":"<<pair->command_buffer_kind
 <<",\"buffer_slots\":"<<v.buffer_after<<",\"userdata_slots\":"<<v.userdata_after;}
 out<<",\"descriptor_binding\":false,\"gpu_execution\":false}\n";return out.str();
}
inline int graphics_pair_sink_control(){
 uint32_t spirv[5]{0x07230203,0x10000,0,1,0},userdata[4]{11,22,33,44},flat[17]{11,22,33,44};flat[16]=99;
 CtrGraphicsResourcesV1 resources{};resources.version=1;resources.size=sizeof(resources);resources.flattened_userdata_words=17;
 CtrCompiledGraphicsStageV1 ps{1,sizeof(ps),0,5,0x1000,spirv,&resources,userdata,4},vs=ps;vs.vertex=1;vs.code_address=0x2000;
 CtrCompiledGraphicsStageV2 fragment{2,sizeof(fragment),ps,flat,17},vertex{2,sizeof(vertex),vs,flat,17};
 CtrGraphicsBindingContractV1 pb{};pb.version=1;pb.size=sizeof(pb);auto vb=pb;
 CtrCompiledGraphicsPairV1 pair{1,sizeof(pair),&fragment,&vertex,&pb,&vb};GraphicsPairSink accepted{0x1000,0x2000,0,{}};
 if(graphics_pair_sink(&accepted,&pair) || !accepted.output)return 1;
 flat[16]=0;spirv[2]=99;
 if(accepted.output->fragment->flattened_userdata[16]!=99 || accepted.output->vertex->spirv[2]!=0)return 2;
 unsigned rejected=0;
 for(unsigned i=0;i<8;++i){auto bad=pair;auto bad_vertex=vertex;auto bad_binding=vb;GraphicsPairSink fresh{0x1000,0x2000,0,{}};
  switch(i){case 0:bad.vertex=nullptr;break;case 1:bad_vertex.stage.vertex=0;bad.vertex=&bad_vertex;break;
   case 2:bad_binding.unified_before=1;bad.vertex_bindings=&bad_binding;break;case 3:bad_binding.descriptor_count=121;bad.vertex_bindings=&bad_binding;break;case 4:bad.version=0;break;case 5:bad_binding.userdata_after=1;bad.vertex_bindings=&bad_binding;break;case 6:bad_binding.unified_after=1;bad_binding.descriptor_count=1;bad_binding.descriptors[0]={0,0,0,1,0,0};bad.vertex_bindings=&bad_binding;break;case 7:bad_binding.unified_after=1;bad_binding.descriptor_count=1;bad_binding.descriptors[0]={1,0,0,2,UINT32_MAX,0};bad.vertex_bindings=&bad_binding;break;}
  if(graphics_pair_sink(&fresh,&bad)==112 && !fresh.output)++rejected;
 }
 if(graphics_pair_sink(&accepted,&pair)==112 && accepted.output->fragment->flattened_userdata[16]==99)++rejected;
 if(rejected!=9)return 3;
 CtrGraphicsPipelineStageV1 pm{},vm{};pm.version=vm.version=1;pm.size=vm.size=sizeof(pm);
 resources.has_fetch_shader=1;vm.has_fetch=1;vm.attribute_count=1;vm.attributes[0].semantic=7;vm.vertex_offset_sgpr=-1;
 CtrCompiledGraphicsPairV2 pipeline{2,sizeof(pipeline),&pair,&pm,&vm};GraphicsPairSink joined{0x1000,0x2000,0,{}};
 if(graphics_pair_sink_v2(&joined,&pipeline) || !joined.output)return 4;
 vm.attributes[0].semantic=99;if(joined.output->vertex_pipeline.attributes[0].semantic!=7 || joined.output->vertex_pipeline.vertex_offset_sgpr!=-1)return 5;
 for(unsigned i=0;i<6;++i){auto bad=pipeline;auto metadata=vm;GraphicsPairSink fresh{0x1000,0x2000,0,{}};
  if(i==0)metadata.attribute_count=33;if(i==1)metadata.has_fetch=2;if(i==2)metadata.persp_sample=2;
  if(i==3){metadata.has_fetch=0;metadata.attribute_count=0;}
  if(i==4)metadata.vertex_offset_sgpr=-2;if(i==5)metadata.instance_offset_sgpr=16;
  bad.vertex_pipeline=&metadata;
  if(graphics_pair_sink_v2(&fresh,&bad)!=112 || fresh.output || fresh.calls)return 6;
 }
 if(graphics_pair_sink_v2(&joined,&pipeline)!=112 || joined.output->vertex_pipeline.attributes[0].semantic!=7)return 7;

 std::cout<<"{\"scope\":\"graphics_pair_sink_control\",\"passed\":true,\"negative_rejections\":9,\"atomic_delivery\":true,\"deep_copy\":true,\"synthetic\":true,\"gpu_execution\":false}\n";return 0;
}

inline int graphics_pair_input_control(){
 auto stage=std::make_shared<GraphicsStageOwner>();stage->resources.version=1;stage->resources.size=sizeof(stage->resources);stage->resources.flattened_userdata_words=16;stage->flattened_userdata.resize(16);
 GraphicsPairOwner pair;pair.fragment=stage;pair.vertex=stage;auto input=std::make_shared<GraphicsStageOwner>(*stage);
 if(graphics_pair_attach_inputs(pair,input,input) || pair.fragment_inputs!=input || pair.vertex_inputs!=input)return 1;
 unsigned rejected=0;
 for(unsigned i=0;i<4;++i){auto bad=std::make_shared<GraphicsStageOwner>(*input);switch(i){case 0:bad->userdata[0]=9;break;case 1:bad->flattened_userdata[0]=8;break;case 2:bad->resources.mrt_mask=3;break;case 3:bad->resources.binding_count=129;break;}
  if(graphics_pair_attach_inputs(pair,input,bad)==112 && pair.vertex_inputs==input)++rejected;
 }
 if(graphics_pair_attach_inputs(pair,input,{})==60 && pair.vertex_inputs==input)++rejected;
 auto fmask=std::make_shared<GraphicsStageOwner>(*stage);fmask->resources.fmasks=1;fmask->resources.binding_count=1;fmask->resources.bindings[0].kind=3;
 pair.vertex=fmask;if(graphics_pair_attach_inputs(pair,input,fmask)==88 && pair.vertex_inputs==input)++rejected;
 if(rejected!=6)return 2;
 std::cout<<"{\"scope\":\"graphics_pair_input_control\",\"passed\":true,\"negative_rejections\":6,\"atomic_attachment\":true,\"synthetic\":true,\"gpu_execution\":false}\n";return 0;
}

}
