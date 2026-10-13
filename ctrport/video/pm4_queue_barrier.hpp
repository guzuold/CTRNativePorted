// SPDX-License-Identifier: GPL-2.0-only
#pragma once
// Synchronous owner queue barrier. Actual fence wait before returning0.
// GPU memory availability only: no guest CPU/GPU alias coherence implied.
static int ctr_pm4_queue_barrier(VkDevice device,PFN_vkGetDeviceProcAddr get,uint32_t family) {
#define LOAD(name) const auto name=reinterpret_cast<PFN_##name>(get(device,#name));if(!name)return 91
    LOAD(vkGetDeviceQueue);LOAD(vkCreateCommandPool);LOAD(vkDestroyCommandPool);
    LOAD(vkAllocateCommandBuffers);LOAD(vkBeginCommandBuffer);LOAD(vkEndCommandBuffer);
    LOAD(vkCmdPipelineBarrier);LOAD(vkCreateFence);LOAD(vkDestroyFence);
    LOAD(vkQueueSubmit);LOAD(vkWaitForFences);LOAD(vkQueueWaitIdle);
#undef LOAD
    VkCommandPool pool{};VkFence fence{};VkQueue queue{};bool submitted=false;
    vkGetDeviceQueue(device,family,0,&queue);if(!queue)return 91;
    const auto run=[&]() -> int {
        VkCommandPoolCreateInfo pi{};pi.sType=VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;pi.queueFamilyIndex=family;
        if(vkCreateCommandPool(device,&pi,nullptr,&pool)!=VK_SUCCESS)return 91;
        VkCommandBufferAllocateInfo ai{};ai.sType=VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        ai.commandPool=pool;ai.level=VK_COMMAND_BUFFER_LEVEL_PRIMARY;ai.commandBufferCount=1;
        VkCommandBuffer command{};
        if(vkAllocateCommandBuffers(device,&ai,&command)!=VK_SUCCESS)return 91;
        VkCommandBufferBeginInfo bi{};bi.sType=VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        bi.flags=VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
        if(vkBeginCommandBuffer(command,&bi)!=VK_SUCCESS)return 91;
        VkMemoryBarrier barrier{};barrier.sType=VK_STRUCTURE_TYPE_MEMORY_BARRIER;
        barrier.srcAccessMask=VK_ACCESS_MEMORY_WRITE_BIT | VK_ACCESS_HOST_WRITE_BIT;
        barrier.dstAccessMask=VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT | VK_ACCESS_HOST_READ_BIT;
        vkCmdPipelineBarrier(command,VK_PIPELINE_STAGE_ALL_COMMANDS_BIT | VK_PIPELINE_STAGE_HOST_BIT,
            VK_PIPELINE_STAGE_ALL_COMMANDS_BIT | VK_PIPELINE_STAGE_HOST_BIT,0,1,&barrier,0,nullptr,0,nullptr);
        if(vkEndCommandBuffer(command)!=VK_SUCCESS)return 91;
        VkFenceCreateInfo fi{};fi.sType=VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
        if(vkCreateFence(device,&fi,nullptr,&fence)!=VK_SUCCESS)return 91;
        VkSubmitInfo si{};si.sType=VK_STRUCTURE_TYPE_SUBMIT_INFO;si.commandBufferCount=1;si.pCommandBuffers=&command;
        if(vkQueueSubmit(queue,1,&si,fence)!=VK_SUCCESS)return 91;
        submitted=true;
        const auto waited=vkWaitForFences(device,1,&fence,VK_TRUE,5000000000ULL);
        return waited==VK_SUCCESS ? 0 : waited==VK_TIMEOUT ? 52 : 91;
    };
    const int result=run();
    // Failure after submit still retains resources until work actually stops.
    if(submitted && result) vkQueueWaitIdle(queue);
    if(fence)vkDestroyFence(device,fence,nullptr);
    if(pool)vkDestroyCommandPool(device,pool,nullptr);
    return result;
}
