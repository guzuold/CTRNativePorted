// SPDX-License-Identifier: GPL-2.0-only
#pragma once
#include "graphics_layout_owner.hpp"
#include "compute_dispatch_submit.hpp"
// Session-owned real compute pipeline; retains descriptor layout. No dispatch authority.
class CtrComputePipelineOwner {
 bool failed_=false;
 VkDevice device_{};VkPipeline pipeline_{};PFN_vkDestroyPipeline destroy_{};
 std::shared_ptr<CtrGraphicsLayoutOwner> layout_;
public:
 int Dispatch(PFN_vkGetDeviceProcAddr get,uint32_t family,const void* push,const uint32_t* groups){
  if(failed_)return 91;
  const int ready=layout_->ReadyCompute();if(ready)return ready;
  const int status=ctr_compute_dispatch_submit(device_,get,family,pipeline_,layout_->PipelineLayout(),layout_->DescriptorSet(),push,groups);
  if(status)failed_=true;return status;
 }
 ~CtrComputePipelineOwner(){if(pipeline_)destroy_(device_,pipeline_,nullptr);}
 static int Create(VkDevice device,PFN_vkGetDeviceProcAddr get,std::shared_ptr<CtrGraphicsLayoutOwner> layout,
  const uint32_t* code,uint32_t words,std::unique_ptr<CtrComputePipelineOwner>& output){
  if(!device || !get || !layout || !layout->IsCompute() || !code || words<5 || words>4194304 || code[0]!=0x07230203)return 60;
  bool entry=false;
  for(uint32_t i=5;i<words;){const auto n=code[i]>>16;if(!n || n>words-i)return 60;
   if((code[i]&65535)==15){if(n<4 || code[i+1]!=5)return 60;entry=true;}i+=n;}
  if(!entry)return 60;
  const auto createShader=reinterpret_cast<PFN_vkCreateShaderModule>(get(device,"vkCreateShaderModule"));
  const auto destroyShader=reinterpret_cast<PFN_vkDestroyShaderModule>(get(device,"vkDestroyShaderModule"));
  const auto createPipeline=reinterpret_cast<PFN_vkCreateComputePipelines>(get(device,"vkCreateComputePipelines"));
  const auto destroyPipeline=reinterpret_cast<PFN_vkDestroyPipeline>(get(device,"vkDestroyPipeline"));
  if(!createShader || !destroyShader || !createPipeline || !destroyPipeline)return 88;
  struct Module {VkDevice device;PFN_vkDestroyShaderModule destroy;VkShaderModule value{};
   ~Module(){if(value)destroy(device,value,nullptr);}} module{device,destroyShader,{}};
  VkShaderModuleCreateInfo sm{};sm.sType=VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;sm.codeSize=size_t(words)*4;sm.pCode=code;
  if(createShader(device,&sm,nullptr,&module.value)!=VK_SUCCESS)return 90;
  auto value=std::make_unique<CtrComputePipelineOwner>();value->device_=device;value->destroy_=destroyPipeline;value->layout_=std::move(layout);
  VkComputePipelineCreateInfo ci{};ci.sType=VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;ci.layout=value->layout_->PipelineLayout();ci.basePipelineIndex=-1;
  ci.stage.sType=VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;ci.stage.stage=VK_SHADER_STAGE_COMPUTE_BIT;ci.stage.module=module.value;ci.stage.pName="main";
  if(createPipeline(device,VK_NULL_HANDLE,1,&ci,nullptr,&value->pipeline_)!=VK_SUCCESS)return 90;
  output=std::move(value);return 0;
 }
};
