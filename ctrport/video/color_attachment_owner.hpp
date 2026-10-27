// SPDX-License-Identifier: GPL-2.0-only
#pragma once
#include <vulkan/vulkan.h>
#include <memory>
#include "sampled_image_upload.hpp"
#include "graphics_draw_submit.hpp"
// Actual color allocation/view with explicit synchronous host RGBA8 initialization.
class CtrColorAttachmentOwner {
    VkDevice device_{};
    VkImage image_{};
    VkDeviceMemory memory_{};
    VkImageView view_{};
    VkFormat format_{};
    VkExtent3D extent_{};
    uint32_t layers_ = 0;
    VkSampleCountFlagBits samples_{};
    VkImageLayout layout_ = VK_IMAGE_LAYOUT_UNDEFINED;
    bool failed_ = false;
    PFN_vkDestroyImageView destroyView_{};
    PFN_vkDestroyImage destroy_{};
    PFN_vkFreeMemory free_{};
    std::unique_ptr<CtrColorAttachmentOwner> multisampleBacking_;

public:
    CtrColorAttachmentOwner* SampleBacking() const { return multisampleBacking_.get(); }
    VkImage Image() const { return image_; }
    VkImageView View() const { return view_; }
    VkExtent2D Extent() const { return {extent_.width, extent_.height}; }
    VkImageLayout Layout() const { return layout_; }
    bool Failed() const { return failed_; }
    void CopyCompleted(int status) {
        if (!status)
            layout_ = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        else if (status != 60 && status != 88)
            failed_ = true;
    }
    int AllocateSampleBacking(PFN_vkGetDeviceProcAddr get, const VkPhysicalDeviceMemoryProperties& memory,
                              const VkImageCreateInfo& info) {
        if (failed_) return 91;
        if (samples_ != VK_SAMPLE_COUNT_1_BIT || layers_ != 1 || info.samples != VK_SAMPLE_COUNT_2_BIT ||
            info.format != format_ || info.extent.width != extent_.width || info.extent.height != extent_.height ||
            !(info.usage & VK_IMAGE_USAGE_SAMPLED_BIT))
            return 88;
        if (multisampleBacking_) return 0;
        std::unique_ptr<CtrColorAttachmentOwner> value;
        const int rc = Create(device_, get, memory, info, VK_IMAGE_ASPECT_COLOR_BIT, value);
        if (rc) return rc;
        value->requirement = requirement;
        multisampleBacking_ = std::move(value);
        return 0;
    }
    int DrawBounded(PFN_vkGetDeviceProcAddr get, uint32_t family, VkPipeline pipeline, VkPipelineLayout layout,
                    VkDescriptorSet descriptors, const VkViewport& viewport, const VkRect2D& scissor,
                    const float* blend, uint32_t cull, uint32_t front, const void* push, uint32_t count,
                    uint32_t instances, uint32_t firstVertex, uint32_t firstInstance, uint32_t vertexCount = 0,
                    const uint32_t* bindings = nullptr, const VkBuffer* buffers = nullptr,
                    const VkDeviceSize* strides = nullptr, VkBuffer indexBuffer = VK_NULL_HANDLE,
                    VkIndexType indexType = VK_INDEX_TYPE_UINT16, VkDeviceSize indexOffset = 0, uint32_t firstIndex = 0,
                    int32_t vertexOffset = 0) {
        if (failed_ || layout_ != VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL) return 91;
        if (samples_ != VK_SAMPLE_COUNT_1_BIT || layers_ != 1) return 88;
        bool completed = false;
        const int status = DrawGraphicsSynchronous(
            device_, get, family, pipeline, layout, descriptors, image_, view_, {extent_.width, extent_.height},
            viewport, scissor, blend, cull, front, push, count, instances, firstVertex, firstInstance, completed,
            vertexCount, bindings, buffers, strides, VK_NULL_HANDLE, VK_NULL_HANDLE, nullptr, nullptr, VK_NULL_HANDLE,
            VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_NULL_HANDLE, VK_NULL_HANDLE,
            VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT, false, indexBuffer, indexType, indexOffset,
            firstIndex, vertexOffset);
        if (status && status != 60 && status != 88) failed_ = true;
        return status;
    }
    VkFormat Format() const { return format_; }
    CtrGraphicsColorRequirementV1 requirement{}; // Immutable original allocation demand.
    int Transfer(PFN_vkGetDeviceProcAddr get, uint32_t family, const VkPhysicalDeviceMemoryProperties& memory,
                 const void* source, void* destination, uint64_t bytes, uint64_t pitch) {
        if (failed_ || (destination && layout_ == VK_IMAGE_LAYOUT_UNDEFINED)) return 91;
        // Buffer-image copies cannot initialize multisampled attachments. Array uploads need explicit ranges.
        if (samples_ != VK_SAMPLE_COUNT_1_BIT || layers_ != 1) return 88;
        bool completed = false;
        const int result = TransferSampledLinear(
            device_, get, family, memory, image_, extent_, layout_, source, destination, bytes, pitch, completed,
            VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, format_ == VK_FORMAT_R8G8_UNORM ? 2u : 4u);
        if (completed) layout_ = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        if (result && result != 60 && result != 88) failed_ = true;
        return result;
    }
    ~CtrColorAttachmentOwner() {
        if (view_) destroyView_(device_, view_, nullptr);
        if (image_) destroy_(device_, image_, nullptr);
        if (memory_) free_(device_, memory_, nullptr);
    }
    static int Create(VkDevice device, PFN_vkGetDeviceProcAddr get, const VkPhysicalDeviceMemoryProperties& properties,
                      const VkImageCreateInfo& info, VkImageAspectFlags aspects,
                      std::unique_ptr<CtrColorAttachmentOwner>& output) {
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
        auto value = std::make_unique<CtrColorAttachmentOwner>();
        value->format_ = info.format;
        value->extent_ = info.extent;
        value->layers_ = info.arrayLayers;
        value->samples_ = info.samples;
        value->destroyView_ = destroyView;
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
        vi.viewType = info.arrayLayers == 1 ? VK_IMAGE_VIEW_TYPE_2D : VK_IMAGE_VIEW_TYPE_2D_ARRAY;
        vi.format = info.format;
        vi.subresourceRange = {aspects, 0, 1, 0, info.arrayLayers};
        if (createView(device, &vi, nullptr, &value->view_) != VK_SUCCESS) return 90;
        output = std::move(value);
        return 0;
    }
};
