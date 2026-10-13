// SPDX-License-Identifier: GPL-2.0-only
#pragma once
#include "tiled_surface.hpp"
// Synthetic detiled buffer -> actual1920x1080 SRGB image -> host readback.
static int ctr_tiling_image_control(VkDevice device,PFN_vkGetDeviceProcAddr get,uint32_t family,
    const VkPhysicalDeviceMemoryProperties& memory,VkBuffer linear,VkBuffer readback,
    const void* expected,void* output) {
#define LOAD(name) const auto name=reinterpret_cast<PFN_##name>(get(device,#name));if(!name)return 91
    LOAD(vkCreateCommandPool);LOAD(vkDestroyCommandPool);LOAD(vkAllocateCommandBuffers);
    LOAD(vkBeginCommandBuffer);LOAD(vkEndCommandBuffer);LOAD(vkCmdPipelineBarrier);
    LOAD(vkCmdCopyBufferToImage);LOAD(vkCmdCopyImageToBuffer);LOAD(vkGetDeviceQueue);
    LOAD(vkCreateFence);LOAD(vkDestroyFence);LOAD(vkQueueSubmit);LOAD(vkWaitForFences);LOAD(vkQueueWaitIdle);
#undef LOAD
    CtrVideoBufferAttributesV1 attributes{0x80000000U,0,0,1920,1080,1920,0,0,0};
    std::unique_ptr<CtrTiledSurface> surface;
    const int created=CtrTiledSurface::Create(device,get,memory,attributes,1,surface);if(created)return created;
    VkCommandPool pool{};VkFence fence{};VkQueue queue{};bool submitted=false;
    vkGetDeviceQueue(device,family,0,&queue);if(!queue)return 91;
    const auto run=[&]() -> int {
        std::memset(output,0x55,8355840);
        VkCommandPoolCreateInfo pi{};pi.sType=VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;pi.queueFamilyIndex=family;
        if(vkCreateCommandPool(device,&pi,nullptr,&pool)!=VK_SUCCESS)return 91;
        VkCommandBufferAllocateInfo ai{};ai.sType=VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        ai.commandPool=pool;ai.level=VK_COMMAND_BUFFER_LEVEL_PRIMARY;ai.commandBufferCount=1;
        VkCommandBuffer command{};if(vkAllocateCommandBuffers(device,&ai,&command)!=VK_SUCCESS)return 91;
        VkCommandBufferBeginInfo bi{};bi.sType=VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        bi.flags=VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
        if(vkBeginCommandBuffer(command,&bi)!=VK_SUCCESS)return 91;
        VkMemoryBarrier mb{};mb.sType=VK_STRUCTURE_TYPE_MEMORY_BARRIER;
        mb.srcAccessMask=VK_ACCESS_SHADER_WRITE_BIT|VK_ACCESS_HOST_WRITE_BIT;
        mb.dstAccessMask=VK_ACCESS_TRANSFER_READ_BIT|VK_ACCESS_TRANSFER_WRITE_BIT;
        vkCmdPipelineBarrier(command,VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT|VK_PIPELINE_STAGE_HOST_BIT,
            VK_PIPELINE_STAGE_TRANSFER_BIT,0,1,&mb,0,nullptr,0,nullptr);
        VkImageMemoryBarrier image{};image.sType=VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        image.oldLayout=VK_IMAGE_LAYOUT_UNDEFINED;image.newLayout=VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        image.dstAccessMask=VK_ACCESS_TRANSFER_WRITE_BIT;image.image=surface->Image();
        image.srcQueueFamilyIndex=VK_QUEUE_FAMILY_IGNORED;image.dstQueueFamilyIndex=VK_QUEUE_FAMILY_IGNORED;
        image.subresourceRange={VK_IMAGE_ASPECT_COLOR_BIT,0,1,0,1};
        vkCmdPipelineBarrier(command,VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,VK_PIPELINE_STAGE_TRANSFER_BIT,
            0,0,nullptr,0,nullptr,1,&image);
        VkBufferImageCopy copy{};copy.bufferRowLength=1920;copy.bufferImageHeight=1088;
        copy.imageSubresource={VK_IMAGE_ASPECT_COLOR_BIT,0,0,1};copy.imageExtent={1920,1080,1};
        vkCmdCopyBufferToImage(command,linear,surface->Image(),VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,1,&copy);
        image.oldLayout=VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;image.newLayout=VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
        image.srcAccessMask=VK_ACCESS_TRANSFER_WRITE_BIT;image.dstAccessMask=VK_ACCESS_TRANSFER_READ_BIT;
        vkCmdPipelineBarrier(command,VK_PIPELINE_STAGE_TRANSFER_BIT,VK_PIPELINE_STAGE_TRANSFER_BIT,0,0,nullptr,0,nullptr,1,&image);
        vkCmdCopyImageToBuffer(command,surface->Image(),VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,readback,1,&copy);
        mb.srcAccessMask=VK_ACCESS_TRANSFER_WRITE_BIT;mb.dstAccessMask=VK_ACCESS_HOST_READ_BIT;
        vkCmdPipelineBarrier(command,VK_PIPELINE_STAGE_TRANSFER_BIT,VK_PIPELINE_STAGE_HOST_BIT,0,1,&mb,0,nullptr,0,nullptr);
        if(vkEndCommandBuffer(command)!=VK_SUCCESS)return 91;
        VkFenceCreateInfo fi{};fi.sType=VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
        if(vkCreateFence(device,&fi,nullptr,&fence)!=VK_SUCCESS)return 91;
        VkSubmitInfo si{};si.sType=VK_STRUCTURE_TYPE_SUBMIT_INFO;si.commandBufferCount=1;si.pCommandBuffers=&command;
        if(vkQueueSubmit(queue,1,&si,fence)!=VK_SUCCESS)return 91;submitted=true;
        const auto waited=vkWaitForFences(device,1,&fence,VK_TRUE,5000000000ULL);
        if(waited!=VK_SUCCESS)return waited==VK_TIMEOUT ? 52 : 91;
        constexpr size_t visible=1920ULL*1080*4;
        if(std::memcmp(expected,output,visible))return 155;
        const auto* bytes=static_cast<const unsigned char*>(output);
        for(size_t i=visible;i<8355840;++i)if(bytes[i]!=0x55)return 155;
        for(size_t i=8355840;i<8355840+256;++i)if(bytes[i]!=0xd3)return 155;
        std::puts("detiled buffer actual SRGB image upload/readback passed visible1920x1080=true padded8rows_untouched=true guard256=true real_fence=true synthetic=true retail_pixels0=true R3=false");return 0;
    };
    const int result=run();if(submitted && result)vkQueueWaitIdle(queue);
    if(fence)vkDestroyFence(device,fence,nullptr);if(pool)vkDestroyCommandPool(device,pool,nullptr);
    return result;
}
