// SPDX-License-Identifier: GPL-2.0-only
#pragma once
#include "session_api.h"
#include <memory>
// Actual Vulkan image backing for the reached standard SDR display surface.
// No guest mapping ownership, contents, PM4 submission or flip completion implied.
class CtrTiledSurface {
    VkDevice device_{}; VkImage image_{}; VkDeviceMemory memory_{};
    PFN_vkDestroyImage destroy_{}; PFN_vkFreeMemory free_{};
public:
    uint64_t guestAddress=0, guestBytes=0;
    uint32_t width=0,height=0,paddedPitch=0,paddedHeight=0;
    CtrTiledSurface()=default;
    CtrTiledSurface(const CtrTiledSurface&)=delete;
    CtrTiledSurface& operator=(const CtrTiledSurface&)=delete;
    ~CtrTiledSurface() {
        if(image_) destroy_(device_,image_,nullptr);
        if(memory_) free_(device_,memory_,nullptr);
    }
    VkImage Image() const {return image_;}
    static int Create(VkDevice device,PFN_vkGetDeviceProcAddr get,
        const VkPhysicalDeviceMemoryProperties& properties,
        const CtrVideoBufferAttributesV1& a,uint64_t address,
        std::unique_ptr<CtrTiledSurface>& output) {
        if(!device || !get || !address) return 60;
        // Exact reached configuration only; Neo/HDR/other modes remain unsupported.
        if(a.tiling!=0 || a.format!=0x80000000U) return 88;
        if(a.aspect || a.option || a.reserved0 || a.reserved1 ||
           a.width!=1920 || a.height!=1080 || a.pitch_pixels!=1920) return 89;
        constexpr uint64_t bytes=uint64_t(1920)*1088*4;
        if(address>UINT64_MAX-bytes) return 112;
        const auto create=reinterpret_cast<PFN_vkCreateImage>(get(device,"vkCreateImage"));
        const auto requirements=reinterpret_cast<PFN_vkGetImageMemoryRequirements>(get(device,"vkGetImageMemoryRequirements"));
        const auto allocate=reinterpret_cast<PFN_vkAllocateMemory>(get(device,"vkAllocateMemory"));
        const auto bind=reinterpret_cast<PFN_vkBindImageMemory>(get(device,"vkBindImageMemory"));
        const auto destroy=reinterpret_cast<PFN_vkDestroyImage>(get(device,"vkDestroyImage"));
        const auto free=reinterpret_cast<PFN_vkFreeMemory>(get(device,"vkFreeMemory"));
        if(!create || !requirements || !allocate || !bind || !destroy || !free) return 88;
        auto value=std::make_unique<CtrTiledSurface>();
        value->device_=device;value->destroy_=destroy;value->free_=free;
        VkImageCreateInfo image{}; image.sType=VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
        image.imageType=VK_IMAGE_TYPE_2D; image.format=VK_FORMAT_R8G8B8A8_SRGB;
        image.extent={a.width,a.height,1};image.mipLevels=1;image.arrayLayers=1;
        image.samples=VK_SAMPLE_COUNT_1_BIT;image.tiling=VK_IMAGE_TILING_OPTIMAL;
        image.usage=VK_IMAGE_USAGE_TRANSFER_SRC_BIT|VK_IMAGE_USAGE_TRANSFER_DST_BIT|
                    VK_IMAGE_USAGE_SAMPLED_BIT|VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
        image.sharingMode=VK_SHARING_MODE_EXCLUSIVE;image.initialLayout=VK_IMAGE_LAYOUT_UNDEFINED;
        if(create(device,&image,nullptr,&value->image_)!=VK_SUCCESS) return 90;
        VkMemoryRequirements needed{};requirements(device,value->image_,&needed);
        uint32_t type=0;
        for(;type<properties.memoryTypeCount;++type)
            if((needed.memoryTypeBits&(1U<<type)) &&
               (properties.memoryTypes[type].propertyFlags&VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT)) break;
        if(type==properties.memoryTypeCount) return 88;
        VkMemoryAllocateInfo allocation{};allocation.sType=VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        allocation.allocationSize=needed.size;allocation.memoryTypeIndex=type;
        if(allocate(device,&allocation,nullptr,&value->memory_)!=VK_SUCCESS ||
           bind(device,value->image_,value->memory_,0)!=VK_SUCCESS) return 90;
        value->guestAddress=address;value->guestBytes=bytes;value->width=a.width;
        value->height=a.height;value->paddedPitch=1920;value->paddedHeight=1088;
        output=std::move(value);return 0;
    }
};
