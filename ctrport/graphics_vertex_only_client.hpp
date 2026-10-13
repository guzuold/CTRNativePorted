// SPDX-License-Identifier: GPL-2.0-only
#pragma once
#include "graphics_pair_client.hpp"
#include "graphics_key_client.hpp"
#include "draw_packet_inputs.hpp"
namespace ctrport {
// A genuinely absent fragment stage is represented by absence, never a fake PS.
struct GraphicsVertexOnlyOwner {
 uint64_t submission=0;uint32_t buffer=0,kind=0,word=0,opcode=0;
 std::vector<uint32_t> packet;std::shared_ptr<const CtrPm4::Registers> registers;
 std::shared_ptr<GraphicsStageOwner> vertex;
 std::shared_ptr<const GraphicsStageOwner> inputs;int input_status=88;
 CtrGraphicsBindingContractV1 bindings{};CtrGraphicsPipelineStageV1 pipeline{};
 CtrGraphicsProfileV1 profile{};
 CtrGraphicsKeyV1 graphics_key{};int graphics_key_status=88;GraphicsVertexFetchOwner vertex_fetch;DrawPacketInputs draw_inputs;int draw_inputs_status=88;
};
inline int graphics_vertex_only_attach_inputs(GraphicsVertexOnlyOwner& owner,const std::shared_ptr<const GraphicsStageOwner>& inputs){
 if(!owner.vertex || !inputs || owner.inputs)return 60;const auto& a=*owner.vertex;const auto& b=*inputs;
 if(a.userdata_words!=b.userdata_words || a.userdata!=b.userdata || a.flattened_userdata!=b.flattened_userdata ||
 !graphics_resources_valid(a.resources) || !graphics_resources_valid(b.resources) ||
 std::memcmp(&a.resources,&b.resources,15*sizeof(uint32_t)) ||
 std::memcmp(a.resources.bindings,b.resources.bindings,a.resources.binding_count*sizeof(CtrGraphicsResourceIdentityV1)))return 88;
 owner.inputs=inputs;owner.input_status=graphics_input_capture_status(b);return 0;
}
// Current genuine fragmentless retail shader has one disabled ClipPlanes binding.
// Other resources remain explicitly unsupported by this narrow transport.
inline int graphics_vertex_only_null_writes(const GraphicsVertexOnlyOwner& owner,std::vector<CtrGraphicsBufferWriteV1>& output){
 if(!owner.inputs || owner.input_status)return 88;const auto& input=*owner.inputs;const auto& b=owner.bindings;
 if(b.descriptor_count!=1 || input.resources.buffers!=1 || input.resources.binding_count!=1)return 88;
 const auto& d=b.descriptors[0];const auto& r=input.resources.bindings[0];
 if(d.kind!=0 || d.count!=1 || !d.storage || d.buffer_slot!=0 || d.sharp_index!=r.sharp_index ||
 r.kind!=0 || r.buffer_type!=6 || !(r.flags&2))return 88;
 const auto found=std::find_if(input.clip_planes.begin(),input.clip_planes.end(),[&](const auto& c){return c.sharp==d.sharp_index;});
 if(found==input.clip_planes.end() || found->status || found->value.enabled_mask || found->value.bytes)return 88;
 std::vector<CtrGraphicsBufferWriteV1> prepared{{d.binding,d.buffer_slot,1,1,0}};output=std::move(prepared);return 0;
}
struct GraphicsVertexOnlySink{uint64_t address;unsigned calls=0;std::shared_ptr<GraphicsVertexOnlyOwner> output;};
inline int graphics_vertex_only_sink(void* raw,const CtrCompiledGraphicsVertexOnlyV1* view){
 if(!raw || !view)return 60;auto& target=*static_cast<GraphicsVertexOnlySink*>(raw);
 if(target.calls++ || view->version!=1 || view->size!=sizeof(*view) || !view->vertex || !view->vertex_bindings || !view->vertex_pipeline)return 112;
 const auto& b=*view->vertex_bindings;const auto& p=*view->vertex_pipeline;

 if(b.version!=1 || b.size!=sizeof(b) || b.unified_before || b.buffer_before || b.userdata_before || b.userdata_count>16 ||
 b.descriptor_count>120 || b.userdata_after!=b.userdata_count || p.version!=1 || p.size!=sizeof(p) || p.attribute_count>32 || p.has_fetch>1 || (p.has_fetch && (p.vertex_offset_sgpr < -1 || p.vertex_offset_sgpr>=16 || p.instance_offset_sgpr < -1 || p.instance_offset_sgpr>=16)) || (!p.has_fetch && p.attribute_count))return 112;
 for(uint32_t i=0;i<b.descriptor_count;++i){const auto& d=b.descriptors[i];
  if(d.kind>2 || d.storage>1 || !d.count || d.binding>=b.unified_after || d.count>b.unified_after-d.binding)return 112;
  if(d.kind==0){if(d.count!=1 || d.buffer_slot>=b.buffer_after)return 112;}else if(d.buffer_slot!=UINT32_MAX)return 112;
 }
 GraphicsStageSinkContext vertex{target.address,true,0,{}};const int status=graphics_stage_sink_v2(&vertex,view->vertex);if(status)return status;
 if(vertex.output->resources.has_fetch_shader!=p.has_fetch)return 112;
 try{auto owned=std::make_shared<GraphicsVertexOnlyOwner>();owned->vertex=std::move(vertex.output);owned->bindings=b;owned->pipeline=p;target.output=std::move(owned);return 0;}catch(...){return 112;}
}
inline int compile_graphics_vertex_only_on_host(const CtrShaderInputsV1& inputs,const CtrShaderOwnedRangesV1& ranges,
 const CtrPm4::Registers& registers,const CtrGraphicsProfileV1& profile,uint64_t vs,
 std::shared_ptr<GraphicsVertexOnlyOwner>& output,std::string& diagnostic){
 if(!vs || registers.words[0x2c08] || (registers.words[0x2c09]&255))return 88;
 wchar_t path[4096]{};const auto length=GetFullPathNameW(L"build/CTRPort-infra/graphics_companions/ctr_graphics_companion_v28.dll",4096,path,nullptr);if(!length || length>=4096)return 88;
 HMODULE module=LoadLibraryExW(path,nullptr,LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR|LOAD_LIBRARY_SEARCH_DEFAULT_DIRS);if(!module)return 88;
 struct Loaded{HMODULE value;~Loaded(){FreeLibrary(value);}}loaded{module};
 const auto call=reinterpret_cast<decltype(&ctr_graphics_compile_vertex_only_v1)>(GetProcAddress(module,"ctr_graphics_compile_vertex_only_v1"));if(!call)return 88;
 CtrShaderRangeLeaseV1 lease{};const int captured=ranges.begin(ranges.context,vs,65564,&lease);if(captured)return captured;
 const uint64_t bytes=lease.bytes&~uint64_t(3);const int released=ranges.end(ranges.context,&lease);if(released)return released;
 GraphicsVertexOnlySink sink{vs,0,{}};char reason[2048]{};
 const int status=call(&inputs,&ranges,vs,bytes,registers.words.data(),static_cast<uint32_t>(registers.words.size()),&profile,graphics_vertex_only_sink,&sink,reason,sizeof(reason));
 diagnostic=reason;if(status)return status;if(sink.calls!=1 || !sink.output)return 112;output=std::move(sink.output);return 0;
}
inline int compile_graphics_vertex_only(const CtrShaderInputsV1& inputs,const CtrShaderOwnedRangesV1& ranges,
 const CtrPm4::Registers& registers,const CtrGraphicsProfileV1& profile,uint64_t vs,
 std::shared_ptr<GraphicsVertexOnlyOwner>& output,std::string& diagnostic){
 struct Work{const CtrShaderInputsV1& inputs;const CtrShaderOwnedRangesV1& ranges;const CtrPm4::Registers& registers;const CtrGraphicsProfileV1& profile;uint64_t vs;std::shared_ptr<GraphicsVertexOnlyOwner> output;std::string reason;int status=112;};
 Work work{inputs,ranges,registers,profile,vs,{}, {},112};
 auto entry=[](void* raw)->DWORD{auto& w=*static_cast<Work*>(raw);try{w.status=compile_graphics_vertex_only_on_host(w.inputs,w.ranges,w.registers,w.profile,w.vs,w.output,w.reason);}catch(...){w.status=112;w.reason="Vertex-only host exception";}return 0;};
 HANDLE thread=CreateThread(nullptr,8*1024*1024,entry,&work,0,nullptr);if(!thread)return 112;
 const auto waited=WaitForSingleObject(thread,INFINITE);CloseHandle(thread);if(waited!=WAIT_OBJECT_0)std::terminate();
 diagnostic=std::move(work.reason);if(!work.status)output=std::move(work.output);return work.status;
}
inline int graphics_vertex_only_sink_control(){
 uint32_t spirv[5]{0x07230203,0x10000,0,1,0},userdata[1]{17},flat[16]{17};
 CtrGraphicsResourcesV1 r{};r.version=1;r.size=sizeof(r);r.flattened_userdata_words=16;
 CtrCompiledGraphicsStageV1 stage{1,sizeof(stage),1,5,0x2000,spirv,&r,userdata,1};
 CtrCompiledGraphicsStageV2 v{2,sizeof(v),stage,flat,16};CtrGraphicsBindingContractV1 b{};b.version=1;b.size=sizeof(b);
 CtrGraphicsPipelineStageV1 p{};p.version=1;p.size=sizeof(p);
 CtrCompiledGraphicsVertexOnlyV1 delivery{1,sizeof(delivery),&v,&b,&p};GraphicsVertexOnlySink sink{0x2000,0,{}};
 if(graphics_vertex_only_sink(&sink,&delivery) || !sink.output)return 1;
 spirv[1]=99;flat[0]=99;p.step_rate0=99;
 if(sink.output->vertex->spirv[1]!=0x10000 || sink.output->vertex->flattened_userdata[0]!=17 || sink.output->pipeline.step_rate0)return 2;
 unsigned rejected=0;b.unified_before=1;GraphicsVertexOnlySink bad{0x2000,0,{}};
 if(graphics_vertex_only_sink(&bad,&delivery)==112 && !bad.output)++rejected;
 b.unified_before=0;p.attribute_count=33;bad={0x2000,0,{}};
 if(graphics_vertex_only_sink(&bad,&delivery)==112 && !bad.output)++rejected;
 if(graphics_vertex_only_sink(&sink,&delivery)==112 && sink.output->vertex->spirv[1]==0x10000)++rejected;
 // Retail depth-only VS has zero direct user SGPRs; a null zero-length view is legal.
 p.attribute_count=0;stage.userdata=nullptr;stage.userdata_words=0;v.stage=stage;
 GraphicsVertexOnlySink zero{0x2000,0,{}};
 if(graphics_vertex_only_sink(&zero,&delivery) || !zero.output || zero.output->vertex->userdata_words)return 4;
 stage.userdata_words=1;v.stage=stage;GraphicsVertexOnlySink absent{0x2000,0,{}};
 if(graphics_vertex_only_sink(&absent,&delivery)!=112 || absent.output)return 5;
 if(rejected!=3)return 3;
 std::cout<<"{\"scope\":\"authored_vertex_only_sink\",\"passed\":true,\"negative_cases\":3,\"zero_userdata_supported\":true,\"mutation_isolated\":true,\"synthetic\":true,\"GPU_execution\":false}\n";return 0;
}
}
