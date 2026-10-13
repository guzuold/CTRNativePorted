// SPDX-License-Identifier: GPL-2.0-only
#pragma once
#include "transfer_phase_timing.hpp"
#include <vulkan/vulkan.h>
#include "graphics_companion.h"
// Internal handles stay owned by caller until this synchronous fence has completed.
inline int DrawGraphicsSynchronous(VkDevice device,PFN_vkGetDeviceProcAddr get,uint32_t family,
 VkPipeline pipeline,VkPipelineLayout layout,VkDescriptorSet descriptors,VkImage colorImage,VkImageView color,VkExtent2D extent,
 const VkViewport& viewport,const VkRect2D& scissor,const float* blend,uint32_t cull,uint32_t front,
 const void* push,uint32_t count,uint32_t instances,uint32_t firstVertex,uint32_t firstInstance,bool& completed,uint32_t vertexCount=0,const uint32_t* bindings=nullptr,const VkBuffer* buffers=nullptr,const VkDeviceSize* strides=nullptr,VkImage depthImage=VK_NULL_HANDLE,VkImageView depthView=VK_NULL_HANDLE,const CtrGraphicsDynamicStateV1* depthState=nullptr,uint64_t* visibility=nullptr,VkImage copySource=VK_NULL_HANDLE,VkImageLayout copyDestinationOld=VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,VkImage secondColorImage=VK_NULL_HANDLE,VkImageView secondColorView=VK_NULL_HANDLE,VkImageAspectFlags depthAspects=VK_IMAGE_ASPECT_DEPTH_BIT|VK_IMAGE_ASPECT_STENCIL_BIT,bool noAttachments=false,VkBuffer indexBuffer=VK_NULL_HANDLE,VkIndexType indexType=VK_INDEX_TYPE_UINT16,VkDeviceSize indexOffset=0,uint32_t firstIndex=0,int32_t vertexOffset=0,bool inactiveSecondColor=false){
 completed=false;
 if(indexBuffer && ((indexType!=VK_INDEX_TYPE_UINT16 && indexType!=VK_INDEX_TYPE_UINT32) || indexOffset%(indexType==VK_INDEX_TYPE_UINT16?2u:4u)))return 60;
 const bool hasDepth=depthState!=nullptr;
 const bool depthOnly=hasDepth && !colorImage;
 if(secondColorImage && (!secondColorView || depthOnly || copySource))return 60;
 if(inactiveSecondColor && (secondColorImage || secondColorView || hasDepth || noAttachments || copySource))return 88;
 if(copySource && (depthOnly || copySource==colorImage || visibility))return 60;
 if(!device || !get || !pipeline || !layout || (!depthOnly && !noAttachments && (!colorImage || !color)) ||
    (hasDepth && (!depthImage || !depthView)) || !extent.width || !extent.height || !push || !blend)return 60;
 const auto stencil=[](uint32_t op){const VkStencilOp map[]={VK_STENCIL_OP_KEEP,VK_STENCIL_OP_ZERO,VK_STENCIL_OP_REPLACE,VK_STENCIL_OP_REPLACE,VK_STENCIL_OP_REPLACE,VK_STENCIL_OP_INCREMENT_AND_CLAMP,VK_STENCIL_OP_DECREMENT_AND_CLAMP,VK_STENCIL_OP_INVERT,VK_STENCIL_OP_INCREMENT_AND_WRAP,VK_STENCIL_OP_DECREMENT_AND_WRAP};return map[op];};
 if(hasDepth){
  if(depthState->depth_compare>7)return 60;
  for(const auto* face:{&depthState->front,&depthState->back})if(depthState->stencil_test){
   if(face->compare_op>7 || face->fail_op>15 || face->pass_op>15 || face->depth_fail_op>15)return 60;
   for(auto op:{face->fail_op,face->pass_op,face->depth_fail_op})if(op==2 || op>=10)return 88;
  }
 }

#define LOAD(name) const auto name=reinterpret_cast<PFN_##name>(get(device,#name));if(!name)return 88
 LOAD(vkGetDeviceQueue);LOAD(vkCreateCommandPool);LOAD(vkDestroyCommandPool);LOAD(vkAllocateCommandBuffers);
 LOAD(vkBeginCommandBuffer);LOAD(vkEndCommandBuffer);LOAD(vkCreateFence);LOAD(vkDestroyFence);
 LOAD(vkQueueSubmit);LOAD(vkWaitForFences);LOAD(vkQueueWaitIdle);
 LOAD(vkCmdPipelineBarrier);LOAD(vkCmdBeginRenderingKHR);LOAD(vkCmdEndRenderingKHR);LOAD(vkCmdBindPipeline);LOAD(vkCmdBindDescriptorSets);
 LOAD(vkCmdBindVertexBuffers2EXT);LOAD(vkCmdPushConstants);LOAD(vkCmdSetViewportWithCountEXT);LOAD(vkCmdSetScissorWithCountEXT);
 LOAD(vkCmdSetDepthWriteEnableEXT);LOAD(vkCmdSetDepthCompareOpEXT);LOAD(vkCmdSetStencilOpEXT);
 LOAD(vkCmdSetStencilReference);LOAD(vkCmdSetStencilCompareMask);LOAD(vkCmdSetStencilWriteMask);
 LOAD(vkCmdSetDepthBoundsTestEnableEXT);LOAD(vkCmdSetDepthTestEnableEXT);LOAD(vkCmdSetStencilTestEnableEXT);LOAD(vkCmdSetDepthBiasEnableEXT);
 LOAD(vkCmdSetPrimitiveRestartEnableEXT);LOAD(vkCmdSetRasterizerDiscardEnableEXT);LOAD(vkCmdSetCullModeEXT);
 LOAD(vkCmdBindIndexBuffer);LOAD(vkCmdDrawIndexed);
 LOAD(vkCmdSetFrontFaceEXT);LOAD(vkCmdSetLineWidth);LOAD(vkCmdSetBlendConstants);LOAD(vkCmdDraw);
#undef LOAD
 const auto createQuery=reinterpret_cast<PFN_vkCreateQueryPool>(get(device,"vkCreateQueryPool"));
 const auto destroyQuery=reinterpret_cast<PFN_vkDestroyQueryPool>(get(device,"vkDestroyQueryPool"));
 const auto resetQuery=reinterpret_cast<PFN_vkCmdResetQueryPool>(get(device,"vkCmdResetQueryPool"));
 const auto beginQuery=reinterpret_cast<PFN_vkCmdBeginQuery>(get(device,"vkCmdBeginQuery"));
 const auto endQuery=reinterpret_cast<PFN_vkCmdEndQuery>(get(device,"vkCmdEndQuery"));
 const auto queryResult=reinterpret_cast<PFN_vkGetQueryPoolResults>(get(device,"vkGetQueryPoolResults"));
 if(visibility && (!createQuery || !destroyQuery || !resetQuery || !beginQuery || !endQuery || !queryResult))return 88;
 VkQueryPool query{};
 VkQueue queue{};vkGetDeviceQueue(device,family,0,&queue);if(!queue)return 91;
 VkCommandPool pool{};VkFence fence{};bool submitted=false;
 CtrTransferPhaseTiming timing("graphics_draw",count);timing.phase="record";
 const auto run=[&]()->int{
  if(visibility){VkQueryPoolCreateInfo qi{};qi.sType=VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO;qi.queryType=VK_QUERY_TYPE_OCCLUSION;qi.queryCount=1;
   if(createQuery(device,&qi,nullptr,&query)!=VK_SUCCESS)return 90;}
  VkCommandPoolCreateInfo pi{};pi.sType=VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;pi.queueFamilyIndex=family;
  if(vkCreateCommandPool(device,&pi,nullptr,&pool)!=VK_SUCCESS)return 90;
  VkCommandBufferAllocateInfo ai{};ai.sType=VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
  ai.commandPool=pool;ai.level=VK_COMMAND_BUFFER_LEVEL_PRIMARY;ai.commandBufferCount=1;
  VkCommandBuffer cmd{};if(vkAllocateCommandBuffers(device,&ai,&cmd)!=VK_SUCCESS)return 90;
  VkCommandBufferBeginInfo bi{};bi.sType=VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;bi.flags=VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
  if(vkBeginCommandBuffer(cmd,&bi)!=VK_SUCCESS)return 90;
  if(visibility)resetQuery(cmd,query,0,1);
  if(copySource){
   VkImageMemoryBarrier source{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
   source.srcAccessMask=VK_ACCESS_MEMORY_WRITE_BIT;source.dstAccessMask=VK_ACCESS_SHADER_READ_BIT;
   source.oldLayout=VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;source.newLayout=VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
   source.srcQueueFamilyIndex=source.dstQueueFamilyIndex=VK_QUEUE_FAMILY_IGNORED;
   source.image=copySource;source.subresourceRange={VK_IMAGE_ASPECT_COLOR_BIT,0,1,0,1};
   vkCmdPipelineBarrier(cmd,VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,0,0,nullptr,0,nullptr,1,&source);
  }
  // Fence completion does not replace the image memory dependency for the next LOAD.
  VkImageMemoryBarrier dependency{};dependency.sType=VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
  dependency.srcAccessMask=VK_ACCESS_MEMORY_WRITE_BIT;
  dependency.dstAccessMask=depthOnly?(VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT|VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT):(VK_ACCESS_COLOR_ATTACHMENT_READ_BIT|VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT);
  dependency.oldLayout=dependency.newLayout=depthOnly?VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL:VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
  if(copySource){dependency.oldLayout=copyDestinationOld;if(copyDestinationOld==VK_IMAGE_LAYOUT_UNDEFINED)dependency.srcAccessMask=0;}
  dependency.srcQueueFamilyIndex=dependency.dstQueueFamilyIndex=VK_QUEUE_FAMILY_IGNORED;
  dependency.image=depthOnly?depthImage:colorImage;dependency.subresourceRange={static_cast<VkImageAspectFlags>(depthOnly?depthAspects:VK_IMAGE_ASPECT_COLOR_BIT),0,1,0,1};
  if(!noAttachments)vkCmdPipelineBarrier(cmd,VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,(depthOnly?(VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT|VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT):VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT),
    0,0,nullptr,0,nullptr,1,&dependency);
  if(secondColorImage){
   auto second=dependency;second.image=secondColorImage;
   vkCmdPipelineBarrier(cmd,VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,0,0,nullptr,0,nullptr,1,&second);
  }
  if(hasDepth && !depthOnly){auto depthBarrier=dependency;depthBarrier.image=depthImage;
   depthBarrier.oldLayout=depthBarrier.newLayout=VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
   depthBarrier.dstAccessMask=VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT|VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
   depthBarrier.subresourceRange.aspectMask=VK_IMAGE_ASPECT_DEPTH_BIT|VK_IMAGE_ASPECT_STENCIL_BIT;
   vkCmdPipelineBarrier(cmd,VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT|VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT,0,0,nullptr,0,nullptr,1,&depthBarrier);}
  VkRenderingAttachmentInfo attachment{};attachment.sType=VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
  attachment.imageView=depthOnly?depthView:color;attachment.imageLayout=depthOnly?VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL:VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
  attachment.loadOp=copySource?VK_ATTACHMENT_LOAD_OP_DONT_CARE:VK_ATTACHMENT_LOAD_OP_LOAD;attachment.storeOp=VK_ATTACHMENT_STORE_OP_STORE;
  VkRenderingInfo rendering{};rendering.sType=VK_STRUCTURE_TYPE_RENDERING_INFO;rendering.renderArea={{0,0},extent};
  rendering.layerCount=1;rendering.colorAttachmentCount=(depthOnly || noAttachments)?0u:1u;rendering.pColorAttachments=(depthOnly || noAttachments)?nullptr:&attachment;
  VkRenderingAttachmentInfo colors[2]={attachment,attachment};
  VkRenderingAttachmentInfo depthAttachment=attachment;
  if(secondColorImage || inactiveSecondColor){colors[1].imageView=secondColorView;rendering.colorAttachmentCount=2;rendering.pColorAttachments=colors;}
  if(hasDepth && !depthOnly){depthAttachment.imageView=depthView;depthAttachment.imageLayout=VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
   rendering.pDepthAttachment=&depthAttachment;if(depthAspects&VK_IMAGE_ASPECT_STENCIL_BIT)rendering.pStencilAttachment=&depthAttachment;}
  else if(depthOnly){rendering.pDepthAttachment=&attachment;if(depthAspects&VK_IMAGE_ASPECT_STENCIL_BIT)rendering.pStencilAttachment=&attachment;}
  vkCmdBeginRenderingKHR(cmd,&rendering);vkCmdBindPipeline(cmd,VK_PIPELINE_BIND_POINT_GRAPHICS,pipeline);
  if(descriptors)vkCmdBindDescriptorSets(cmd,VK_PIPELINE_BIND_POINT_GRAPHICS,layout,0,1,&descriptors,0,nullptr);
  vkCmdPushConstants(cmd,layout,VK_SHADER_STAGE_ALL_GRAPHICS,0,120,push);
  vkCmdSetViewportWithCountEXT(cmd,1,&viewport);vkCmdSetScissorWithCountEXT(cmd,1,&scissor);
  vkCmdSetDepthBoundsTestEnableEXT(cmd,VK_FALSE);vkCmdSetDepthTestEnableEXT(cmd,VK_FALSE);vkCmdSetStencilTestEnableEXT(cmd,VK_FALSE);vkCmdSetDepthBiasEnableEXT(cmd,VK_FALSE);
  if(hasDepth){
   vkCmdSetDepthTestEnableEXT(cmd,depthState->depth_test);vkCmdSetDepthWriteEnableEXT(cmd,depthState->depth_write);
   vkCmdSetDepthCompareOpEXT(cmd,static_cast<VkCompareOp>(depthState->depth_compare));
   vkCmdSetStencilTestEnableEXT(cmd,depthState->stencil_test);
   if(depthState->stencil_test){
    const CtrGraphicsStencilFaceV1* faces[]={&depthState->front,&depthState->back};
    for(uint32_t i=0;i<2;++i){const auto& f=*faces[i];const auto face=i?VK_STENCIL_FACE_BACK_BIT:VK_STENCIL_FACE_FRONT_BIT;
     vkCmdSetStencilOpEXT(cmd,face,stencil(f.fail_op),stencil(f.pass_op),stencil(f.depth_fail_op),static_cast<VkCompareOp>(f.compare_op));
     vkCmdSetStencilReference(cmd,face,f.reference);vkCmdSetStencilCompareMask(cmd,face,f.compare_mask);vkCmdSetStencilWriteMask(cmd,face,f.write_mask);
    }
   }
  }
  vkCmdSetPrimitiveRestartEnableEXT(cmd,VK_FALSE);vkCmdSetRasterizerDiscardEnableEXT(cmd,VK_FALSE);
  vkCmdSetCullModeEXT(cmd,cull);vkCmdSetFrontFaceEXT(cmd,static_cast<VkFrontFace>(front));
  vkCmdSetLineWidth(cmd,1);vkCmdSetBlendConstants(cmd,blend);
  for(uint32_t i=0;i<vertexCount;++i){const VkDeviceSize offset=0;vkCmdBindVertexBuffers2EXT(cmd,bindings[i],1,&buffers[i],&offset,nullptr,&strides[i]);}
  if(visibility)beginQuery(cmd,query,0,0); // Boolean visibility only; precise-query feature not enabled.
  if(indexBuffer){vkCmdBindIndexBuffer(cmd,indexBuffer,indexOffset,indexType);vkCmdDrawIndexed(cmd,count,instances,firstIndex,vertexOffset,firstInstance);}
  else vkCmdDraw(cmd,count,instances,firstVertex,firstInstance);
  if(visibility)endQuery(cmd,query,0);
  vkCmdEndRenderingKHR(cmd);
  if(copySource){
   VkImageMemoryBarrier source{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
   source.srcAccessMask=VK_ACCESS_SHADER_READ_BIT;source.dstAccessMask=VK_ACCESS_COLOR_ATTACHMENT_READ_BIT|VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
   source.oldLayout=VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;source.newLayout=VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
   source.srcQueueFamilyIndex=source.dstQueueFamilyIndex=VK_QUEUE_FAMILY_IGNORED;
   source.image=copySource;source.subresourceRange={VK_IMAGE_ASPECT_COLOR_BIT,0,1,0,1};
   vkCmdPipelineBarrier(cmd,VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,0,0,nullptr,0,nullptr,1,&source);
  }
  if(vkEndCommandBuffer(cmd)!=VK_SUCCESS)return 90;
  VkFenceCreateInfo fi{};fi.sType=VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;if(vkCreateFence(device,&fi,nullptr,&fence)!=VK_SUCCESS)return 90;
  VkSubmitInfo si{};si.sType=VK_STRUCTURE_TYPE_SUBMIT_INFO;si.commandBufferCount=1;si.pCommandBuffers=&cmd;
  timing.Next("submit");
  if(vkQueueSubmit(queue,1,&si,fence)!=VK_SUCCESS)return 91;submitted=true;
  timing.Next("fence_wait");
  const auto result=vkWaitForFences(device,1,&fence,VK_TRUE,5000000000ULL);
  timing.Next("query_result");
  if(result==VK_SUCCESS){completed=true;
   if(visibility){uint64_t value=0;if(queryResult(device,query,0,1,sizeof(value),&value,sizeof(value),VK_QUERY_RESULT_64_BIT|VK_QUERY_RESULT_WAIT_BIT)!=VK_SUCCESS)return 91;*visibility=value;}
   return 0;}return result==VK_TIMEOUT?52:91;
 };
 const int status=run();timing.status=status;timing.Next("cleanup_drain");if(submitted && !completed)vkQueueWaitIdle(queue);
 if(query)destroyQuery(device,query,nullptr);
 if(fence)vkDestroyFence(device,fence,nullptr);if(pool)vkDestroyCommandPool(device,pool,nullptr);return status;
}
