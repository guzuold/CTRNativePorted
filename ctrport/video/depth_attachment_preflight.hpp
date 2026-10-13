// SPDX-License-Identifier: GPL-2.0-only
#pragma once
#include <vulkan/vulkan.h>
#include <cstdint>
namespace CtrPm4 {
// Public format mapping: shadPS4 renderer_vulkan/liverpool_to_vk.cpp DepthFormat.
// This validates Vulkan backing only, never guest tiling, HTILE or initialized content.
struct DepthAttachmentShape {uint32_t width,height,samples,z_format,stencil_format;};
inline int PreflightDepthAttachment(const DepthAttachmentShape& shape,VkPhysicalDevice physical,
 PFN_vkGetPhysicalDeviceFormatProperties formatQuery,PFN_vkGetPhysicalDeviceImageFormatProperties imageQuery,
 VkImageCreateInfo& output,VkImageAspectFlags& aspects) {
 if(!physical || !shape.width || !shape.height)return 60;
 if(!formatQuery || !imageQuery)return 88;
 VkFormat format=VK_FORMAT_UNDEFINED;VkImageAspectFlags mask=0;
 if(shape.stencil_format>1)return 88;
 switch(shape.z_format) {
 case 0:if(shape.stencil_format)format=VK_FORMAT_D32_SFLOAT_S8_UINT;break;
 case 1:format=shape.stencil_format?VK_FORMAT_D16_UNORM_S8_UINT:VK_FORMAT_D16_UNORM;mask|=VK_IMAGE_ASPECT_DEPTH_BIT;break;
 case 3:format=shape.stencil_format?VK_FORMAT_D32_SFLOAT_S8_UINT:VK_FORMAT_D32_SFLOAT;mask|=VK_IMAGE_ASPECT_DEPTH_BIT;break;
 default:return 88;
 }
 if(shape.stencil_format)mask|=VK_IMAGE_ASPECT_STENCIL_BIT;
 if(format==VK_FORMAT_UNDEFINED)return 88;
 switch(shape.samples){case 1:case 2:case 4:case 8:case 16:case 32:case 64:break;default:return 88;}
 VkFormatProperties properties{};formatQuery(physical,format,&properties);
 // Z32/stencil sampled aliases retain the genuine owner for sample1 or MS2 consumers.
 const bool sampled=((shape.samples==1 || shape.samples==2) && shape.z_format==3 && shape.stencil_format==1) ||
   (shape.samples==1 && shape.z_format==1 && !shape.stencil_format && shape.width==2048 && shape.height==8192);
 const bool download=shape.samples==1 && shape.z_format==1 && !shape.stencil_format;
 const VkFormatFeatureFlags needed=VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT|VK_FORMAT_FEATURE_TRANSFER_DST_BIT|
   (sampled?VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT:0)|(download?VK_FORMAT_FEATURE_TRANSFER_SRC_BIT:0);
 if((properties.optimalTilingFeatures&needed)!=needed)return 88;
 VkImageFormatProperties limits{};
 const VkImageUsageFlags usage=VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT|VK_IMAGE_USAGE_TRANSFER_DST_BIT|
   (sampled?VK_IMAGE_USAGE_SAMPLED_BIT:0)|(download?VK_IMAGE_USAGE_TRANSFER_SRC_BIT:0);
 if(imageQuery(physical,format,VK_IMAGE_TYPE_2D,VK_IMAGE_TILING_OPTIMAL,usage,0,&limits)!=VK_SUCCESS ||
    limits.maxExtent.width<shape.width || limits.maxExtent.height<shape.height || !limits.maxExtent.depth ||
    !limits.maxMipLevels || !limits.maxArrayLayers || !(limits.sampleCounts&shape.samples))return 88;
 VkImageCreateInfo prepared{};prepared.sType=VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
 prepared.imageType=VK_IMAGE_TYPE_2D;prepared.format=format;prepared.extent={shape.width,shape.height,1};
 prepared.mipLevels=1;prepared.arrayLayers=1;prepared.samples=static_cast<VkSampleCountFlagBits>(shape.samples);
 prepared.tiling=VK_IMAGE_TILING_OPTIMAL;prepared.usage=usage;prepared.sharingMode=VK_SHARING_MODE_EXCLUSIVE;
 prepared.initialLayout=VK_IMAGE_LAYOUT_UNDEFINED;
 output=prepared;aspects=mask;return 0;
}
}
