// SPDX-FileCopyrightText: Copyright 2024-2026 shadPS4 Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "graphics_companion.h"
#include <vulkan/vulkan.h>
// Public LiverpoolToVK BlendFactor/BlendOp and whole attachment blend block from
// vk_graphics_pipeline.cpp, pinned e3ce810f3a653f43ac64ebab63023de281a4103a.
// Unsupported public enums fail88, no assertion/default-success substitute.
namespace CtrPm4 {
inline int GraphicsBlendFactor(uint32_t value,VkBlendFactor& output) {
    static constexpr VkBlendFactor factors[]={VK_BLEND_FACTOR_ZERO,VK_BLEND_FACTOR_ONE,
      VK_BLEND_FACTOR_SRC_COLOR,VK_BLEND_FACTOR_ONE_MINUS_SRC_COLOR,VK_BLEND_FACTOR_SRC_ALPHA,
      VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,VK_BLEND_FACTOR_DST_ALPHA,VK_BLEND_FACTOR_ONE_MINUS_DST_ALPHA,
      VK_BLEND_FACTOR_DST_COLOR,VK_BLEND_FACTOR_ONE_MINUS_DST_COLOR,VK_BLEND_FACTOR_SRC_ALPHA_SATURATE,
      VK_BLEND_FACTOR_MAX_ENUM,VK_BLEND_FACTOR_MAX_ENUM,VK_BLEND_FACTOR_CONSTANT_COLOR,
      VK_BLEND_FACTOR_ONE_MINUS_CONSTANT_COLOR,VK_BLEND_FACTOR_SRC1_COLOR,VK_BLEND_FACTOR_ONE_MINUS_SRC1_COLOR,
      VK_BLEND_FACTOR_SRC1_ALPHA,VK_BLEND_FACTOR_ONE_MINUS_SRC1_ALPHA,VK_BLEND_FACTOR_CONSTANT_ALPHA,
      VK_BLEND_FACTOR_ONE_MINUS_CONSTANT_ALPHA};
    if(value>=21 || factors[value]==VK_BLEND_FACTOR_MAX_ENUM)return 88;output=factors[value];return 0;
}
inline int GraphicsBlendOp(uint32_t value,VkBlendOp& output) {
    static constexpr VkBlendOp ops[]={VK_BLEND_OP_ADD,VK_BLEND_OP_SUBTRACT,VK_BLEND_OP_MIN,VK_BLEND_OP_MAX,VK_BLEND_OP_REVERSE_SUBTRACT};
    if(value>=5)return 88;output=ops[value];return 0;
}
inline int GraphicsAttachmentBlend(const CtrGraphicsKeyColorV1& color,uint32_t shaderMask,
                                  VkPipelineColorBlendAttachmentState& output) {
    if(color.blend_enable>1 || color.separate_alpha_blend>1 || color.write_mask>15 || shaderMask>15)return 60;
    VkPipelineColorBlendAttachmentState state{};
    if(GraphicsBlendFactor(color.color_src_factor,state.srcColorBlendFactor) ||
       GraphicsBlendFactor(color.color_dst_factor,state.dstColorBlendFactor) ||
       GraphicsBlendOp(color.color_func,state.colorBlendOp))return 88;
    state.srcAlphaBlendFactor=state.srcColorBlendFactor;state.dstAlphaBlendFactor=state.dstColorBlendFactor;
    state.alphaBlendOp=state.colorBlendOp;
    if(color.separate_alpha_blend && (GraphicsBlendFactor(color.alpha_src_factor,state.srcAlphaBlendFactor) ||
       GraphicsBlendFactor(color.alpha_dst_factor,state.dstAlphaBlendFactor) ||
       GraphicsBlendOp(color.alpha_func,state.alphaBlendOp)))return 88;
    // Upstream asserts for scaled min/max. Preserve explicit missing instead of emitting wrong blend.
    if(((state.colorBlendOp==VK_BLEND_OP_MIN || state.colorBlendOp==VK_BLEND_OP_MAX) &&
        (state.srcColorBlendFactor!=VK_BLEND_FACTOR_ONE || state.dstColorBlendFactor!=VK_BLEND_FACTOR_ONE)) ||
       ((state.alphaBlendOp==VK_BLEND_OP_MIN || state.alphaBlendOp==VK_BLEND_OP_MAX) &&
        (state.srcAlphaBlendFactor!=VK_BLEND_FACTOR_ONE || state.dstAlphaBlendFactor!=VK_BLEND_FACTOR_ONE)))return 88;
    state.blendEnable=color.blend_enable;state.colorWriteMask=color.write_mask;
    if(!(shaderMask&8)) {
        if(state.srcColorBlendFactor==VK_BLEND_FACTOR_SRC_ALPHA)state.srcColorBlendFactor=VK_BLEND_FACTOR_ONE;
        else if(state.srcColorBlendFactor==VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA)state.srcColorBlendFactor=VK_BLEND_FACTOR_ZERO;
        if(state.dstColorBlendFactor==VK_BLEND_FACTOR_SRC_ALPHA)state.dstColorBlendFactor=VK_BLEND_FACTOR_ONE;
        else if(state.dstColorBlendFactor==VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA)state.dstColorBlendFactor=VK_BLEND_FACTOR_ZERO;
    }
    output=state;return 0;
}
}
