// SPDX-License-Identifier: GPL-2.0-only
#pragma once
#include <cstring>
#include "transfer_phase_timing.hpp"
#include "transfer_memory_type.hpp"
// Explicit linear four-byte texels (RGBA8 or R32float) host snapshot only. Never accepts tiled guest payload.
// Synchronous fence lifetime: caller retains image, helper retains staging until completion.
inline int TransferSampledLinear(VkDevice device,PFN_vkGetDeviceProcAddr get,uint32_t family,
 const VkPhysicalDeviceMemoryProperties& memory,VkImage image,VkExtent3D extent,
 VkImageLayout oldLayout,const void* source,void* destination,uint64_t sourceBytes,uint64_t pitch,bool& completed,
 VkImageLayout finalLayout=VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,uint32_t texelBytes=4,VkImageAspectFlags aspect=VK_IMAGE_ASPECT_COLOR_BIT,bool bc3=false,uint32_t mip=0,uint32_t layer=0) {
 completed=false;
 if(texelBytes!=1 && texelBytes!=2 && texelBytes!=4 && !(bc3 && (texelBytes==8 || texelBytes==16)))return 88;
 if(aspect!=VK_IMAGE_ASPECT_COLOR_BIT && aspect!=VK_IMAGE_ASPECT_DEPTH_BIT)return 88;
 const uint32_t rows=bc3?(extent.height+3)/4:extent.height;
 const uint64_t row=uint64_t(bc3?(extent.width+3)/4:extent.width)*texelBytes;
 const bool readback=destination!=nullptr;
 if((!source && !destination) || !extent.width || !extent.height || pitch<row || pitch>SIZE_MAX/rows ||
    sourceBytes<pitch*(rows-1)+row)return 60;
#define LOAD(name) const auto name=reinterpret_cast<PFN_##name>(get(device,#name));if(!name)return 88
 LOAD(vkGetDeviceQueue);LOAD(vkCreateCommandPool);LOAD(vkDestroyCommandPool);
 LOAD(vkCreateBuffer);LOAD(vkDestroyBuffer);LOAD(vkGetBufferMemoryRequirements);
 LOAD(vkAllocateMemory);LOAD(vkFreeMemory);LOAD(vkBindBufferMemory);LOAD(vkMapMemory);LOAD(vkUnmapMemory);
 LOAD(vkAllocateCommandBuffers);LOAD(vkBeginCommandBuffer);LOAD(vkEndCommandBuffer);
 LOAD(vkCmdPipelineBarrier);LOAD(vkCmdCopyBufferToImage);LOAD(vkCmdCopyImageToBuffer);LOAD(vkCreateFence);LOAD(vkDestroyFence);
 LOAD(vkQueueSubmit);LOAD(vkWaitForFences);LOAD(vkQueueWaitIdle);
#undef LOAD
 CtrTransferPhaseTiming timing(readback?"image_readback":"image_upload",sourceBytes);
 VkCommandPool pool{};VkBuffer buffer{};VkDeviceMemory allocation{};VkFence fence{};VkQueue queue{};
 bool submitted=false;vkGetDeviceQueue(device,family,0,&queue);if(!queue)return 91;
 const auto run=[&]() -> int {
  VkBufferCreateInfo ci{};ci.sType=VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
  ci.size=row*rows;ci.usage=readback?VK_BUFFER_USAGE_TRANSFER_DST_BIT:VK_BUFFER_USAGE_TRANSFER_SRC_BIT;ci.sharingMode=VK_SHARING_MODE_EXCLUSIVE;
  if(vkCreateBuffer(device,&ci,nullptr,&buffer)!=VK_SUCCESS)return 90;
  VkMemoryRequirements needed{};vkGetBufferMemoryRequirements(device,buffer,&needed);
  const uint32_t type=CtrTransferMemoryType(memory,needed.memoryTypeBits,readback);
  if(type==UINT32_MAX)return 88;
  if(timing.enabled)std::fprintf(stderr,"CTR_TRANSFER_MEMORY operation=%s buffer=0 type=%u flags=%u compatible_bits=%u bytes=%llu\n",readback?"image_readback":"image_upload",type,memory.memoryTypes[type].propertyFlags,needed.memoryTypeBits,static_cast<unsigned long long>(ci.size));
  VkMemoryAllocateInfo ma{};ma.sType=VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
  ma.allocationSize=needed.size;ma.memoryTypeIndex=type;
  if(vkAllocateMemory(device,&ma,nullptr,&allocation)!=VK_SUCCESS ||
    vkBindBufferMemory(device,buffer,allocation,0)!=VK_SUCCESS)return 90;
  timing.Next("input_map_copy");
  if(!readback) {
   void* mapped{};if(vkMapMemory(device,allocation,0,ci.size,0,&mapped)!=VK_SUCCESS)return 90;
   for(uint32_t y=0;y<rows;++y)std::memcpy(static_cast<unsigned char*>(mapped)+size_t(y*row),
     static_cast<const unsigned char*>(source)+size_t(y*pitch),size_t(row));
   vkUnmapMemory(device,allocation);
  }
  timing.Next("record");
  VkCommandPoolCreateInfo pi{};pi.sType=VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;pi.queueFamilyIndex=family;
  if(vkCreateCommandPool(device,&pi,nullptr,&pool)!=VK_SUCCESS)return 90;
  VkCommandBufferAllocateInfo ai{};ai.sType=VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
  ai.commandPool=pool;ai.level=VK_COMMAND_BUFFER_LEVEL_PRIMARY;ai.commandBufferCount=1;
  VkCommandBuffer command{};if(vkAllocateCommandBuffers(device,&ai,&command)!=VK_SUCCESS)return 90;
  VkCommandBufferBeginInfo bi{};bi.sType=VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;bi.flags=VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
  if(vkBeginCommandBuffer(command,&bi)!=VK_SUCCESS)return 90;
  VkImageMemoryBarrier barrier{};barrier.sType=VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
  barrier.oldLayout=oldLayout;barrier.newLayout=readback?VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL:VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
  barrier.srcAccessMask=oldLayout==VK_IMAGE_LAYOUT_UNDEFINED?0:
    oldLayout==VK_IMAGE_LAYOUT_GENERAL?(VK_ACCESS_SHADER_READ_BIT|VK_ACCESS_SHADER_WRITE_BIT):
    oldLayout==VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL?(VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT|VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT):
    oldLayout==VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL?(VK_ACCESS_COLOR_ATTACHMENT_READ_BIT|VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT):VK_ACCESS_SHADER_READ_BIT;
  barrier.dstAccessMask=readback?VK_ACCESS_TRANSFER_READ_BIT:VK_ACCESS_TRANSFER_WRITE_BIT;barrier.srcQueueFamilyIndex=VK_QUEUE_FAMILY_IGNORED;
  barrier.dstQueueFamilyIndex=VK_QUEUE_FAMILY_IGNORED;barrier.image=image;
  barrier.subresourceRange={aspect,mip,1,layer,1};
  vkCmdPipelineBarrier(command,VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,VK_PIPELINE_STAGE_TRANSFER_BIT,0,0,nullptr,0,nullptr,1,&barrier);
  VkBufferImageCopy copy{};copy.imageSubresource={aspect,mip,layer,1};copy.imageExtent=extent;
  if(readback)vkCmdCopyImageToBuffer(command,image,VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,buffer,1,&copy);
  else vkCmdCopyBufferToImage(command,buffer,image,VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,1,&copy);
  barrier.oldLayout=readback?VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL:VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;barrier.newLayout=finalLayout;
  barrier.srcAccessMask=readback?VK_ACCESS_TRANSFER_READ_BIT:VK_ACCESS_TRANSFER_WRITE_BIT;barrier.dstAccessMask=finalLayout==VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL?(VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT|VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT):finalLayout==VK_IMAGE_LAYOUT_GENERAL?(VK_ACCESS_SHADER_READ_BIT|VK_ACCESS_SHADER_WRITE_BIT):finalLayout==VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL?
    (VK_ACCESS_COLOR_ATTACHMENT_READ_BIT|VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT):VK_ACCESS_SHADER_READ_BIT;
  vkCmdPipelineBarrier(command,VK_PIPELINE_STAGE_TRANSFER_BIT,VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,0,0,nullptr,0,nullptr,1,&barrier);
  if(readback) {
   VkMemoryBarrier host{};host.sType=VK_STRUCTURE_TYPE_MEMORY_BARRIER;
   host.srcAccessMask=VK_ACCESS_TRANSFER_WRITE_BIT;host.dstAccessMask=VK_ACCESS_HOST_READ_BIT;
   vkCmdPipelineBarrier(command,VK_PIPELINE_STAGE_TRANSFER_BIT,VK_PIPELINE_STAGE_HOST_BIT,0,1,&host,0,nullptr,0,nullptr);
  }
  if(vkEndCommandBuffer(command)!=VK_SUCCESS)return 90;
  VkFenceCreateInfo fi{};fi.sType=VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
  if(vkCreateFence(device,&fi,nullptr,&fence)!=VK_SUCCESS)return 90;
  VkSubmitInfo si{};si.sType=VK_STRUCTURE_TYPE_SUBMIT_INFO;si.commandBufferCount=1;si.pCommandBuffers=&command;
  timing.Next("submit");
  if(vkQueueSubmit(queue,1,&si,fence)!=VK_SUCCESS)return 90;
  submitted=true;timing.Next("fence_wait");const auto result=vkWaitForFences(device,1,&fence,VK_TRUE,5000000000ULL);
  timing.Next("output_map_copy");
  if(result==VK_SUCCESS) {
   completed=true;
   if(readback) {
    void* mapped{};if(vkMapMemory(device,allocation,0,ci.size,0,&mapped)!=VK_SUCCESS)return 90;
    for(uint32_t y=0;y<rows;++y)std::memcpy(static_cast<unsigned char*>(destination)+size_t(y*pitch),
     static_cast<const unsigned char*>(mapped)+size_t(y*row),size_t(row));
    vkUnmapMemory(device,allocation);
   }
   return 0;
  }
  return result==VK_TIMEOUT?52:91;
 };
 const int result=run();timing.status=result;timing.Next("cleanup_drain");
 // Failed submitted work is drained before releasing staging. Device loss is terminal.
 if(submitted && !completed)vkQueueWaitIdle(queue);
 if(fence)vkDestroyFence(device,fence,nullptr);
 if(pool)vkDestroyCommandPool(device,pool,nullptr);
 if(buffer)vkDestroyBuffer(device,buffer,nullptr);
 if(allocation)vkFreeMemory(device,allocation,nullptr);
 return result;
}

inline int UploadSampledLinear(VkDevice device,PFN_vkGetDeviceProcAddr get,uint32_t family,
 const VkPhysicalDeviceMemoryProperties& memory,VkImage image,VkExtent3D extent,
 VkImageLayout oldLayout,const void* source,uint64_t bytes,uint64_t pitch,bool& completed) {
 return TransferSampledLinear(device,get,family,memory,image,extent,oldLayout,source,nullptr,bytes,pitch,completed);
}
