// SPDX-License-Identifier: GPL-2.0-only
#pragma once
#include "graphics_companion.h"
#include "bc1_layout.hpp"
#include <vulkan/vulkan.h>
namespace CtrPm4 {
// Public RGBA8 mapping; transfer staging requires one sample and one mip here.
// Public LiverpoolToVK ComponentSwizzle and RGBA8 RemapSwizzle policy:
// engine/shadPS4/src/video_core/{renderer_vulkan/liverpool_to_vk.cpp,amdgpu/pixel_format.h}.
inline int SampledRgba8Components(const uint32_t* selectors,VkComponentMapping& output) {
 if(!selectors)return 60;
 VkComponentSwizzle channels[4]{};
 for(unsigned i=0;i<4;++i) {
  switch(selectors[i]) {
  case 0:channels[i]=VK_COMPONENT_SWIZZLE_ZERO;break;
  case 1:channels[i]=VK_COMPONENT_SWIZZLE_ONE;break;
  case 4:channels[i]=VK_COMPONENT_SWIZZLE_R;break;
  case 5:channels[i]=VK_COMPONENT_SWIZZLE_G;break;
  case 6:channels[i]=VK_COMPONENT_SWIZZLE_B;break;
  case 7:channels[i]=VK_COMPONENT_SWIZZLE_A;break;
  default:return 88;
  }
 }
 output={channels[0],channels[1],channels[2],channels[3]};return 0;
}
inline int PreflightSampledImage(const CtrGraphicsImageParametersV1& p,VkPhysicalDevice physical,
 PFN_vkGetPhysicalDeviceFormatProperties formatQuery,PFN_vkGetPhysicalDeviceImageFormatProperties imageQuery,
 VkImageCreateInfo& output) {
 if(p.version!=1 || p.size!=sizeof(p) || !physical || !p.width || !p.height)return 60;
 const bool r16=p.data_format==2 && p.number_format==0;
 const bool r8=p.data_format==1 && p.number_format==0;
 const bool bc3=p.data_format==37 && p.number_format==0;
 const bool bc1=p.data_format==35;
 Bc1Layout bc1Layout{};if(bc1){const int rc=Bc1LayoutFor(p,bc1Layout);if(rc)return rc;}
 const bool r32=p.data_format==4 && p.number_format==7;
 const bool rg16=p.data_format==5 && p.number_format==7;
 const bool oneD=p.type==8;
 if(r16 && (p.type!=9 || p.width!=2048 || p.height!=8192 || p.pitch!=2048 || p.tile_mode!=0 || p.pow2pad || p.swizzle[0]!=4 || p.swizzle[1]!=4 || p.swizzle[2]!=4 || p.swizzle[3]!=4))return 88;
 if(r8 && (p.type!=9 || p.tile_mode!=8 || (p.width!=1024 && p.width!=2048) || p.height!=p.width || p.pitch!=p.width || p.pow2pad ||
    p.swizzle[0]!=0 || p.swizzle[1]!=0 || p.swizzle[2]!=0 || p.swizzle[3]!=4))return 88;
 if(oneD && (!((p.width==64 && p.pitch==128) || (p.width==1024 && p.pitch==1024)) || p.height!=1 || p.data_format!=10 || p.number_format ||
    p.tile_mode!=14 || p.pow2pad || p.swizzle[0]!=6 || p.swizzle[1]!=5 || p.swizzle[2]!=4 || p.swizzle[3]!=7))return 88;
 if(rg16 && (p.width!=64 || p.height!=64 || p.pitch!=64 || p.tile_mode!=13 || p.pow2pad ||
    p.swizzle[0]!=4 || p.swizzle[1]!=5 || p.swizzle[2]!=0 || p.swizzle[3]!=1))return 88;
 if(r32 && (!((p.width==1920 && p.height==1080 && p.pitch==1920) || (p.width==960 && p.height==540 && p.pitch==1024)) || p.tile_mode!=14 || p.pow2pad ||
    p.swizzle[0]!=4 || p.swizzle[1]!=0 || p.swizzle[2]!=0 || p.swizzle[3]!=1))return 88;
 if(bc3 && (p.tile_mode!=13 || p.pow2pad>1 || !p.pitch || p.pitch<p.width ||
    p.width>4096 || p.height>4096 || p.pitch>4096 || !p.guest_address || !p.levels || p.levels>2 ||
    (p.levels==2 && p.width<2 && p.height<2)))return 88;
 if(!formatQuery || !imageQuery || (!oneD && !bc1 && p.type!=9) || (!r16 && !r8 && !bc1 && !bc3 && !r32 && !rg16 && ((p.data_format!=10 && p.data_format!=3) || p.number_format!=0)) ||
    p.depth!=1 || (!bc1 && p.layers!=1) || (!bc1 && !bc3 && p.levels!=1) || p.samples!=1 || p.compression || p.alt_tile ||
    p.base_array || (!bc1 && p.last_array) || p.base_level)return 88;
 const VkFormat format=r16?VK_FORMAT_R16_UNORM:r8?VK_FORMAT_R8_UNORM:bc1?VK_FORMAT_BC1_RGBA_UNORM_BLOCK:bc3?VK_FORMAT_BC3_UNORM_BLOCK:rg16?VK_FORMAT_R16G16_SFLOAT:r32?VK_FORMAT_R32_SFLOAT:(p.data_format==3?VK_FORMAT_R8G8_UNORM:VK_FORMAT_R8G8B8A8_UNORM);
 VkFormatProperties properties{};formatQuery(physical,format,&properties);
 const auto needed=VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT|VK_FORMAT_FEATURE_TRANSFER_DST_BIT|VK_FORMAT_FEATURE_TRANSFER_SRC_BIT;
 if((properties.optimalTilingFeatures&needed)!=needed)return 88;
 VkImageFormatProperties image{};
 const VkImageUsageFlags usage=VK_IMAGE_USAGE_SAMPLED_BIT|VK_IMAGE_USAGE_TRANSFER_DST_BIT|VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
 const auto imageType=oneD?VK_IMAGE_TYPE_1D:VK_IMAGE_TYPE_2D;
 if(imageQuery(physical,format,imageType,VK_IMAGE_TILING_OPTIMAL,usage,0,&image)!=VK_SUCCESS ||
    image.maxExtent.width<p.width || image.maxExtent.height<p.height || !image.maxExtent.depth ||
    image.maxMipLevels<p.levels || image.maxArrayLayers<p.layers || !(image.sampleCounts&VK_SAMPLE_COUNT_1_BIT))return 88;
 VkImageCreateInfo prepared{};prepared.sType=VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
 prepared.imageType=imageType;prepared.format=format;
 prepared.extent={p.width,p.height,1};prepared.mipLevels=p.levels;prepared.arrayLayers=p.layers;
 prepared.samples=VK_SAMPLE_COUNT_1_BIT;prepared.tiling=VK_IMAGE_TILING_OPTIMAL;
 prepared.usage=usage;prepared.sharingMode=VK_SHARING_MODE_EXCLUSIVE;prepared.initialLayout=VK_IMAGE_LAYOUT_UNDEFINED;
 output=prepared;return 0;
}
}
