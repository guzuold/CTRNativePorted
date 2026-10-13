// SPDX-License-Identifier: GPL-2.0-only
#pragma once
#include "graphics_companion.h"
#include <vulkan/vulkan.h>
#include <algorithm>
#include <cmath>
namespace CtrPm4 {
// Public LiverpoolToVK and sampler.cpp mappings; no custom-border fallback.
inline int PreflightSampler(const CtrGraphicsSamplerParametersV1& p,
 const VkPhysicalDeviceLimits& limits,bool anisotropyEnabled,VkSamplerCreateInfo& output) {
 if(p.version!=1 || p.size!=sizeof(p) || p.mag_filter>3 || p.min_filter>3 ||
    p.mip_filter>2 || p.compare_func>7 || p.border_color>2 || p.anisotropic>1 ||
    p.unnormalized_in_shader>1 || !std::isfinite(p.lod_bias) || !std::isfinite(p.min_lod) ||
    !std::isfinite(p.max_lod) || !std::isfinite(p.max_anisotropy) ||
    p.min_lod<0 || p.max_lod<p.min_lod || p.max_anisotropy<1)return 60;
 auto clamp=[](uint32_t value,VkSamplerAddressMode& mode)->bool {
    switch(value) {
    case 0:mode=VK_SAMPLER_ADDRESS_MODE_REPEAT;return true;
    case 1:mode=VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT;return true;
    case 2:mode=VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;return true;
    case 6:mode=VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;return true;
    default:return false; // mirrored clamp feature and approximate half-border pending
    }
 };
 VkSamplerCreateInfo prepared{};prepared.sType=VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
 if(!clamp(p.clamp_x,prepared.addressModeU) || !clamp(p.clamp_y,prepared.addressModeV) ||
    !clamp(p.clamp_z,prepared.addressModeW))return 88;
 const bool aniso=p.mag_filter>=2 || p.min_filter>=2;
 if(aniso!=bool(p.anisotropic))return 60;
 if((aniso && (!anisotropyEnabled || limits.maxSamplerAnisotropy<1)) ||
    !std::isfinite(limits.maxSamplerLodBias) || limits.maxSamplerLodBias<0 ||
    p.lod_bias < -limits.maxSamplerLodBias)return 88;
 prepared.magFilter=(p.mag_filter&1)?VK_FILTER_LINEAR:VK_FILTER_NEAREST;
 prepared.minFilter=(p.min_filter&1)?VK_FILTER_LINEAR:VK_FILTER_NEAREST;
 prepared.mipmapMode=p.mip_filter==2?VK_SAMPLER_MIPMAP_MODE_LINEAR:VK_SAMPLER_MIPMAP_MODE_NEAREST;
 prepared.mipLodBias=std::min(p.lod_bias,limits.maxSamplerLodBias);
 prepared.anisotropyEnable=aniso;prepared.maxAnisotropy=aniso?std::min(p.max_anisotropy,limits.maxSamplerAnisotropy):1;
 prepared.compareEnable=p.compare_func!=0;prepared.compareOp=static_cast<VkCompareOp>(p.compare_func);
 prepared.minLod=p.min_lod;prepared.maxLod=p.max_lod;
 prepared.borderColor=p.border_color==0?VK_BORDER_COLOR_FLOAT_TRANSPARENT_BLACK:
    p.border_color==1?VK_BORDER_COLOR_FLOAT_OPAQUE_BLACK:VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE;
 prepared.unnormalizedCoordinates=VK_FALSE; // public compiler handles this in shader
 output=prepared;return 0;
}
}
