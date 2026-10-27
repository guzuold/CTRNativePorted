// SPDX-License-Identifier: GPL-2.0-only
#pragma once
#include <vector>
#include <cstring>
#include "transfer_phase_timing.hpp"
#include "transfer_memory_type.hpp"
// Whole public address functions (pipeline matches direction/sample count); host spans, one mip/layer, non-alt bank swizzle0.
static int ctr_thin_detile(VkDevice device, PFN_vkGetDeviceProcAddr get, uint32_t family,
                           const VkPhysicalDeviceMemoryProperties& memory, VkPipelineLayout layout, VkPipeline pipeline,
                           VkDescriptorSetLayout descriptorLayout, uint32_t pitch, uint32_t paddedHeight,
                           const void* source, uint64_t sourceBytes, std::vector<unsigned char>& output,
                           bool tile = false, uint32_t texelBytes = 4, uint32_t samples = 1, bool micro13 = false,
                           bool tallDepth16 = false) {
    if (!device || !get || !source || !pitch || !paddedHeight || !layout || !pipeline || !descriptorLayout) return 60;
    if (micro13 && ((texelBytes != 4 && texelBytes != 8 && texelBytes != 16) || samples != 1)) return 88;
    if (samples != 1 && !(samples == 2 && texelBytes == 4)) return 88;
    if (texelBytes != 2 && texelBytes != 4 && !((texelBytes == 8 || texelBytes == 16) && micro13)) return 88;
    const bool tall16 =
        tallDepth16 && pitch == 2048 && paddedHeight == 8192 && texelBytes == 2 && samples == 1 && !micro13;
    if (pitch > 4096 || (paddedHeight > 4096 && !tall16) || pitch % (micro13 ? 8 : 128) ||
        paddedHeight % (micro13           ? 8
                        : texelBytes == 2 ? 128
                                          : 64))
        return 88;
    const uint64_t required = uint64_t(pitch) * paddedHeight * texelBytes * samples;
    // One X invocation per pixel; MS2 adapter dispatches samples on Y. X group count stays below65535.
    if (uint64_t(pitch) * paddedHeight / 64 > 65535 && !tall16) return 88;
    if (sourceBytes != required) return 60;
    CtrTransferPhaseTiming timing(tile ? "tile" : "detile", required);
    std::vector<unsigned char> converted(static_cast<size_t>(required));
#define LOAD(name)                                                                                                     \
    const auto name = reinterpret_cast<PFN_##name>(get(device, #name));                                                \
    if (!name) return 91
    LOAD(vkCreateBuffer);
    LOAD(vkDestroyBuffer);
    LOAD(vkGetBufferMemoryRequirements);
    LOAD(vkAllocateMemory);
    LOAD(vkFreeMemory);
    LOAD(vkBindBufferMemory);
    LOAD(vkMapMemory);
    LOAD(vkUnmapMemory);
    LOAD(vkCreateDescriptorPool);
    LOAD(vkDestroyDescriptorPool);
    LOAD(vkAllocateDescriptorSets);
    LOAD(vkUpdateDescriptorSets);
    LOAD(vkCreateCommandPool);
    LOAD(vkDestroyCommandPool);
    LOAD(vkAllocateCommandBuffers);
    LOAD(vkBeginCommandBuffer);
    LOAD(vkEndCommandBuffer);
    LOAD(vkCmdPipelineBarrier);
    LOAD(vkCmdBindPipeline);
    LOAD(vkCmdBindDescriptorSets);
    LOAD(vkCmdDispatch);
    LOAD(vkGetDeviceQueue);
    LOAD(vkQueueSubmit);
    LOAD(vkCreateFence);
    LOAD(vkDestroyFence);
    LOAD(vkWaitForFences);
    LOAD(vkQueueWaitIdle);
#undef LOAD
    const uint32_t texels = pitch * paddedHeight;
    const VkDeviceSize bytes = required;
    VkBuffer buffers[4]{};
    VkDeviceMemory allocations[4]{};
    void* mapped[4]{};
    VkDescriptorPool descriptors{};
    VkCommandPool commands{};
    VkFence fence{};
    VkQueue queue{};
    bool submitted = false;
    vkGetDeviceQueue(device, family, 0, &queue);
    if (!queue) return 91;
    const auto run = [&]() -> int {
        for (uint32_t i = 1; i < 4; ++i) {
            const VkDeviceSize size = i == 3 ? 268 : bytes + 256;
            VkBufferCreateInfo bi{};
            bi.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
            bi.size = size;
            bi.usage = i == 3 ? VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT
                              : (VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT |
                                 VK_BUFFER_USAGE_TRANSFER_DST_BIT);
            bi.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
            if (vkCreateBuffer(device, &bi, nullptr, &buffers[i]) != VK_SUCCESS) return 91;
            VkMemoryRequirements requirements{};
            vkGetBufferMemoryRequirements(device, buffers[i], &requirements);
            const uint32_t type = CtrTransferMemoryType(memory, requirements.memoryTypeBits, true);
            if (type == UINT32_MAX) return 88;
            if (timing.enabled)
                std::fprintf(
                    stderr,
                    "CTR_TRANSFER_MEMORY operation=%s buffer=%u type=%u flags=%u compatible_bits=%u bytes=%llu\n",
                    tile ? "tile" : "detile", i, type, memory.memoryTypes[type].propertyFlags,
                    requirements.memoryTypeBits, static_cast<unsigned long long>(size));
            VkMemoryAllocateInfo ai{};
            ai.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
            ai.allocationSize = requirements.size;
            ai.memoryTypeIndex = type;
            if (vkAllocateMemory(device, &ai, nullptr, &allocations[i]) != VK_SUCCESS ||
                vkBindBufferMemory(device, buffers[i], allocations[i], 0) != VK_SUCCESS ||
                vkMapMemory(device, allocations[i], 0, size, 0, &mapped[i]) != VK_SUCCESS)
                return 91;
        }
        timing.Next("input_copy");
        std::memcpy(mapped[1], source, static_cast<size_t>(bytes));
        std::memset(mapped[2], 0x57, static_cast<size_t>(bytes));
        for (uint32_t i = 1; i < 3; ++i) std::memset(static_cast<unsigned char*>(mapped[i]) + bytes, 0xd3, 256);
        auto* uniform = static_cast<uint32_t*>(mapped[3]);
        std::memset(uniform, 0, 268);
        uniform[0] = 0;
        uniform[1] = 1;
        uniform[2] = 1; // bank_swizzle,slices,mips
        uniform[3] = static_cast<uint32_t>(bytes);
        uniform[4] = pitch;
        uniform[5] = paddedHeight;
        uniform[6] = 0;
        timing.Next("record");
        const VkDescriptorPoolSize sizes[] = {{VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 2},
                                              {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1}};
        VkDescriptorPoolCreateInfo pi{};
        pi.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
        pi.maxSets = 1;
        pi.poolSizeCount = 2;
        pi.pPoolSizes = sizes;
        if (vkCreateDescriptorPool(device, &pi, nullptr, &descriptors) != VK_SUCCESS) return 91;
        const VkDescriptorSetLayout layouts[] = {descriptorLayout};
        VkDescriptorSet sets[1]{};
        VkDescriptorSetAllocateInfo di{};
        di.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        di.descriptorPool = descriptors;
        di.descriptorSetCount = 1;
        di.pSetLayouts = layouts;
        if (vkAllocateDescriptorSets(device, &di, sets) != VK_SUCCESS) return 91;
        VkDescriptorBufferInfo infos[3]{};
        VkWriteDescriptorSet writes[3]{};
        // Both shaders binding0=tiled, binding1=linear; direction is compiled.
        const uint32_t indices[] = {tile ? 2u : 1u, tile ? 1u : 2u, 3};
        for (uint32_t i = 0; i < 3; ++i) {
            infos[i] = {buffers[indices[i]], 0, i % 3 == 2 ? 268 : bytes};
            writes[i].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            writes[i].dstSet = sets[i / 3];
            writes[i].dstBinding = i % 3;
            writes[i].descriptorCount = 1;
            writes[i].pBufferInfo = &infos[i];
            writes[i].descriptorType =
                i % 3 == 2 ? VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER : VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        }
        vkUpdateDescriptorSets(device, 3, writes, 0, nullptr);
        VkCommandPoolCreateInfo ci{};
        ci.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
        ci.queueFamilyIndex = family;
        if (vkCreateCommandPool(device, &ci, nullptr, &commands) != VK_SUCCESS) return 91;
        VkCommandBufferAllocateInfo ca{};
        ca.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        ca.commandPool = commands;
        ca.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        ca.commandBufferCount = 1;
        VkCommandBuffer command{};
        if (vkAllocateCommandBuffers(device, &ca, &command) != VK_SUCCESS) return 91;
        VkCommandBufferBeginInfo begin{};
        begin.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        begin.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
        if (vkBeginCommandBuffer(command, &begin) != VK_SUCCESS) return 91;
        VkMemoryBarrier barrier{};
        barrier.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
        barrier.srcAccessMask = VK_ACCESS_HOST_WRITE_BIT;
        barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT;
        vkCmdPipelineBarrier(command, VK_PIPELINE_STAGE_HOST_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 1, &barrier,
                             0, nullptr, 0, nullptr);
        for (uint32_t i = 0; i < 1; ++i) {
            vkCmdBindPipeline(command, VK_PIPELINE_BIND_POINT_COMPUTE, pipeline);
            vkCmdBindDescriptorSets(command, VK_PIPELINE_BIND_POINT_COMPUTE, layout, 0, 1, &sets[i], 0, nullptr);
            vkCmdDispatch(command, tall16 ? 32768u : texels / 64, tall16 ? 8u : samples,
                          1); // Exact aligned domain; public shader/adapter has no tail bound check.
            barrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
            barrier.dstAccessMask = VK_ACCESS_HOST_READ_BIT;
            vkCmdPipelineBarrier(command, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_HOST_BIT, 0, 1,
                                 &barrier, 0, nullptr, 0, nullptr);
        }
        if (vkEndCommandBuffer(command) != VK_SUCCESS) return 91;
        VkFenceCreateInfo fi{};
        fi.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
        if (vkCreateFence(device, &fi, nullptr, &fence) != VK_SUCCESS) return 91;
        VkSubmitInfo si{};
        si.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        si.commandBufferCount = 1;
        si.pCommandBuffers = &command;
        timing.Next("submit");
        if (vkQueueSubmit(queue, 1, &si, fence) != VK_SUCCESS) return 91;
        submitted = true;
        timing.Next("fence_wait");
        const auto waited = vkWaitForFences(device, 1, &fence, VK_TRUE, 5000000000ULL);
        if (waited != VK_SUCCESS) return waited == VK_TIMEOUT ? 52 : 91;
        timing.Next("validate_output_copy");
        // Guarded backing exceeds the descriptor range, exposing accidental writes.
        for (uint32_t i = 1; i < 3; ++i)
            for (size_t j = 0; j < 256; ++j)
                if (static_cast<unsigned char*>(mapped[i])[static_cast<size_t>(bytes) + j] != 0xd3) return 151;
        if (std::memcmp(mapped[1], source, static_cast<size_t>(bytes))) return 151;
        for (uint32_t i = 0; i < 67; ++i) {
            const uint32_t expected = i == 1 || i == 2 ? 1U
                                      : i == 3         ? static_cast<uint32_t>(bytes)
                                      : i == 4         ? pitch
                                      : i == 5         ? paddedHeight
                                                       : 0U;
            if (uniform[i] != expected) return 151;
        }
        std::memcpy(converted.data(), mapped[2], static_cast<size_t>(bytes));
        output.swap(converted);
        return 0;
    };
    const int result = run();
    timing.status = result;
    timing.Next("cleanup_drain");
    if (submitted && result) vkQueueWaitIdle(queue);
    if (fence) vkDestroyFence(device, fence, nullptr);
    if (commands) vkDestroyCommandPool(device, commands, nullptr);
    if (descriptors) vkDestroyDescriptorPool(device, descriptors, nullptr);
    for (uint32_t i = 1; i < 4; ++i) {
        if (mapped[i]) vkUnmapMemory(device, allocations[i]);
        if (buffers[i]) vkDestroyBuffer(device, buffers[i], nullptr);
        if (allocations[i]) vkFreeMemory(device, allocations[i], nullptr);
    }
    return result;
}
