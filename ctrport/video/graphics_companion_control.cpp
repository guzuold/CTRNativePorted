// SPDX-License-Identifier: GPL-2.0-only
#include "graphics_companion.h"
#include "pair_compile_timing.hpp"
#include <array>
#include <cstring>
#include <cstdio>
#include <vector>
#include <fstream>
struct Fixture {std::array<uint32_t,16> code{};unsigned captures=0,releases=0;};
static int Capture(void* context,uint64_t address,uint64_t size,CtrShaderSnapshot* out){
    auto& f=*static_cast<Fixture*>(context);++f.captures;
    if(address!=0x1000 || size!=64)return 112;
    *out={f.code.data(),size,address,1};return 0;
}
static int Release(void* context,const CtrShaderSnapshot* out){
    auto& f=*static_cast<Fixture*>(context);++f.releases;
    return out->snapshot_token==1 ? 0 : 88;
}
struct SinkState {std::vector<uint32_t> spirv;CtrGraphicsResourcesV1 resources{};unsigned calls=0;int result=0;};
static int Sink(void* context,const CtrCompiledGraphicsStageV1* stage){
    auto& state=*static_cast<SinkState*>(context);++state.calls;
    if(!stage || stage->version!=1 || stage->size!=sizeof(*stage) || stage->spirv_words<5 ||
       !stage->spirv || stage->spirv[0]!=0x07230203 || !stage->resources ||
       stage->resources->version!=1 || stage->code_address!=0x1000 || !stage->userdata)return 89;
    if(state.result)return state.result;
    state.spirv.assign(stage->spirv,stage->spirv+stage->spirv_words);state.resources=*stage->resources;
    return 0;
}
struct SinkStateV2 { SinkState base; std::vector<uint32_t> flattened; };
static int SinkV2(void* context,const CtrCompiledGraphicsStageV2* stage){
    auto& state=*static_cast<SinkStateV2*>(context);
    if(!stage || stage->version!=2 || stage->size!=sizeof(*stage) ||
       !stage->flattened_userdata || stage->flattened_words<16)return 89;
    int result=Sink(&state.base,&stage->stage);if(result)return result;
    state.flattened.assign(stage->flattened_userdata,stage->flattened_userdata+stage->flattened_words);
    return 0;
}
struct PairState {unsigned calls=0;int reject=0;std::vector<uint32_t> vertex;};
static int PairSink(void* context,const CtrCompiledGraphicsPairV1* pair) {
    auto& state=*static_cast<PairState*>(context);++state.calls;
    if(!pair || pair->version!=1 || pair->size!=sizeof(*pair) || !pair->fragment || !pair->vertex ||
       !pair->fragment_bindings || !pair->vertex_bindings)return 89;
    const auto& ps=*pair->fragment_bindings;const auto& vs=*pair->vertex_bindings;
    if(ps.version!=1 || vs.version!=1 || ps.size!=sizeof(ps) || vs.size!=sizeof(vs) ||
       ps.unified_before || ps.buffer_before || ps.userdata_before ||
       vs.unified_before!=ps.unified_after || vs.buffer_before!=ps.buffer_after ||
       vs.userdata_before!=ps.userdata_after || pair->fragment->stage.vertex ||
       pair->vertex->stage.vertex!=1 || pair->vertex->stage.spirv[0]!=0x07230203)return 89;
    if(state.reject)return state.reject;
    state.vertex.assign(pair->vertex->stage.spirv,pair->vertex->stage.spirv+pair->vertex->stage.spirv_words);
    return 0;
}
static int PairSinkV2(void* context,const CtrCompiledGraphicsPairV2* extended){
 if(!extended || extended->version!=2 || extended->size!=sizeof(*extended) || !extended->fragment_pipeline || !extended->vertex_pipeline)return 89;
 const auto& ps=*extended->fragment_pipeline;const auto& vs=*extended->vertex_pipeline;
 if(ps.version!=1 || vs.version!=1 || ps.size!=sizeof(ps) || vs.size!=sizeof(vs) ||
    ps.has_fetch || ps.attribute_count || vs.has_fetch || vs.attribute_count ||
    ps.mrt_mask || ps.persp_sample || ps.linear_sample || vs.vertex_offset_sgpr!=-1 || vs.instance_offset_sgpr!=-1)return 89;
 return PairSink(context,extended->pair);
}
static int VertexOnlySink(void* context,const CtrCompiledGraphicsVertexOnlyV1* delivery) {
 auto& state=*static_cast<PairState*>(context);++state.calls;
 if(!delivery || delivery->version!=1 || delivery->size!=sizeof(*delivery) || !delivery->vertex ||
    !delivery->vertex_bindings || !delivery->vertex_pipeline)return 89;
 const auto& bindings=*delivery->vertex_bindings;
 if(bindings.unified_before || bindings.buffer_before || bindings.userdata_before ||
    delivery->vertex->stage.vertex!=1 || delivery->vertex->stage.spirv[0]!=0x07230203 ||
    delivery->vertex_pipeline->version!=1)return 89;
 if(state.reject)return state.reject;
 state.vertex.assign(delivery->vertex->stage.spirv,delivery->vertex->stage.spirv+delivery->vertex->stage.spirv_words);
 return 0;
}
struct ComputeControl {unsigned calls=0;int reject=0;std::vector<uint32_t> words;std::array<uint32_t,80> bank{};CtrGraphicsBindingContractV1 bindings{};};
static int ComputeSink(void* context,const CtrCompiledComputeV1* delivery){
 auto& state=*static_cast<ComputeControl*>(context);++state.calls;
 if(!delivery || delivery->version!=1 || delivery->size!=sizeof(*delivery) || !delivery->stage || !delivery->bindings || delivery->compute_words!=80 || !delivery->compute_registers)return 99;
 const auto& stage=delivery->stage->stage;bool compute=false,local=false;
 if(!stage.spirv || stage.spirv_words<5 || stage.spirv[0]!=0x07230203 || !stage.resources)return 99;
 for(uint32_t i=5;i<stage.spirv_words;){const auto n=stage.spirv[i]>>16;if(!n || n>stage.spirv_words-i)return 99;
  if((stage.spirv[i]&65535)==15){if(n<3 || stage.spirv[i+1]!=5)return 99;compute=true;}
  if((stage.spirv[i]&65535)==16 && n>=6 && stage.spirv[i+2]==17){if(stage.spirv[i+3]!=8 || stage.spirv[i+4]!=8 || stage.spirv[i+5]!=1)return 99;local=true;}i+=n;}
 if(!compute || !local || delivery->bindings->version!=1 || delivery->bindings->size!=sizeof(CtrGraphicsBindingContractV1))return 99;state.words.assign(stage.spirv,stage.spirv+stage.spirv_words);std::memcpy(state.bank.data(),delivery->compute_registers,320);state.bindings=*delivery->bindings;return state.reject;
}
struct AuxControl {unsigned calls=0;int reject=0;std::vector<uint32_t> words;};
static int AuxSink(void* context,uint32_t kind,const uint32_t* words,uint32_t count){
 auto& state=*static_cast<AuxControl*>(context);++state.calls;
 if(!words || count<5 || words[0]!=0x07230203 || words[1]!=0x00010500)return 99;
 bool entry=false;
 for(uint32_t i=5;i<count;){const auto n=words[i]>>16;if(!n || n>count-i)return 99;
  if((words[i]&65535)==15){if(n<3 || words[i+1]!=(kind==0?1u:2u))return 99;entry=true;}i+=n;}
 if(!entry)return 99;state.words.assign(words,words+count);return state.reject;
}
int main(){
    {
        std::array<unsigned char,65> identity{};for(size_t i=0;i<identity.size();++i)identity[i]=static_cast<unsigned char>(i*37+11);
        const auto first=CtrPm4::ExactInputSha256(identity.data(),identity.size());
        const auto repeated=CtrPm4::ExactInputSha256(identity.data(),identity.size());
        identity[31]^=0x80;const auto changed=CtrPm4::ExactInputSha256(identity.data(),identity.size());
        if(first.size()!=64 || first!=repeated || first==changed)return 90;
        std::puts("exact input SHA256 unchanged repeat identical and one-byte change distinct; full65B extent");
    }
    std::array<uint32_t,8> invalidImage{};CtrGraphicsEffectiveImageClassV1 effective{};
    if(ctr_graphics_effective_image_classify_v1(invalidImage.data(),8,0,0,&effective) || effective.raw_type_valid || !effective.effective_type_valid || !(effective.null_reasons&1) || effective.fallback_reason!=1)return 250;
    CtrShaderProvenanceV1 provenanceSentinel{};provenanceSentinel.flat_index=999;
    if(ctr_graphics_provenance_span_v1(112,8,&provenanceSentinel,1)!=60 || ctr_graphics_provenance_span_v1(112,1,&provenanceSentinel,1)!=91 || provenanceSentinel.flat_index!=999)return 249;
    if(ctr_graphics_provenance_observer_v1(nullptr,reinterpret_cast<void*>(1))!=60 || ctr_graphics_provenance_observer_v1(nullptr,nullptr))return 247;

 {AuxControl aux;
  for(uint32_t kind=0;kind<2;++kind){
   if(ctr_graphics_auxiliary_vertex_only_v1(kind,AuxSink,&aux) || aux.calls!=kind+1)return 201;
   const auto owned=aux.words;
   std::ofstream emitted(kind==0?"build/CTRPort-data/rectlist_public.tcs.spv":"build/CTRPort-data/rectlist_public.tes.spv",std::ios::binary);
   emitted.write(reinterpret_cast<const char*>(owned.data()),static_cast<std::streamsize>(owned.size()*4));
   if(!emitted)return 205;
   if(ctr_graphics_auxiliary_vertex_only_v1(2,AuxSink,&aux)!=60 || aux.words!=owned)return 202;
  }
  if(ctr_graphics_auxiliary_vertex_only_v1(0,nullptr,&aux)!=60)return 203;
  aux.reject=73;if(ctr_graphics_auxiliary_vertex_only_v1(0,AuxSink,&aux)!=73 || aux.calls!=3)return 204;
  std::puts("whole public RectListTCS/PassthroughTES SPIRV1.5 executionmodels1/2 owned sink, invalid60/reject73; vertex-only zero varyings; no GPU pipeline authority");
 }

    {
        uint32_t format=0;
        if(ctr_graphics_surface_format_v1(10,0,&format) || format!=37 ||
           ctr_graphics_surface_format_v1(10,9,&format) || format!=43 ||
           ctr_graphics_surface_format_v1(14,7,&format) || format!=109)return 177;
        const auto previous=format;
        if(ctr_graphics_surface_format_v1(0,0,&format)!=88 ||
           ctr_graphics_surface_format_v1(63,15,&format)!=88 ||
           ctr_graphics_surface_format_v1(64,0,&format)!=60 ||
           ctr_graphics_surface_format_v1(10,16,&format)!=60 ||
           ctr_graphics_surface_format_v1(10,0,nullptr)!=60 || format!=previous)return 177;
        unsigned mapped=0,missing=0;
        for(unsigned d=0;d<64;++d)for(unsigned n=0;n<16;++n){format=0xffffffff;
          int status=ctr_graphics_surface_format_v1(d,n,&format);
          if(status==0){if(!format || format==0xffffffff)return 177;++mapped;}
          else if(status==88){if(format!=0xffffffff)return 177;++missing;}
          else return 177;}
        std::printf("surface_format_CABI=1024pairs mapped=%u missing=%u invalid_atomic60 synthetic=true\n",mapped,missing);
    }
    {
        struct VertexFixture {std::array<uint32_t,4> sharp{0x8000,16u<<16,4,10u<<15};unsigned begin=0,end=0,capture=0,release=0;} fixture;
        CtrShaderInputsV1 api{1,sizeof(CtrShaderInputsV1),&fixture,
          [](void* p,uint64_t address,uint64_t bytes,CtrShaderSnapshot* out)->int{
            auto& f=*static_cast<VertexFixture*>(p);++f.capture;if(address!=0x2000 || bytes!=16)return 88;
            *out={f.sharp.data(),16,address,1};return 0;},
          [](void* p,const CtrShaderSnapshot*)->int{++static_cast<VertexFixture*>(p)->release;return 0;}};
        CtrShaderOwnedRangesV1 ranges{1,sizeof(CtrShaderOwnedRangesV1),&fixture,
          [](void* p,uint64_t address,uint64_t cap,CtrShaderRangeLeaseV1* out)->int{
            ++static_cast<VertexFixture*>(p)->begin;if(address!=0x2000 || cap!=16)return 88;
            *out={address,16,1};return 0;},
          [](void* p,const CtrShaderRangeLeaseV1*)->int{++static_cast<VertexFixture*>(p)->end;return 0;}};
        std::array<uint32_t,2> ud{0x2000,0};CtrGraphicsPipelineStageV1 stage{};
        stage.version=1;stage.size=sizeof(stage);stage.has_fetch=1;stage.attribute_count=1;
        stage.step_rate0=7;stage.step_rate1=13;stage.attributes[0]={3,0,4,0,0,0,5,0,0};
        CtrGraphicsVertexInputsV1 result{};
        for(unsigned rate=0;rate<4;++rate){stage.attributes[0].instance_data=rate;
          if(ctr_graphics_vertex_inputs_v1(&api,&ranges,ud.data(),2,&stage,&result) ||
             result.count!=1 || result.inputs[0].location!=3 || result.inputs[0].binding!=3 ||
             result.inputs[0].guest_address!=0x8005 || result.inputs[0].stride!=16 ||
             result.inputs[0].buffer_size!=64 || result.inputs[0].offset!=0 ||
             result.inputs[0].input_rate!=(rate?1u:0u) ||
             result.inputs[0].divisor!=(rate==1?7u:rate==2?13u:1u))return 176;}
        const auto previous=result;stage.attributes[0].sgpr_base=2;
        if(ctr_graphics_vertex_inputs_v1(&api,&ranges,ud.data(),2,&stage,&result)!=88 ||
           std::memcmp(&previous,&result,sizeof(result)) || fixture.begin!=4 || fixture.end!=4 ||
           fixture.capture!=4 || fixture.release!=4)return 176;
        stage.attributes[0].sgpr_base=0;stage.attributes[0].instance_data=4;
        if(ctr_graphics_vertex_inputs_v1(&api,&ranges,ud.data(),2,&stage,&result)!=60 ||
           std::memcmp(&previous,&result,sizeof(result)))return 176;
        stage.has_fetch=0;stage.attribute_count=0;
        if(ctr_graphics_vertex_inputs_v1(nullptr,nullptr,nullptr,0,&stage,&result) || result.count)return 176;
        std::puts("vertex_inputs_CABI=4rates_exactownedsharp_instoffset_stride_size_atomicmissing88_invalid60_nofetch0 passed synthetic=true");
    }
    {
        std::vector<uint32_t> bank(0xD000);CtrGraphicsKeyV1 output{};
        for(unsigned mask=0;mask<256;++mask) {
            unsigned width=0;for(unsigned value=mask;value;value>>=1)++width;
            if(ctr_graphics_key_decode_v1(bank.data(),uint32_t(bank.size()),mask,0,0,&output) ||
               output.version!=1 || output.size!=sizeof(output) || output.mrt_mask!=mask ||
               output.num_color_attachments!=width || output.depth_samples!=1 || output.num_samples!=1)return 175;
        }
        const auto previous=output;
        if(ctr_graphics_key_decode_v1(nullptr,0xD000,0,0,0,&output)!=60 ||
           ctr_graphics_key_decode_v1(bank.data(),0xCFFF,0,0,0,&output)!=60 ||
           ctr_graphics_key_decode_v1(bank.data(),0xD000,256,0,0,&output)!=60 ||
           ctr_graphics_key_decode_v1(bank.data(),0xD000,0,2,0,&output)!=60 ||
           ctr_graphics_key_decode_v1(bank.data(),0xD000,0,0,2,&output)!=60 ||
           ctr_graphics_key_decode_v1(bank.data(),0xD000,0,0,0,nullptr)!=60 ||
           std::memcmp(&previous,&output,sizeof(output)))return 175;
        std::puts("graphics_key_CABI=256MRT_nullcolors_explicitcaps_invalid_atomic60 passed synthetic=true");
    }
    {
        std::vector<uint32_t> bank(0xD000);CtrGraphicsViewportStateV1 state{};
        if(ctr_graphics_viewport_state_v1(bank.data(),0xD000,32768,32768,0,&state) || state.version!=1 ||
           state.count!=1 || state.viewports[0].x!=-1 || state.viewports[0].width!=1 ||
           state.viewports[0].maxDepth!=1 || state.scissors[0].extent.width!=1)return 184;
        const auto saved=state;
        if(ctr_graphics_viewport_state_v1(nullptr,0xD000,32768,32768,0,&state)!=60 ||
           ctr_graphics_viewport_state_v1(bank.data(),0,32768,32768,0,&state)!=60 ||
           ctr_graphics_viewport_state_v1(bank.data(),0xD000,0,32768,0,&state)!=60 ||
           ctr_graphics_viewport_state_v1(bank.data(),0xD000,32768,32768,2,&state)!=60 ||
           std::memcmp(&saved,&state,sizeof(state)))return 184;
        const auto putFloat=[&](uint32_t index,float value){std::memcpy(&bank[index],&value,4);};
        bank[0xA206]=63;bank[0xA00D]=bank[0xA082]=bank[0xA091]=1000|(1000u<<16);
        for(uint32_t i=0;i<16;++i){const uint32_t index=0xA10F+6*i;
            putFloat(index,100);putFloat(index+1,300);putFloat(index+2,200);putFloat(index+3,400);
            putFloat(index+4,2);putFloat(index+5,0);}
        if(ctr_graphics_viewport_state_v1(bank.data(),0xD000,32768,32768,0,&state) || state.count!=16)return 185;
        for(uint32_t i=0;i<16;++i){const auto& v=state.viewports[i];
            if(v.x!=200 || v.y!=200 || v.width!=200 || v.height!=400 || v.minDepth!=0 || v.maxDepth!=1 ||
               state.scissors[i].extent.width!=1000)return 185;}
        if(ctr_graphics_viewport_state_v1(bank.data(),0xD000,32768,32768,1,&state) ||
           state.viewports[0].minDepth!=-2 || state.viewports[0].maxDepth!=2)return 185;
        bank[0xA204]=1u<<16;
        if(ctr_graphics_viewport_state_v1(bank.data(),0xD000,8192,4096,0,&state) ||
           state.viewports[0].x!=0 || state.viewports[0].width!=8192 || state.viewports[0].height!=4096)return 185;
        std::puts("viewport_state_CABI=whole_public_emptyfallback_16viewports_depthclamp_unrestricted_clipdisabled_actualmax_invalidatomic60 synthetic_noGPU");
    }
    {
        std::vector<uint32_t> bank(0xD000);bank[0xC242]=4;bank[0xA282]=8;
        CtrGraphicsDynamicStateV1 state{};
        if(ctr_graphics_dynamic_state_v1(bank.data(),0xD000,0,0,0,&state) || state.version!=1 ||
           state.depth_test || state.stencil_test || state.line_width!=1 || state.primitive_restart)return 186;
        const auto saved=state;
        if(ctr_graphics_dynamic_state_v1(nullptr,0xD000,0,0,0,&state)!=60 ||
           ctr_graphics_dynamic_state_v1(bank.data(),0xD000,2,0,0,&state)!=60 ||
           ctr_graphics_dynamic_state_v1(bank.data(),0xD000,0,2,0,&state)!=60 ||
           ctr_graphics_dynamic_state_v1(bank.data(),0xD000,0,0,2,&state)!=60 ||
           std::memcmp(&saved,&state,sizeof(state)))return 186;
        bank[0xA010]=1;bank[0xA011]=1;bank[0xA012]=bank[0xA013]=1;
        bank[0xA200]=1|2|4|(5u<<4)|(7u<<8);bank[0xA10B]=4;bank[0xA10C]=0x56341209;
        if(ctr_graphics_dynamic_state_v1(bank.data(),0xD000,0,0,0,&state) || !state.depth_test ||
           !state.depth_write || state.depth_compare!=5 || !state.stencil_test || state.front.reference!=0x56 ||
           state.back.reference!=0x56 || state.front.write_mask!=0x34 || state.front.compare_mask!=0x12)return 187;
        bank[0xA000]=3;
        if(ctr_graphics_dynamic_state_v1(bank.data(),0xD000,0,0,0,&state) || state.depth_write || state.front.write_mask)return 187;
        bank[0xA2A5]=1;
        if(ctr_graphics_dynamic_state_v1(bank.data(),0xD000,0,0,0,&state) || state.primitive_restart)return 187;
        if(ctr_graphics_dynamic_state_v1(bank.data(),0xD000,0,1,0,&state) || !state.primitive_restart)return 187;
        const auto restartState=state;
        if(ctr_graphics_dynamic_state_v1(bank.data(),0xD000,1,1,0,&state)!=88 ||
           std::memcmp(&state,&restartState,sizeof(state)))return 187;
        bank[0xA103]=0xffff;
        if(ctr_graphics_dynamic_state_v1(bank.data(),0xD000,1,1,0,&state))return 187;
        std::puts("dynamic_state_CABI=whole_public_depth_stencil_opval_clearmask_backfallback_restartcaps_badindex88_invalidatomic60 synthetic_noGPU");
    }
    {
        std::vector<uint32_t> bank(0xD000);CtrGraphicsBindingContractV1 ps{},vs{};
        ps.version=vs.version=1;ps.size=vs.size=sizeof(ps);
        ps.unified_after=1;ps.buffer_after=1;vs.unified_before=1;vs.unified_after=3;
        vs.buffer_before=1;vs.buffer_after=3;
        uint32_t offsets[]={0,0,0};CtrGraphicsPushDataV1 push{};
        if(ctr_graphics_push_data_v1(bank.data(),0xD000,&ps,&vs,offsets,3,&push))return 188;
        float values[4]{};std::memcpy(values,push.bytes,16);
        if(values[0]!=0 || values[1]!=0 || values[2]!=1 || values[3]!=1)return 188;
        for(uint32_t i=16;i<128;++i)if(push.bytes[i])return 188;
        bank[0xA206]=15;
        const float viewport[]={100,300,200,400};std::memcpy(&bank[0xA10F],viewport,16);
        ps.userdata_count=2;ps.userdata_after=2;ps.packed_userdata[0]=0x11223344;ps.packed_userdata[1]=0x55667788;
        vs.userdata_before=2;vs.userdata_after=3;vs.userdata_count=1;vs.packed_userdata[0]=0xaabbccdd;
        offsets[1]=255;
        if(ctr_graphics_push_data_v1(bank.data(),0xD000,&ps,&vs,offsets,3,&push))return 188;
        std::memcpy(values,push.bytes,16);uint32_t packed[3]{};std::memcpy(packed,push.bytes+16,12);
        if(values[0]!=300 || values[1]!=400 || values[2]!=100 || values[3]!=200 ||
           packed[0]!=0x11223344 || packed[1]!=0x55667788 || packed[2]!=0xaabbccdd || push.bytes[81]!=255)return 188;
        const auto saved=push;offsets[1]=256;
        if(ctr_graphics_push_data_v1(bank.data(),0xD000,&ps,&vs,offsets,3,&push)!=88 ||
           std::memcmp(&saved,&push,sizeof(push)))return 188;
        offsets[1]=0;vs.userdata_before=1;
        if(ctr_graphics_push_data_v1(bank.data(),0xD000,&ps,&vs,offsets,3,&push)!=60 ||
           std::memcmp(&saved,&push,sizeof(push)))return 188;
        std::puts("push_data_CABI=whole_MakeUserData_payload120_block128_PSfirst_userdata_offsets0and255_missing256atomic88_badcontract60 synthetic_noGPU");
    }
    {
        std::vector<uint32_t> bank(0xD000);uint32_t userdata[16]{};userdata[2]=17;userdata[3]=23;
        CtrGraphicsPipelineStageV1 vertex{};vertex.version=1;vertex.size=sizeof(vertex);
        vertex.vertex_offset_sgpr=2;vertex.instance_offset_sgpr=3;
        CtrGraphicsDrawParametersV1 draw{};
        auto decode=[&]{return ctr_graphics_draw_parameters_v1(bank.data(),0xD000,&vertex,userdata,16,&draw);};
        if(decode() || draw.vertex_offset || draw.instance_offset || draw.instance_count!=1)return 190;
        vertex.has_fetch=1;
        if(decode() || draw.vertex_offset!=17 || draw.instance_offset!=23)return 190;
        bank[0xA102]=41;bank[0xC24D]=7;
        if(decode() || draw.vertex_offset!=41 || draw.instance_offset!=23 || draw.instance_count!=7)return 190;
        vertex.vertex_offset_sgpr=-1;vertex.instance_offset_sgpr=-1;bank[0xA102]=0;
        if(decode() || draw.vertex_offset || draw.instance_offset)return 190;
        const auto saved=draw;vertex.instance_offset_sgpr=16;
        if(decode()!=60 || std::memcmp(&saved,&draw,sizeof(draw)))return 190;
        std::puts("draw_parameters_CABI=whole_GetDrawOffsets_NumInstances_nofetch_fetch_precedence_sentinels_invalidatomic60 synthetic_noGPU");
    }
    Fixture fixture;fixture.code[0]=0xbeeb03ff;fixture.code[1]=3;fixture.code[2]=0xbf810000;
    std::memcpy(reinterpret_cast<char*>(fixture.code.data())+32,"OrbShdr",7);
    fixture.code[10]=12<<8;
    CtrShaderInputsV1 inputs{1,sizeof(CtrShaderInputsV1),&fixture,Capture,Release};
    std::array<uint32_t,0xD000> flat{};flat[0x2c48]=0x10;flat[0x2c08]=0x10;
    CtrGraphicsProfileV1 profile{};profile.version=1;profile.size=sizeof(profile);
    profile.max_ubo_size=65536;profile.max_viewport_width=32768;profile.max_viewport_height=32768;
    profile.max_shared_memory_size=32768;profile.supported_spirv=0x10000;profile.subgroup_size=32;
    uint32_t words=999;char reason[256]{};
    CtrGraphicsResourcesV1 resources{};
    auto run=[&](uint32_t vertex){return ctr_graphics_compile_diagnostic_v2(&inputs,0x1000,64,
        flat.data(),uint32_t(flat.size()),vertex,&profile,&words,reason,sizeof(reason),&resources,sizeof(resources));};
    {
      std::array<uint32_t,80> cs{};cs[7]=8;cs[8]=8;cs[9]=1;cs[12]=0x10;cs[19]=12u<<1;
      CtrShaderOwnedRangesV1 ranges{1,sizeof(CtrShaderOwnedRangesV1),&fixture,
       [](void*,uint64_t,uint64_t,CtrShaderRangeLeaseV1*)->int{return 88;},[](void*,const CtrShaderRangeLeaseV1*)->int{return 88;}};
      ComputeControl control;
      const auto compile=[&](uint64_t address,uint32_t count,CtrCompiledComputeSinkV1 sink){return ctr_compute_compile_v1(&inputs,&ranges,address,64,flat.data(),uint32_t(flat.size()),cs.data(),count,&profile,sink,&control,reason,sizeof(reason));};
      if(compile(0x1000,80,ComputeSink) || control.calls!=1 || control.words.empty())return 206;
      std::ofstream emittedCompute("build/CTRPort-data/compute_public_control.spv",std::ios::binary);emittedCompute.write(reinterpret_cast<const char*>(control.words.data()),static_cast<std::streamsize>(control.words.size()*4));emittedCompute.close();if(!emittedCompute)return 211;
      const auto owned=control.words;
      if(compile(0x2000,80,ComputeSink)!=60 || control.calls!=1 || control.words!=owned)return 207;
      if(compile(0x1000,79,ComputeSink)!=60 || compile(0x1000,80,nullptr)!=60 || control.calls!=1)return 208;
      cs[7]=0;if(compile(0x1000,80,ComputeSink)!=88 || control.calls!=1)return 209;cs[7]=8;
      control.reject=73;if(compile(0x1000,80,ComputeSink)!=73 || control.calls!=2)return 210;
      fixture.captures=fixture.releases=0;
      std::puts("compute whole public translator_emitter GLCompute5 owned80word_CSbank descriptor_contract sinkcopy address60 length/null60 zeroGroup88 sink73; authored_noGPUdispatch");
    }
    const int first=run(1);std::printf("status=%d words=%u reason=%s captures=%u releases=%u\n",first,words,reason,fixture.captures,fixture.releases);if(first || words<5 || reason[0] || fixture.captures!=1 || fixture.releases!=1)return 1;
    if(resources.version!=1 || resources.size!=sizeof(resources) || resources.binding_count!=resources.buffers+resources.images+resources.samplers+resources.fmasks)return 7;
    const auto preserved=words;
    if(run(0) || words<5 || reason[0] || fixture.captures!=2 || fixture.releases!=2)return 2;
    words=preserved;
    const auto oldResources=resources;
    profile.support_int64=2;
    if(run(1)!=60 || words!=preserved || fixture.captures!=2 || std::memcmp(&resources,&oldResources,sizeof(resources)))return 3;
    profile.support_int64=0;profile.max_ubo_size=0;
    if(run(1)!=60 || words!=preserved || fixture.captures!=2)return 4;
    profile.max_ubo_size=65536;flat[0x2c48]=0x20;
    if(run(1)!=60 || words!=preserved || fixture.captures!=2)return 5;
    flat[0x2c48]=0x10;fixture.code[10]=0;
    if(run(1)!=60 || words!=preserved || fixture.captures!=3 || fixture.releases!=3)return 6;
    const auto finalResources=resources;
    if(ctr_graphics_compile_diagnostic_v2(&inputs,0x1000,64,flat.data(),uint32_t(flat.size()),1,
       &profile,&words,reason,sizeof(reason),&resources,0)!=60 || words!=preserved ||
       std::memcmp(&resources,&finalResources,sizeof(resources)) || fixture.captures!=3)return 8;
    fixture.code[10]=12<<8;SinkState sink;
    auto deliver=[&](CtrCompiledGraphicsSinkV1 callback){return ctr_graphics_compile_stage_v1(&inputs,0x1000,64,
        flat.data(),uint32_t(flat.size()),1,&profile,callback,&sink,reason,sizeof(reason));};
    if(deliver(Sink) || sink.calls!=1 || sink.spirv.size()<5 || sink.resources.version!=1 ||
       fixture.captures!=4 || fixture.releases!=4)return 9;
    const auto ownedSpirv=sink.spirv;sink.result=73;
    if(deliver(Sink)!=73 || sink.calls!=2 || sink.spirv!=ownedSpirv || !reason[0] ||
       fixture.captures!=5 || fixture.releases!=5)return 10;
    if(deliver(nullptr)!=60 || sink.calls!=2 || fixture.captures!=5)return 11;
    fixture.code[10]=0;
    if(deliver(Sink)!=60 || sink.calls!=2 || fixture.captures!=6 || fixture.releases!=6)return 12;
    std::puts("synchronous stage sink passed owned SPIRV copy survives DLL return, resource metadata copied, sink73 propagated, null60 and malformed60 never invoke sink; no GPU work=true");
    {
        std::array<uint32_t,16> userdata{};userdata[4]=0x1000;userdata[5]=16u<<16;userdata[6]=4;
        CtrGraphicsResourceIdentityV1 resource{0,4,1,0,0};CtrGraphicsBufferRequirementV1 requirement{};
        if(ctr_graphics_buffer_requirement_v1(&resource,userdata.data(),8,&requirement) ||
           requirement.guest_address!=0x1000 || requirement.bytes!=64 || requirement.stride!=16 ||
           requirement.num_records!=4 || !requirement.is_written)return 13;
        const auto preservedRequirement=requirement;resource.buffer_type=6;
        if(ctr_graphics_buffer_requirement_v1(&resource,userdata.data(),8,&requirement)!=88 ||
           std::memcmp(&requirement,&preservedRequirement,sizeof(requirement)))return 14;
        resource.buffer_type=0;
        if(ctr_graphics_buffer_requirement_v1(&resource,userdata.data(),7,&requirement)!=88)return 15;
        userdata[6]=0xffffffff;
        if(ctr_graphics_buffer_requirement_v1(&resource,userdata.data(),8,&requirement)!=88)return 16;
        userdata[6]=4;userdata[7]=3u<<30;
        if(ctr_graphics_buffer_requirement_v1(&resource,userdata.data(),8,&requirement)!=88)return 17;
        userdata[7]=0;userdata[4]=1;
        if(ctr_graphics_buffer_requirement_v1(&resource,userdata.data(),8,&requirement)!=88 ||
           std::memcmp(&requirement,&preservedRequirement,sizeof(requirement)))return 18;
        std::puts("compiler identified direct userdata buffer requirements passed exact address/stride/extent, special/partial/overflow/type/sentinel atomic88; ownership/coherence NOT admitted");
    }
    {
        // Independently enumerated block-grid allocations for the bounded original census.
        const uint32_t cases[][8]={{4,4,32,1,0,32,32,1024},
            {860,1120,864,1,0,864,1120,967680},{64,64,64,2,1,64,64,5120},
            {1024,1024,1024,1,0,1024,1024,1048576},{256,256,256,1,0,256,256,65536},
            {1360,520,1376,1,0,1376,544,748544},{120,120,128,1,0,128,128,16384},
            {33,17,33,1,1,64,32,2048},{1,1,1,1,0,32,32,1024}};
        CtrGraphicsImageParametersV1 p{};p.version=1;p.size=sizeof(p);p.type=9;
        p.depth=1;p.layers=1;p.samples=1;p.data_format=37;p.tile_mode=13;p.guest_address=0x100000;
        CtrGraphicsImageExtentV1 extent{};
        for(const auto& c:cases) {
            p.width=c[0];p.height=c[1];p.pitch=c[2];p.levels=c[3];p.pow2pad=c[4];
            if(ctr_graphics_image_extent_v2(&p,&extent) || extent.pitch!=c[5] ||
               extent.padded_height!=c[6] || extent.bytes!=c[7] || extent.guest_address!=p.guest_address)return 64;
        }
        const auto saved=extent;
        const auto rejected=[&](const CtrGraphicsImageParametersV1& bad,int expected) {
            return ctr_graphics_image_extent_v2(&bad,&extent)==expected && !memcmp(&extent,&saved,sizeof(saved));
        };
        auto bad=p;bad.number_format=7;if(!rejected(bad,88))return 65;
        bad=p;bad.tile_mode=14;if(!rejected(bad,88))return 65;
        bad=p;bad.levels=3;if(!rejected(bad,88))return 65;
        bad=p;bad.levels=0;if(!rejected(bad,88))return 65;
        bad=p;bad.samples=2;if(!rejected(bad,88))return 65;
        bad=p;bad.pow2pad=2;if(!rejected(bad,88))return 65;
        bad=p;bad.compression=1;if(!rejected(bad,88))return 65;
        bad=p;bad.alt_tile=1;if(!rejected(bad,88))return 65;
        bad=p;bad.base_level=1;if(!rejected(bad,88))return 65;
        bad=p;bad.guest_address=UINT64_MAX-1023;if(!rejected(bad,88))return 65;
        bad=p;bad.width=0;if(!rejected(bad,60))return 65;
        std::puts("BC3 tile13 whole public footprint9 shapes including two mips/pow2;11 atomic rejections; upload not admitted");
    }
    {
        fixture.code.fill(0);fixture.code[0]=0xbeeb03ff;fixture.code[1]=3;fixture.code[2]=0xbf810000;
        std::memcpy(reinterpret_cast<char*>(fixture.code.data())+32,"OrbShdr",7);fixture.code[10]=12<<8;
        CtrShaderOwnedRangesV1 ranges{1,sizeof(CtrShaderOwnedRangesV1),&fixture,
            [](void*,uint64_t,uint64_t,CtrShaderRangeLeaseV1*)->int{return 88;},
            [](void*,const CtrShaderRangeLeaseV1*)->int{return 88;}};
        SinkState delivered;
        if(ctr_graphics_compile_stage_v2(&inputs,&ranges,0x1000,64,flat.data(),uint32_t(flat.size()),1,
            &profile,Sink,&delivered,reason,sizeof(reason)) || delivered.calls!=1)return 19;
        SinkStateV2 extended;
        if(ctr_graphics_compile_stage_v3(&inputs,&ranges,0x1000,64,flat.data(),uint32_t(flat.size()),1,
            &profile,SinkV2,&extended,reason,sizeof(reason)) || extended.base.calls!=1 ||
            extended.flattened.size()!=16)return 31;
        const auto savedFlat=extended.flattened;extended.base.result=73;
        if(ctr_graphics_compile_stage_v3(&inputs,&ranges,0x1000,64,flat.data(),uint32_t(flat.size()),1,
            &profile,SinkV2,&extended,reason,sizeof(reason))!=73 || extended.flattened!=savedFlat)return 32;
        if(ctr_graphics_compile_stage_v3(&inputs,&ranges,0x1000,64,flat.data(),uint32_t(flat.size()),1,
            &profile,nullptr,&extended,reason,sizeof(reason))!=60)return 33;
        std::puts("stage v3 flattened16 live sink deep copy, rejected sink73 and null60; authored nofetch only");
        ranges.version=2;
        if(ctr_graphics_compile_stage_v2(&inputs,&ranges,0x1000,64,flat.data(),uint32_t(flat.size()),1,
            &profile,Sink,&delivered,reason,sizeof(reason))!=60 || delivered.calls!=1)return 20;
        std::puts("owned stage v2 explicit range ABI accepted, invalid version atomic60; nofetch authored only");
    }
    {
        CtrGraphicsResourceIdentityV1 resource{};resource.kind=0;resource.flags=2;resource.buffer_type=6;
        CtrGraphicsClipPlanesV1 planes{};
        flat[0xA204]=0;
        if(ctr_graphics_clip_planes_v1(&resource,flat.data(),uint32_t(flat.size()),&planes) ||
            planes.bytes || planes.enabled_mask)return 21;
        for(uint32_t i=0;i<24;++i)flat[0xA16F+i]=0x80000000u+i;
        for(uint32_t mask=1;mask<64;++mask) {
            flat[0xA204]=mask;
            if(ctr_graphics_clip_planes_v1(&resource,flat.data(),uint32_t(flat.size()),&planes) ||
                planes.bytes!=96 || planes.enabled_mask!=mask)return 22;
            for(uint32_t i=0;i<24;++i)if(planes.coefficients[i]!=0x80000000u+i)return 23;
        }
        const auto preservedPlanes=planes;resource.flags=3;
        if(ctr_graphics_clip_planes_v1(&resource,flat.data(),uint32_t(flat.size()),&planes)!=88 ||
            memcmp(&planes,&preservedPlanes,sizeof(planes)))return 24;
        resource.flags=2;resource.buffer_type=0;
        if(ctr_graphics_clip_planes_v1(&resource,flat.data(),uint32_t(flat.size()),&planes)!=88 ||
            memcmp(&planes,&preservedPlanes,sizeof(planes)))return 25;
        if(ctr_graphics_clip_planes_v1(&resource,flat.data(),1,&planes)!=60)return 26;
        std::puts("inline ClipPlanes all64 masks exact96B coefficient bits, disabled unread0B, wrong/written resource atomic88 and truncated60; noGPU admission");
    }
    {
        // Actual first-draw register metadata supplied by infra, no game payload.
        flat[0xA318]=285788928;flat[0xA319]=15;flat[0xA31A]=127;
        flat[0xA31B]=0;flat[0xA31C]=34856;flat[0xA31D]=462;
        CtrGraphicsColorRequirementV1 color{};
        if(ctr_graphics_color_requirement_v1(flat.data(),uint32_t(flat.size()),0,&color) ||
            color.pitch!=128 || color.height!=64 || color.bytes!=32768 || color.layers!=1 ||
            color.guest_address!=(uint64_t(285788928)<<8))return 27;
        const auto savedColor=color;
        for(uint32_t flag:{1u<<7,1u<<13,1u<<14,1u<<28,1u<<31}) {
            flat[0xA31C]=34856|flag;
            if(ctr_graphics_color_requirement_v1(flat.data(),uint32_t(flat.size()),0,&color)!=88 ||
                memcmp(&color,&savedColor,sizeof(color)))return 28;
        }
        flat[0xA31C]=34856;flat[0xA31A]=0x3fffff;
        flat[0xA31D]=462|(3u<<15); // samples8 -> public slice u32 overflow
        if(ctr_graphics_color_requirement_v1(flat.data(),uint32_t(flat.size()),0,&color)!=88)return 29;
        if(ctr_graphics_color_requirement_v1(flat.data(),uint32_t(flat.size()),8,&color)!=60)return 30;
        std::puts("color requirement firstdraw metadata exact32768B, unsupported flags5/overflow/slot atomic rejected; allocation requirement only");
        // Original1377 second MRT metadata; public ColorBuffer format3 = RG8 UNORM.
        flat[0xA327]=286087552;flat[0xA328]=239;flat[0xA329]=34559;
        flat[0xA32A]=0;flat[0xA32B]=32780;flat[0xA32C]=462;
        if(ctr_graphics_color_requirement_v1(flat.data(),uint32_t(flat.size()),1,&color) ||
           color.data_format!=3 || color.number_format!=0 || color.tile_mode!=14 ||
           color.pitch!=1920 || color.height!=1152 || color.bytes!=4423680 ||
           color.layers!=1 || color.samples!=1 || color.guest_address!=(uint64_t(286087552)<<8))return 204;
        // Original2336 slot0: public R32 FLOAT, two-color single-sample pass.
        flat[0xA318]=286220032;flat[0xA319]=239;flat[0xA31A]=32639;
        flat[0xA31B]=0;flat[0xA31C]=263952;flat[0xA31D]=462;
        CtrGraphicsColorRequirementV1 r32{};
        if(ctr_graphics_color_requirement_v1(flat.data(),uint32_t(flat.size()),0,&r32) ||
           r32.data_format!=4 || r32.number_format!=7 || r32.tile_mode!=14 ||
           r32.pitch!=1920 || r32.height!=1088 || r32.bytes!=8355840 ||
           r32.layers!=1 || r32.samples!=1 || r32.guest_address!=(uint64_t(286220032)<<8))return 219;
        std::puts("original2336 color0 public R32float tile14 pitch1920 padded1088 bytes8355840 metadata only");
        const auto rg8Saved=color;flat[0xA32B]=32780|(1u<<14);
        if(ctr_graphics_color_requirement_v1(flat.data(),uint32_t(flat.size()),1,&color)!=88 ||
           memcmp(&color,&rg8Saved,sizeof(color)))return 204;
        flat[0xA32B]=32780;
        std::puts("original1377 MRT1 public metadata format3 RG8 UNORM tile14 pitch1920 padded1152 bytes4423680 exact; compression88 atomic; metadata only no MRT GPU authority");

    }
    {
        std::array<uint32_t,24> flattened{};flattened[20]=0x123400;
        flattened[21]=16u<<16;flattened[22]=4;
        CtrGraphicsResourceIdentityV1 resource{};resource.sharp_index=20;
        CtrGraphicsBufferRequirementV1 requirement{};
        if(ctr_graphics_buffer_requirement_v2(&resource,flattened.data(),24,&requirement) ||
           requirement.guest_address!=0x123400 || requirement.bytes!=64)return 34;
        const auto saved=requirement;
        if(ctr_graphics_buffer_requirement_v1(&resource,flattened.data(),24,&requirement)!=60 ||
           ctr_graphics_buffer_requirement_v2(&resource,flattened.data(),23,&requirement)!=88 ||
           ctr_graphics_buffer_requirement_v2(&resource,flattened.data(),65537,&requirement)!=60 ||
           memcmp(&saved,&requirement,sizeof(saved)))return 35;
        std::puts("flattened buffer requirement sharp20 exact64B; legacy60/truncated88/oversized60 atomic");
    }
    {
        std::array<uint32_t,20> source{},copy{};source[19]=0xDEADBEEF;copy.fill(0x12345678);
        CtrGraphicsResourceIdentityV1 resource{};resource.buffer_type=1;
        uint32_t bytes=999;
        if(ctr_graphics_flatbuf_copy_v1(&resource,source.data(),20,copy.data(),20,&bytes) ||
           copy!=source || bytes!=80)return 36;
        const auto saved=copy;resource.flags=1;
        if(ctr_graphics_flatbuf_copy_v1(&resource,source.data(),20,copy.data(),20,&bytes)!=88 ||
           copy!=saved || bytes!=80)return 37;
        resource.flags=0;resource.buffer_type=0;
        if(ctr_graphics_flatbuf_copy_v1(&resource,source.data(),20,copy.data(),20,&bytes)!=88 ||
           ctr_graphics_flatbuf_copy_v1(&resource,source.data(),20,copy.data(),19,&bytes)!=60 ||
           ctr_graphics_flatbuf_copy_v1(&resource,source.data(),65537,copy.data(),20,&bytes)!=60 ||
           copy!=saved || bytes!=80)return 38;
        std::puts("Flatbuf whole20DWORD exact80B; wrong/written88/capacity/extent60 atomic; no GPU binding");
    }
    {
        std::array<uint32_t,32> flatDescriptors{};
        for(uint32_t i=0;i<32;++i)flatDescriptors[i]=0x80000000u+i;
        std::array<uint32_t,8> copied{};uint32_t descriptorWords=999;
        CtrGraphicsResourceIdentityV1 resource{};resource.kind=1;resource.sharp_index=20;
        if(ctr_graphics_descriptor_copy_v1(&resource,flatDescriptors.data(),32,copied.data(),8,&descriptorWords) ||
           descriptorWords!=8)return 39;
        for(uint32_t i=0;i<8;++i)if(copied[i]!=flatDescriptors[20+i])return 40;
        resource.kind=2;resource.sharp_index=28;
        if(ctr_graphics_descriptor_copy_v1(&resource,flatDescriptors.data(),32,copied.data(),8,&descriptorWords) ||
           descriptorWords!=4)return 41;
        for(uint32_t i=0;i<4;++i)if(copied[i]!=flatDescriptors[28+i])return 42;
        const auto saved=copied;resource.kind=1;
        if(ctr_graphics_descriptor_copy_v1(&resource,flatDescriptors.data(),32,copied.data(),8,&descriptorWords)!=88 ||
           copied!=saved || descriptorWords!=4)return 43;
        resource.sharp_index=20;
        if(ctr_graphics_descriptor_copy_v1(&resource,flatDescriptors.data(),32,copied.data(),7,&descriptorWords)!=60 ||
           copied!=saved || descriptorWords!=4)return 44;
        resource.flags=2;
        if(ctr_graphics_descriptor_copy_v1(&resource,flatDescriptors.data(),32,copied.data(),8,&descriptorWords)!=88 ||
           copied!=saved || descriptorWords!=4)return 45;
        resource.flags=0;resource.kind=3;
        if(ctr_graphics_descriptor_copy_v1(&resource,flatDescriptors.data(),32,copied.data(),8,&descriptorWords)!=88 ||
           copied!=saved || descriptorWords!=4)return 46;
        std::puts("image8/sampler4 exact compiler-flat descriptor bits; truncated88/capacity60/inline88/fmask88 atomic; no allocation or GPU policy");
    }
    {
        std::array<uint32_t,4> descriptor{};
        descriptor[0]=2u|(1u<<3)|(6u<<6)|(3u<<12)|(1u<<15);
        descriptor[1]=256u|(512u<<12);
        descriptor[2]=0x3F00u|(3u<<20)|(1u<<22)|(2u<<26);
        descriptor[3]=2u<<30;
        CtrGraphicsSamplerParametersV1 parameters{};
        for(uint32_t ratio=0;ratio<5;++ratio) {
            descriptor[0]=(descriptor[0]&~(7u<<9))|(ratio<<9);
            if(ctr_graphics_sampler_parameters_v1(descriptor.data(),4,&parameters) ||
               parameters.clamp_x!=2 || parameters.clamp_y!=1 || parameters.clamp_z!=6 ||
               parameters.compare_func!=3 || parameters.border_color!=2 ||
               parameters.lod_bias!=-1.0f || parameters.min_lod!=1.0f || parameters.max_lod!=2.0f ||
               parameters.max_anisotropy!=float(1u<<ratio) || !parameters.anisotropic ||
               !parameters.unnormalized_in_shader || parameters.mip_filter!=2)return 47;
        }
        const auto saved=parameters;descriptor[0]|=7u<<9;
        if(ctr_graphics_sampler_parameters_v1(descriptor.data(),4,&parameters)!=88 ||
           memcmp(&parameters,&saved,sizeof(saved)))return 48;
        descriptor[0]&=~(7u<<9);descriptor[3]=3u<<30;
        if(ctr_graphics_sampler_parameters_v1(descriptor.data(),4,&parameters)!=88 ||
           ctr_graphics_sampler_parameters_v1(descriptor.data(),3,&parameters)!=60 ||
           memcmp(&parameters,&saved,sizeof(saved)))return 49;
        descriptor[3]=0;descriptor[2]|=3u<<26;
        if(ctr_graphics_sampler_parameters_v1(descriptor.data(),4,&parameters)!=88)return 50;
        std::puts("sampler public fractional signedLOD/aniso5/clamp/filter/compare decoded; malformed/custom/partial atomic missing, no Vulkan admission");
    }
    {
        std::array<uint32_t,8> descriptor{};
        descriptor[0]=0x123456;descriptor[1]=10u<<20;
        descriptor[2]=127u|(63u<<14);descriptor[3]=9u<<28;
        descriptor[4]=127u<<13;
        CtrGraphicsImageParametersV1 parameters{};
        if(ctr_graphics_image_parameters_v1(descriptor.data(),8,&parameters) ||
           parameters.guest_address!=0x12345600ULL || parameters.width!=128 ||
           parameters.height!=64 || parameters.pitch!=128 || parameters.layers!=1 ||
           parameters.levels!=1 || parameters.samples!=1 || parameters.data_format!=10)return 51;
        descriptor[3]=11u<<28;descriptor[4]=(127u<<13)|1;
        if(ctr_graphics_image_parameters_v1(descriptor.data(),8,&parameters) || parameters.layers!=12)return 52;
        const auto saved=parameters;descriptor[3]=0;
        if(ctr_graphics_image_parameters_v1(descriptor.data(),8,&parameters)!=88 ||
           ctr_graphics_image_parameters_v1(descriptor.data(),7,&parameters)!=60 ||
           memcmp(&parameters,&saved,sizeof(saved)))return 53;
        std::puts("image public metadata128x64/pitch128/address, cube12layers; invalid88/partial60 atomic; byte extent and Vulkan allocation pending");
    }
    {
        std::array<uint32_t,8> sharp{};CtrGraphicsImageClassV1 result{};
        if(ctr_graphics_image_classify_v1(sharp.data(),8,&result) || result.null_reasons!=3 || result.type_valid)return 70;
        sharp[0]=0x123456; // Nonzero payload, invalid format: public null, invalid type.
        if(ctr_graphics_image_classify_v1(sharp.data(),8,&result) || result.null_reasons!=2 || result.type_valid)return 71;
        sharp[1]=10u<<20; // Invalid type alone is not public null.
        if(ctr_graphics_image_classify_v1(sharp.data(),8,&result) || result.null_reasons || result.type_valid)return 72;
        sharp[3]=9u<<28;sharp[0]=0;
        if(ctr_graphics_image_classify_v1(sharp.data(),8,&result) || result.null_reasons!=1 || !result.type_valid)return 73;
        sharp[0]=0x123456;
        if(ctr_graphics_image_classify_v1(sharp.data(),8,&result) || result.null_reasons || !result.type_valid)return 74;
        const auto saved=result;
        if(ctr_graphics_image_classify_v1(sharp.data(),7,&result)!=60 ||
           ctr_graphics_image_classify_v1(nullptr,8,&result)!=60 ||
           ctr_graphics_image_classify_v1(sharp.data(),8,nullptr)!=60 || memcmp(&saved,&result,sizeof(saved)))return 75;
        std::puts("image classification5public cases/3negative: null reason independent from type validity; no Vulkan admission");
    }
    {
        CtrGraphicsImageParametersV1 p{};p.version=1;p.size=sizeof(p);
        p.type=9;p.width=1920;p.height=1080;p.pitch=1920;p.depth=1;p.layers=1;p.levels=1;
        p.samples=1;p.data_format=10;p.tile_mode=14;p.guest_address=0x100000;
        CtrGraphicsImageExtentV1 extent{};
        if(ctr_graphics_image_extent_v1(&p,&extent) || extent.bytes!=8355840 ||
           extent.pitch!=1920 || extent.padded_height!=1088)return 54;
        const auto saved=extent;p.levels=2;
        if(ctr_graphics_image_extent_v1(&p,&extent)!=88 || memcmp(&extent,&saved,sizeof(saved)))return 55;
        p.levels=1;p.alt_tile=1;
        if(ctr_graphics_image_extent_v1(&p,&extent)!=88)return 56;
        p.alt_tile=0;p.guest_address=UINT64_MAX;
        if(ctr_graphics_image_extent_v1(&p,&extent)!=88)return 57;
        p.width=0;
        if(ctr_graphics_image_extent_v1(&p,&extent)!=60 || memcmp(&extent,&saved,sizeof(saved)))return 58;
        std::puts("whole public tile14 RGBA8 image allocation1920x1088/8355840B, mip/alt/overflow88 andzero60 atomic; no ownership or GPU admission");
    }
    {
        CtrGraphicsImageParametersV1 p{};p.version=1;p.size=sizeof(p);p.type=9;
        p.width=4;p.height=4;p.pitch=16;p.depth=1;p.layers=1;p.levels=1;p.samples=1;
        p.data_format=10;p.tile_mode=8;p.guest_address=0x100000;
        CtrGraphicsImageExtentV1 extent{};
        if(ctr_graphics_image_extent_v2(&p,&extent) || extent.bytes!=256 || extent.pitch!=16 || extent.padded_height!=4)return 59;
        p.width=64;p.height=64;p.pitch=64;p.data_format=5;p.tile_mode=13;
        if(ctr_graphics_image_extent_v2(&p,&extent) || extent.bytes!=16384)return 60;
        p.width=1920;p.height=1080;p.pitch=1920;p.data_format=3;p.tile_mode=14;
        if(ctr_graphics_image_extent_v2(&p,&extent) || extent.bytes!=4423680)return 61;
        p.data_format=4;p.number_format=7;p.tile_mode=0;p.pitch=2048;
        if(ctr_graphics_image_extent_v2(&p,&extent) || extent.bytes!=9437184 || extent.pitch!=2048 || extent.padded_height!=1152)return 61;
        std::puts("public Depth2DThin64 tile0 R32float1920x1080 pitch2048 padded1152 bytes9437184 sizing exact");
        const auto saved=extent;p.levels=2;
        if(ctr_graphics_image_extent_v2(&p,&extent)!=88 || memcmp(&extent,&saved,sizeof(saved)))return 62;
        p.levels=1;p.pitch=4097;
        if(ctr_graphics_image_extent_v2(&p,&extent)!=60 || memcmp(&extent,&saved,sizeof(saved)))return 63;
        {CtrGraphicsImageParametersV1 r{};r.version=1;r.size=sizeof(r);r.type=9;r.width=r.pitch=2048;r.height=8192;r.depth=r.layers=r.levels=r.samples=1;r.data_format=2;r.guest_address=0x100000;
         if(ctr_graphics_image_extent_v2(&r,&extent) || extent.pitch!=2048 || extent.padded_height!=8192 || extent.bytes!=33554432)return 64;
         const auto keep=extent;r.guest_address=UINT64_MAX-1;
         if(ctr_graphics_image_extent_v2(&r,&extent)!=88 || memcmp(&extent,&keep,sizeof(keep)))return 64;}
        for(uint32_t side:{1024u,2048u}){
          CtrGraphicsImageParametersV1 r{};r.version=1;r.size=sizeof(r);r.type=9;r.width=r.height=r.pitch=side;r.depth=r.layers=r.levels=r.samples=1;r.data_format=1;r.tile_mode=8;r.guest_address=0x100000;
          if(ctr_graphics_image_extent_v2(&r,&extent) || extent.pitch!=side || extent.padded_height!=side || extent.bytes!=uint64_t(side)*side)return 64;
          const auto keep=extent;r.tile_mode=14;
          if(ctr_graphics_image_extent_v2(&r,&extent)!=88 || memcmp(&extent,&keep,sizeof(keep)))return 64;
        }
        std::puts("expanded whole public linear256B/micro16384B/macro16bit4423680B sizing andunsupported88/bounded60 atomic; no GPU image admission");
    }
    {
        fixture.code.fill(0);fixture.code[0]=0xbeeb03ff;fixture.code[1]=3;fixture.code[2]=0xbf810000;
        std::memcpy(reinterpret_cast<char*>(fixture.code.data())+32,"OrbShdr",7);fixture.code[10]=12<<8;
        std::array<uint32_t,0xD000> pairFlat{};pairFlat[0x2c48]=0x10;pairFlat[0x2c08]=0x10;
        PairState pair;
        auto compilePair=[&](uint64_t vs,CtrCompiledGraphicsPairSinkV1 sink){
            return ctr_graphics_compile_pair_v1(&inputs,nullptr,0x1000,64,vs,64,pairFlat.data(),
                uint32_t(pairFlat.size()),&profile,sink,&pair,reason,sizeof(reason));};
        if(compilePair(0x1000,PairSink) || pair.calls!=1 || pair.vertex.size()<5)return 64;
        const auto savedPairVertex=pair.vertex;
        if(compilePair(0x2000,PairSink)==0 || pair.calls!=1 || pair.vertex!=savedPairVertex)return 65;
        if(compilePair(0x1000,nullptr)!=60 || pair.calls!=1)return 66;
        pair.reject=73;
        if(compilePair(0x1000,PairSink)!=73 || pair.calls!=2 || pair.vertex!=savedPairVertex)return 67;
        pair=PairState{};
        auto compilePair2=[&](uint64_t vs,CtrCompiledGraphicsPairSinkV2 sink){
            return ctr_graphics_compile_pair_v2(&inputs,nullptr,0x1000,64,vs,64,pairFlat.data(),
                uint32_t(pairFlat.size()),&profile,sink,&pair,reason,sizeof(reason));};
        if(compilePair2(0x1000,PairSinkV2) || pair.calls!=1 || pair.vertex.size()<5)return 76;
        if(compilePair2(0x2000,PairSinkV2)==0 || pair.calls!=1)return 77;
        if(compilePair2(0x1000,nullptr)!=60 || pair.calls!=1)return 78;
        pair.reject=73;if(compilePair2(0x1000,PairSinkV2)!=73 || pair.calls!=2)return 79;

        pair=PairState{};pairFlat[0x2c08]=0;
        auto vertexOnly=[&](uint64_t address,CtrCompiledGraphicsVertexOnlySinkV1 sink){
            return ctr_graphics_compile_vertex_only_v1(&inputs,nullptr,address,64,pairFlat.data(),
                uint32_t(pairFlat.size()),&profile,sink,&pair,reason,sizeof(reason));};
        if(vertexOnly(0x1000,VertexOnlySink) || pair.calls!=1 || pair.vertex.size()<5)return 80;
        const auto vertexOnlySaved=pair.vertex;
        if(vertexOnly(0x2000,VertexOnlySink)==0 || pair.calls!=1 || pair.vertex!=vertexOnlySaved)return 81;
        pairFlat[0x2c08]=0x10;
        if(vertexOnly(0x1000,VertexOnlySink)!=88 || pair.calls!=1)return 82;
        pairFlat[0x2c08]=0;pairFlat[0x2c09]=1;
        if(vertexOnly(0x1000,VertexOnlySink)!=88 || pair.calls!=1)return 83;
        pairFlat[0x2c09]=0;
        if(vertexOnly(0x1000,nullptr)!=60 || pair.calls!=1)return 84;
        pair.reject=73;if(vertexOnly(0x1000,VertexOnlySink)!=73 || pair.calls!=2)return 85;
        std::puts("vertex-only originalPSzero actual VS compiler sharedbindingorigin0; failedcompile atomic nonzeroPS88 null60 sink73; authored only");
        std::puts("paired v2 same-pass pipeline metadata MRT/sample/nofetch/signedSGPR checked; failedsecond atomic/null60/sink73; authored only");
        std::puts("paired PS-first/VS shared binding C17 ABI passed actual authored compilation, owned sink copy, failedsecondstage no delivery, null60 and sink73; GPUbinding=false retail=false");
    }
    std::puts("C++17 companion ABI: authored VS/PS actual compiler SPIRV, invalid bool/profile/identity60 beforecapture, malformed footer60 releases snapshot, output atomic; synthetic only");return 0;
}
