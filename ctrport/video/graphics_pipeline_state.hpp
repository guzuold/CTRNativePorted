// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "graphics_fixed_state.hpp"
#include "graphics_blend_state.hpp"
#include <array>
// Public vk_graphics_pipeline.cpp e3ce810f3a653f43ac64ebab63023de281a4103a.
// Bounded first-draw domain: paired VS/PS, triangles, single color/sample, no depth.
// No caller-supplied Vulkan state or inferred defaults from an absent guest bank.
// Exact observed retail12300 inactive second export: no backing image is invented.
inline bool CtrInactiveSecondColor(const CtrGraphicsKeyV1& k) {
    return k.primitive_type == 4 && k.num_color_attachments == 2 && k.mrt_mask == 3 && k.num_samples == 1 &&
           !k.z_format && !k.stencil_format && k.colors[0].data_format == 10 && k.colors[0].number_format == 0 &&
           k.colors[0].samples == 1 && !k.colors[1].data_format && !k.colors[1].write_mask && !k.colors[1].samples;
}
struct CtrGraphicsPipelineState {
    VkPipelineVertexInputStateCreateInfo vertex{};
    VkPipelineInputAssemblyStateCreateInfo assembly{};
    VkPipelineViewportStateCreateInfo viewport{};
    VkPipelineRasterizationStateCreateInfo raster{};
    VkPipelineMultisampleStateCreateInfo samples{};
    VkPipelineDepthStencilStateCreateInfo depth{};
    VkPipelineColorBlendStateCreateInfo blend{};
    VkPipelineColorBlendAttachmentState attachment{};
    std::array<VkPipelineColorBlendAttachmentState, 2> mrtAttachments{};
    std::array<VkFormat, 2> mrtFormats{};
    VkPipelineRenderingCreateInfo rendering{};
    VkPipelineDynamicStateCreateInfo dynamic{};
    VkGraphicsPipelineCreateInfo pipeline{};
    VkFormat color{};
    VkPipelineTessellationStateCreateInfo tessellation{};
    std::array<VkVertexInputAttributeDescription, 32> attributes{};
    std::array<VkVertexInputBindingDescription, 32> bindings{};
    std::array<VkDynamicState, 21> states{};
    int Prepare(const CtrGraphicsKeyV1& key, const CtrGraphicsVertexInputsV1& inputs, const uint32_t* formats,
                VkFormat colorFormat, uint32_t core, uint32_t sampleShading, VkFormat depthFormat = VK_FORMAT_UNDEFINED,
                VkFormat secondColor = VK_FORMAT_UNDEFINED, bool noAttachments = false, bool inactiveDepthMs2 = false,
                bool inactiveColorSlot = false) {
        const bool inactiveSecond =
            inactiveColorSlot && CtrInactiveSecondColor(key) && secondColor == VK_FORMAT_UNDEFINED;
        if (inactiveColorSlot && !inactiveSecond) return 88;
        const bool mrt = secondColor != VK_FORMAT_UNDEFINED || inactiveSecond;
        const bool hasDepth = depthFormat != VK_FORMAT_UNDEFINED;
        const bool rectDepth = hasDepth && colorFormat == VK_FORMAT_UNDEFINED;
        const bool combined = hasDepth && !rectDepth;
        const bool rectNoAttachments = noAttachments && key.primitive_type == 17;
        if (key.version != 1 || key.size != sizeof(key) || inputs.version != 1 || inputs.size != sizeof(inputs) ||
            inputs.count > 32 || (inputs.count && !formats) || sampleShading > 1)
            return 60;
        if (inactiveDepthMs2 &&
            (hasDepth || mrt || noAttachments || key.primitive_type != 4 || key.num_color_attachments != 1 ||
             key.mrt_mask != 1 || key.num_samples != 2 || key.depth_samples != 2 || key.colors[0].samples != 2 ||
             key.colors[0].data_format != 10 || key.colors[0].number_format != 0 || key.z_format != 3 ||
             key.stencil_format != 1 || colorFormat != VK_FORMAT_R8G8B8A8_UNORM || sampleShading))
            return 88;
        if (noAttachments) {
            if (hasDepth || mrt || key.num_samples != 1 || sampleShading) return 88;
            if (rectNoAttachments) {
                if (key.mrt_mask || key.num_color_attachments || inputs.count || key.depth_samples != 1 ||
                    !(core & 1024))
                    return 88;
            } else if (key.primitive_type != 4 || key.mrt_mask != 1 || key.num_color_attachments != 1)
                return 88;
            for (const auto& c : key.colors)
                if (c.data_format || c.write_mask) return 88;
        }
        if (combined) {
            if (key.primitive_type != 4 || key.num_color_attachments != (mrt ? 2u : 1u) ||
                key.mrt_mask != (mrt ? 3u : 1u) ||
                !((key.num_samples == 2 && key.depth_samples == 2 && key.colors[0].samples == 2 &&
                   (!mrt || key.colors[1].samples == 2)) ||
                  (!mrt && key.num_samples == 1 && key.depth_samples == 1 && key.colors[0].samples == 1 &&
                   ((key.colors[0].data_format == 4 && key.colors[0].number_format == 7) ||
                    (key.colors[0].data_format == 10 && key.colors[0].number_format == 0 &&
                     colorFormat == VK_FORMAT_R8G8B8A8_UNORM)))) ||
                key.z_format != 3 || key.stencil_format != 1 || depthFormat != VK_FORMAT_D32_SFLOAT_S8_UINT ||
                !(core & 16) || colorFormat == VK_FORMAT_UNDEFINED)
                return 88;
        }
        if (rectDepth) {
            if (key.primitive_type != 17 || key.num_color_attachments || key.mrt_mask || inputs.count ||
                !(((key.num_samples == 1 || key.num_samples == 2) && key.depth_samples == key.num_samples &&
                   key.z_format == 3 && key.stencil_format == 1 && depthFormat == VK_FORMAT_D32_SFLOAT_S8_UINT) ||
                  (key.num_samples == 1 && key.depth_samples == 1 && key.z_format == 1 && !key.stencil_format &&
                   depthFormat == VK_FORMAT_D16_UNORM)) ||
                sampleShading || !(core & 1024))
                return 88;
        } else if (!combined && !noAttachments && !inactiveDepthMs2 &&
                   ((key.primitive_type != 4 && !(key.primitive_type == 6 && !mrt)) ||
                    key.num_color_attachments != (mrt ? 2u : 1u) || key.mrt_mask != (mrt ? 3u : 1u) ||
                    key.num_samples != 1 || key.colors[0].samples != 1 || key.z_format || key.stencil_format ||
                    colorFormat == VK_FORMAT_UNDEFINED ||
                    (mrt && ((!inactiveSecond && key.colors[1].samples != 1) || !(core & 16)))))
            return 88;
        vertex.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
        vertex.vertexAttributeDescriptionCount = vertex.vertexBindingDescriptionCount = inputs.count;
        vertex.pVertexAttributeDescriptions = attributes.data();
        vertex.pVertexBindingDescriptions = bindings.data();
        for (uint32_t i = 0; i < inputs.count; ++i) {
            const auto& v = inputs.inputs[i];
            if (v.input_rate && v.divisor != 1) return 88;
            attributes[i] = {v.location, v.binding, static_cast<VkFormat>(formats[i]), v.offset};
            bindings[i] = {v.binding, v.stride, static_cast<VkVertexInputRate>(v.input_rate)};
        }
        assembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
        int rc = CtrPm4::GraphicsTopology(key.primitive_type, assembly.topology);
        if (rc) return rc;
        viewport.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
        raster.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
        // Public fallback with depth-clip extension disabled on this selected device.
        raster.depthClampEnable = key.depth_clamp_enable && !key.depth_clip_enable;
        if (raster.depthClampEnable && !(core & 1)) return 88;
        rc = CtrPm4::GraphicsPolygonMode(key.polygon_mode, raster.polygonMode);
        if (rc) return rc;
        if (raster.polygonMode != VK_POLYGON_MODE_FILL && !(core & 32)) return 88;
        raster.lineWidth = 1.0f;
        samples.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
        samples.rasterizationSamples = inactiveDepthMs2 ? VK_SAMPLE_COUNT_2_BIT
                                       : hasDepth       ? static_cast<VkSampleCountFlagBits>(key.depth_samples)
                                                        : VK_SAMPLE_COUNT_1_BIT;
        samples.sampleShadingEnable = sampleShading;
        if (sampleShading && !(core & 2)) return 88;
        depth.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
        if (!rectDepth && !noAttachments) {
            rc = CtrPm4::GraphicsAttachmentBlend(key.colors[0], key.cb_shader_mask & 15, attachment);
            if (rc) return rc;
            const auto dual = [](VkBlendFactor f) {
                return f >= VK_BLEND_FACTOR_SRC1_COLOR && f <= VK_BLEND_FACTOR_ONE_MINUS_SRC1_ALPHA;
            };
            if (!(core & 8) && (dual(attachment.srcColorBlendFactor) || dual(attachment.dstColorBlendFactor) ||
                                dual(attachment.srcAlphaBlendFactor) || dual(attachment.dstAlphaBlendFactor)))
                return 88;
            blend.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
            blend.logicOpEnable = key.logic_op != 0xcc;
            if (blend.logicOpEnable && !(core & 4)) return 88;
            rc = CtrPm4::GraphicsLogicOp(key.logic_op, blend.logicOp);
            if (rc) return rc;
            blend.attachmentCount = 1;
            blend.pAttachments = &attachment;
            if (mrt) {
                mrtAttachments[0] = attachment;
                if (inactiveSecond)
                    mrtAttachments[1] = {};
                else {
                    rc = CtrPm4::GraphicsAttachmentBlend(key.colors[1], (key.cb_shader_mask >> 4) & 15,
                                                         mrtAttachments[1]);
                    if (rc) return rc;
                }
                const auto& a = mrtAttachments[1];
                if (!(core & 8) && (dual(a.srcColorBlendFactor) || dual(a.dstColorBlendFactor) ||
                                    dual(a.srcAlphaBlendFactor) || dual(a.dstAlphaBlendFactor)))
                    return 88;
                blend.attachmentCount = 2;
                blend.pAttachments = mrtAttachments.data();
            }
        } else {
            blend.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
        }
        for (auto& c : blend.blendConstants) c = 1.0f;
        color = colorFormat;
        rendering.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO;
        rendering.colorAttachmentCount = (rectDepth || noAttachments) ? 0u : 1u;
        rendering.pColorAttachmentFormats = (rectDepth || noAttachments) ? nullptr : &color;
        if (mrt) {
            mrtFormats = {colorFormat, secondColor};
            rendering.colorAttachmentCount = 2;
            rendering.pColorAttachmentFormats = mrtFormats.data();
        }
        if (rectDepth && mrt) return 88;
        if (hasDepth) {
            rendering.depthAttachmentFormat = depthFormat;
            rendering.stencilAttachmentFormat = key.stencil_format ? depthFormat : VK_FORMAT_UNDEFINED;
        }
        const VkDynamicState base[] = {
            VK_DYNAMIC_STATE_VIEWPORT_WITH_COUNT,  VK_DYNAMIC_STATE_SCISSOR_WITH_COUNT,
            VK_DYNAMIC_STATE_BLEND_CONSTANTS,      VK_DYNAMIC_STATE_DEPTH_TEST_ENABLE,
            VK_DYNAMIC_STATE_DEPTH_WRITE_ENABLE,   VK_DYNAMIC_STATE_DEPTH_COMPARE_OP,
            VK_DYNAMIC_STATE_DEPTH_BIAS_ENABLE,    VK_DYNAMIC_STATE_DEPTH_BIAS,
            VK_DYNAMIC_STATE_STENCIL_TEST_ENABLE,  VK_DYNAMIC_STATE_STENCIL_REFERENCE,
            VK_DYNAMIC_STATE_STENCIL_COMPARE_MASK, VK_DYNAMIC_STATE_STENCIL_WRITE_MASK,
            VK_DYNAMIC_STATE_STENCIL_OP,           VK_DYNAMIC_STATE_CULL_MODE,
            VK_DYNAMIC_STATE_FRONT_FACE,           VK_DYNAMIC_STATE_RASTERIZER_DISCARD_ENABLE,
            VK_DYNAMIC_STATE_LINE_WIDTH,           VK_DYNAMIC_STATE_PRIMITIVE_RESTART_ENABLE};
        uint32_t n = 0;
        for (auto state : base) states[n++] = state;
        if (core & 64) {
            states[n++] = VK_DYNAMIC_STATE_DEPTH_BOUNDS_TEST_ENABLE;
            states[n++] = VK_DYNAMIC_STATE_DEPTH_BOUNDS;
        }
        if (inputs.count) states[n++] = VK_DYNAMIC_STATE_VERTEX_INPUT_BINDING_STRIDE;
        dynamic.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
        dynamic.dynamicStateCount = n;
        dynamic.pDynamicStates = states.data();
        pipeline.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
        pipeline.pNext = &rendering;
        pipeline.pVertexInputState = &vertex;
        pipeline.pInputAssemblyState = &assembly;
        pipeline.pViewportState = &viewport;
        pipeline.pRasterizationState = &raster;
        pipeline.pMultisampleState = &samples;
        pipeline.pDepthStencilState = &depth;
        pipeline.pColorBlendState = &blend;
        pipeline.pDynamicState = &dynamic;
        pipeline.basePipelineIndex = -1;
        if (rectDepth && mrt) return 88;
        if (rectDepth || rectNoAttachments) {
            tessellation.sType = VK_STRUCTURE_TYPE_PIPELINE_TESSELLATION_STATE_CREATE_INFO;
            tessellation.patchControlPoints = 3;
            pipeline.pTessellationState = &tessellation;
        }
        return 0;
    }
};
