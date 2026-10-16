// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include "video_core/renderer_vulkan/vk_instance.h"
#include "video_core/renderer_vulkan/vk_scheduler.h"
#include <vk_mem_alloc.h>
namespace Vulkan {
// Opt-in test evidence, copied after GPU completion. Never part of normal rendering.
inline void UpscaleReadback(const Instance& instance, Scheduler& scheduler, vk::Image image,
                           vk::ImageLayout layout, vk::Extent2D size, u32 bytes_per_pixel,
                           const char* name) {
    const char* directory = std::getenv("CTR_UPSCALE_CAPTURE_DIR");
    if (!directory) return;
    const auto allocator = instance.GetAllocator();
    const VkDeviceSize bytes = VkDeviceSize(size.width) * size.height * bytes_per_pixel;
    const VkBufferCreateInfo ci{.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        .size = bytes, .usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT};
    const VmaAllocationCreateInfo aci{
        .flags = VMA_ALLOCATION_CREATE_MAPPED_BIT | VMA_ALLOCATION_CREATE_HOST_ACCESS_RANDOM_BIT,
        .usage = VMA_MEMORY_USAGE_AUTO_PREFER_HOST};
    VkBuffer buffer{}; VmaAllocation allocation{}; VmaAllocationInfo info{};
    if (vmaCreateBuffer(allocator, &ci, &aci, &buffer, &allocation, &info) != VK_SUCCESS)
        throw std::runtime_error("Upscale evidence readback allocation failed");
    const auto cmd = scheduler.CommandBuffer();
    vk::ImageMemoryBarrier2 barrier{
        .srcStageMask = vk::PipelineStageFlagBits2::eAllCommands,
        .srcAccessMask = vk::AccessFlagBits2::eMemoryRead | vk::AccessFlagBits2::eMemoryWrite,
        .dstStageMask = vk::PipelineStageFlagBits2::eTransfer,
        .dstAccessMask = vk::AccessFlagBits2::eTransferRead,
        .oldLayout = layout, .newLayout = vk::ImageLayout::eTransferSrcOptimal,
        .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .image = image, .subresourceRange = {vk::ImageAspectFlagBits::eColor,0,1,0,1}};
    cmd.pipelineBarrier2({.imageMemoryBarrierCount = 1, .pImageMemoryBarriers = &barrier});
    const vk::BufferImageCopy region{.imageSubresource={vk::ImageAspectFlagBits::eColor,0,0,1},
                                    .imageExtent={size.width,size.height,1}};
    cmd.copyImageToBuffer(image, vk::ImageLayout::eTransferSrcOptimal, buffer, region);
    std::swap(barrier.oldLayout, barrier.newLayout);
    barrier.srcStageMask = vk::PipelineStageFlagBits2::eTransfer;
    barrier.srcAccessMask = vk::AccessFlagBits2::eTransferRead;
    barrier.dstStageMask = vk::PipelineStageFlagBits2::eAllCommands;
    barrier.dstAccessMask = vk::AccessFlagBits2::eMemoryRead | vk::AccessFlagBits2::eMemoryWrite;
    cmd.pipelineBarrier2({.imageMemoryBarrierCount=1,.pImageMemoryBarriers=&barrier});
    auto path = std::filesystem::path(directory) / (std::string(name) + "_" +
        std::to_string(size.width)+"x"+std::to_string(size.height)+".raw");
    scheduler.DeferPriorityOperation([allocator,buffer,allocation,info,bytes,path] {
        vmaInvalidateAllocation(allocator,allocation,0,VK_WHOLE_SIZE);
        if (FILE* file = std::fopen(path.string().c_str(), "wb")) {
            const auto written = std::fwrite(info.pMappedData,1,bytes,file);
            std::fclose(file);
            std::fprintf(stderr,"CTR_UPSCALE_READBACK file=%s bytes=%llu expected=%llu\n",
                path.string().c_str(),(unsigned long long)written,(unsigned long long)bytes);
        } else std::fprintf(stderr,"CTR_UPSCALE_READBACK_ERROR file=%s\n",path.string().c_str());
        vmaDestroyBuffer(allocator,buffer,allocation);
    });
}
}
