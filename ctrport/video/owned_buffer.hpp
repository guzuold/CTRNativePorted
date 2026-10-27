// SPDX-License-Identifier: GPL-2.0-only
#pragma once
#include <vulkan/vulkan.h>
#include <memory>
#include <cstring>
// Immutable initialized host-coherent GPU buffer. No guest address is retained.
// No submission or raw handle export; future executor must retain owner to fence.
class CtrOwnedBuffer {
    VkDevice device_{};
    VkBuffer buffer_{};
    VkDeviceMemory memory_{};
    void* mapped_{};
    uint64_t bytes_{};
    VkBufferUsageFlags usage_{};
    uint32_t indexBytes_{};
    PFN_vkDestroyBuffer destroy_{};
    PFN_vkFreeMemory free_{};
    PFN_vkUnmapMemory unmap_{};

public:
    // Immutable mapped indices are the authority for vertex range admission. Handles remain internal.
    int IndexBuffer(uint32_t indexBytes, uint64_t offset, uint32_t first, uint32_t count, VkBuffer& output,
                    uint32_t& minimum, uint32_t& maximum) const {
        if (!(usage_ & VK_BUFFER_USAGE_INDEX_BUFFER_BIT) || indexBytes != indexBytes_) return 88;
        if (!count || (indexBytes != 2 && indexBytes != 4) || offset % indexBytes || offset > bytes_) return 60;
        const uint64_t end = (uint64_t(first) + count) * indexBytes;
        if (end > bytes_ - offset) return 88;
        uint32_t lo = UINT32_MAX, hi = 0;
        const auto* data = static_cast<const unsigned char*>(mapped_) + offset + uint64_t(first) * indexBytes;
        for (uint32_t i = 0; i < count; ++i) {
            uint32_t value = 0;
            std::memcpy(&value, data + uint64_t(i) * indexBytes, indexBytes);
            lo = value < lo ? value : lo;
            hi = value > hi ? value : hi;
        }
        output = buffer_;
        minimum = lo;
        maximum = hi;
        return 0;
    }
    ~CtrOwnedBuffer() {
        if (mapped_) unmap_(device_, memory_);
        if (buffer_) destroy_(device_, buffer_, nullptr);
        if (memory_) free_(device_, memory_, nullptr);
    }
    int VertexBuffer(uint64_t bytes, VkBuffer& output) const {
        if (!(usage_ & VK_BUFFER_USAGE_VERTEX_BUFFER_BIT)) return 88;
        if (bytes != bytes_) return 60;
        output = buffer_;
        return 0;
    }
    int StorageDescriptor(VkDescriptorBufferInfo& info) const {
        if (!(usage_ & VK_BUFFER_USAGE_STORAGE_BUFFER_BIT)) return 88;
        info = {buffer_, 0, bytes_};
        return 0;
    }
    int CopyInitialized(void* output, uint64_t bytes) const {
        if (!output || bytes != bytes_) return 60;
        std::memcpy(output, mapped_, static_cast<size_t>(bytes));
        return 0;
    }
    static int Create(VkDevice device, PFN_vkGetDeviceProcAddr get, const VkPhysicalDeviceMemoryProperties& properties,
                      const void* source, uint64_t bytes, VkBufferUsageFlags usage,
                      std::unique_ptr<CtrOwnedBuffer>& output, uint32_t indexBytes = 0) {
        if (!device || !get || !source || !bytes || bytes > 32ULL * 1024 * 1024) return 60;
        if ((usage & VK_BUFFER_USAGE_INDEX_BUFFER_BIT) && ((indexBytes != 2 && indexBytes != 4) || bytes % indexBytes))
            return 60;
        auto value = std::make_unique<CtrOwnedBuffer>();
        value->device_ = device;
        value->bytes_ = bytes;
        value->usage_ = usage;
        value->indexBytes_ = indexBytes;
        const auto create = reinterpret_cast<PFN_vkCreateBuffer>(get(device, "vkCreateBuffer"));
        const auto requirements =
            reinterpret_cast<PFN_vkGetBufferMemoryRequirements>(get(device, "vkGetBufferMemoryRequirements"));
        const auto allocate = reinterpret_cast<PFN_vkAllocateMemory>(get(device, "vkAllocateMemory"));
        const auto bind = reinterpret_cast<PFN_vkBindBufferMemory>(get(device, "vkBindBufferMemory"));
        const auto map = reinterpret_cast<PFN_vkMapMemory>(get(device, "vkMapMemory"));
        value->destroy_ = reinterpret_cast<PFN_vkDestroyBuffer>(get(device, "vkDestroyBuffer"));
        value->free_ = reinterpret_cast<PFN_vkFreeMemory>(get(device, "vkFreeMemory"));
        value->unmap_ = reinterpret_cast<PFN_vkUnmapMemory>(get(device, "vkUnmapMemory"));
        if (!create || !requirements || !allocate || !bind || !map || !value->destroy_ || !value->free_ ||
            !value->unmap_)
            return 88;
        VkBufferCreateInfo info{};
        info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        info.size = bytes;
        info.usage = usage;
        info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        if (create(device, &info, nullptr, &value->buffer_) != VK_SUCCESS) return 90;
        VkMemoryRequirements needed{};
        requirements(device, value->buffer_, &needed);
        uint32_t type = 0;
        const auto flags = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
        for (; type < properties.memoryTypeCount; ++type)
            if ((needed.memoryTypeBits & (1U << type)) && (properties.memoryTypes[type].propertyFlags & flags) == flags)
                break;
        if (type == properties.memoryTypeCount) return 88;
        VkMemoryAllocateInfo ai{};
        ai.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        ai.allocationSize = needed.size;
        ai.memoryTypeIndex = type;
        if (allocate(device, &ai, nullptr, &value->memory_) != VK_SUCCESS ||
            bind(device, value->buffer_, value->memory_, 0) != VK_SUCCESS ||
            map(device, value->memory_, 0, bytes, 0, &value->mapped_) != VK_SUCCESS)
            return 90;
        std::memcpy(value->mapped_, source, static_cast<size_t>(bytes));
        output = std::move(value);
        return 0;
    }
};
