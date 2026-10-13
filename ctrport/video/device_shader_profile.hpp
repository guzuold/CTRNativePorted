// SPDX-License-Identifier: GPL-2.0-only
#pragma once
#include "graphics_companion.h"
// This is specifically the current device-create policy: swapchain extension,
// no VkPhysicalDeviceFeatures enabled; Vulkan12 only scalarBlockLayout enabled.
// Optional shader/atomic/barycentric features are therefore genuinely disabled.
// Revisit this builder whenever device-create enables additional shader features.
inline int CaptureCreatedDeviceShaderProfile(VkPhysicalDevice physical,
    PFN_vkGetPhysicalDeviceProperties2 query,uint32_t instanceApi,uint32_t deviceApi,
    CtrGraphicsProfileV1& output) {
    if(!physical || !query || instanceApi<VK_API_VERSION_1_2 || deviceApi<VK_API_VERSION_1_2)return 88;
    VkPhysicalDeviceVulkan12Properties fp{};fp.sType=VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_PROPERTIES;
    VkPhysicalDeviceSubgroupProperties subgroup{};subgroup.sType=VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SUBGROUP_PROPERTIES;
    subgroup.pNext=&fp;
    VkPhysicalDeviceProperties2 properties{};properties.sType=VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2;
    properties.pNext=&subgroup;query(physical,&properties);
    const auto& limits=properties.properties.limits;
    if(!subgroup.subgroupSize || limits.maxUniformBufferRange<=limits.minUniformBufferOffsetAlignment ||
       !limits.maxViewportDimensions[0] || !limits.maxViewportDimensions[1] || !limits.maxComputeSharedMemorySize)return 88;
    CtrGraphicsProfileV1 prepared{};prepared.version=1;prepared.size=sizeof(prepared);
    prepared.max_ubo_size=limits.maxUniformBufferRange-limits.minUniformBufferOffsetAlignment;
    prepared.max_viewport_width=limits.maxViewportDimensions[0];
    prepared.max_viewport_height=limits.maxViewportDimensions[1];
    prepared.max_shared_memory_size=limits.maxComputeSharedMemorySize;
    prepared.supported_spirv=0x10500; // Core Vulkan1.2; no SPIRV1.6 extension enabled.
    prepared.subgroup_size=subgroup.subgroupSize;
    prepared.supports_denorm_behavior_independence=fp.denormBehaviorIndependence!=VK_SHADER_FLOAT_CONTROLS_INDEPENDENCE_NONE;
    prepared.supports_rounding_mode_independence=fp.roundingModeIndependence!=VK_SHADER_FLOAT_CONTROLS_INDEPENDENCE_NONE;
    prepared.support_fp16_denorm_preserve=fp.shaderDenormPreserveFloat16!=0;
    prepared.support_fp16_denorm_flush=fp.shaderDenormFlushToZeroFloat16!=0;
    prepared.support_fp16_round_to_zero=fp.shaderRoundingModeRTZFloat16!=0;
    prepared.support_fp16_signed_zero_inf_nan_preserve=fp.shaderSignedZeroInfNanPreserveFloat16!=0;
    prepared.support_fp32_denorm_preserve=fp.shaderDenormPreserveFloat32!=0;
    prepared.support_fp32_denorm_flush=fp.shaderDenormFlushToZeroFloat32!=0;
    prepared.support_fp32_round_to_zero=fp.shaderRoundingModeRTZFloat32!=0;
    prepared.support_fp32_signed_zero_inf_nan_preserve=fp.shaderSignedZeroInfNanPreserveFloat32!=0;
    prepared.support_fp64_denorm_preserve=fp.shaderDenormPreserveFloat64!=0;
    prepared.support_fp64_denorm_flush=fp.shaderDenormFlushToZeroFloat64!=0;
    prepared.support_fp64_round_to_zero=fp.shaderRoundingModeRTZFloat64!=0;
    prepared.support_fp64_signed_zero_inf_nan_preserve=fp.shaderSignedZeroInfNanPreserveFloat64!=0;

    // Whole pinned public driver policy for currently disabled optional features.
    prepared.needs_lds_barriers=fp.driverID==VK_DRIVER_ID_NVIDIA_PROPRIETARY || fp.driverID==VK_DRIVER_ID_MESA_KOSMICKRISP;
    prepared.needs_buffer_offsets=limits.minStorageBufferOffsetAlignment>4;
    prepared.needs_unorm_fixup=fp.driverID==VK_DRIVER_ID_MESA_KOSMICKRISP;
    prepared.needs_clip_distance_emulation=fp.driverID==VK_DRIVER_ID_NVIDIA_PROPRIETARY;
    // manual interpolation requires enabled barycentric (disabled here).
    // depth_clip_control, numeric optional types, all extension capabilities,
    // clamp and origin policies remain false under the explicit create policy.
    output=prepared;return 0;
}
