// SPDX-License-Identifier: GPL-2.0-only
#pragma once
#include "graphics_companion.h"
#include <vulkan/vulkan.h>
namespace CtrPm4 {
// Public SurfaceFormats RGBA8 entries; device query checks requested usage,
// not guest tiling, writable ownership, layout transitions or pipeline admission.
inline int PreflightColorFormat(const CtrGraphicsColorRequirementV1& requirement, VkPhysicalDevice physical,
                                PFN_vkGetPhysicalDeviceFormatProperties query, bool blending, VkFormat& output) {
    if (requirement.version != 1 || requirement.size != sizeof(requirement) || !physical) return 60;
    if (!query || (requirement.data_format != 10 && requirement.data_format != 3 && requirement.data_format != 4))
        return 88;
    if (requirement.data_format == 3 && requirement.number_format != 0) return 88;
    VkFormat format;
    if (requirement.data_format == 4) {
        if (requirement.number_format != 7) return 88;
        format = VK_FORMAT_R32_SFLOAT;
    } else
        switch (requirement.number_format) {
        case 0: format = requirement.data_format == 3 ? VK_FORMAT_R8G8_UNORM : VK_FORMAT_R8G8B8A8_UNORM; break;
        case 1: format = VK_FORMAT_R8G8B8A8_SNORM; break;
        case 4: format = VK_FORMAT_R8G8B8A8_UINT; break;
        case 5: format = VK_FORMAT_R8G8B8A8_SINT; break;
        case 9: format = VK_FORMAT_R8G8B8A8_SRGB; break;
        default: return 88;
        }
    VkFormatProperties properties{};
    query(physical, format, &properties);
    const VkFormatFeatureFlags needed = VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BIT | VK_FORMAT_FEATURE_TRANSFER_SRC_BIT |
                                        VK_FORMAT_FEATURE_TRANSFER_DST_BIT |
                                        (blending ? VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BLEND_BIT : 0);
    if ((properties.optimalTilingFeatures & needed) != needed) return 88;
    output = format;
    return 0;
}
inline int PreflightColorImage(const CtrGraphicsColorRequirementV1& requirement, VkPhysicalDevice physical,
                               PFN_vkGetPhysicalDeviceFormatProperties formatQuery,
                               PFN_vkGetPhysicalDeviceImageFormatProperties imageQuery, bool blending,
                               VkImageCreateInfo& output, bool sampledBacking = false) {
    if (!requirement.pitch || !requirement.height || !requirement.layers || !requirement.samples ||
        (requirement.samples & (requirement.samples - 1)) || requirement.samples > 64)
        return 60;
    VkFormat format = VK_FORMAT_UNDEFINED;
    const int status = PreflightColorFormat(requirement, physical, formatQuery, blending, format);
    if (status) return status;
    if (!imageQuery) return 88;
    if (sampledBacking) {
        VkFormatProperties sampled{};
        formatQuery(physical, format, &sampled);
        if (!(sampled.optimalTilingFeatures & VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT)) return 88;
    }
    const VkImageUsageFlags usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT |
                                    VK_IMAGE_USAGE_TRANSFER_DST_BIT | (sampledBacking ? VK_IMAGE_USAGE_SAMPLED_BIT : 0);
    VkImageFormatProperties properties{};
    if (imageQuery(physical, format, VK_IMAGE_TYPE_2D, VK_IMAGE_TILING_OPTIMAL, usage, 0, &properties) != VK_SUCCESS)
        return 88;
    if (requirement.pitch > properties.maxExtent.width || requirement.height > properties.maxExtent.height ||
        properties.maxExtent.depth < 1 || properties.maxMipLevels < 1 ||
        requirement.layers > properties.maxArrayLayers || !(properties.sampleCounts & requirement.samples))
        return 88;
    VkImageCreateInfo prepared{VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};
    prepared.imageType = VK_IMAGE_TYPE_2D;
    prepared.format = format;
    prepared.extent = {requirement.pitch, requirement.height, 1};
    prepared.mipLevels = 1;
    prepared.arrayLayers = requirement.layers;
    prepared.samples = VkSampleCountFlagBits(requirement.samples);
    prepared.tiling = VK_IMAGE_TILING_OPTIMAL;
    prepared.usage = usage;
    prepared.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    prepared.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    output = prepared;
    return 0;
}
}
