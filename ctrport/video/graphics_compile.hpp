// SPDX-License-Identifier: GPL-2.0-only
#pragma once
#include "graphics_shader_state.hpp"
#include "shader_recompiler/frontend/fetch_shader.h"
#include "graphics_resource_metadata.hpp"
#include "graphics_binding_metadata.hpp"
#include "shader_code_capture.hpp"
#include "shader_recompiler/recompiler.h"
#include "shader_recompiler/profile.h"
#include "shader_recompiler/backend/spirv/emit_spirv.h"
#include <string>
#include "srt_missing_diagnostic.hpp"
namespace CtrPm4 {
// Synchronous host diagnostic compiler; never queues GPU work. Caller supplies
// observed stage-specific RuntimeInfo and enabled-device Profile, not defaults.
// Optional shared counters are committed only after whole stage compilation and
// metadata capture succeed. No successful placeholder resource binding.
inline int CompileGraphicsDiagnostic(const OwnedShaderBinary& binary,
    const GraphicsShaderState& state,bool vertex,Shader::RuntimeInfo runtime,
    const Shader::Profile& profile,std::vector<uint32_t>& output,std::string& diagnostic,
    CtrGraphicsResourcesV1* resources=nullptr,std::vector<uint32_t>* flattened=nullptr,
    Shader::Backend::Bindings* sharedBindings=nullptr,GraphicsBindingMetadata* bindingMetadata=nullptr,CtrGraphicsPipelineStageV1* pipelineMetadata=nullptr,bool compute=false) {
    const auto stage=compute ? Shader::Stage::Compute : (vertex ? Shader::Stage::Vertex : Shader::Stage::Fragment);
    if(runtime.stage!=stage || binary.guestAddress!=state.codeAddress ||
       state.userRegisters>16 || !binary.CodeWords() || binary.CodeWords()>binary.words.size()) {
        diagnostic="Invalid graphics compiler input identity, extent or stage";return 60;
    }
    runtime.num_user_data=state.userRegisters;runtime.num_input_vgprs=state.inputVgprs;
    runtime.num_allocated_vgprs=state.allocatedVgprs;
    runtime.fp_denorm_mode32=static_cast<AmdGpu::FpDenormMode>(state.fpDenorm32);
    runtime.fp_denorm_mode16_64=static_cast<AmdGpu::FpDenormMode>(state.fpDenorm64);
    runtime.fp_round_mode32=static_cast<AmdGpu::FpRoundMode>(state.fpRound32);
    runtime.fp_round_mode16_64=static_cast<AmdGpu::FpRoundMode>(state.fpRound64);
    try {
        Shader::Info info{};info.stage=stage;info.pgm_base=binary.guestAddress;
        info.l_stage=compute ? Shader::LogicalStage::Compute : (vertex ? Shader::LogicalStage::Vertex : Shader::LogicalStage::Fragment);
        info.user_data=state.userData;
        Shader::Pools pools;
        auto program=Shader::TranslateProgram(std::span<const u32>(binary.Code(),binary.CodeWords()),
            pools,info,runtime,profile);
        auto bindings=sharedBindings ? *sharedBindings : Shader::Backend::Bindings{};
        const auto before=bindings;
        if(before.user_data>16 || info.ud_mask.NumRegs()>16-before.user_data ||
           before.buffer>48 || info.buffers.size()>48-before.buffer) {
            diagnostic="Graphics shared push userdata/buffer slot capacity exceeded";return 88;
        }
        auto prepared=Shader::Backend::SPIRV::EmitSPIRV(profile,runtime,program,bindings);
        if(prepared.size()<5 || prepared[0]!=0x07230203U) {
            diagnostic="Compiler returned invalid SPIRV";return 88;
        }
        const auto metadata=CaptureCompilerResourceMetadata(info);
        GraphicsBindingMetadata bindingContract;
        if(bindingMetadata)bindingContract=CaptureGraphicsBindingMetadata(info,before,bindings);
        CtrGraphicsPipelineStageV1 stageMetadata{};
        if(pipelineMetadata){
            stageMetadata.version=1;stageMetadata.size=sizeof(stageMetadata);stageMetadata.mrt_mask=info.mrt_mask;
            stageMetadata.vertex_offset_sgpr=stageMetadata.instance_offset_sgpr=-1;
            if(vertex){
                stageMetadata.step_rate0=runtime.vs_info.step_rate_0;stageMetadata.step_rate1=runtime.vs_info.step_rate_1;
                stageMetadata.has_fetch=info.has_fetch_shader?1:0;
                if(info.has_fetch_shader){
                    const auto fetch=Shader::Gcn::ParseFetchShader(info);
                    if(!fetch || fetch->attributes.size()>32){diagnostic="Missing or oversized public fetch pipeline metadata";return 88;}
                    stageMetadata.attribute_count=static_cast<uint32_t>(fetch->attributes.size());
                    stageMetadata.vertex_offset_sgpr=fetch->vertex_offset_sgpr;stageMetadata.instance_offset_sgpr=fetch->instance_offset_sgpr;
                    for(size_t i=0;i<fetch->attributes.size();++i){const auto& a=fetch->attributes[i];
                        stageMetadata.attributes[i]={a.semantic,a.dest_vgpr,a.num_elements,a.sgpr_base,a.dword_offset,a.instance_data,a.inst_offset,a.data_format,a.num_format};}
                }
            }else{stageMetadata.persp_sample=runtime.fs_info.addr_flags.persp_sample_ena;stageMetadata.linear_sample=runtime.fs_info.addr_flags.linear_sample_ena;}
        }
        if(flattened)*flattened=info.flattened_ud_buf;
        output=std::move(prepared);if(resources)*resources=metadata;
        if(bindingMetadata)*bindingMetadata=std::move(bindingContract);
        if(pipelineMetadata)*pipelineMetadata=stageMetadata;
        if(sharedBindings)*sharedBindings=bindings;
        diagnostic.clear();return 0;
    } catch(const SrtMissingDiagnostic& error) {diagnostic=error.Describe();return 88;}
    catch(const std::exception& error) {diagnostic=error.what();return 88;}
}
}
