// SPDX-License-Identifier: GPL-2.0-only
#pragma once
#include "graphics_layout_owner.hpp"
#include "color_attachment_owner.hpp"
#include "depth_attachment_owner.hpp"
#include <cstdint>
#include <memory>
#include "transfer_phase_timing.hpp"
// Internal synchronous allocation. The caller supplies validated public pipeline state;
// no raw Vulkan handle crosses the session C ABI. Shader modules need not outlive
// vkCreateGraphicsPipelines; the layout (and its descriptors/resources) does.
class CtrGraphicsPipelineOwner {
 VkDevice device_{};VkPipeline pipeline_{};PFN_vkDestroyPipeline destroy_{};
 std::shared_ptr<CtrGraphicsLayoutOwner> layout_;
public:
 bool noAttachments=false,failed=false;
 std::shared_ptr<CtrGraphicsLayoutOwner> Layout() const {return layout_;}
 CtrGraphicsVertexInputsV1 vertices{};std::array<uint32_t,32> formats{};uint64_t colorToken=0,depthToken=0;
 std::array<std::shared_ptr<CtrColorAttachmentOwner>,2> mrtOwners{};
 std::array<uint64_t,2> mrtTokens{};
 std::shared_ptr<CtrDepthAttachmentOwner> mrtDepthOwner;
 bool colorMs2WithoutDepth=false,inactiveSecondColor=false;
 VkPipeline Pipeline() const {return pipeline_;}
 ~CtrGraphicsPipelineOwner(){if(pipeline_)destroy_(device_,pipeline_,nullptr);}
 static int Create(VkDevice device,PFN_vkGetDeviceProcAddr get,
   std::shared_ptr<CtrGraphicsLayoutOwner> layout,const VkGraphicsPipelineCreateInfo& state,
   const uint32_t* vertex,uint32_t vertexWords,const uint32_t* fragment,uint32_t fragmentWords,
   std::unique_ptr<CtrGraphicsPipelineOwner>& output) {
  const uint32_t* codes[]={vertex,fragment};const uint32_t sizes[]={vertexWords,fragmentWords};
  const VkShaderStageFlagBits kinds[]={VK_SHADER_STAGE_VERTEX_BIT,VK_SHADER_STAGE_FRAGMENT_BIT};
  return CreateStages(device,get,std::move(layout),state,codes,sizes,kinds,2,output);
 }
 // VS + public RectListTCS + public PassthroughTES; no fragment module is synthesized.
 static int CreateRectListVertexOnly(VkDevice device,PFN_vkGetDeviceProcAddr get,
   std::shared_ptr<CtrGraphicsLayoutOwner> layout,const VkGraphicsPipelineCreateInfo& state,
   const uint32_t* vertex,uint32_t vertexWords,const uint32_t* control,uint32_t controlWords,
   const uint32_t* evaluation,uint32_t evaluationWords,std::unique_ptr<CtrGraphicsPipelineOwner>& output){
  const uint32_t* codes[]={vertex,control,evaluation};const uint32_t sizes[]={vertexWords,controlWords,evaluationWords};
  const VkShaderStageFlagBits kinds[]={VK_SHADER_STAGE_VERTEX_BIT,VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT,VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT};
  if(!state.pTessellationState || state.pTessellationState->patchControlPoints!=3 ||
     !state.pInputAssemblyState || state.pInputAssemblyState->topology!=VK_PRIMITIVE_TOPOLOGY_PATCH_LIST)return 60;
  return CreateStages(device,get,std::move(layout),state,codes,sizes,kinds,3,output);
 }
private:
 static int CreateStages(VkDevice device,PFN_vkGetDeviceProcAddr get,
   std::shared_ptr<CtrGraphicsLayoutOwner> layout,const VkGraphicsPipelineCreateInfo& state,
   const uint32_t* const* codes,const uint32_t* sizes,const VkShaderStageFlagBits* kinds,uint32_t count,
   std::unique_ptr<CtrGraphicsPipelineOwner>& output){
  if(!device || !get || !layout || layout->IsCompute() || !layout->PipelineLayout() ||
     state.sType!=VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO || state.stageCount || state.pStages ||
     state.layout || state.basePipelineHandle || state.basePipelineIndex!=-1)return 60;
  for(uint32_t i=0;i<count;++i)if(!codes[i] || sizes[i]<5 || sizes[i]>4194304 || codes[i][0]!=0x07230203)return 60;
  const auto createShader=reinterpret_cast<PFN_vkCreateShaderModule>(get(device,"vkCreateShaderModule"));
  const auto destroyShader=reinterpret_cast<PFN_vkDestroyShaderModule>(get(device,"vkDestroyShaderModule"));
  const auto createPipeline=reinterpret_cast<PFN_vkCreateGraphicsPipelines>(get(device,"vkCreateGraphicsPipelines"));
  const auto destroyPipeline=reinterpret_cast<PFN_vkDestroyPipeline>(get(device,"vkDestroyPipeline"));
  if(!createShader || !destroyShader || !createPipeline || !destroyPipeline)return 88;
  CtrTransferPhaseTiming timing("graphics_pipeline",count);timing.phase="shader_modules";
  struct Modules {
   VkDevice device;PFN_vkDestroyShaderModule destroy;VkShaderModule values[3]{};
   ~Modules(){for(auto module:values)if(module)destroy(device,module,nullptr);}
  } modules{device,destroyShader,{}};
  for(uint32_t i=0;i<count;++i){VkShaderModuleCreateInfo ci{};ci.sType=VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
   ci.codeSize=size_t(sizes[i])*sizeof(uint32_t);ci.pCode=codes[i];
   if(createShader(device,&ci,nullptr,&modules.values[i])!=VK_SUCCESS)return 90;
  }
  VkPipelineShaderStageCreateInfo stages[3]{};
  for(uint32_t i=0;i<count;++i){stages[i].sType=VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
   stages[i].stage=kinds[i];
   stages[i].module=modules.values[i];stages[i].pName="main";}
  auto value=std::make_unique<CtrGraphicsPipelineOwner>();
  value->device_=device;value->destroy_=destroyPipeline;value->layout_=std::move(layout);
  auto ci=state;ci.stageCount=count;ci.pStages=stages;ci.layout=value->layout_->PipelineLayout();
  // Vulkan may return an error with a non-null pipeline. RAII cleans that result too.
  timing.Next("pipeline_create");
  if(createPipeline(device,VK_NULL_HANDLE,1,&ci,nullptr,&value->pipeline_)!=VK_SUCCESS)return 90;
  timing.status=0;timing.Next("publish");output=std::move(value);return 0;
 }
};
