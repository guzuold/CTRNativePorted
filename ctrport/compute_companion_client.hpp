// SPDX-License-Identifier: GPL-2.0-only
#pragma once
#include "graphics_key_client.hpp"
namespace ctrport {
struct ComputeStageOwner {std::shared_ptr<GraphicsStageOwner> stage;CtrGraphicsBindingContractV1 bindings{};std::array<uint32_t,80> registers{};std::string resource_metadata;CtrGraphicsPushDataV1 push{};int push_status=88;};
struct ComputeStageSink {uint64_t address=0;const uint32_t* expected=nullptr;unsigned calls=0;std::shared_ptr<ComputeStageOwner> output;};
// Original compute shader and full public shadPS4 translator; source revision
// e3ce810f3a653f43ac64ebab63023de281a4103a GPL-2.0-or-later.
// Synchronous deep copy. Transport validation never implies GPU dispatch support.
inline int compute_stage_sink(void* raw,const CtrCompiledComputeV1* view){
 if(!raw || !view)return 60;auto& sink=*static_cast<ComputeStageSink*>(raw);
 if(sink.calls || !sink.expected || view->version!=1 || view->size!=sizeof(*view) || !view->stage || !view->bindings || !view->compute_registers || view->compute_words!=80)return 112;
 if(!std::equal(view->compute_registers,view->compute_registers+80,sink.expected))return 112;
 const auto& b=*view->bindings;
 if(b.version!=1 || b.size!=sizeof(b) || b.descriptor_count>120 || b.userdata_count>16 || b.unified_before || b.buffer_before || b.userdata_before || b.unified_after>120 || b.buffer_after>120)return 112;
 for(uint32_t i=0;i<b.descriptor_count;++i){const auto& d=b.descriptors[i];if(d.kind>2 || d.storage>1 || !d.count || d.binding>=b.unified_after || d.count>b.unified_after-d.binding || (d.kind==0 && (d.count!=1 || d.buffer_slot>=b.buffer_after)) || (d.kind!=0 && d.buffer_slot!=UINT32_MAX))return 112;}
 GraphicsStageSinkContext temporary{sink.address,false,0,{}};const int copied=graphics_stage_sink_v2(&temporary,view->stage);if(copied)return copied;
 const auto& words=temporary.output->spirv;unsigned entries=0;
 for(size_t at=5;at<words.size();){const uint32_t count=words[at]>>16,opcode=words[at]&65535;if(!count || count>words.size()-at)return 112;
  if(opcode==15){if(count<4 || words[at+1]!=5 || ++entries!=1)return 112;}at+=count;
 }
 if(entries!=1)return 112;
 try{auto owner=std::make_shared<ComputeStageOwner>();owner->stage=std::move(temporary.output);owner->bindings=b;std::copy_n(view->compute_registers,80,owner->registers.begin());sink.output=std::move(owner);++sink.calls;return 0;}catch(...){return 112;}
}
inline int compute_stage_transport_control(){
 // Authored structural transport, not executable SPIR-V or a compute oracle.
 uint32_t spirv[]={0x07230203,0x10500,0,2,0,(4u<<16)|15,5,1,0};uint32_t flat[16]{};uint32_t bank[80]{};bank[7]=123;
 CtrGraphicsResourcesV1 resources{};resources.version=1;resources.size=sizeof(resources);resources.flattened_userdata_words=16;
 CtrCompiledGraphicsStageV2 stage{2,sizeof(stage),{1,sizeof(CtrCompiledGraphicsStageV1),0,9,4096,spirv,&resources,flat,0},flat,16};
 CtrGraphicsBindingContractV1 bindings{};bindings.version=1;bindings.size=sizeof(bindings);
 CtrCompiledComputeV1 view{1,sizeof(view),&stage,&bindings,bank,80};ComputeStageSink sink{4096,bank,0,{}};
 if(compute_stage_sink(&sink,&view) || sink.calls!=1 || !sink.output || sink.output->registers[7]!=123)return 1;
 bank[7]=456;flat[9]=789;spirv[7]=99;
 if(sink.output->registers[7]!=123 || sink.output->stage->flattened_userdata[9] || sink.output->stage->spirv[7]!=1)return 2;
 if(compute_stage_sink(&sink,&view)!=112)return 3;
 ComputeStageSink wrong{4096,bank,0,{}};spirv[6]=4;
 if(compute_stage_sink(&wrong,&view)!=112 || wrong.calls || wrong.output)return 4;
 spirv[6]=5;spirv[5]=15;
 if(compute_stage_sink(&wrong,&view)!=112 || wrong.calls || wrong.output)return 5;
 spirv[5]=(4u<<16)|15;view.compute_words=79;
 if(compute_stage_sink(&wrong,&view)!=112 || wrong.calls || wrong.output)return 6;
 view.compute_words=80;uint32_t other[80]{};wrong.expected=other;
 if(compute_stage_sink(&wrong,&view)!=112 || wrong.calls || wrong.output)return 7;
 if(compute_stage_sink(nullptr,&view)!=60)return 8;
 std::cout<<"{\"scope\":\"compute_stage_transport_control\",\"passed\":true,\"deepcopy_isolation\":true,\"wrong_bank_model_length_rejected\":true,\"synthetic_transport_only\":true,\"GPU_execution\":false}\n";return 0;
}
inline int compile_owned_compute(const CtrShaderInputsV1& inputs,const CtrShaderOwnedRangesV1& ranges,const GraphicsSubmissionOwner::Compute& compute,const CtrGraphicsProfileV1& profile,std::shared_ptr<ComputeStageOwner>& output,std::string& diagnostic){
 return graphics_metadata_host_call([&](){
  if(!compute.registers || compute.binary.words.empty() || compute.binary.guestAddress!=compute.state.codeAddress)return 60;
  wchar_t path[4096]{};const auto length=GetFullPathNameW(L"build/CTRPort-infra/graphics_companions/ctr_graphics_companion_v33.dll",4096,path,nullptr);if(!length || length>=4096)return 88;
  HMODULE module=LoadLibraryExW(path,nullptr,LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR|LOAD_LIBRARY_SEARCH_DEFAULT_DIRS);if(!module)return 88;
  struct Loaded{HMODULE module;~Loaded(){FreeLibrary(module);}}loaded{module};
  const auto call=reinterpret_cast<decltype(&ctr_compute_compile_v1)>(GetProcAddress(module,"ctr_compute_compile_v1"));if(!call)return 88;
  ComputeStageSink sink{compute.state.codeAddress,compute.registers->compute.data(),0,{}};char reason[2048]{};
  const int status=call(&inputs,&ranges,compute.state.codeAddress,compute.binary.words.size()*uint64_t(4),compute.registers->words.data(),static_cast<uint32_t>(compute.registers->words.size()),compute.registers->compute.data(),80,&profile,compute_stage_sink,&sink,reason,sizeof(reason));diagnostic=reason;
  if(status)return status;if(sink.calls!=1 || !sink.output)return 112;
  const auto descriptor_copy=reinterpret_cast<decltype(&ctr_graphics_descriptor_copy_v1)>(GetProcAddress(module,"ctr_graphics_descriptor_copy_v1"));
  const auto image_parameters=reinterpret_cast<decltype(&ctr_graphics_image_parameters_v1)>(GetProcAddress(module,"ctr_graphics_image_parameters_v1"));
  const auto image_extent=reinterpret_cast<decltype(&ctr_graphics_image_extent_v2)>(GetProcAddress(module,"ctr_graphics_image_extent_v2"));
  const auto requirement=reinterpret_cast<decltype(&ctr_graphics_buffer_requirement_v2)>(GetProcAddress(module,"ctr_graphics_buffer_requirement_v2"));
  if(!descriptor_copy || !image_parameters || !image_extent || !requirement)return 88;
  std::ostringstream metadata;metadata<<"[";auto& stage=*sink.output->stage;
  for(uint32_t i=0;i<stage.resources.binding_count;++i){const auto& b=stage.resources.bindings[i];if(i)metadata<<",";
   metadata<<"{\"kind\":"<<b.kind<<",\"sharp\":"<<b.sharp_index<<",\"flags\":"<<b.flags<<",\"buffer_type\":"<<b.buffer_type;
   if(b.kind==1){uint32_t descriptor[8]{},copied=0;CtrGraphicsImageParametersV1 image{};CtrGraphicsImageExtentV1 extent{};
    int decoded=descriptor_copy(&b,stage.flattened_userdata.data(),static_cast<uint32_t>(stage.flattened_userdata.size()),descriptor,8,&copied);
    if(!decoded)decoded=image_parameters(descriptor,copied,&image);
    metadata<<",\"decode_status\":"<<decoded;
    if(!decoded){const int sized=image_extent(&image,&extent);GraphicsStageOwner::Descriptor owned{b.kind,b.sharp_index};owned.status=0;owned.copied_words=copied;std::copy_n(descriptor,8,owned.words.begin());owned.image_status=0;owned.image=image;owned.image_extent_status=sized;owned.image_extent=extent;
     if(!sized){CtrGraphicsBufferRequirementV1 needed{1,sizeof(CtrGraphicsBufferRequirementV1),b.sharp_index,0,extent.guest_address,extent.bytes,0,0};owned.image_payload=capture_graphics_buffer(inputs,ranges,needed,32*1024*1024,32*1024*1024);}
     metadata<<",\"CPU_image_capture_status\":"<<owned.image_payload.capture_status<<",\"CPU_image_owned_bytes\":"<<owned.image_payload.snapshot.size()<<",\"GPU_writer_coherence\":false";
     stage.descriptors.push_back(std::move(owned));metadata<<",\"address\":"<<image.guest_address<<",\"width\":"<<image.width<<",\"height\":"<<image.height<<",\"pitch\":"<<image.pitch<<",\"samples\":"<<image.samples<<",\"format\":"<<image.data_format<<",\"number_format\":"<<image.number_format<<",\"tile\":"<<image.tile_mode<<",\"extent_status\":"<<sized<<",\"bytes\":"<<extent.bytes;}
   }else if(b.kind==0){CtrGraphicsBufferRequirementV1 needed{};const int decoded=requirement(&b,stage.flattened_userdata.data(),static_cast<uint32_t>(stage.flattened_userdata.size()),&needed);
    metadata<<",\"decode_status\":"<<decoded;if(!decoded){auto captured=capture_graphics_buffer(inputs,ranges,needed,16*1024*1024);metadata<<",\"address\":"<<needed.guest_address<<",\"bytes\":"<<needed.bytes<<",\"written\":"<<needed.is_written<<",\"capture_status\":"<<captured.capture_status<<",\"lease_status\":"<<captured.lease_status<<",\"release_status\":"<<captured.release_status<<",\"lease_end_status\":"<<captured.lease_end_status<<",\"owned_bytes\":"<<captured.snapshot.size();stage.buffers.push_back(std::move(captured));}
   }metadata<<"}";
  }metadata<<"]";sink.output->resource_metadata=metadata.str();
  // Same public BindResources MakeUserData/PushUd path for the sole compute stage.
  // Zero offsets apply only to future dedicated owner buffers beginning at byte zero.
  const auto push_data=reinterpret_cast<decltype(&ctr_graphics_push_data_v1)>(GetProcAddress(module,"ctr_graphics_push_data_v1"));
  if(!push_data)return 88;CtrGraphicsBindingContractV1 empty{};empty.version=1;empty.size=sizeof(empty);uint32_t offsets[120]{};
  sink.output->push_status=push_data(compute.registers->words.data(),static_cast<uint32_t>(compute.registers->words.size()),&empty,&sink.output->bindings,offsets,sink.output->bindings.buffer_after,&sink.output->push);
output=std::move(sink.output);return 0;
 });
}
}
