// SPDX-License-Identifier: GPL-2.0-only
// Single acquired image; bounded waits; no guest rendering or R3 credit.
static int present_control(VkDevice device, PFN_vkGetDeviceProcAddr get,
                           VkSwapchainKHR swap, uint32_t family, uint32_t count,
                           VkExtent2D extent, VkFormat format,
                           const VkPhysicalDeviceMemoryProperties& memory,
                           const void* source, size_t sourceBytes, size_t rowPitch) {
    // Caller owns this immutable host snapshot until the synchronous copy returns.
    if (!extent.width || !extent.height || extent.width > 16384 || extent.height > 16384) return 34;
    if (format != VK_FORMAT_B8G8R8A8_UNORM && format != VK_FORMAT_B8G8R8A8_SRGB &&
        format != VK_FORMAT_R8G8B8A8_UNORM && format != VK_FORMAT_R8G8B8A8_SRGB) return 35;
    const size_t rowBytes = size_t(extent.width) * 4;
    if (!source || rowPitch < rowBytes || rowPitch > SIZE_MAX / extent.height ||
        sourceBytes < rowPitch * (extent.height - 1) + rowBytes) return 41;
#define LOAD(name) const auto name = reinterpret_cast<PFN_##name>(get(device, #name)); if (!name) return 20
    LOAD(vkGetSwapchainImagesKHR); LOAD(vkGetDeviceQueue); LOAD(vkCreateCommandPool);
    LOAD(vkCreateBuffer); LOAD(vkDestroyBuffer); LOAD(vkGetBufferMemoryRequirements);
    LOAD(vkAllocateMemory); LOAD(vkFreeMemory); LOAD(vkBindBufferMemory);
    LOAD(vkMapMemory); LOAD(vkUnmapMemory); LOAD(vkCmdCopyBufferToImage);
    LOAD(vkDestroyCommandPool); LOAD(vkAllocateCommandBuffers); LOAD(vkBeginCommandBuffer);
    LOAD(vkEndCommandBuffer); LOAD(vkCmdPipelineBarrier);
    LOAD(vkCreateSemaphore); LOAD(vkDestroySemaphore); LOAD(vkCreateFence);
    LOAD(vkDestroyFence); LOAD(vkAcquireNextImageKHR); LOAD(vkQueueSubmit);
    LOAD(vkQueuePresentKHR); LOAD(vkWaitForFences); LOAD(vkDeviceWaitIdle);
#undef LOAD
    VkCommandPool pool{}; VkSemaphore acquired{}, rendered{}; VkFence fence{};
    VkBuffer upload{}; VkDeviceMemory allocation{};
    int status = 21;
    // Cleanup waits for submitted work before destroying synchronization/resources.
    auto run = [&]() -> int {
        const VkDeviceSize bytes = VkDeviceSize(extent.width) * extent.height * 4;
        VkBufferCreateInfo ub{}; ub.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        ub.size = bytes; ub.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
        ub.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        if (vkCreateBuffer(device, &ub, nullptr, &upload) != VK_SUCCESS) return 36;
        VkMemoryRequirements requirements{}; vkGetBufferMemoryRequirements(device, upload, &requirements);
        uint32_t type = UINT32_MAX;
        const auto flags = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
        for (uint32_t i = 0; i < memory.memoryTypeCount; ++i)
            if ((requirements.memoryTypeBits & (1u << i)) && (memory.memoryTypes[i].propertyFlags & flags) == flags) { type = i; break; }
        if (type == UINT32_MAX) return 37;
        VkMemoryAllocateInfo ma{}; ma.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        ma.allocationSize = requirements.size; ma.memoryTypeIndex = type;
        if (vkAllocateMemory(device, &ma, nullptr, &allocation) != VK_SUCCESS) return 38;
        if (vkBindBufferMemory(device, upload, allocation, 0) != VK_SUCCESS) return 39;
        void* mapped{};
        if (vkMapMemory(device, allocation, 0, bytes, 0, &mapped) != VK_SUCCESS) return 40;
        auto* pixels = static_cast<unsigned char*>(mapped);
        for (uint32_t y = 0; y < extent.height; ++y)
            std::memcpy(pixels + size_t(y) * rowBytes,
                        static_cast<const unsigned char*>(source) + size_t(y) * rowPitch, rowBytes);
        vkUnmapMemory(device, allocation);
        std::vector<VkImage> images(count);
        if (vkGetSwapchainImagesKHR(device, swap, &count, images.data()) != VK_SUCCESS) return 22;
        VkCommandPoolCreateInfo pi{}; pi.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO; pi.queueFamilyIndex = family;
        if (vkCreateCommandPool(device, &pi, nullptr, &pool) != VK_SUCCESS) return 23;
        VkSemaphoreCreateInfo si{}; si.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
        if (vkCreateSemaphore(device, &si, nullptr, &acquired) != VK_SUCCESS ||
            vkCreateSemaphore(device, &si, nullptr, &rendered) != VK_SUCCESS) return 24;
        VkFenceCreateInfo fi{}; fi.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
        if (vkCreateFence(device, &fi, nullptr, &fence) != VK_SUCCESS) return 25;
        uint32_t index{};
        const auto ar = vkAcquireNextImageKHR(device, swap, 5000000000ULL, acquired, VK_NULL_HANDLE, &index);
        if ((ar != VK_SUCCESS && ar != VK_SUBOPTIMAL_KHR) || index >= count) return 26;
        VkCommandBufferAllocateInfo ai{}; ai.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        ai.commandPool = pool; ai.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY; ai.commandBufferCount = 1;
        VkCommandBuffer command{};
        if (vkAllocateCommandBuffers(device, &ai, &command) != VK_SUCCESS) return 27;
        VkCommandBufferBeginInfo bi{}; bi.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        bi.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
        if (vkBeginCommandBuffer(command, &bi) != VK_SUCCESS) return 28;
        VkImageMemoryBarrier barrier{}; barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        barrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED; barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED; barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.image = images[index]; barrier.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
        barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        vkCmdPipelineBarrier(command, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
                             0, 0, nullptr, 0, nullptr, 1, &barrier);
        VkBufferImageCopy copy{}; copy.imageSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1};
        copy.imageExtent = {extent.width, extent.height, 1};
        vkCmdCopyBufferToImage(command, upload, images[index], VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copy);
        barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL; barrier.newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
        barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT; barrier.dstAccessMask = 0;
        vkCmdPipelineBarrier(command, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
                             0, 0, nullptr, 0, nullptr, 1, &barrier);
        if (vkEndCommandBuffer(command) != VK_SUCCESS) return 29;
        VkQueue queue{}; vkGetDeviceQueue(device, family, 0, &queue);
        VkPipelineStageFlags waitStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
        VkSubmitInfo submit{}; submit.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        submit.waitSemaphoreCount = 1; submit.pWaitSemaphores = &acquired; submit.pWaitDstStageMask = &waitStage;
        submit.commandBufferCount = 1; submit.pCommandBuffers = &command;
        submit.signalSemaphoreCount = 1; submit.pSignalSemaphores = &rendered;
        if (vkQueueSubmit(queue, 1, &submit, fence) != VK_SUCCESS) return 30;
        VkPresentInfoKHR present{}; present.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
        present.waitSemaphoreCount = 1; present.pWaitSemaphores = &rendered;
        present.swapchainCount = 1; present.pSwapchains = &swap; present.pImageIndices = &index;
        const auto pr = vkQueuePresentKHR(queue, &present);
        if (pr != VK_SUCCESS && pr != VK_SUBOPTIMAL_KHR) return 31;
        if (vkWaitForFences(device, 1, &fence, VK_TRUE, 5000000000ULL) != VK_SUCCESS) return 32;
        std::printf("host_control acquire/upload/submit/present passed index=%u present_result=%d; window_visibility=caller_configured visual_observation=false R3=false\n", index, pr);
        return 0;
    };
    status = run();
    const auto idle = vkDeviceWaitIdle(device);
    if (idle != VK_SUCCESS) {
        // Pending GPU work cannot safely outlive destroyed upload/synchronization objects.
        std::fprintf(stderr, "present_control device idle failed=%d; terminating control\n", idle);
        ExitProcess(33);
    }
    if (upload) vkDestroyBuffer(device, upload, nullptr);
    if (allocation) vkFreeMemory(device, allocation, nullptr);
    if (fence) vkDestroyFence(device, fence, nullptr);
    if (rendered) vkDestroySemaphore(device, rendered, nullptr);
    if (acquired) vkDestroySemaphore(device, acquired, nullptr);
    if (pool) vkDestroyCommandPool(device, pool, nullptr);
    if (status) std::fprintf(stderr, "present_control failed=%d\n", status);
    return status;
}
