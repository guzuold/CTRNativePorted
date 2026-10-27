// SPDX-License-Identifier: GPL-2.0-only
#pragma once
#include "sampled_image_owner.hpp"
#include <vector>
// Authored pixels after whole public Thin2DThin detiling; never a guest admission API.
static int ctr_thin_image_control(VkDevice device, PFN_vkGetDeviceProcAddr get, uint32_t family,
                                  const VkPhysicalDeviceMemoryProperties& memory, const void* linear,
                                  const void* expected) {
    VkImageCreateInfo ci{};
    ci.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    ci.imageType = VK_IMAGE_TYPE_2D;
    ci.format = VK_FORMAT_R8G8B8A8_UNORM;
    ci.extent = {1920, 1080, 1};
    ci.mipLevels = 1;
    ci.arrayLayers = 1;
    ci.samples = VK_SAMPLE_COUNT_1_BIT;
    ci.tiling = VK_IMAGE_TILING_OPTIMAL;
    ci.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    ci.usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    ci.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    VkComponentMapping components{};
    std::unique_ptr<CtrSampledImageOwner> image;
    int status = CtrSampledImageOwner::Create(device, get, memory, ci, components, image);
    if (status) return status;
    constexpr uint64_t pitch = 1920 * 4, paddedBytes = pitch * 1088, visibleBytes = pitch * 1080;
    status = image->Upload(device, get, family, memory, linear, paddedBytes, pitch);
    if (status) return status;
    std::vector<unsigned char> result(static_cast<size_t>(paddedBytes) + 256, 0xd3);
    status = image->Readback(device, get, family, memory, result.data(), result.size(), pitch);
    if (status) return status;
    if (std::memcmp(result.data(), expected, static_cast<size_t>(visibleBytes))) return 147;
    for (size_t i = static_cast<size_t>(visibleBytes); i < result.size(); ++i)
        if (result[i] != 0xd3) return 151;
    std::puts(
        "Thin2DThin32 detiled payload -> owned UNORM sampled image/view -> fenced readback exact8294400B padding8rows/guard256 preserved; synthetic=true guest_upload=false R3=false");
    return 0;
}
