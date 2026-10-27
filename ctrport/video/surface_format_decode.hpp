// SPDX-FileCopyrightText: Copyright 2024-2026 shadPS4 Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include <array>
#include <span>
#include <vulkan/vulkan.hpp>
#include "video_core/amdgpu/pixel_format.h"
// Whole public SurfaceFormats table, pinned e3ce810f3a653f43ac64ebab63023de281a4103a.
// Upstream software feature flags are not physical-device vertex-buffer authority.
namespace CtrPm4 {
struct SurfaceFormatInfo {
    AmdGpu::DataFormat data_format;
    AmdGpu::NumberFormat number_format;
    vk::Format vk_format;
};
constexpr SurfaceFormatInfo CreateSurfaceFormatInfo(AmdGpu::DataFormat d, AmdGpu::NumberFormat n, vk::Format v) {
    return {d, n, v};
}
inline std::span<const SurfaceFormatInfo> SurfaceFormats() {
    // Uscaled, Sscaled, and Ubnorm formats are automatically remapped and handled in shader.
    static constexpr std::array formats{
        // Invalid
        CreateSurfaceFormatInfo(AmdGpu::DataFormat::FormatInvalid, AmdGpu::NumberFormat::Unorm, vk::Format::eUndefined),
        CreateSurfaceFormatInfo(AmdGpu::DataFormat::FormatInvalid, AmdGpu::NumberFormat::Snorm, vk::Format::eUndefined),
        CreateSurfaceFormatInfo(AmdGpu::DataFormat::FormatInvalid, AmdGpu::NumberFormat::Uscaled,
                                vk::Format::eUndefined),
        CreateSurfaceFormatInfo(AmdGpu::DataFormat::FormatInvalid, AmdGpu::NumberFormat::Sscaled,
                                vk::Format::eUndefined),
        CreateSurfaceFormatInfo(AmdGpu::DataFormat::FormatInvalid, AmdGpu::NumberFormat::Uint, vk::Format::eUndefined),
        CreateSurfaceFormatInfo(AmdGpu::DataFormat::FormatInvalid, AmdGpu::NumberFormat::Sint, vk::Format::eUndefined),
        CreateSurfaceFormatInfo(AmdGpu::DataFormat::FormatInvalid, AmdGpu::NumberFormat::SnormNz,
                                vk::Format::eUndefined),
        CreateSurfaceFormatInfo(AmdGpu::DataFormat::FormatInvalid, AmdGpu::NumberFormat::Float, vk::Format::eUndefined),
        CreateSurfaceFormatInfo(AmdGpu::DataFormat::FormatInvalid, AmdGpu::NumberFormat::Srgb, vk::Format::eUndefined),
        CreateSurfaceFormatInfo(AmdGpu::DataFormat::FormatInvalid, AmdGpu::NumberFormat::Ubnorm,
                                vk::Format::eUndefined),
        CreateSurfaceFormatInfo(AmdGpu::DataFormat::FormatInvalid, AmdGpu::NumberFormat::UbnormNz,
                                vk::Format::eUndefined),
        CreateSurfaceFormatInfo(AmdGpu::DataFormat::FormatInvalid, AmdGpu::NumberFormat::Ubint, vk::Format::eUndefined),
        CreateSurfaceFormatInfo(AmdGpu::DataFormat::FormatInvalid, AmdGpu::NumberFormat::Ubscaled,
                                vk::Format::eUndefined),
        // 8
        CreateSurfaceFormatInfo(AmdGpu::DataFormat::Format8, AmdGpu::NumberFormat::Unorm, vk::Format::eR8Unorm),
        CreateSurfaceFormatInfo(AmdGpu::DataFormat::Format8, AmdGpu::NumberFormat::Snorm, vk::Format::eR8Snorm),
        CreateSurfaceFormatInfo(AmdGpu::DataFormat::Format8, AmdGpu::NumberFormat::Uint, vk::Format::eR8Uint),
        CreateSurfaceFormatInfo(AmdGpu::DataFormat::Format8, AmdGpu::NumberFormat::Sint, vk::Format::eR8Sint),
        CreateSurfaceFormatInfo(AmdGpu::DataFormat::Format8, AmdGpu::NumberFormat::Srgb, vk::Format::eR8Srgb),
        // 16
        CreateSurfaceFormatInfo(AmdGpu::DataFormat::Format16, AmdGpu::NumberFormat::Unorm, vk::Format::eR16Unorm),
        CreateSurfaceFormatInfo(AmdGpu::DataFormat::Format16, AmdGpu::NumberFormat::Snorm, vk::Format::eR16Snorm),
        CreateSurfaceFormatInfo(AmdGpu::DataFormat::Format16, AmdGpu::NumberFormat::Uint, vk::Format::eR16Uint),
        CreateSurfaceFormatInfo(AmdGpu::DataFormat::Format16, AmdGpu::NumberFormat::Sint, vk::Format::eR16Sint),
        CreateSurfaceFormatInfo(AmdGpu::DataFormat::Format16, AmdGpu::NumberFormat::Float, vk::Format::eR16Sfloat),
        // 8_8
        CreateSurfaceFormatInfo(AmdGpu::DataFormat::Format8_8, AmdGpu::NumberFormat::Unorm, vk::Format::eR8G8Unorm),
        CreateSurfaceFormatInfo(AmdGpu::DataFormat::Format8_8, AmdGpu::NumberFormat::Snorm, vk::Format::eR8G8Snorm),
        CreateSurfaceFormatInfo(AmdGpu::DataFormat::Format8_8, AmdGpu::NumberFormat::Uint, vk::Format::eR8G8Uint),
        CreateSurfaceFormatInfo(AmdGpu::DataFormat::Format8_8, AmdGpu::NumberFormat::Sint, vk::Format::eR8G8Sint),
        CreateSurfaceFormatInfo(AmdGpu::DataFormat::Format8_8, AmdGpu::NumberFormat::Srgb, vk::Format::eR8G8Srgb),
        // 32
        CreateSurfaceFormatInfo(AmdGpu::DataFormat::Format32, AmdGpu::NumberFormat::Uint, vk::Format::eR32Uint),
        CreateSurfaceFormatInfo(AmdGpu::DataFormat::Format32, AmdGpu::NumberFormat::Sint, vk::Format::eR32Sint),
        CreateSurfaceFormatInfo(AmdGpu::DataFormat::Format32, AmdGpu::NumberFormat::Float, vk::Format::eR32Sfloat),
        // 16_16
        CreateSurfaceFormatInfo(AmdGpu::DataFormat::Format16_16, AmdGpu::NumberFormat::Unorm, vk::Format::eR16G16Unorm),
        CreateSurfaceFormatInfo(AmdGpu::DataFormat::Format16_16, AmdGpu::NumberFormat::Snorm, vk::Format::eR16G16Snorm),
        CreateSurfaceFormatInfo(AmdGpu::DataFormat::Format16_16, AmdGpu::NumberFormat::Uint, vk::Format::eR16G16Uint),
        CreateSurfaceFormatInfo(AmdGpu::DataFormat::Format16_16, AmdGpu::NumberFormat::Sint, vk::Format::eR16G16Sint),
        CreateSurfaceFormatInfo(AmdGpu::DataFormat::Format16_16, AmdGpu::NumberFormat::Float,
                                vk::Format::eR16G16Sfloat),
        // 10_11_11
        CreateSurfaceFormatInfo(AmdGpu::DataFormat::Format10_11_11, AmdGpu::NumberFormat::Float,
                                vk::Format::eB10G11R11UfloatPack32),
        // 11_11_10 - Remapped to 10_11_11.
        // 10_10_10_2 - Remapped to 2_10_10_10.
        // 2_10_10_10
        CreateSurfaceFormatInfo(AmdGpu::DataFormat::Format2_10_10_10, AmdGpu::NumberFormat::Unorm,
                                vk::Format::eA2B10G10R10UnormPack32),
        CreateSurfaceFormatInfo(AmdGpu::DataFormat::Format2_10_10_10, AmdGpu::NumberFormat::Snorm,
                                vk::Format::eA2B10G10R10SnormPack32),
        CreateSurfaceFormatInfo(AmdGpu::DataFormat::Format2_10_10_10, AmdGpu::NumberFormat::Uint,
                                vk::Format::eA2B10G10R10UintPack32),
        CreateSurfaceFormatInfo(AmdGpu::DataFormat::Format2_10_10_10, AmdGpu::NumberFormat::Sint,
                                vk::Format::eA2B10G10R10SintPack32),
        // 8_8_8_8
        CreateSurfaceFormatInfo(AmdGpu::DataFormat::Format8_8_8_8, AmdGpu::NumberFormat::Unorm,
                                vk::Format::eR8G8B8A8Unorm),
        CreateSurfaceFormatInfo(AmdGpu::DataFormat::Format8_8_8_8, AmdGpu::NumberFormat::Snorm,
                                vk::Format::eR8G8B8A8Snorm),
        CreateSurfaceFormatInfo(AmdGpu::DataFormat::Format8_8_8_8, AmdGpu::NumberFormat::Uint,
                                vk::Format::eR8G8B8A8Uint),
        CreateSurfaceFormatInfo(AmdGpu::DataFormat::Format8_8_8_8, AmdGpu::NumberFormat::Sint,
                                vk::Format::eR8G8B8A8Sint),
        CreateSurfaceFormatInfo(AmdGpu::DataFormat::Format8_8_8_8, AmdGpu::NumberFormat::Srgb,
                                vk::Format::eR8G8B8A8Srgb),
        // 32_32
        CreateSurfaceFormatInfo(AmdGpu::DataFormat::Format32_32, AmdGpu::NumberFormat::Uint, vk::Format::eR32G32Uint),
        CreateSurfaceFormatInfo(AmdGpu::DataFormat::Format32_32, AmdGpu::NumberFormat::Sint, vk::Format::eR32G32Sint),
        CreateSurfaceFormatInfo(AmdGpu::DataFormat::Format32_32, AmdGpu::NumberFormat::Float,
                                vk::Format::eR32G32Sfloat),
        // 16_16_16_16
        CreateSurfaceFormatInfo(AmdGpu::DataFormat::Format16_16_16_16, AmdGpu::NumberFormat::Unorm,
                                vk::Format::eR16G16B16A16Unorm),
        CreateSurfaceFormatInfo(AmdGpu::DataFormat::Format16_16_16_16, AmdGpu::NumberFormat::Snorm,
                                vk::Format::eR16G16B16A16Snorm),
        CreateSurfaceFormatInfo(AmdGpu::DataFormat::Format16_16_16_16, AmdGpu::NumberFormat::Uint,
                                vk::Format::eR16G16B16A16Uint),
        CreateSurfaceFormatInfo(AmdGpu::DataFormat::Format16_16_16_16, AmdGpu::NumberFormat::Sint,
                                vk::Format::eR16G16B16A16Sint),
        CreateSurfaceFormatInfo(AmdGpu::DataFormat::Format16_16_16_16, AmdGpu::NumberFormat::SnormNz,
                                vk::Format::eR16G16B16A16Snorm),
        CreateSurfaceFormatInfo(AmdGpu::DataFormat::Format16_16_16_16, AmdGpu::NumberFormat::Float,
                                vk::Format::eR16G16B16A16Sfloat),
        // 32_32_32
        CreateSurfaceFormatInfo(AmdGpu::DataFormat::Format32_32_32, AmdGpu::NumberFormat::Uint,
                                vk::Format::eR32G32B32Uint),
        CreateSurfaceFormatInfo(AmdGpu::DataFormat::Format32_32_32, AmdGpu::NumberFormat::Sint,
                                vk::Format::eR32G32B32Sint),
        CreateSurfaceFormatInfo(AmdGpu::DataFormat::Format32_32_32, AmdGpu::NumberFormat::Float,
                                vk::Format::eR32G32B32Sfloat),
        // 32_32_32_32
        CreateSurfaceFormatInfo(AmdGpu::DataFormat::Format32_32_32_32, AmdGpu::NumberFormat::Uint,
                                vk::Format::eR32G32B32A32Uint),
        CreateSurfaceFormatInfo(AmdGpu::DataFormat::Format32_32_32_32, AmdGpu::NumberFormat::Sint,
                                vk::Format::eR32G32B32A32Sint),
        CreateSurfaceFormatInfo(AmdGpu::DataFormat::Format32_32_32_32, AmdGpu::NumberFormat::Float,
                                vk::Format::eR32G32B32A32Sfloat),
        // 5_6_5
        CreateSurfaceFormatInfo(AmdGpu::DataFormat::Format5_6_5, AmdGpu::NumberFormat::Unorm,
                                vk::Format::eR5G6B5UnormPack16),
        // 1_5_5_5
        CreateSurfaceFormatInfo(AmdGpu::DataFormat::Format1_5_5_5, AmdGpu::NumberFormat::Unorm,
                                vk::Format::eA1R5G5B5UnormPack16),
        // 5_5_5_1
        CreateSurfaceFormatInfo(AmdGpu::DataFormat::Format5_5_5_1, AmdGpu::NumberFormat::Unorm,
                                vk::Format::eR5G5B5A1UnormPack16),
        // 4_4_4_4
        CreateSurfaceFormatInfo(AmdGpu::DataFormat::Format4_4_4_4, AmdGpu::NumberFormat::Unorm,
                                vk::Format::eB4G4R4A4UnormPack16),
        // 8_24
        // 24_8
        // X24_8_32
        // GB_GR
        // BG_RG
        // 5_9_9_9
        CreateSurfaceFormatInfo(AmdGpu::DataFormat::Format5_9_9_9, AmdGpu::NumberFormat::Float,
                                vk::Format::eE5B9G9R9UfloatPack32),
        // BC1
        CreateSurfaceFormatInfo(AmdGpu::DataFormat::FormatBc1, AmdGpu::NumberFormat::Unorm,
                                vk::Format::eBc1RgbaUnormBlock),
        CreateSurfaceFormatInfo(AmdGpu::DataFormat::FormatBc1, AmdGpu::NumberFormat::Srgb,
                                vk::Format::eBc1RgbaSrgbBlock),
        // BC2
        CreateSurfaceFormatInfo(AmdGpu::DataFormat::FormatBc2, AmdGpu::NumberFormat::Unorm, vk::Format::eBc2UnormBlock),
        CreateSurfaceFormatInfo(AmdGpu::DataFormat::FormatBc2, AmdGpu::NumberFormat::Srgb, vk::Format::eBc2SrgbBlock),
        // BC3
        CreateSurfaceFormatInfo(AmdGpu::DataFormat::FormatBc3, AmdGpu::NumberFormat::Unorm, vk::Format::eBc3UnormBlock),
        CreateSurfaceFormatInfo(AmdGpu::DataFormat::FormatBc3, AmdGpu::NumberFormat::Srgb, vk::Format::eBc3SrgbBlock),
        // BC4
        CreateSurfaceFormatInfo(AmdGpu::DataFormat::FormatBc4, AmdGpu::NumberFormat::Unorm, vk::Format::eBc4UnormBlock),
        CreateSurfaceFormatInfo(AmdGpu::DataFormat::FormatBc4, AmdGpu::NumberFormat::Snorm, vk::Format::eBc4SnormBlock),
        // BC5
        CreateSurfaceFormatInfo(AmdGpu::DataFormat::FormatBc5, AmdGpu::NumberFormat::Unorm, vk::Format::eBc5UnormBlock),
        CreateSurfaceFormatInfo(AmdGpu::DataFormat::FormatBc5, AmdGpu::NumberFormat::Snorm, vk::Format::eBc5SnormBlock),
        // BC6
        CreateSurfaceFormatInfo(AmdGpu::DataFormat::FormatBc6, AmdGpu::NumberFormat::Unorm,
                                vk::Format::eBc6HUfloatBlock),
        CreateSurfaceFormatInfo(AmdGpu::DataFormat::FormatBc6, AmdGpu::NumberFormat::Snorm,
                                vk::Format::eBc6HSfloatBlock),
        // BC7
        CreateSurfaceFormatInfo(AmdGpu::DataFormat::FormatBc7, AmdGpu::NumberFormat::Unorm, vk::Format::eBc7UnormBlock),
        CreateSurfaceFormatInfo(AmdGpu::DataFormat::FormatBc7, AmdGpu::NumberFormat::Srgb, vk::Format::eBc7SrgbBlock),
    };
    return formats;
}

inline int DecodeSurfaceFormat(uint32_t data, uint32_t number, uint32_t& output) {
    if (data >= 64 || number >= 16) return 60;
    for (const auto& entry : SurfaceFormats()) {
        if (uint32_t(entry.data_format) == data && uint32_t(entry.number_format) == number) {
            if (entry.vk_format == vk::Format::eUndefined) return 88;
            output = uint32_t(entry.vk_format);
            return 0;
        }
    }
    return 88;
}
}
