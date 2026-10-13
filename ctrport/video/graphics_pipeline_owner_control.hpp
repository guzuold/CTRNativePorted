// SPDX-License-Identifier: GPL-2.0-only
#pragma once
#include "graphics_pipeline_owner.hpp"
#include "compute_pipeline_owner.hpp"
#include <cstring>
namespace CtrPipelineOwnerControl {
// Allocation-failure injection only. These callbacks never implement guest services
// and never submit GPU work. Real-device pipeline creation is separately pending.
inline uint32_t shaderCalls=0,shaderDestroyed=0,pipelineDestroyed=0,fail=0,expectedStages=2;
inline VKAPI_ATTR VkResult VKAPI_CALL Shader(VkDevice,const VkShaderModuleCreateInfo* ci,const VkAllocationCallbacks*,VkShaderModule* out){
 ++shaderCalls;if(!ci || ci->codeSize!=20 || ci->pCode[0]!=0x07230203)return VK_ERROR_UNKNOWN;
 if(fail==shaderCalls)return VK_ERROR_OUT_OF_HOST_MEMORY;
 *out=reinterpret_cast<VkShaderModule>(uintptr_t(shaderCalls));return VK_SUCCESS;
}
inline VKAPI_ATTR void VKAPI_CALL DestroyShader(VkDevice,VkShaderModule,const VkAllocationCallbacks*){++shaderDestroyed;}
inline VKAPI_ATTR void VKAPI_CALL DestroyPipeline(VkDevice,VkPipeline,const VkAllocationCallbacks*){++pipelineDestroyed;}
inline VKAPI_ATTR VkResult VKAPI_CALL Pipeline(VkDevice,VkPipelineCache,uint32_t count,const VkGraphicsPipelineCreateInfo* ci,const VkAllocationCallbacks*,VkPipeline* out){
 if(count!=1 || ci->stageCount!=expectedStages || !ci->layout || ci->pStages[0].stage!=VK_SHADER_STAGE_VERTEX_BIT ||
    std::strcmp(ci->pStages[0].pName,"main"))return VK_ERROR_UNKNOWN;
 if(expectedStages==2 && ci->pStages[1].stage!=VK_SHADER_STAGE_FRAGMENT_BIT)return VK_ERROR_UNKNOWN;
 if(expectedStages==3 && (ci->pStages[1].stage!=VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT ||
    ci->pStages[2].stage!=VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT || !ci->pTessellationState ||
    ci->pTessellationState->patchControlPoints!=3))return VK_ERROR_UNKNOWN;
 *out=reinterpret_cast<VkPipeline>(uintptr_t(3));return fail==expectedStages+1?VK_ERROR_OUT_OF_HOST_MEMORY:VK_SUCCESS;
}
inline VKAPI_ATTR PFN_vkVoidFunction VKAPI_CALL Get(VkDevice,const char* name){
 if(!std::strcmp(name,"vkCreateShaderModule"))return reinterpret_cast<PFN_vkVoidFunction>(Shader);
 if(!std::strcmp(name,"vkDestroyShaderModule"))return reinterpret_cast<PFN_vkVoidFunction>(DestroyShader);
 if(!std::strcmp(name,"vkCreateGraphicsPipelines"))return reinterpret_cast<PFN_vkVoidFunction>(Pipeline);
 if(!std::strcmp(name,"vkDestroyPipeline"))return reinterpret_cast<PFN_vkVoidFunction>(DestroyPipeline);
 return nullptr;
}
inline bool Run(VkDevice device,const std::shared_ptr<CtrGraphicsLayoutOwner>& layout){
 const uint32_t code[]={0x07230203,0x10000,0,1,0}; // Header only: injected callbacks, not executable SPIRV.
 VkGraphicsPipelineCreateInfo ci{};ci.sType=VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;ci.basePipelineIndex=-1;
 for(uint32_t step=0;step<4;++step){fail=step;shaderCalls=shaderDestroyed=pipelineDestroyed=0;
  std::unique_ptr<CtrGraphicsPipelineOwner> owner;
  const int rc=CtrGraphicsPipelineOwner::Create(device,Get,layout,ci,code,5,code,5,owner);
  if(step==0){if(rc || !owner || !owner->Pipeline() || shaderDestroyed!=2)return false;owner.reset();if(pipelineDestroyed!=1)return false;}
  else if(rc!=90 || owner || shaderDestroyed!=(step==1?0u:step==2?1u:2u) || pipelineDestroyed!=(step==3?1u:0u))return false;
 }
 std::unique_ptr<CtrGraphicsPipelineOwner> untouched;shaderCalls=0;
 if(CtrGraphicsPipelineOwner::Create(device,Get,layout,ci,code,4,code,5,untouched)!=60 || untouched || shaderCalls)return false;
 expectedStages=3;
 VkPipelineTessellationStateCreateInfo tess{};tess.sType=VK_STRUCTURE_TYPE_PIPELINE_TESSELLATION_STATE_CREATE_INFO;tess.patchControlPoints=3;
 VkPipelineInputAssemblyStateCreateInfo assembly{};assembly.sType=VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;assembly.topology=VK_PRIMITIVE_TOPOLOGY_PATCH_LIST;
 ci.pTessellationState=&tess;ci.pInputAssemblyState=&assembly;
 for(uint32_t step=0;step<5;++step){fail=step;shaderCalls=shaderDestroyed=pipelineDestroyed=0;
  std::unique_ptr<CtrGraphicsPipelineOwner> owner;
  const int rc=CtrGraphicsPipelineOwner::CreateRectListVertexOnly(device,Get,layout,ci,code,5,code,5,code,5,owner);
  if(!step){if(rc || !owner || shaderDestroyed!=3)return false;owner.reset();if(pipelineDestroyed!=1)return false;}
  else if(rc!=90 || owner || shaderDestroyed!=(step==4?3u:step-1) || pipelineDestroyed!=(step==4?1u:0u))return false;
 }
 shaderCalls=0;tess.patchControlPoints=4;
 if(CtrGraphicsPipelineOwner::CreateRectListVertexOnly(device,Get,layout,ci,code,5,code,5,code,5,untouched)!=60 || untouched || shaderCalls)return false;
 tess.patchControlPoints=3;
 if(CtrGraphicsPipelineOwner::CreateRectListVertexOnly(device,Get,layout,ci,code,5,code,5,code,4,untouched)!=60 || untouched || shaderCalls)return false;
 expectedStages=2;
 return true;
}
// Synthetic Vulkan allocation failures for the whole compute owner. No GPU dispatch.
inline uint32_t computeFail=0,computeShaderDestroyed=0,computePipelineDestroyed=0,computeShaderCalls=0;
inline VKAPI_ATTR VkResult VKAPI_CALL ComputeShader(VkDevice,const VkShaderModuleCreateInfo* ci,const VkAllocationCallbacks*,VkShaderModule* out){
 ++computeShaderCalls;if(!ci || ci->codeSize!=36 || ci->pCode[0]!=0x07230203)return VK_ERROR_UNKNOWN;
 if(computeFail==1)return VK_ERROR_OUT_OF_HOST_MEMORY;
 *out=reinterpret_cast<VkShaderModule>(uintptr_t(1));return computeFail==2?VK_ERROR_OUT_OF_HOST_MEMORY:VK_SUCCESS;
}
inline VKAPI_ATTR void VKAPI_CALL ComputeDestroyShader(VkDevice,VkShaderModule,const VkAllocationCallbacks*){++computeShaderDestroyed;}
inline VKAPI_ATTR void VKAPI_CALL ComputeDestroyPipeline(VkDevice,VkPipeline,const VkAllocationCallbacks*){++computePipelineDestroyed;}
inline VKAPI_ATTR VkResult VKAPI_CALL ComputePipeline(VkDevice,VkPipelineCache,uint32_t count,const VkComputePipelineCreateInfo* ci,const VkAllocationCallbacks*,VkPipeline* out){
 if(count!=1 || !ci || ci->sType!=VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO || !ci->layout || ci->stage.stage!=VK_SHADER_STAGE_COMPUTE_BIT ||
    !ci->stage.module || std::strcmp(ci->stage.pName,"main") || ci->basePipelineIndex!=-1)return VK_ERROR_UNKNOWN;
 if(computeFail==3)return VK_ERROR_OUT_OF_HOST_MEMORY;
 *out=reinterpret_cast<VkPipeline>(uintptr_t(2));return computeFail==4?VK_ERROR_OUT_OF_HOST_MEMORY:VK_SUCCESS;
}
inline VKAPI_ATTR PFN_vkVoidFunction VKAPI_CALL ComputeGet(VkDevice,const char* name){
 if(computeFail==5)return nullptr;
 if(!std::strcmp(name,"vkCreateShaderModule"))return reinterpret_cast<PFN_vkVoidFunction>(ComputeShader);
 if(!std::strcmp(name,"vkDestroyShaderModule"))return reinterpret_cast<PFN_vkVoidFunction>(ComputeDestroyShader);
 if(!std::strcmp(name,"vkCreateComputePipelines"))return reinterpret_cast<PFN_vkVoidFunction>(ComputePipeline);
 if(!std::strcmp(name,"vkDestroyPipeline"))return reinterpret_cast<PFN_vkVoidFunction>(ComputeDestroyPipeline);return nullptr;
}
inline bool RunCompute(VkDevice device,const std::shared_ptr<CtrGraphicsLayoutOwner>& layout){
 // Header + GLCompute entry marker only: fake allocation callbacks, never a GPU executable.
 const uint32_t code[]={0x07230203,0x10000,0,1,0,(4u<<16)|15,5,1,0};
 for(uint32_t step=0;step<=5;++step){computeFail=step;computeShaderDestroyed=computePipelineDestroyed=computeShaderCalls=0;
  std::unique_ptr<CtrComputePipelineOwner> owner;const int rc=CtrComputePipelineOwner::Create(device,ComputeGet,layout,code,9,owner);
  if(!step){if(rc || !owner || computeShaderDestroyed!=1)return false;owner.reset();if(computePipelineDestroyed!=1)return false;}
  else if(rc!=(step==5?88:90) || owner || computeShaderDestroyed!=((step>=2 && step<=4)?1u:0u) || computePipelineDestroyed!=(step==4?1u:0u))return false;
 }
 computeFail=0;computeShaderCalls=0;std::unique_ptr<CtrComputePipelineOwner> owner;
 if(CtrComputePipelineOwner::Create(device,ComputeGet,layout,code,8,owner)!=60 || owner || computeShaderCalls)return false;
 return true;
}

}
