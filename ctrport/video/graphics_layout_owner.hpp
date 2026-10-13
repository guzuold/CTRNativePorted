// SPDX-License-Identifier: GPL-2.0-only
#pragma once
#include "graphics_companion.h"
#include "owned_buffer.hpp"
#include "sampled_image_owner.hpp"
#include <vulkan/vulkan.h>
#include <memory>
#include <vector>
#include <array>
// Public layout policy: vk_graphics_pipeline.cpp BuildDescSetLayout, PushData128.
// Ordinary descriptor sets; no push-descriptor extension or raw handle export.
struct CtrOwnedSampler {
 VkDevice device{};VkSampler sampler{};PFN_vkDestroySampler destroy{};
 ~CtrOwnedSampler(){if(sampler)destroy(device,sampler,nullptr);}
};
class CtrGraphicsLayoutOwner {
 bool compute_=false;
 VkDevice device_{};VkDescriptorSetLayout setLayout_{};VkPipelineLayout pipelineLayout_{};
 VkDescriptorPool pool_{};VkDescriptorSet set_{};
 PFN_vkDestroyDescriptorSetLayout destroySet_{};PFN_vkDestroyPipelineLayout destroyPipeline_{};PFN_vkDestroyDescriptorPool destroyPool_{};
 std::vector<CtrGraphicsDescriptorBindingV1> bufferBindings_;
 std::vector<std::shared_ptr<CtrOwnedBuffer>> retainedBuffers_;
 std::vector<CtrGraphicsDescriptorBindingV1> samplerBindings_;
 std::vector<std::shared_ptr<CtrOwnedSampler>> retainedSamplers_;
 std::vector<CtrGraphicsDescriptorBindingV1> imageBindings_;
 std::vector<std::shared_ptr<CtrSampledImageOwner>> retainedImages_;
 bool imagesWritten_=false;
 bool samplersWritten_=false;
 bool buffersWritten_=false;
public:
 bool ReadyBuffersOnly() const {return imageBindings_.empty() && samplerBindings_.empty() && (bufferBindings_.empty() || buffersWritten_);}
 int ReadyGraphics() const {
  if(compute_)return 60;
  return ReadyResources();
 }
 int ReadyCompute() const {
  if(!compute_)return 60;return ReadyResources();
 }
 int ReadyResources() const {
  if((!bufferBindings_.empty() && !buffersWritten_) || (!imageBindings_.empty() && !imagesWritten_) || (!samplerBindings_.empty() && !samplersWritten_))return 88;
  for(const auto& image:retainedImages_){if(!image)continue;VkDescriptorImageInfo info{};const int status=image->IsStorage()?image->StorageDescriptor(info):image->SampledDescriptor(info);if(status)return status;}
  return 0;
 }
 bool Empty() const {return bufferBindings_.empty() && imageBindings_.empty() && samplerBindings_.empty();}
 bool IsCompute() const {return compute_;}
 VkPipelineLayout PipelineLayout() const {return pipelineLayout_;}
 VkDescriptorSet DescriptorSet() const {return set_;}
 int WriteImages(PFN_vkGetDeviceProcAddr get,const uint32_t* bindings,uint32_t count,
  const std::vector<std::shared_ptr<CtrSampledImageOwner>>& owners,const uint32_t* nulls=nullptr,bool nullAuthority=false) {
  if(count!=imageBindings_.size() || owners.size()!=count || (count && !bindings))return 60;
  if(imagesWritten_)return 91;
  std::vector<VkDescriptorImageInfo> infos(count);std::vector<VkWriteDescriptorSet> writes(count);
  for(uint32_t i=0;i<count;++i){
   const auto& d=imageBindings_[i];if(bindings[i]!=d.binding)return 60;
   if(d.count!=1)return 88; // Storage/mip-fallback arrays need distinct real views/layout policy.
   if(nulls && nulls[i]>1)return 60;
   if(nulls && nulls[i]){if(owners[i])return 60;if(d.storage || !nullAuthority)return 88;infos[i].imageView=VK_NULL_HANDLE;infos[i].imageLayout=VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;}
   else {if(!owners[i])return 62;const int status=d.storage?owners[i]->StorageDescriptor(infos[i]):owners[i]->SampledDescriptor(infos[i]);if(status)return status;}
   writes[i].sType=VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;writes[i].dstSet=set_;writes[i].dstBinding=bindings[i];
   writes[i].descriptorCount=1;writes[i].descriptorType=d.storage?VK_DESCRIPTOR_TYPE_STORAGE_IMAGE:VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;writes[i].pImageInfo=&infos[i];
  }
  const auto update=reinterpret_cast<PFN_vkUpdateDescriptorSets>(get(device_,"vkUpdateDescriptorSets"));if(!update)return 88;
  auto retained=owners;update(device_,count,writes.data(),0,nullptr);
  retainedImages_=std::move(retained);imagesWritten_=true;return 0;
 }
 int WriteSamplers(PFN_vkGetDeviceProcAddr get,const uint32_t* bindings,uint32_t count,
  const std::vector<std::shared_ptr<CtrOwnedSampler>>& owners) {
  if(count!=samplerBindings_.size() || owners.size()!=count || (count && !bindings))return 60;
  if(samplersWritten_)return 91;
  std::vector<VkDescriptorImageInfo> infos(count);std::vector<VkWriteDescriptorSet> writes(count);
  for(uint32_t i=0;i<count;++i){
   if(bindings[i]!=samplerBindings_[i].binding)return 60;
   if(!owners[i])return 62;
   infos[i].sampler=owners[i]->sampler;
   writes[i].sType=VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;writes[i].dstSet=set_;writes[i].dstBinding=bindings[i];
   writes[i].descriptorCount=1;writes[i].descriptorType=VK_DESCRIPTOR_TYPE_SAMPLER;writes[i].pImageInfo=&infos[i];
  }
  const auto update=reinterpret_cast<PFN_vkUpdateDescriptorSets>(get(device_,"vkUpdateDescriptorSets"));if(!update)return 88;
  auto retained=owners;update(device_,count,writes.data(),0,nullptr);
  retainedSamplers_=std::move(retained);samplersWritten_=true;return 0;
 }
 int WriteBuffers(PFN_vkGetDeviceProcAddr get,bool nullAuthority,
  const CtrGraphicsBufferWriteV1* input,uint32_t count,
  const std::vector<std::shared_ptr<CtrOwnedBuffer>>& owners) {
  if(count!=bufferBindings_.size() || (count && !input) || owners.size()!=count)return 60;
  if(buffersWritten_)return 91;
  std::vector<VkDescriptorBufferInfo> infos(count);std::vector<VkWriteDescriptorSet> writes(count);
  for(uint32_t i=0;i<count;++i){
   const auto& d=bufferBindings_[i];const auto& w=input[i];
   if(w.binding!=d.binding || w.buffer_slot!=d.buffer_slot || w.storage!=d.storage || w.null_clip_planes>1)return 60;
   if(!d.storage)return 88; // No uniform-buffer owners currently admitted.
   if(w.null_clip_planes){if(w.buffer_token || owners[i])return 60;if(!nullAuthority)return 88;infos[i]={VK_NULL_HANDLE,0,VK_WHOLE_SIZE};}
   else {if(!w.buffer_token || !owners[i])return 62;const int status=owners[i]->StorageDescriptor(infos[i]);if(status)return status;}
   writes[i].sType=VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;writes[i].dstSet=set_;writes[i].dstBinding=d.binding;
   writes[i].descriptorCount=1;writes[i].descriptorType=VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;writes[i].pBufferInfo=&infos[i];
  }
  const auto update=reinterpret_cast<PFN_vkUpdateDescriptorSets>(get(device_,"vkUpdateDescriptorSets"));if(!update)return 88;
  auto retained=owners; // Allocate before mutation; references survive public token release.
  update(device_,count,writes.data(),0,nullptr);retainedBuffers_=std::move(retained);buffersWritten_=true;return 0;
 }
 ~CtrGraphicsLayoutOwner(){if(pool_)destroyPool_(device_,pool_,nullptr);if(pipelineLayout_)destroyPipeline_(device_,pipelineLayout_,nullptr);if(setLayout_)destroySet_(device_,setLayout_,nullptr);}
 static int Create(VkDevice device,PFN_vkGetDeviceProcAddr get,const VkPhysicalDeviceLimits& limits,
  const CtrGraphicsBindingContractV1& ps,const CtrGraphicsBindingContractV1& vs,std::unique_ptr<CtrGraphicsLayoutOwner>& output,bool fragmentPresent=true,bool compute=false) {
  if(!device || !get || (compute && fragmentPresent))return 60;
  if(ps.version!=1 || vs.version!=1 || ps.size!=sizeof(ps) || vs.size!=sizeof(vs))return 60;
  if(ps.unified_before || ps.buffer_before || ps.userdata_before ||
    vs.unified_before!=ps.unified_after || vs.buffer_before!=ps.buffer_after || vs.userdata_before!=ps.userdata_after)return 60;
  if(limits.maxPushConstantsSize<128)return 88;
  std::vector<VkDescriptorSetLayoutBinding> bindings;std::array<uint32_t,5> totals{};
  const std::array<VkDescriptorType,5> types={VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,VK_DESCRIPTOR_TYPE_SAMPLER};
  const auto append=[&](const CtrGraphicsBindingContractV1& c,VkShaderStageFlags stage) -> int {
   if(c.descriptor_count>120 || c.userdata_count>16 || c.userdata_before>16 || c.userdata_after>16 ||
      c.userdata_before+c.userdata_count!=c.userdata_after || c.buffer_before>48 || c.buffer_after>48 || c.unified_before>1024 || c.unified_after>1024)return 60;
   uint32_t next=c.unified_before,buffer=c.buffer_before;std::array<uint32_t,5> counts{};
   for(uint32_t i=0;i<c.descriptor_count;++i) {
    const auto& d=c.descriptors[i];if(!d.count || d.count>1024 || d.binding!=next || next>1024-d.count || d.storage>1)return 60;
    uint32_t type=0;
    if(d.kind==0){if(d.count!=1 || d.buffer_slot!=buffer++)return 60;type=d.storage?0:1;}
    else if(d.kind==1){if(d.buffer_slot!=UINT32_MAX)return 60;type=d.storage?2:3;}
    else if(d.kind==2){if(d.count!=1 || d.storage || d.buffer_slot!=UINT32_MAX)return 60;type=4;}
    else return 60;
    counts[type]+=d.count;totals[type]+=d.count;next+=d.count;
    bindings.push_back({d.binding,types[type],d.count,stage,nullptr});
   }
   if(next!=c.unified_after || buffer!=c.buffer_after)return 60;
   if(counts[0]>limits.maxPerStageDescriptorStorageBuffers || counts[1]>limits.maxPerStageDescriptorUniformBuffers ||
      counts[2]>limits.maxPerStageDescriptorStorageImages || counts[3]>limits.maxPerStageDescriptorSampledImages || counts[4]>limits.maxPerStageDescriptorSamplers ||
      counts[0]+counts[1]+counts[2]+counts[3]+counts[4]>limits.maxPerStageResources)return 88;
   return 0;
  };
  if(!fragmentPresent && (ps.descriptor_count || ps.unified_after || ps.buffer_after || ps.userdata_after))return 60;
  int status=fragmentPresent?append(ps,VK_SHADER_STAGE_FRAGMENT_BIT):0;if(status)return status;
  status=append(vs,compute?VK_SHADER_STAGE_COMPUTE_BIT:VK_SHADER_STAGE_VERTEX_BIT);if(status)return status;
  if(totals[0]>limits.maxDescriptorSetStorageBuffers || totals[1]>limits.maxDescriptorSetUniformBuffers ||
     totals[2]>limits.maxDescriptorSetStorageImages || totals[3]>limits.maxDescriptorSetSampledImages || totals[4]>limits.maxDescriptorSetSamplers)return 88;
#define LAYOUT_LOAD(name) const auto name=reinterpret_cast<PFN_##name>(get(device,#name));if(!name)return 88
  LAYOUT_LOAD(vkCreateDescriptorSetLayout);LAYOUT_LOAD(vkDestroyDescriptorSetLayout);
  LAYOUT_LOAD(vkCreatePipelineLayout);LAYOUT_LOAD(vkDestroyPipelineLayout);
  LAYOUT_LOAD(vkCreateDescriptorPool);LAYOUT_LOAD(vkDestroyDescriptorPool);LAYOUT_LOAD(vkAllocateDescriptorSets);
#undef LAYOUT_LOAD
  auto value=std::make_unique<CtrGraphicsLayoutOwner>();value->device_=device;
  for(const auto* c:{&ps,&vs})for(uint32_t i=0;i<c->descriptor_count;++i)
   if(c->descriptors[i].kind==0)value->bufferBindings_.push_back(c->descriptors[i]);
   else if(c->descriptors[i].kind==2)value->samplerBindings_.push_back(c->descriptors[i]);
   else if(c->descriptors[i].kind==1)value->imageBindings_.push_back(c->descriptors[i]);
  value->compute_=compute;value->destroySet_=vkDestroyDescriptorSetLayout;value->destroyPipeline_=vkDestroyPipelineLayout;value->destroyPool_=vkDestroyDescriptorPool;
  VkDescriptorSetLayoutCreateInfo ci{};ci.sType=VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
  ci.bindingCount=static_cast<uint32_t>(bindings.size());ci.pBindings=bindings.data();
  if(vkCreateDescriptorSetLayout(device,&ci,nullptr,&value->setLayout_)!=VK_SUCCESS)return 90;
  VkPushConstantRange push{VkShaderStageFlags(compute?VK_SHADER_STAGE_COMPUTE_BIT:fragmentPresent?VK_SHADER_STAGE_ALL_GRAPHICS:VK_SHADER_STAGE_VERTEX_BIT),0,128};
  VkPipelineLayoutCreateInfo pi{};pi.sType=VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
  pi.setLayoutCount=1;pi.pSetLayouts=&value->setLayout_;pi.pushConstantRangeCount=1;pi.pPushConstantRanges=&push;
  if(vkCreatePipelineLayout(device,&pi,nullptr,&value->pipelineLayout_)!=VK_SUCCESS)return 90;
  std::vector<VkDescriptorPoolSize> sizes;for(uint32_t i=0;i<5;++i)if(totals[i])sizes.push_back({types[i],totals[i]});
  VkDescriptorPoolCreateInfo poolInfo{};poolInfo.sType=VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
  poolInfo.maxSets=1;poolInfo.poolSizeCount=static_cast<uint32_t>(sizes.size());poolInfo.pPoolSizes=sizes.data();
  if(vkCreateDescriptorPool(device,&poolInfo,nullptr,&value->pool_)!=VK_SUCCESS)return 90;
  VkDescriptorSetAllocateInfo ai{};ai.sType=VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
  ai.descriptorPool=value->pool_;ai.descriptorSetCount=1;ai.pSetLayouts=&value->setLayout_;
  if(vkAllocateDescriptorSets(device,&ai,&value->set_)!=VK_SUCCESS)return 90;
  output=std::move(value);return 0;
 }
};
