// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "graphics_pipeline_state.hpp"
#include "graphics_pipeline_owner.hpp"
// Whole public BlitHelper::CreateMsCopyPipeline policy, pinned
// e3ce810f3a653f43ac64ebab63023de281a4103a. Auxiliary shader pipeline,
// never a modified guest graphics key. Source shader variant selected by caller.
inline int CreateMsBackingCopyPipeline(VkDevice device,PFN_vkGetDeviceProcAddr get,
 const VkPhysicalDeviceLimits& limits,VkFormat format,uint32_t destinationSamples,
 const uint32_t* vertex,uint32_t vertexWords,const uint32_t* fragment,uint32_t fragmentWords,
 uint32_t core,std::unique_ptr<CtrGraphicsPipelineOwner>& output){
 if(destinationSamples!=1 && destinationSamples!=2)return 88;
 if(format!=VK_FORMAT_R8G8B8A8_UNORM && format!=VK_FORMAT_R8G8_UNORM)return 88;
 CtrGraphicsBindingContractV1 ps{},vs{};ps.version=vs.version=1;ps.size=sizeof(ps);vs.size=sizeof(vs);
 ps.descriptor_count=1;ps.unified_after=1;ps.descriptors[0]={1,0,0,1,UINT32_MAX,0};
 vs.unified_before=vs.unified_after=1;
 std::unique_ptr<CtrGraphicsLayoutOwner> ownedLayout;
 int rc=CtrGraphicsLayoutOwner::Create(device,get,limits,ps,vs,ownedLayout);if(rc)return rc;
 std::shared_ptr<CtrGraphicsLayoutOwner> layout=std::move(ownedLayout);
 CtrGraphicsKeyV1 auxiliary{};auxiliary.version=1;auxiliary.size=sizeof(auxiliary);
 auxiliary.primitive_type=4;auxiliary.polygon_mode=2;auxiliary.logic_op=0xcc;
 auxiliary.mrt_mask=auxiliary.num_color_attachments=auxiliary.num_samples=1;
 auxiliary.cb_shader_mask=15;auxiliary.colors[0].samples=1;auxiliary.colors[0].write_mask=15;
 CtrGraphicsVertexInputsV1 inputs{};inputs.version=1;inputs.size=sizeof(inputs);
 CtrGraphicsPipelineState state;rc=state.Prepare(auxiliary,inputs,nullptr,format,core,0);if(rc)return rc;
 // Public auxiliary multisampling is exactly destination backing samples;
 // no guest key/shader state is changed or admitted here.
 state.samples.rasterizationSamples=static_cast<VkSampleCountFlagBits>(destinationSamples);
 return CtrGraphicsPipelineOwner::Create(device,get,std::move(layout),state.pipeline,
     vertex,vertexWords,fragment,fragmentWords,output);
}

// Synchronous public full-screen backing copy; source restored to COLOR layout.
// Both owners stay alive through real fence completion or failed-submit drain.
inline int CopyMsBacking(VkDevice device,PFN_vkGetDeviceProcAddr get,uint32_t family,
 CtrGraphicsPipelineOwner& pipeline,CtrColorAttachmentOwner& source,CtrColorAttachmentOwner& destination){
 if(source.Failed() || destination.Failed() || source.Layout()!=VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL)return 91;
 if(source.Format()!=destination.Format() || source.Extent().width!=destination.Extent().width || source.Extent().height!=destination.Extent().height)return 88;
 const auto update=reinterpret_cast<PFN_vkUpdateDescriptorSets>(get(device,"vkUpdateDescriptorSets"));if(!update)return 88;
 const auto layout=pipeline.Layout();VkDescriptorImageInfo info{VK_NULL_HANDLE,source.View(),VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL};
 VkWriteDescriptorSet write{VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};write.dstSet=layout->DescriptorSet();write.dstBinding=0;
 write.descriptorCount=1;write.descriptorType=VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;write.pImageInfo=&info;update(device,1,&write,0,nullptr);
 const auto extent=source.Extent();VkViewport viewport{0,0,float(extent.width),float(extent.height),0,1};VkRect2D scissor{{0,0},extent};
 const float blend[]={1,1,1,1};unsigned char push[128]{};bool completed=false;
 const int rc=DrawGraphicsSynchronous(device,get,family,pipeline.Pipeline(),layout->PipelineLayout(),layout->DescriptorSet(),
  destination.Image(),destination.View(),extent,viewport,scissor,blend,VK_CULL_MODE_NONE,VK_FRONT_FACE_COUNTER_CLOCKWISE,
  push,3,1,0,0,completed,0,nullptr,nullptr,nullptr,VK_NULL_HANDLE,VK_NULL_HANDLE,nullptr,nullptr,source.Image(),destination.Layout());
 source.CopyCompleted(rc);destination.CopyCompleted(rc);return rc;
}
