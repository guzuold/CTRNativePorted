// SPDX-License-Identifier: GPL-2.0-only
#pragma once
#include "graphics_companion.h"
#include "shader_recompiler/info.h"
#include <cstring>
namespace CtrPm4 {
inline int ClassifyEffectiveImage(const uint32_t* descriptor, uint32_t words, uint32_t depth, uint32_t r128,
                                  CtrGraphicsEffectiveImageClassV1* output) {
    if (!descriptor || !output || words != 8 || depth > 1 || r128 > 1) return 60;
    Shader::Info info{};
    info.flattened_ud_buf.assign(descriptor, descriptor + 8);
    Shader::ImageResource resource{};
    resource.sharp_idx = 0;
    resource.is_depth = depth != 0;
    resource.is_r128 = r128 != 0;
    AmdGpu::Image raw{};
    if (!r128)
        std::memcpy(&raw, descriptor, sizeof(raw));
    else {
        std::memcpy(&raw, descriptor, 16);
        raw.pitch = raw.width;
    }
    const auto effective = resource.GetSharp(info);
    CtrGraphicsEffectiveImageClassV1 prepared{
        1, sizeof(CtrGraphicsEffectiveImageClassV1), raw.Valid() ? 1u : 0u, effective.Valid() ? 1u : 0u, 0, 0};
    if (!raw.Valid())
        prepared.fallback_reason = 1;
    else if (depth && raw.GetDataFmt() != AmdGpu::DataFormat::Format16 &&
             raw.GetDataFmt() != AmdGpu::DataFormat::Format32)
        prepared.fallback_reason = 2;
    if (!effective.Address()) prepared.null_reasons |= 1;
    if (effective.GetDataFmt() == AmdGpu::DataFormat::FormatInvalid) prepared.null_reasons |= 2;
    *output = prepared;
    return 0;
}
}
