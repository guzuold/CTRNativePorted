// SPDX-License-Identifier: GPL-2.0-only
#pragma once
#include "sampled_image_preflight.hpp"
#include <memory>
#include "depth_attachment_owner.hpp"
#include "sampled_image_upload.hpp"
// Owns sampled image and layout; linear uploads are synchronous. No guest alias or shader binding.
class CtrSampledImageOwner {
    std::shared_ptr<CtrDepthAttachmentOwner> depthAlias_;
    bool storage_ = false;
    VkDevice device_{};
    VkImage image_{};
    VkDeviceMemory memory_{};
    VkImageView view_{};
    VkExtent3D extent_{};
    VkImageLayout layout_ = VK_IMAGE_LAYOUT_UNDEFINED;
    bool failed_ = false;
    PFN_vkDestroyImageView destroyView_{};
    PFN_vkDestroyImage destroy_{};
    PFN_vkFreeMemory free_{};

public:
    CtrGraphicsImageParametersV1 parameters{}; // Immutable session admission metadata, copied at creation.
    CtrSampledImageOwner() = default;
    CtrSampledImageOwner(const CtrSampledImageOwner&) = delete;
    CtrSampledImageOwner& operator=(const CtrSampledImageOwner&) = delete;
    ~CtrSampledImageOwner() {
        if (view_) destroyView_(device_, view_, nullptr);
        if (image_) destroy_(device_, image_, nullptr);
        if (memory_) free_(device_, memory_, nullptr);
    }
    static std::shared_ptr<CtrSampledImageOwner> AliasDepth(const std::shared_ptr<CtrDepthAttachmentOwner>& depth) {
        auto value = std::make_shared<CtrSampledImageOwner>();
        value->depthAlias_ = depth;
        return value;
    }
    bool IsStorage() const { return storage_; }
    int StorageDescriptor(VkDescriptorImageInfo& output) const {
        if (!storage_) return 88;
        if (failed_ || layout_ != VK_IMAGE_LAYOUT_GENERAL) return 91;
        output = {VK_NULL_HANDLE, view_, layout_};
        return 0;
    }
    int SampledDescriptor(VkDescriptorImageInfo& output) const {
        if (depthAlias_) return depthAlias_->SampledDescriptor(output);
        if (storage_) return 88;
        if (failed_ || layout_ != VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL) return 91;
        output = {VK_NULL_HANDLE, view_, layout_};
        return 0;
    }
    int Upload(VkDevice device, PFN_vkGetDeviceProcAddr get, uint32_t family,
               const VkPhysicalDeviceMemoryProperties& memory, const void* source, uint64_t bytes, uint64_t pitch) {
        if (depthAlias_ || (parameters.data_format == 37 || parameters.data_format == 35)) return 88;
        if (failed_) return 91;
        bool completed = false;
        const auto finalLayout = storage_ ? VK_IMAGE_LAYOUT_GENERAL : VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        const int status = TransferSampledLinear(device, get, family, memory, image_, extent_, layout_, source, nullptr,
                                                 bytes, pitch, completed, finalLayout,
                                                 parameters.data_format == 1                                    ? 1u
                                                 : (parameters.data_format == 2 || parameters.data_format == 3) ? 2u
                                                                                                                : 4u);
        if (completed)
            layout_ = finalLayout;
        else if (status != 60 && status != 88)
            failed_ = true;
        return status;
    }
    int Readback(VkDevice device, PFN_vkGetDeviceProcAddr get, uint32_t family,
                 const VkPhysicalDeviceMemoryProperties& memory, void* destination, uint64_t bytes, uint64_t pitch) {
        if (depthAlias_ || (parameters.data_format == 37 || parameters.data_format == 35)) return 88;
        if (failed_ || layout_ == VK_IMAGE_LAYOUT_UNDEFINED) return 91;
        bool completed = false;
        const int status = TransferSampledLinear(
            device, get, family, memory, image_, extent_, layout_, nullptr, destination, bytes, pitch, completed,
            storage_ ? VK_IMAGE_LAYOUT_GENERAL : VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
            parameters.data_format == 1                                    ? 1u
            : (parameters.data_format == 2 || parameters.data_format == 3) ? 2u
                                                                           : 4u);
        if (status != 0 && status != 60 && status != 88) failed_ = true;
        return status;
    }
    // Internal complete-mip transaction; caller owns validated padded block buffers.
    int TransferBc3(VkDevice device, PFN_vkGetDeviceProcAddr get, uint32_t family,
                    const VkPhysicalDeviceMemoryProperties& memory, std::vector<std::vector<unsigned char>>& levels,
                    const std::vector<uint32_t>& pitches, bool readback) {
        const uint32_t layers = parameters.layers, blockBytes = parameters.data_format == 35 ? 8u : 16u;
        if ((parameters.data_format != 37 && parameters.data_format != 35) ||
            levels.size() != parameters.levels * layers || pitches.size() != parameters.levels)
            return 88;
        if (failed_ || (readback && layout_ == VK_IMAGE_LAYOUT_UNDEFINED)) return 91;
        for (uint32_t mip = 0; mip < parameters.levels; ++mip)
            for (uint32_t layer = 0; layer < layers; ++layer) {
                const size_t index = size_t(mip) * layers + layer;
                bool completed = false;
                const VkExtent3D e{std::max(extent_.width >> mip, 1u), std::max(extent_.height >> mip, 1u), 1};
                const int rc = TransferSampledLinear(
                    device, get, family, memory, image_, e, layout_, readback ? nullptr : levels[index].data(),
                    readback ? levels[index].data() : nullptr, levels[index].size(),
                    uint64_t(pitches[mip]) * blockBytes, completed, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                    blockBytes, VK_IMAGE_ASPECT_COLOR_BIT, true, mip, layer);
                if (rc) {
                    failed_ = true;
                    return rc;
                }
            }
        layout_ = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        return 0;
    }
    static int Create(VkDevice device, PFN_vkGetDeviceProcAddr get, const VkPhysicalDeviceMemoryProperties& properties,
                      const VkImageCreateInfo& info, const VkComponentMapping& components,
                      std::unique_ptr<CtrSampledImageOwner>& output) {
        if (!device || !get) return 60;
        const auto createView = reinterpret_cast<PFN_vkCreateImageView>(get(device, "vkCreateImageView"));
        const auto destroyView = reinterpret_cast<PFN_vkDestroyImageView>(get(device, "vkDestroyImageView"));
        const auto create = reinterpret_cast<PFN_vkCreateImage>(get(device, "vkCreateImage"));
        const auto requirements =
            reinterpret_cast<PFN_vkGetImageMemoryRequirements>(get(device, "vkGetImageMemoryRequirements"));
        const auto allocate = reinterpret_cast<PFN_vkAllocateMemory>(get(device, "vkAllocateMemory"));
        const auto bind = reinterpret_cast<PFN_vkBindImageMemory>(get(device, "vkBindImageMemory"));
        const auto destroy = reinterpret_cast<PFN_vkDestroyImage>(get(device, "vkDestroyImage"));
        const auto free = reinterpret_cast<PFN_vkFreeMemory>(get(device, "vkFreeMemory"));
        if (!createView || !destroyView || !create || !requirements || !allocate || !bind || !destroy || !free)
            return 88;
        auto value = std::make_unique<CtrSampledImageOwner>();
        value->storage_ = (info.usage & VK_IMAGE_USAGE_STORAGE_BIT) != 0;
        value->destroyView_ = destroyView;
        value->extent_ = info.extent;
        value->device_ = device;
        value->destroy_ = destroy;
        value->free_ = free;
        if (create(device, &info, nullptr, &value->image_) != VK_SUCCESS) return 90;
        VkMemoryRequirements needed{};
        requirements(device, value->image_, &needed);
        uint32_t type = 0;
        for (; type < properties.memoryTypeCount; ++type)
            if ((needed.memoryTypeBits & (1U << type)) &&
                (properties.memoryTypes[type].propertyFlags & VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT))
                break;
        if (type == properties.memoryTypeCount) return 88;
        VkMemoryAllocateInfo allocation{};
        allocation.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        allocation.allocationSize = needed.size;
        allocation.memoryTypeIndex = type;
        if (allocate(device, &allocation, nullptr, &value->memory_) != VK_SUCCESS ||
            bind(device, value->image_, value->memory_, 0) != VK_SUCCESS)
            return 90;
        VkImageViewCreateInfo vi{};
        vi.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        vi.image = value->image_;
        vi.viewType = info.imageType == VK_IMAGE_TYPE_1D ? VK_IMAGE_VIEW_TYPE_1D
                      : info.arrayLayers > 1             ? VK_IMAGE_VIEW_TYPE_2D_ARRAY
                                                         : VK_IMAGE_VIEW_TYPE_2D;
        vi.format = info.format;
        vi.components = components;
        vi.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, info.mipLevels, 0, info.arrayLayers == 8 ? 6u : 1u};
        if (createView(device, &vi, nullptr, &value->view_) != VK_SUCCESS) return 90;
        output = std::move(value);
        return 0;
    }
};
