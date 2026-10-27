// SPDX-License-Identifier: GPL-2.0-only
#pragma once
#include <fstream>
#include <string>
#include <functional>
#include <chrono>
#include <cstdlib>
#include <cstdio>
#include "tiling_roundtrip_control.hpp"
// Whole public tiling.comp binaries built locally; no game shader bytes.
// Borrowed handles live only within body: caller must fence all submitted work before return.
// Device, enabled feature and queue authority must come from the selected session.
static int
ctr_with_tiling_pipelines(VkDevice device, PFN_vkGetDeviceProcAddr get, const char* directory, bool scalar,
                          bool compute, bool thin,
                          const std::function<int(VkPipelineLayout, const VkPipeline*, VkDescriptorSetLayout)>& body,
                          uint32_t texelBytes = 4, bool depth0 = false, uint32_t samples = 1, bool micro13 = false) {
    if (micro13 && ((texelBytes != 4 && texelBytes != 8 && texelBytes != 16) || samples != 1 || depth0 || !thin))
        return 88;
    if (samples != 1 && !(samples == 2 && texelBytes == 4 && thin && !depth0)) return 88;
    if ((texelBytes != 2 && texelBytes != 4 && !((texelBytes == 8 || texelBytes == 16) && micro13)) ||
        (texelBytes == 2 && !thin))
        return 88;
    if (!directory || !*directory || !scalar || !compute || !body) return 88;
    if (!device || !get) return 60;
#define LOAD(name)                                                                                                     \
    const auto name = reinterpret_cast<PFN_##name>(get(device, #name));                                                \
    if (!name) return 91
    LOAD(vkCreateShaderModule);
    LOAD(vkDestroyShaderModule);
    LOAD(vkCreateDescriptorSetLayout);
    LOAD(vkDestroyDescriptorSetLayout);
    LOAD(vkCreatePipelineLayout);
    LOAD(vkDestroyPipelineLayout);
    LOAD(vkCreateComputePipelines);
    LOAD(vkDestroyPipeline);
#undef LOAD
    char timingEnv[16]{};
    size_t timingLength = 0;
    const bool timing = getenv_s(&timingLength, timingEnv, sizeof(timingEnv), "CTR_TILING_TIMING") == 0 &&
                        timingLength == 2 && timingEnv[0] == '1';
    using Clock = std::chrono::steady_clock;
    const auto started = Clock::now();
    auto bodyStarted = started, bodyEnded = started;
    bool bodyEntered = false, bodyReturned = false;
    double pipelineMs[2]{};
    VkShaderModule shaders[2]{};
    VkPipeline pipelines[2]{};
    VkDescriptorSetLayout descriptors{};
    VkPipelineLayout layout{};
    const auto run = [&]() -> int {
        VkDescriptorSetLayoutBinding bindings[3]{};
        for (uint32_t i = 0; i < 3; ++i) {
            bindings[i].binding = i;
            bindings[i].descriptorCount = 1;
            bindings[i].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
            bindings[i].descriptorType = i == 2 ? VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER : VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        }
        VkDescriptorSetLayoutCreateInfo di{};
        di.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        di.bindingCount = 3;
        di.pBindings = bindings;
        if (vkCreateDescriptorSetLayout(device, &di, nullptr, &descriptors) != VK_SUCCESS) return 91;
        VkPipelineLayoutCreateInfo li{};
        li.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        li.setLayoutCount = 1;
        li.pSetLayouts = &descriptors;
        if (vkCreatePipelineLayout(device, &li, nullptr, &layout) != VK_SUCCESS) return 91;
        const char* files[2] = {
            thin ? (texelBytes == 2 ? "/thin16_tile.spv" : "/thin32_tile.spv") : "/display32_tile.spv",
            thin ? (texelBytes == 2 ? "/thin16_detile.spv" : "/thin32_detile.spv") : "/display32_detile.spv"};
        if (depth0) {
            files[0] = texelBytes == 2 ? "/depth0_16_flattened_tile.spv" : "/depth0_32_tile.spv";
            files[1] = texelBytes == 2 ? "/depth0_16_flattened_detile.spv" : "/depth0_32_detile.spv";
        }
        if (micro13) {
            files[0] = "/micro13_32_tile.spv";
            files[1] = "/micro13_32_detile.spv";
        }
        if (micro13 && texelBytes == 8) {
            files[0] = "/micro13_64_tile.spv";
            files[1] = "/micro13_64_detile.spv";
        }
        if (micro13 && texelBytes == 16) {
            files[0] = "/micro13_128_tile.spv";
            files[1] = "/micro13_128_detile.spv";
        }
        if (samples == 2) {
            files[0] = "/depth1_ms2_tile.spv";
            files[1] = "/depth1_ms2_detile.spv";
        }
        for (uint32_t i = 0; i < 2; ++i) {
            std::ifstream input(std::string(directory) + files[i], std::ios::binary | std::ios::ate);
            if (!input) return 88;
            const auto length = input.tellg();
            if (length < 20 || length > 1024 * 1024 || (static_cast<size_t>(length) & 3)) return 60;
            std::vector<uint32_t> code(static_cast<size_t>(length) / 4);
            input.seekg(0);
            if (!input.read(reinterpret_cast<char*>(code.data()), static_cast<std::streamsize>(length)) ||
                code[0] != 0x07230203)
                return 60;
            VkShaderModuleCreateInfo si{};
            si.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
            si.codeSize = static_cast<size_t>(length);
            si.pCode = code.data();
            if (vkCreateShaderModule(device, &si, nullptr, &shaders[i]) != VK_SUCCESS) return 91;
            VkComputePipelineCreateInfo ci{};
            ci.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
            ci.layout = layout;
            ci.stage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
            ci.stage.stage = VK_SHADER_STAGE_COMPUTE_BIT;
            ci.stage.module = shaders[i];
            ci.stage.pName = "main";
            const auto createStarted = Clock::now();
            const auto created = vkCreateComputePipelines(device, VK_NULL_HANDLE, 1, &ci, nullptr, &pipelines[i]);
            pipelineMs[i] = std::chrono::duration<double, std::milli>(Clock::now() - createStarted).count();
            if (created != VK_SUCCESS) return 91;
        }
        if (!pipelines[0] || !pipelines[1] || pipelines[0] == pipelines[1]) return 91;
        std::puts(
            "whole public tile/detile Vulkan compute pipelines created; bindings storage0/storage1/scalarUniform2; dispatch_control_next=true retail_shader0=true");
        bodyStarted = Clock::now();
        bodyEntered = true;
        const int result = body(layout, pipelines, descriptors);
        bodyEnded = Clock::now();
        bodyReturned = true;
        return result;
    };
    int status = 90;
    try {
        status = run();
    } catch (...) {
        status = 90;
    }
    const auto cleanupStarted = Clock::now();
    for (const auto pipeline : pipelines)
        if (pipeline) vkDestroyPipeline(device, pipeline, nullptr);
    if (layout) vkDestroyPipelineLayout(device, layout, nullptr);
    if (descriptors) vkDestroyDescriptorSetLayout(device, descriptors, nullptr);
    for (const auto shader : shaders)
        if (shader) vkDestroyShaderModule(device, shader, nullptr);
    if (timing) {
        const auto ended = Clock::now();
        const auto ms = [](auto a, auto b) { return std::chrono::duration<double, std::milli>(b - a).count(); };
        std::fprintf(
            stderr,
            "CTR_TILING_TIMING bytes=%u thin=%u depth0=%u samples=%u micro13=%u status=%d body_entered=%u body_returned=%u prepare_ms=%.6f pipeline0_ms=%.6f pipeline1_ms=%.6f body_ms=%.6f cleanup_ms=%.6f total_ms=%.6f\n",
            texelBytes, thin ? 1u : 0u, depth0 ? 1u : 0u, samples, micro13 ? 1u : 0u, status, bodyEntered ? 1u : 0u,
            bodyReturned ? 1u : 0u, ms(started, bodyEntered ? bodyStarted : cleanupStarted), pipelineMs[0],
            pipelineMs[1], bodyEntered ? ms(bodyStarted, bodyReturned ? bodyEnded : cleanupStarted) : 0.0,
            ms(cleanupStarted, ended), ms(started, ended));
    }
    return status;
}

// Synthetic fixture wrapper; the trusted pipeline builder is also usable by session owners.
static int ctr_tiling_pipeline_control(VkDevice device, PFN_vkGetDeviceProcAddr get, const char* directory, bool scalar,
                                       bool compute, uint32_t family, const VkPhysicalDeviceMemoryProperties& memory,
                                       bool thin = false) {
    return ctr_with_tiling_pipelines(
        device, get, directory, scalar, compute, thin,
        [&](VkPipelineLayout layout, const VkPipeline* pipelines, VkDescriptorSetLayout descriptors) -> int {
            const int roundtrip =
                ctr_tiling_roundtrip(device, get, family, memory, layout, pipelines, descriptors, thin);
            if (roundtrip || !thin) return roundtrip;
            const uint32_t extents[][2] = {{256, 1088}, {256, 128}, {1024, 576}, {128, 64},
                                           {512, 320},  {256, 192}, {128, 128},  {1024, 64}};
            for (const auto& extent : extents) {
                const int checked = ctr_tiling_roundtrip(device, get, family, memory, layout, pipelines, descriptors,
                                                         true, extent[0], extent[1]);
                if (checked) return checked;
            }
            return 0;
        });
}
