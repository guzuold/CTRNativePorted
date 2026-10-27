#include "depth_ms2_extent.hpp"
#include "guest_attribute_write.h"
#include "guest_color_write.h"
#include "gnm_init.h"
#include "gnm_shader_emit.h"
#include "../direct_memory_copy.hpp"
#include "../../engine/shadPS4/src/core/libraries/gnmdriver/gnmdriver_init.h"
// SPDX-License-Identifier: GPL-2.0-only
// Host Vulkan control only; no PS4 service implementation or game rendering.
#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#define VK_NO_PROTOTYPES
#define VK_USE_PLATFORM_WIN32_KHR
#include <windows.h>
#include <vulkan/vulkan.h>
#include <cstdio>
#include <vector>
#include <cstring>
#include <cstdint>
#include "memory_change.h"
#include "splash_lifecycle.h"
#include "pm4_state_capture.h"
#include "device_shader_profile.hpp"
#include "ms_copy_pipeline.hpp"
#include "pm4_init_plan.hpp"
#include "compute_shader_state.hpp"
#include "shader_binary_metadata.hpp"
#include "shader_code_capture.hpp"
#include "graphics_shader_state.hpp"
#include "../shader_snapshot_owner.hpp"
#include "tiling_pipeline_control.hpp"
#include "pm4_queue_barrier.hpp"
#include "present_control.hpp"
#include "presentation_session.hpp"
#include "graphics_blend_state.hpp"
#include "graphics_fixed_state.hpp"
#include "graphics_pipeline_owner_control.hpp"
#include "tiled_surface.hpp"
#include "tiled_surface_registry.hpp"
#include "tiled_session.hpp"
#include "session_api.h"
#include "status_layout.h"
#include "status_pack.hpp"
#include "../display_event_host.hpp"
#include "../guest_copy_host.hpp"
#include "guest_status_copy.h"
#include "guest_linear_capture.hpp"
#include "../owner_dispatch_host.hpp"

extern "C" int ctr_video_graphics_rect_no_attachment_pipeline_create_v1(void* context, const CtrGraphicsKeyV1* key,
                                                                        uint64_t layout, const uint32_t* vs,
                                                                        uint32_t vsWords, const uint32_t* tcs,
                                                                        uint32_t tcsWords, const uint32_t* tes,
                                                                        uint32_t tesWords, uint64_t* token) {
    if (!context) return 60;
    try {
        return static_cast<PresentationSession*>(context)->CreateRectListDepthPipeline(
            key, layout, 0, vs, vsWords, tcs, tcsWords, tes, tesWords, token, UINT32_MAX, true);
    } catch (...) {
        return 90;
    }
}
extern "C" int ctr_video_graphics_no_attachment_pipeline_create_v1(void* context, const CtrGraphicsKeyV1* key,
                                                                   const CtrGraphicsVertexInputsV1* inputs,
                                                                   const uint32_t* formats, uint32_t count,
                                                                   uint64_t layout, const uint32_t* vs,
                                                                   uint32_t vsWords, const uint32_t* ps,
                                                                   uint32_t psWords, uint64_t* token) {
    if (!context) return 60;
    try {
        return static_cast<PresentationSession*>(context)->CreateGraphicsPipeline(
            key, inputs, formats, count, layout, 0, vs, vsWords, ps, psWords, 0, token, true);
    } catch (...) {
        return 90;
    }
}
extern "C" int ctr_video_graphics_no_attachment_draw_v1(void* context, uint64_t pipeline,
                                                        const CtrGraphicsViewportStateV1* viewport,
                                                        const CtrGraphicsDynamicStateV1* dynamic,
                                                        const CtrGraphicsPushDataV1* push,
                                                        const CtrGraphicsDrawV1* draw) {
    if (!context) return 60;
    try {
        return static_cast<PresentationSession*>(context)->DrawGraphics(pipeline, 0, viewport, dynamic, push, draw,
                                                                        true);
    } catch (...) {
        return 90;
    }
}
extern "C" int ctr_video_image_readback_tiled_v1(void* c, uint64_t token, const void* prior, uint64_t bytes,
                                                 const CtrGraphicsImageExtentV1* extent, void* output) {
    if (!c) return 60;
    try {
        return static_cast<PresentationSession*>(c)->ReadbackImageTiled(token, prior, bytes, extent, output);
    } catch (...) {
        return 90;
    }
}
extern "C" int ctr_video_storage_image_create_v1(void* c, const CtrGraphicsImageParametersV1* p, uint64_t* token) {
    if (!c) return 60;
    try {
        return static_cast<PresentationSession*>(c)->CreateStorageImage(p, token);
    } catch (...) {
        return 90;
    }
}
extern "C" int ctr_video_compute_dispatch_v1(void* c, uint64_t pipeline, const CtrGraphicsPushDataV1* push,
                                             const uint32_t* groups) {
    if (!c) return 60;
    try {
        return static_cast<PresentationSession*>(c)->DispatchCompute(pipeline, push, groups);
    } catch (...) {
        return 90;
    }
}
extern "C" int ctr_video_compute_layout_create_v1(void* c, const CtrGraphicsBindingContractV1* cs, uint64_t* token) {
    if (!c || !cs) return 60;
    CtrGraphicsBindingContractV1 empty{};
    empty.version = 1;
    empty.size = sizeof(empty);
    try {
        return static_cast<PresentationSession*>(c)->CreateGraphicsLayout(&empty, cs, token, false, true);
    } catch (...) {
        return 90;
    }
}
extern "C" int ctr_video_compute_pipeline_create_v1(void* c, uint64_t layout, const uint32_t* code, uint32_t words,
                                                    uint64_t* token) {
    if (!c) return 60;
    try {
        return static_cast<PresentationSession*>(c)->CreateComputePipeline(layout, code, words, token);
    } catch (...) {
        return 90;
    }
}
extern "C" int ctr_video_compute_pipeline_release_v1(void* c, uint64_t token) {
    if (!c) return 60;
    try {
        return static_cast<PresentationSession*>(c)->ReleaseComputePipeline(token);
    } catch (...) {
        return 90;
    }
}
extern "C" int ctr_video_queue_memory_visibility_v1(void* context) {
    if (!context) return 60;
    try {
        return static_cast<PresentationSession*>(context)->QueueMemoryVisibility();
    } catch (...) {
        return 90;
    }
}
extern "C" int ctr_video_graphics_rect_depth_draw_v1(void* context, uint64_t pipeline, uint64_t depth,
                                                     const CtrGraphicsViewportStateV1* viewport,
                                                     const CtrGraphicsDynamicStateV1* state,
                                                     const CtrGraphicsPushDataV1* push, const CtrGraphicsDrawV1* draw) {
    if (!context) return 60;
    try {
        return static_cast<PresentationSession*>(context)->DrawRectListDepth(pipeline, depth, viewport, state, push,
                                                                             draw);
    } catch (...) {
        return 90;
    }
}
extern "C" int ctr_video_graphics_rect_depth_pipeline_create_v1(void* context, const CtrGraphicsKeyV1* key,
                                                                uint64_t layout, uint64_t depth, const uint32_t* vs,
                                                                uint32_t vsWords, const uint32_t* tcs,
                                                                uint32_t tcsWords, const uint32_t* tes,
                                                                uint32_t tesWords, uint64_t* token) {
    if (!context) return 60;
    try {
        return static_cast<PresentationSession*>(context)->CreateRectListDepthPipeline(
            key, layout, depth, vs, vsWords, tcs, tcsWords, tes, tesWords, token);
    } catch (...) {
        return 90;
    }
}
extern "C" int ctr_video_hide_system_splash_v1(void* c) {
    return c ? static_cast<PresentationSession*>(c)->HideSystemSplash() : 60;
}

extern "C" int ctr_video_register_tiled_mapped_v1(void* c, const std::shared_ptr<ctrport::DirectMemoryBacking>* owner,
                                                  uint32_t first, uint32_t count,
                                                  const CtrVideoBufferAttributesV1* attributes,
                                                  const uint64_t* addresses, uint32_t* group) {
    if (!c || !owner || !*owner || !attributes || !addresses || !group) return 60;
    try {
        return static_cast<PresentationSession*>(c)->RegisterTiled(*owner, first, count, *attributes, addresses,
                                                                   *group);
    } catch (...) {
        return 90;
    }
}
extern "C" int ctr_video_submit_tiled_snapshot_v1(void* c, uint32_t slot, uint64_t address, const void* source,
                                                  uint64_t bytes, uint64_t argument) {
    if (!c) return 60;
    try {
        return static_cast<PresentationSession*>(c)->SubmitTiledSnapshot(slot, address, source, bytes, argument);
    } catch (...) {
        return 90;
    }
}
extern "C" int ctr_video_complete_tiled_flip_v1(void* c, uint32_t slot, const CtrVideoCompletionV1* completion) {
    if (!c) return 60;
    try {
        return static_cast<PresentationSession*>(c)->CompleteTiledFlip(slot, completion);
    } catch (...) {
        return 90;
    }
}
extern "C" int ctr_video_adjust_color_v1(void* c, const CtrVideoColorSettingsV1* settings) {
    if (!c || !settings) return 60;
    const auto copied = *settings;
    try {
        return static_cast<PresentationSession*>(c)->AdjustIdentityColor(copied.gamma);
    } catch (...) {
        return 90;
    }
}
extern "C" int ctr_video_bind_clock_v1(void* c, const CtrDisplayEventApi* clock) {
    if (!c || !clock) return 60;
    try {
        return static_cast<PresentationSession*>(c)->BindSessionClock(*clock);
    } catch (...) {
        return 90;
    }
}
extern "C" int ctr_video_end_tiled_frame_v1(void* c, const CtrVideoCompletionV1* completion, uint64_t* frame) {
    if (!c) return 60;
    try {
        return static_cast<PresentationSession*>(c)->EndTiledFrame(completion, frame);
    } catch (...) {
        return 90;
    }
}
extern "C" int ctr_video_unregister_tiled_v1(void* c, uint32_t group) {
    return c ? static_cast<PresentationSession*>(c)->UnregisterTiled(group) : 60;
}
extern "C" int ctr_video_tiled_label_address_v1(void* c, uint64_t* address) {
    if (!c || !address) return 60;
    return static_cast<PresentationSession*>(c)->TiledLabelAddress(*address);
}

extern "C" int ctr_video_admit_memory_change_v1(void* c, uint64_t address, uint64_t bytes) {
    return c ? static_cast<PresentationSession*>(c)->AdmitMemoryChange(address, bytes) : 60;
}
extern "C" int ctr_video_graphics_dynamic_preflight_v1(void* context, const CtrGraphicsViewportStateV1* viewport,
                                                       const CtrGraphicsDynamicStateV1* state) {
    if (!context) return 60;
    return static_cast<PresentationSession*>(context)->PreflightGraphicsDynamic(viewport, state);
}
extern "C" int ctr_video_graphics_pipeline_create_v1(void* context, const CtrGraphicsKeyV1* key,
                                                     const CtrGraphicsVertexInputsV1* vertices, const uint32_t* formats,
                                                     uint32_t formatCount, uint64_t layout, uint64_t color,
                                                     const uint32_t* vs, uint32_t vsWords, const uint32_t* ps,
                                                     uint32_t psWords, uint32_t sampleShading, uint64_t* token) {
    if (!context) return 60;
    try {
        return static_cast<PresentationSession*>(context)->CreateGraphicsPipeline(
            key, vertices, formats, formatCount, layout, color, vs, vsWords, ps, psWords, sampleShading, token);
    } catch (...) {
        return 90;
    }
}
extern "C" int ctr_video_graphics_pipeline_release_v1(void* context, uint64_t token) {
    if (!context) return 60;
    return static_cast<PresentationSession*>(context)->ReleaseGraphicsPipeline(token);
}
extern "C" int ctr_video_graphics_vertex_layout_create_v1(void* context, const CtrGraphicsBindingContractV1* vs,
                                                          uint64_t* token) {
    if (!context) return 60;
    try {
        return static_cast<PresentationSession*>(context)->CreateGraphicsLayout(nullptr, vs, token, false);
    } catch (...) {
        return 90;
    }
}
extern "C" int ctr_video_graphics_layout_create_v1(void* context, const CtrGraphicsBindingContractV1* ps,
                                                   const CtrGraphicsBindingContractV1* vs, uint64_t* token) {
    if (!context) return 60;
    try {
        return static_cast<PresentationSession*>(context)->CreateGraphicsLayout(ps, vs, token);
    } catch (...) {
        return 90;
    }
}
extern "C" int ctr_video_graphics_vertex_inputs_preflight_v1(void* context, const CtrGraphicsVertexInputsV1* inputs,
                                                             const uint32_t* formats, uint32_t count) {
    return context ? static_cast<PresentationSession*>(context)->PreflightVertexInputs(inputs, formats, count) : 60;
}
extern "C" int ctr_video_graphics_core_authority_v1(void* context, uint32_t required) {
    return context ? static_cast<PresentationSession*>(context)->GraphicsCoreAuthority(required) : 60;
}
extern "C" int ctr_video_graphics_dynamic_authority_v1(void* context, uint32_t required) {
    return context ? static_cast<PresentationSession*>(context)->GraphicsDynamicAuthority(required) : 60;
}
extern "C" int ctr_video_dynamic_rendering_authority_v1(void* context) {
    return context ? static_cast<PresentationSession*>(context)->DynamicRenderingAuthority() : 60;
}
extern "C" int ctr_video_color_attachment_create_v1(void* context, const CtrGraphicsColorRequirementV1* p,
                                                    uint32_t blending, uint64_t* token) {
    if (!context) return 60;
    try {
        return static_cast<PresentationSession*>(context)->CreateColorAttachment(p, blending, token);
    } catch (...) {
        return 90;
    }
}
extern "C" int ctr_video_color_attachment_upload_tiled_v1(void* context, uint64_t token, const void* source,
                                                          uint64_t bytes, const CtrGraphicsImageExtentV1* extent) {
    if (!context) return 60;
    try {
        return static_cast<PresentationSession*>(context)->UploadColorTiled(token, source, bytes, extent);
    } catch (...) {
        return 90;
    }
}
extern "C" int ctr_video_color_to_sampled_v1(void* context, uint64_t colorToken, uint64_t imageToken) {
    if (!context) return 60;
    try {
        return static_cast<PresentationSession*>(context)->CopyColorToSampled(colorToken, imageToken);
    } catch (...) {
        return 90;
    }
}
extern "C" int ctr_video_color_attachment_upload_linear_v1(void* context, uint64_t token, const void* source,
                                                           uint64_t bytes, uint64_t pitch) {
    if (!context) return 60;
    try {
        return static_cast<PresentationSession*>(context)->TransferColorAttachment(token, source, nullptr, bytes,
                                                                                   pitch);
    } catch (...) {
        return 90;
    }
}
extern "C" int ctr_video_color_attachment_readback_linear_v1(void* context, uint64_t token, void* destination,
                                                             uint64_t bytes, uint64_t pitch) {
    if (!context) return 60;
    try {
        return static_cast<PresentationSession*>(context)->TransferColorAttachment(token, nullptr, destination, bytes,
                                                                                   pitch);
    } catch (...) {
        return 90;
    }
}
extern "C" int ctr_video_color_attachment_release_v1(void* context, uint64_t token) {
    return context ? static_cast<PresentationSession*>(context)->ReleaseColorAttachment(token) : 60;
}
extern "C" int ctr_video_graphics_layout_images_null_v1(void* context, uint64_t token, const uint32_t* bindings,
                                                        const uint64_t* tokens, const uint32_t* nulls, uint32_t count) {
    if (!context || (count && !nulls)) return 60;
    try {
        return static_cast<PresentationSession*>(context)->WriteGraphicsImages(token, bindings, tokens, count, nulls);
    } catch (...) {
        return 90;
    }
}
extern "C" int ctr_video_graphics_layout_images_v1(void* context, uint64_t token, const uint32_t* bindings,
                                                   const uint64_t* tokens, uint32_t count) {
    if (!context) return 60;
    try {
        return static_cast<PresentationSession*>(context)->WriteGraphicsImages(token, bindings, tokens, count);
    } catch (...) {
        return 90;
    }
}
extern "C" int ctr_video_graphics_layout_samplers_v1(void* context, uint64_t token, const uint32_t* bindings,
                                                     const uint64_t* tokens, uint32_t count) {
    if (!context) return 60;
    try {
        return static_cast<PresentationSession*>(context)->WriteGraphicsSamplers(token, bindings, tokens, count);
    } catch (...) {
        return 90;
    }
}
extern "C" int ctr_video_graphics_layout_buffers_v1(void* context, uint64_t token,
                                                    const CtrGraphicsBufferWriteV1* writes, uint32_t count) {
    if (!context) return 60;
    try {
        return static_cast<PresentationSession*>(context)->WriteGraphicsBuffers(token, writes, count);
    } catch (...) {
        return 90;
    }
}
extern "C" int ctr_video_graphics_layout_release_v1(void* context, uint64_t token) {
    if (!context) return 60;
    return static_cast<PresentationSession*>(context)->ReleaseGraphicsLayout(token);
}
extern "C" int ctr_video_depth_attachment_create_v1(void* context, const CtrDepthAttachmentParametersV1* p,
                                                    uint64_t* token) {
    if (!context) return 60;
    try {
        return static_cast<PresentationSession*>(context)->CreateDepthAttachment(p, token);
    } catch (...) {
        return 90;
    }
}
extern "C" int ctr_video_depth_single_spans_v1(const CtrDepthSingleLayoutV1* p, uint64_t stencil,
                                               CtrDepthBackingSpansV1* output) {
    return CtrPm4::DepthSingleSpans(p, stencil, output);
}
extern "C" int ctr_video_depth_single_residency_register_v1(void* context, uint64_t token,
                                                            const CtrDepthSingleLayoutV1* layout, uint64_t stencil,
                                                            uint64_t* generation) {
    if (!context) return 60;
    try {
        return static_cast<PresentationSession*>(context)->RegisterDepthResidency(token, layout, stencil, generation,
                                                                                  true);
    } catch (...) {
        return 90;
    }
}
extern "C" int ctr_video_depth_ms2_spans_v1(const CtrDepthMS2LayoutV1* p, uint64_t stencil,
                                            CtrDepthBackingSpansV1* output) {
    return CtrPm4::DepthMS2Spans(p, stencil, output);
}
extern "C" int ctr_video_depth_ms2_extent_v1(const CtrDepthMS2LayoutV1* p, CtrGraphicsImageExtentV1* output) {
    return CtrPm4::DepthMS2Extent(p, output);
}
extern "C" int ctr_video_depth_ms2_retile_samples_v1(void* context, uint64_t token, const CtrDepthMS2LayoutV1* p,
                                                     const void* prior, uint64_t bytes, const void* samples,
                                                     uint64_t sampleBytes, void* output) {
    if (!context) return 60;
    try {
        return static_cast<PresentationSession*>(context)->RetileDepthMS2Samples(token, p, prior, bytes, samples,
                                                                                 sampleBytes, output);
    } catch (...) {
        return 90;
    }
}
extern "C" int ctr_video_depth_d16_extent_v1(const CtrDepthD16LayoutV1* p, CtrGraphicsImageExtentV1* output) {
    return CtrPm4::DepthD16Extent(p, output);
}
extern "C" int ctr_video_depth_d16_read_tiled_v1(void* context, uint64_t token, const CtrDepthD16LayoutV1* p,
                                                 const void* prior, uint64_t bytes, void* output) {
    if (!context) return 60;
    try {
        return static_cast<PresentationSession*>(context)->ReadDepthD16Tiled(token, p, prior, bytes, output);
    } catch (...) {
        return 90;
    }
}
extern "C" int ctr_video_depth_d16_read_linear_v1(void* context, uint64_t token, void* destination, uint64_t bytes,
                                                  uint64_t pitch) {
    if (!context) return 60;
    try {
        return static_cast<PresentationSession*>(context)->ReadDepthD16(token, destination, bytes, pitch);
    } catch (...) {
        return 90;
    }
}
extern "C" int ctr_video_depth_attachment_clear_v1(void* context, uint64_t token, uint32_t depthBits,
                                                   uint32_t stencil) {
    if (!context) return 60;
    try {
        return static_cast<PresentationSession*>(context)->ClearDepthAttachment(token, depthBits, stencil);
    } catch (...) {
        return 90;
    }
}
extern "C" int ctr_video_depth_residency_register_v1(void* context, uint64_t token, const CtrDepthMS2LayoutV1* layout,
                                                     uint64_t stencil, uint64_t* generation) {
    if (!context) return 60;
    try {
        return static_cast<PresentationSession*>(context)->RegisterDepthResidency(token, layout, stencil, generation);
    } catch (...) {
        return 90;
    }
}
extern "C" int ctr_video_depth_residency_acquire_v1(void* context, const CtrDepthBackingSpansV1* spans,
                                                    uint64_t generation, uint64_t* lease) {
    if (!context) return 60;
    try {
        return static_cast<PresentationSession*>(context)->AcquireDepthResidency(spans, generation, lease);
    } catch (...) {
        return 90;
    }
}
extern "C" int ctr_video_depth_residency_remove_v1(void* context, uint64_t generation) {
    if (!context) return 60;
    return static_cast<PresentationSession*>(context)->RemoveDepthResidency(generation);
}
extern "C" int ctr_video_depth_attachment_retain_v1(void* context, uint64_t token, uint64_t* lease) {
    if (!context) return 60;
    try {
        return static_cast<PresentationSession*>(context)->RetainDepthAttachment(token, lease);
    } catch (...) {
        return 90;
    }
}
extern "C" int ctr_video_depth_attachment_release_v1(void* context, uint64_t token) {
    if (!context) return 60;
    return static_cast<PresentationSession*>(context)->ReleaseDepthAttachment(token);
}
extern "C" int ctr_video_buffer_create_v1(void* context, const void* source, uint64_t bytes, uint32_t usage,
                                          uint64_t* token) {
    if (!context) return 60;
    try {
        return static_cast<PresentationSession*>(context)->CreateBuffer(source, bytes, usage, token);
    } catch (...) {
        return 90;
    }
}
extern "C" int ctr_video_buffer_copy_initial_v1(void* context, uint64_t token, void* output, uint64_t bytes) {
    if (!context) return 60;
    return static_cast<PresentationSession*>(context)->CopyBufferInitial(token, output, bytes);
}
extern "C" int ctr_video_buffer_release_v1(void* context, uint64_t token) {
    if (!context) return 60;
    return static_cast<PresentationSession*>(context)->ReleaseBuffer(token);
}
extern "C" int ctr_video_sampled_image_create_v1(void* context, const CtrGraphicsImageParametersV1* p,
                                                 uint64_t* token) {
    if (!context) return 60;
    try {
        return static_cast<PresentationSession*>(context)->CreateSampledImage(p, token);
    } catch (...) {
        return 90;
    }
}
extern "C" int ctr_video_sampled_image_upload_linear_v1(void* context, uint64_t token, const void* source,
                                                        uint64_t bytes, uint64_t pitch) {
    if (!context) return 60;
    try {
        return static_cast<PresentationSession*>(context)->UploadSampledImage(token, source, bytes, pitch);
    } catch (...) {
        return 90;
    }
}
extern "C" int ctr_video_sampled_image_readback_linear_v1(void* context, uint64_t token, void* destination,
                                                          uint64_t bytes, uint64_t pitch) {
    if (!context) return 60;
    try {
        return static_cast<PresentationSession*>(context)->ReadbackSampledImage(token, destination, bytes, pitch);
    } catch (...) {
        return 90;
    }
}
extern "C" int ctr_video_sampled_image_release_v1(void* context, uint64_t token) {
    return context ? static_cast<PresentationSession*>(context)->ReleaseSampledImage(token) : 60;
}
extern "C" int ctr_video_sampled_image_upload_tiled_v1(void* context, uint64_t token, const void* source,
                                                       uint64_t bytes, const CtrGraphicsImageExtentV1* extent) {
    if (!context) return 60;
    try {
        return static_cast<PresentationSession*>(context)->UploadSampledTiled(token, source, bytes, extent);
    } catch (...) {
        return 90;
    }
}
extern "C" int ctr_video_sampler_create_v1(void* context, const CtrGraphicsSamplerParametersV1* p, uint64_t* token) {
    if (!context) return 60;
    try {
        return static_cast<PresentationSession*>(context)->CreateSampler(p, token);
    } catch (...) {
        return 90;
    }
}
extern "C" int ctr_video_sampler_release_v1(void* context, uint64_t token) {
    return context ? static_cast<PresentationSession*>(context)->ReleaseSampler(token) : 60;
}
extern "C" int ctr_video_capture_graphics_profile_v1(void* context, CtrGraphicsProfileV1* output) {
    if (!context) return 60;
    return static_cast<PresentationSession*>(context)->CaptureGraphicsProfile(output);
}
extern "C" int ctr_video_capture_pm4_state_v1(void* c, uint32_t* flat, uint32_t flatWords, uint32_t* compute,
                                              uint32_t computeWords, uint64_t* completed) {
    if (!c) return 60;
    return static_cast<PresentationSession*>(c)->CapturePm4State(flat, flatWords, compute, computeWords, completed);
}
extern "C" int ctr_video_submit_init_v1(void* c, const std::shared_ptr<ctrport::DirectMemoryBacking>* owner,
                                        const uint32_t* data, size_t words) {
    if (!c || !owner || !*owner || !data) return 60;
    try {
        return static_cast<PresentationSession*>(c)->SubmitInit(*owner, data, words);
    } catch (...) {
        return 90;
    }
}
extern "C" int ctr_video_configure_output_any_v1(void* c, uint32_t reserved, const CtrVideoOutputModeV1* mode,
                                                 const void* options, uint32_t sizeMode, uint32_t sizeOptions) {
    if (!c || !mode) return 60;
    return static_cast<PresentationSession*>(c)->ConfigureOutputAny(reserved, *mode, options, sizeMode, sizeOptions);
}

extern "C" int ctr_video_session_api_v1(void* context, CtrVideoSessionApiV1* output) {
    if (!context || !output) return 60;
    CtrVideoSessionApiV1 api{};
    api.size = sizeof(api);
    api.version = 1;
    api.context = context;
    api.register_frame = [](void* c, uint32_t slot, const void* data, size_t bytes, size_t pitch,
                            uint64_t* token) -> int {
        if (!c || !token) return 60;
        try {
            const auto value = static_cast<PresentationSession*>(c)->RegisterHostFrame(slot, data, bytes, pitch);
            if (!value) return 61;
            *token = value;
            return 0;
        } catch (...) {
            return 90;
        }
    };
    api.unregister_frame = [](void* c, uint32_t slot, uint64_t token) -> int {
        if (!c) return 60;
        return static_cast<PresentationSession*>(c)->UnregisterHostFrame(slot, token) ? 0 : 62;
    };
    api.enqueue = [](void* c, uint32_t slot, uint64_t token, uint64_t argument) -> int {
        if (!c) return 60;
        try {
            auto& session = *static_cast<PresentationSession*>(c);
            return session.EnqueueHostFrame(session.CaptureHostFrame(slot, token), argument) ? 0 : 63;
        } catch (...) {
            return 90;
        }
    };
    api.attach_events = [](void* c, const CtrDisplayEventApi* kernel, uint64_t queue, uint64_t flipData,
                           uint64_t vblankData) -> int {
        if (!c || !kernel) return 60;
        try {
            return static_cast<PresentationSession*>(c)->AttachEvents(*kernel, queue, flipData, vblankData);
        } catch (...) {
            return 90;
        }
    };
    api.detach_events = [](void* c) -> int {
        if (!c) return 60;
        try {
            return static_cast<PresentationSession*>(c)->DetachEvents();
        } catch (...) {
            return 90;
        }
    };
    api.add_event = [](void* c, const CtrDisplayEventApi* kernel, uint64_t queue, uint64_t ident,
                       uint64_t data) -> int {
        if (!c || !kernel) return 60;
        try {
            return static_cast<PresentationSession*>(c)->AddEvent(*kernel, queue, ident, data);
        } catch (...) {
            return 90;
        }
    };
    api.remove_event = [](void* c, uint64_t queue, uint64_t ident) -> int {
        if (!c) return 60;
        try {
            return static_cast<PresentationSession*>(c)->RemoveEvent(queue, ident);
        } catch (...) {
            return 90;
        }
    };
    api.get_public_flip = [](void* c, void* out, size_t bytes) -> int {
        if (!c || !out || bytes < sizeof(CtrVideoPublicFlipStatus)) return 60;
        CtrVideoProgressV1 value{};
        auto* session = static_cast<PresentationSession*>(c);
        session->Progress(value.completed, value.token, value.argument, value.pending, value.current_slot);
        session->FlipTiming(value.submit_tsc, value.completion_microseconds, value.completion_tsc, value.timing_valid);
        return CtrVideo::PackCpuFlip(value, out, bytes);
    };
    api.get_public_vblank = [](void* c, void* out, size_t bytes) -> int {
        if (!c || !out || bytes < sizeof(CtrVideoPublicVblankStatus)) return 60;
        CtrVideoVblankV1 value{};
        const int error = static_cast<PresentationSession*>(c)->VblankStatus(value.count, value.process_microseconds,
                                                                             value.native_tsc);
        return error ? error : CtrVideo::PackVblank(value, out, bytes);
    };
    api.get_vblank = [](void* c, CtrVideoVblankV1* out) -> int {
        if (!c || !out) return 60;
        CtrVideoVblankV1 value{};
        const int error = static_cast<PresentationSession*>(c)->VblankStatus(value.count, value.process_microseconds,
                                                                             value.native_tsc);
        if (error) return error;
        *out = value;
        return 0;
    };
    api.get_progress = [](void* c, CtrVideoProgressV1* out) -> int {
        if (!c || !out) return 60;
        CtrVideoProgressV1 value{};
        static_cast<PresentationSession*>(c)->Progress(value.completed, value.token, value.argument, value.pending,
                                                       value.current_slot);
        static_cast<PresentationSession*>(c)->FlipTiming(value.submit_tsc, value.completion_microseconds,
                                                         value.completion_tsc, value.timing_valid);
        *out = value;
        return 0;
    };
    api.start_pacing = [](void* c) -> int {
        if (!c) return 60;
        try {
            return static_cast<PresentationSession*>(c)->StartPacing() ? 0 : 64;
        } catch (...) {
            return 90;
        }
    };
    api.stop_pacing = [](void* c) -> int {
        if (!c) return 60;
        try {
            return static_cast<PresentationSession*>(c)->StopPacing() ? 0 : 65;
        } catch (...) {
            return 90;
        }
    };
    api.register_group = [](void* c, uint32_t first, uint32_t count, const CtrVideoHostBufferV1* inputs,
                            uint32_t* group, uint64_t* tokens, size_t capacity) -> int {
        if (!c || !inputs || !group || !tokens || !count || count > 16 || capacity < count) return 60;
        try {
            std::array<PresentationSession::HostBufferInput, 16> values{};
            for (uint32_t i = 0; i < count; ++i)
                values[i] = {inputs[i].bytes, inputs[i].byte_length, inputs[i].pitch_bytes};
            auto* session = static_cast<PresentationSession*>(c);
            uint32_t result = 0;
            const int error = session->RegisterHostGroup(first, count, values.data(), result);
            if (error) return error;
            for (uint32_t i = 0; i < count; ++i) tokens[i] = session->CaptureHostSlot(first + i)->token;
            *group = result;
            return 0;
        } catch (...) {
            return 90;
        }
    };
    api.register_linear_group = [](void* c, uint32_t first, uint32_t count,
                                   const CtrVideoBufferAttributesV1* attributes, const CtrVideoHostBufferV1* inputs,
                                   uint32_t* group, uint64_t* tokens, size_t capacity) -> int {
        if (!c || !attributes || !inputs || !group || !tokens || !count || count > 16 || capacity < count) return 60;
        try {
            const auto& a = *attributes;
            const CtrVideo::DisplayBufferAttributes captured{
                a.format, a.tiling, a.aspect, a.width, a.height, a.pitch_pixels, a.option, a.reserved0, a.reserved1};
            std::array<PresentationSession::HostBufferInput, 16> values{};
            for (uint32_t i = 0; i < count; ++i) values[i] = {inputs[i].bytes, inputs[i].byte_length, 0};
            auto* session = static_cast<PresentationSession*>(c);
            uint32_t result = 0;
            const int error = session->RegisterLinearGroup(first, count, captured, values.data(), result);
            if (error) return error;
            for (uint32_t i = 0; i < count; ++i) tokens[i] = session->CaptureHostSlot(first + i)->token;
            *group = result;
            return 0;
        } catch (...) {
            return 90;
        }
    };
    api.unregister_group = [](void* c, uint32_t group) -> int {
        if (!c) return 60;
        return static_cast<PresentationSession*>(c)->UnregisterHostGroup(group) ? 0 : 62;
    };
    api.wait_vblank = [](void* c, uint32_t timeout) -> int {
        if (!c || timeout > 5000) return 60;
        try {
            return static_cast<PresentationSession*>(c)->WaitVblank(std::chrono::milliseconds(timeout));
        } catch (...) {
            return 90;
        }
    };
    api.present_next = [](void* c, uint32_t timeout, CtrVideoCompletionV1* out) -> int {
        if (!c || !out || timeout > 5000) return 60;
        try {
            PresentationSession::Completion value{};
            const auto status =
                static_cast<PresentationSession*>(c)->PresentNextAtVblank(std::chrono::milliseconds(timeout), value);
            if (status) return status;
            *out = {value.count, value.token, value.argument};
            return 0;
        } catch (...) {
            return 90;
        }
    };
    api.refresh_frame = [](void* c, uint32_t slot, uint64_t token, const void* source, size_t bytes,
                           size_t pitch) -> int {
        if (!c) return 60;
        try {
            return static_cast<PresentationSession*>(c)->RefreshHostFrame(slot, token, source, bytes, pitch) ? 0 : 77;
        } catch (...) {
            return 90;
        }
    };
    api.submit_linear_frame = [](void* c, uint32_t slot, uint64_t token, const CtrVideoLinearFrameV1* input,
                                 uint64_t argument) -> int {
        if (!c || !input || input->size != sizeof(*input)) return 60;
        try {
            return static_cast<PresentationSession*>(c)->SubmitLinearFrame(slot, token, input->format, input->width,
                                                                           input->height, input->pitch_pixels,
                                                                           input->bytes, input->byte_length, argument);
        } catch (...) {
            return 90;
        }
    };
    api.refresh_linear_frame = [](void* c, uint32_t slot, uint64_t token, const CtrVideoLinearFrameV1* input) -> int {
        if (!c || !input || input->size != sizeof(*input)) return 60;
        try {
            return static_cast<PresentationSession*>(c)->RefreshLinearFrame(slot, token, input->format, input->width,
                                                                            input->height, input->pitch_pixels,
                                                                            input->bytes, input->byte_length);
        } catch (...) {
            return 90;
        }
    };
    api.register_linear_frame = [](void* c, uint32_t slot, const CtrVideoLinearFrameV1* input, uint64_t* token) -> int {
        if (!c || !input || !token || input->size != sizeof(*input)) return 60;
        try {
            return static_cast<PresentationSession*>(c)->RegisterLinearFrame(slot, input->format, input->width,
                                                                             input->height, input->pitch_pixels,
                                                                             input->bytes, input->byte_length, *token);
        } catch (...) {
            return 90;
        }
    };
    api.pump_messages = [](void* c) -> int {
        if (!c) return 60;
        return static_cast<PresentationSession*>(c)->PumpMessages();
    };
    api.get_public_capability = [](void* c, uint64_t* out) -> int {
        if (!c || !out) return 60;
        uint64_t value = 0;
        const int status = static_cast<PresentationSession*>(c)->PublicCapability(value);
        if (status) return status;
        *out = value;
        return 0;
    };
    api.set_flip_rate = [](void* c, int32_t rate) -> int {
        return c ? static_cast<PresentationSession*>(c)->SetFlipRate(rate) : 60;
    };
    *output = api;
    return 0;
}

static LRESULT CALLBACK video_window_proc(HWND window, UINT message, WPARAM wparam, LPARAM lparam) {
    if (message == WM_CLOSE) {
        auto* closed = reinterpret_cast<bool*>(GetWindowLongPtrW(window, GWLP_USERDATA));
        if (closed) {
            *closed = true;
            return 0;
        }
    }
    return DefWindowProcW(window, message, wparam, lparam);
}
static int run_session(CtrVideoSessionBodyV1 body, void* user, const CtrVideoWindowOptionsV1* options = nullptr) {
    // Hidden host window for surface capability control; no frame is claimed.
    const auto module = GetModuleHandleW(nullptr);
    WNDCLASSW wc{};
    wc.lpfnWndProc = video_window_proc;
    wc.hInstance = module;
    wc.lpszClassName = L"CTRPortVulkanControl";
    if (!RegisterClassW(&wc)) return 10;
    int windowWidth = 640, windowHeight = 480;
    if (options) {
        RECT desired{0, 0, static_cast<LONG>(options->width), static_cast<LONG>(options->height)};
        if (!AdjustWindowRect(&desired, WS_OVERLAPPEDWINDOW, FALSE)) {
            UnregisterClassW(wc.lpszClassName, module);
            return 83;
        }
        windowWidth = desired.right - desired.left;
        windowHeight = desired.bottom - desired.top;
    }
    const auto window = CreateWindowExW(0, wc.lpszClassName, L"CTRPort control", WS_OVERLAPPEDWINDOW, CW_USEDEFAULT,
                                        CW_USEDEFAULT, windowWidth, windowHeight, nullptr, nullptr, module, nullptr);
    if (!window) {
        UnregisterClassW(wc.lpszClassName, module);
        return 11;
    }
    bool closeRequested = false;
    SetWindowLongPtrW(window, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(&closeRequested));
    struct WindowCleanup {
        HWND w;
        HINSTANCE m;
        ~WindowCleanup() {
            DestroyWindow(w);
            UnregisterClassW(L"CTRPortVulkanControl", m);
        }
    } windowCleanup{window, module};
    if (options && options->visible) {
        ShowWindow(window, SW_SHOWNOACTIVATE);
        if (!IsWindowVisible(window)) return 84;
        std::printf("host window visible flag accepted=true visual_observation=false\n");
    }
    const auto loader = LoadLibraryExW(L"vulkan-1.dll", nullptr, LOAD_LIBRARY_SEARCH_SYSTEM32);
    if (!loader) {
        std::fprintf(stderr, "missing system Vulkan loader: %lu\n", GetLastError());
        return 1;
    }
    const auto get = reinterpret_cast<PFN_vkGetInstanceProcAddr>(GetProcAddress(loader, "vkGetInstanceProcAddr"));
    if (!get) {
        FreeLibrary(loader);
        return 2;
    }
    const auto create = reinterpret_cast<PFN_vkCreateInstance>(get(nullptr, "vkCreateInstance"));
    if (!create) {
        FreeLibrary(loader);
        return 3;
    }
    VkApplicationInfo app{};
    app.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    app.pApplicationName = "CTRPort host control";
    const auto instanceVersion =
        reinterpret_cast<PFN_vkEnumerateInstanceVersion>(get(nullptr, "vkEnumerateInstanceVersion"));
    uint32_t loaderVersion = VK_API_VERSION_1_0;
    if (instanceVersion && instanceVersion(&loaderVersion) != VK_SUCCESS) {
        FreeLibrary(loader);
        return 140;
    }
    app.apiVersion = loaderVersion >= VK_API_VERSION_1_2 ? VK_API_VERSION_1_2 : VK_API_VERSION_1_0;
    VkInstanceCreateInfo info{};
    info.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    info.pApplicationInfo = &app;
    const char* instanceExtensions[] = {VK_KHR_SURFACE_EXTENSION_NAME, VK_KHR_WIN32_SURFACE_EXTENSION_NAME};
    info.enabledExtensionCount = 2;
    info.ppEnabledExtensionNames = instanceExtensions;
    VkInstance instance{};
    auto result = create(&info, nullptr, &instance);
    if (result != VK_SUCCESS) {
        std::fprintf(stderr, "vkCreateInstance=%d\n", result);
        FreeLibrary(loader);
        return 4;
    }
    const auto destroy = reinterpret_cast<PFN_vkDestroyInstance>(get(instance, "vkDestroyInstance"));
    const auto enumerate =
        reinterpret_cast<PFN_vkEnumeratePhysicalDevices>(get(instance, "vkEnumeratePhysicalDevices"));
    const auto properties =
        reinterpret_cast<PFN_vkGetPhysicalDeviceProperties>(get(instance, "vkGetPhysicalDeviceProperties"));
    const auto memoryProperties =
        reinterpret_cast<PFN_vkGetPhysicalDeviceMemoryProperties>(get(instance, "vkGetPhysicalDeviceMemoryProperties"));
    const auto queues = reinterpret_cast<PFN_vkGetPhysicalDeviceQueueFamilyProperties>(
        get(instance, "vkGetPhysicalDeviceQueueFamilyProperties"));
    const auto deviceCreate = reinterpret_cast<PFN_vkCreateDevice>(get(instance, "vkCreateDevice"));
    const auto deviceGet = reinterpret_cast<PFN_vkGetDeviceProcAddr>(get(instance, "vkGetDeviceProcAddr"));
    const auto surfaceCreate = reinterpret_cast<PFN_vkCreateWin32SurfaceKHR>(get(instance, "vkCreateWin32SurfaceKHR"));
    const auto surfaceDestroy = reinterpret_cast<PFN_vkDestroySurfaceKHR>(get(instance, "vkDestroySurfaceKHR"));
    const auto surfaceSupport = reinterpret_cast<PFN_vkGetPhysicalDeviceSurfaceSupportKHR>(
        get(instance, "vkGetPhysicalDeviceSurfaceSupportKHR"));
    const auto surfaceCaps = reinterpret_cast<PFN_vkGetPhysicalDeviceSurfaceCapabilitiesKHR>(
        get(instance, "vkGetPhysicalDeviceSurfaceCapabilitiesKHR"));
    const auto surfaceFormats = reinterpret_cast<PFN_vkGetPhysicalDeviceSurfaceFormatsKHR>(
        get(instance, "vkGetPhysicalDeviceSurfaceFormatsKHR"));
    const auto features2 =
        reinterpret_cast<PFN_vkGetPhysicalDeviceFeatures2>(get(instance, "vkGetPhysicalDeviceFeatures2"));
    VkSurfaceKHR surface{};
    VkWin32SurfaceCreateInfoKHR sci{};
    sci.sType = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR;
    sci.hinstance = module;
    sci.hwnd = window;
    if (!surfaceCreate || !surfaceDestroy || !surfaceSupport || !surfaceCaps || !surfaceFormats ||
        surfaceCreate(instance, &sci, nullptr, &surface) != VK_SUCCESS) {
        if (destroy) destroy(instance, nullptr);
        FreeLibrary(loader);
        return 12;
    }
    int status = 5;
    bool invoked = false;
    if (destroy && enumerate && properties && memoryProperties && queues && deviceCreate && deviceGet) {
        uint32_t count{};
        result = enumerate(instance, &count, nullptr);
        if (result == VK_SUCCESS && count) {
            std::vector<VkPhysicalDevice> devices(count);
            result = enumerate(instance, &count, devices.data());
            if (result == VK_SUCCESS)
                for (uint32_t d = 0; d < count; ++d) {
                    uint32_t n{};
                    queues(devices[d], &n, nullptr);
                    std::vector<VkQueueFamilyProperties> families(n);
                    queues(devices[d], &n, families.data());
                    for (uint32_t q = 0; q < n; ++q) {
                        if (!families[q].queueCount || !(families[q].queueFlags & VK_QUEUE_GRAPHICS_BIT)) continue;
                        VkBool32 supported{};
                        if (surfaceSupport(devices[d], q, surface, &supported) != VK_SUCCESS || !supported) continue;
                        float priority = 1.0f;
                        VkDeviceQueueCreateInfo qi{};
                        qi.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
                        qi.queueFamilyIndex = q;
                        qi.queueCount = 1;
                        qi.pQueuePriorities = &priority;
                        VkDeviceCreateInfo di{};
                        di.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
                        di.queueCreateInfoCount = 1;
                        di.pQueueCreateInfos = &qi;
                        const char* deviceExtensions[] = {
                            VK_KHR_SWAPCHAIN_EXTENSION_NAME, nullptr, nullptr, nullptr, nullptr, nullptr};
                        di.enabledExtensionCount = 1;
                        di.ppEnabledExtensionNames = deviceExtensions;
                        VkPhysicalDeviceProperties selectedProperties{};
                        properties(devices[d], &selectedProperties);
                        const auto enumDeviceExtensions = reinterpret_cast<PFN_vkEnumerateDeviceExtensionProperties>(
                            get(instance, "vkEnumerateDeviceExtensionProperties"));
                        bool mixedQueried = false, mixedAmd = false;
                        bool robustnessExtension = false, dynamicRenderingExtension = false, edsExtension = false,
                             eds2Extension = false;
                        uint32_t extensionCount = 0;
                        if (enumDeviceExtensions &&
                            enumDeviceExtensions(devices[d], nullptr, &extensionCount, nullptr) == VK_SUCCESS) {
                            std::vector<VkExtensionProperties> extensions(extensionCount);
                            if (enumDeviceExtensions(devices[d], nullptr, &extensionCount, extensions.data()) ==
                                VK_SUCCESS) {
                                mixedQueried = true;
                                for (const auto& extension : extensions) {
                                    if (std::strcmp(extension.extensionName,
                                                    VK_AMD_MIXED_ATTACHMENT_SAMPLES_EXTENSION_NAME) == 0)
                                        mixedAmd = true;
                                    if (std::strcmp(extension.extensionName, VK_EXT_ROBUSTNESS_2_EXTENSION_NAME) == 0)
                                        robustnessExtension = true;
                                    if (std::strcmp(extension.extensionName, VK_KHR_DYNAMIC_RENDERING_EXTENSION_NAME) ==
                                        0)
                                        dynamicRenderingExtension = true;
                                    if (std::strcmp(extension.extensionName,
                                                    VK_EXT_EXTENDED_DYNAMIC_STATE_EXTENSION_NAME) == 0)
                                        edsExtension = true;
                                    if (std::strcmp(extension.extensionName,
                                                    VK_EXT_EXTENDED_DYNAMIC_STATE_2_EXTENSION_NAME) == 0)
                                        eds2Extension = true;
                                }
                            }
                        }
                        VkPhysicalDeviceRobustness2FeaturesEXT availableRobust{};
                        availableRobust.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ROBUSTNESS_2_FEATURES_EXT;
                        VkPhysicalDeviceVulkan12Features available12{};
                        available12.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES;
                        VkPhysicalDeviceVulkan11Features available11{
                            VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_1_FEATURES};
                        VkPhysicalDeviceFeatures2 available{};
                        available.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
                        available.pNext = &available12;
                        VkPhysicalDeviceDynamicRenderingFeaturesKHR availableRendering{};
                        availableRendering.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DYNAMIC_RENDERING_FEATURES_KHR;
                        if (dynamicRenderingExtension) available12.pNext = &availableRendering;
                        if (robustnessExtension) {
                            availableRobust.pNext = available12.pNext;
                            available12.pNext = &availableRobust;
                        }
                        VkPhysicalDeviceExtendedDynamicStateFeaturesEXT availableEds{
                            VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_EXTENDED_DYNAMIC_STATE_FEATURES_EXT};
                        VkPhysicalDeviceExtendedDynamicState2FeaturesEXT availableEds2{
                            VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_EXTENDED_DYNAMIC_STATE_2_FEATURES_EXT};
                        if (edsExtension) {
                            availableEds.pNext = available12.pNext;
                            available12.pNext = &availableEds;
                        }
                        if (eds2Extension) {
                            availableEds2.pNext = available12.pNext;
                            available12.pNext = &availableEds2;
                        }
                        if (app.apiVersion >= VK_API_VERSION_1_2 &&
                            selectedProperties.apiVersion >= VK_API_VERSION_1_2 && features2) {
                            features2(devices[d], &available);
                            available.pNext = &available11;
                            features2(devices[d], &available);
                        }
                        VkPhysicalDeviceFeatures enabledGraphicsCore{};
                        enabledGraphicsCore.textureCompressionBC = available.features.textureCompressionBC;
                        uint32_t graphicsCoreMask = 0;
                        enabledGraphicsCore.shaderStorageImageExtendedFormats =
                            available.features.shaderStorageImageExtendedFormats;
                        enabledGraphicsCore.depthClamp = available.features.depthClamp;
                        if (enabledGraphicsCore.depthClamp) graphicsCoreMask |= (1u << 0);
                        enabledGraphicsCore.sampleRateShading = available.features.sampleRateShading;
                        if (enabledGraphicsCore.sampleRateShading) graphicsCoreMask |= (1u << 1);
                        enabledGraphicsCore.logicOp = available.features.logicOp;
                        if (enabledGraphicsCore.logicOp) graphicsCoreMask |= (1u << 2);
                        enabledGraphicsCore.dualSrcBlend = available.features.dualSrcBlend;
                        if (enabledGraphicsCore.dualSrcBlend) graphicsCoreMask |= (1u << 3);
                        enabledGraphicsCore.independentBlend = available.features.independentBlend;
                        if (enabledGraphicsCore.independentBlend) graphicsCoreMask |= (1u << 4);
                        enabledGraphicsCore.fillModeNonSolid = available.features.fillModeNonSolid;
                        if (enabledGraphicsCore.fillModeNonSolid) graphicsCoreMask |= (1u << 5);
                        enabledGraphicsCore.depthBounds = available.features.depthBounds;
                        if (enabledGraphicsCore.depthBounds) graphicsCoreMask |= (1u << 6);
                        enabledGraphicsCore.shaderClipDistance = available.features.shaderClipDistance;
                        if (enabledGraphicsCore.shaderClipDistance) graphicsCoreMask |= (1u << 7);
                        enabledGraphicsCore.shaderCullDistance = available.features.shaderCullDistance;
                        if (enabledGraphicsCore.shaderCullDistance) graphicsCoreMask |= (1u << 8);
                        enabledGraphicsCore.geometryShader = available.features.geometryShader;
                        if (enabledGraphicsCore.geometryShader) graphicsCoreMask |= (1u << 9);
                        enabledGraphicsCore.tessellationShader = available.features.tessellationShader;
                        if (enabledGraphicsCore.tessellationShader) graphicsCoreMask |= (1u << 10);
                        enabledGraphicsCore.vertexPipelineStoresAndAtomics =
                            available.features.vertexPipelineStoresAndAtomics;
                        if (enabledGraphicsCore.vertexPipelineStoresAndAtomics) graphicsCoreMask |= (1u << 11);
                        enabledGraphicsCore.fragmentStoresAndAtomics = available.features.fragmentStoresAndAtomics;
                        if (enabledGraphicsCore.fragmentStoresAndAtomics) graphicsCoreMask |= (1u << 12);
                        di.pEnabledFeatures = &enabledGraphicsCore;
                        VkPhysicalDeviceVulkan12Features enabled12{};
                        enabled12.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES;
                        enabled12.scalarBlockLayout = available12.scalarBlockLayout;
                        VkPhysicalDeviceRobustness2FeaturesEXT enabledRobust{};
                        enabledRobust.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ROBUSTNESS_2_FEATURES_EXT;
                        enabledRobust.nullDescriptor = availableRobust.nullDescriptor;
                        if (enabled12.scalarBlockLayout) di.pNext = &enabled12;
                        if (enabledRobust.nullDescriptor) {
                            deviceExtensions[di.enabledExtensionCount++] = VK_EXT_ROBUSTNESS_2_EXTENSION_NAME;
                            if (di.pNext)
                                enabled12.pNext = &enabledRobust;
                            else
                                di.pNext = &enabledRobust;
                        }
                        VkPhysicalDeviceDynamicRenderingFeaturesKHR enabledRendering{};
                        enabledRendering.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DYNAMIC_RENDERING_FEATURES_KHR;
                        enabledRendering.dynamicRendering = availableRendering.dynamicRendering;
                        if (enabledRendering.dynamicRendering) {
                            deviceExtensions[di.enabledExtensionCount++] = VK_KHR_DYNAMIC_RENDERING_EXTENSION_NAME;
                            enabledRendering.pNext = const_cast<void*>(di.pNext);
                            di.pNext = &enabledRendering;
                        }
                        VkPhysicalDeviceExtendedDynamicStateFeaturesEXT enabledEds{
                            VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_EXTENDED_DYNAMIC_STATE_FEATURES_EXT};
                        VkPhysicalDeviceExtendedDynamicState2FeaturesEXT enabledEds2{
                            VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_EXTENDED_DYNAMIC_STATE_2_FEATURES_EXT};
                        enabledEds.extendedDynamicState = availableEds.extendedDynamicState;
                        enabledEds2.extendedDynamicState2 = availableEds2.extendedDynamicState2;
                        if (enabledEds.extendedDynamicState) {
                            deviceExtensions[di.enabledExtensionCount++] = VK_EXT_EXTENDED_DYNAMIC_STATE_EXTENSION_NAME;
                            enabledEds.pNext = const_cast<void*>(di.pNext);
                            di.pNext = &enabledEds;
                        }
                        if (enabledEds2.extendedDynamicState2) {
                            deviceExtensions[di.enabledExtensionCount++] =
                                VK_EXT_EXTENDED_DYNAMIC_STATE_2_EXTENSION_NAME;
                            enabledEds2.pNext = const_cast<void*>(di.pNext);
                            di.pNext = &enabledEds2;
                        }
                        std::printf(
                            "null descriptor authority extension=%u queried=%u enabled=%u; no descriptor write or draw\n",
                            robustnessExtension ? 1u : 0u, availableRobust.nullDescriptor,
                            enabledRobust.nullDescriptor);
                        // Whole public tiling.comp uniform has scalar array stride16 and
                        // first mip offset12. Query/enable genuine support, never assume it.
                        VkPhysicalDeviceVulkan11Features enabled11{
                            VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_1_FEATURES};
                        enabled11.storageBuffer16BitAccess = available11.storageBuffer16BitAccess;
                        enabled11.pNext = const_cast<void*>(di.pNext);
                        di.pNext = &enabled11;
                        if (mixedAmd)
                            deviceExtensions[di.enabledExtensionCount++] =
                                VK_AMD_MIXED_ATTACHMENT_SAMPLES_EXTENSION_NAME;
                        VkDevice device{};
                        result = deviceCreate(devices[d], &di, nullptr, &device);
                        if (result != VK_SUCCESS) continue;
                        std::printf(
                            "tiling prerequisite loader_api=%u requested_api=%u device_api=%u scalarBlockLayout_enabled=%u compute_queue=%u; fixture_dispatch_enabled_only_by_control_env=true\n",
                            loaderVersion, app.apiVersion, selectedProperties.apiVersion, enabled12.scalarBlockLayout,
                            (families[q].queueFlags & VK_QUEUE_COMPUTE_BIT) != 0);
                        const auto dd = reinterpret_cast<PFN_vkDestroyDevice>(deviceGet(device, "vkDestroyDevice"));
                        if (!dd) {
                            status = 6;
                            break;
                        }
                        const auto swapCreate =
                            reinterpret_cast<PFN_vkCreateSwapchainKHR>(deviceGet(device, "vkCreateSwapchainKHR"));
                        const auto swapDestroy =
                            reinterpret_cast<PFN_vkDestroySwapchainKHR>(deviceGet(device, "vkDestroySwapchainKHR"));
                        const auto swapImages =
                            reinterpret_cast<PFN_vkGetSwapchainImagesKHR>(deviceGet(device, "vkGetSwapchainImagesKHR"));
                        VkSurfaceCapabilitiesKHR caps{};
                        uint32_t nf{};
                        if (!swapCreate || !swapDestroy || !swapImages ||
                            surfaceCaps(devices[d], surface, &caps) != VK_SUCCESS ||
                            surfaceFormats(devices[d], surface, &nf, nullptr) != VK_SUCCESS || !nf ||
                            !(caps.supportedUsageFlags & VK_IMAGE_USAGE_TRANSFER_DST_BIT)) {
                            dd(device, nullptr);
                            status = 13;
                            break;
                        }
                        std::vector<VkSurfaceFormatKHR> formats(nf);
                        if (surfaceFormats(devices[d], surface, &nf, formats.data()) != VK_SUCCESS) {
                            dd(device, nullptr);
                            status = 14;
                            break;
                        }
                        VkSwapchainCreateInfoKHR si{};
                        si.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
                        si.surface = surface;
                        si.minImageCount = caps.minImageCount;
                        si.imageFormat =
                            formats[0].format == VK_FORMAT_UNDEFINED ? VK_FORMAT_B8G8R8A8_UNORM : formats[0].format;
                        si.imageColorSpace = formats[0].colorSpace;
                        si.imageExtent = caps.currentExtent;
                        if (si.imageExtent.width == UINT32_MAX) {
                            si.imageExtent.width = 640;
                            si.imageExtent.height = 480;
                            if (si.imageExtent.width < caps.minImageExtent.width)
                                si.imageExtent.width = caps.minImageExtent.width;
                            if (si.imageExtent.width > caps.maxImageExtent.width)
                                si.imageExtent.width = caps.maxImageExtent.width;
                            if (si.imageExtent.height < caps.minImageExtent.height)
                                si.imageExtent.height = caps.minImageExtent.height;
                            if (si.imageExtent.height > caps.maxImageExtent.height)
                                si.imageExtent.height = caps.maxImageExtent.height;
                        }
                        si.imageArrayLayers = 1;
                        si.imageUsage = VK_IMAGE_USAGE_TRANSFER_DST_BIT;
                        si.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
                        si.preTransform = caps.currentTransform;
                        for (uint32_t bit = 1; bit <= VK_COMPOSITE_ALPHA_INHERIT_BIT_KHR; bit <<= 1)
                            if (caps.supportedCompositeAlpha & bit) {
                                si.compositeAlpha = static_cast<VkCompositeAlphaFlagBitsKHR>(bit);
                                break;
                            }
                        si.presentMode = VK_PRESENT_MODE_FIFO_KHR;
                        si.clipped = VK_TRUE;
                        VkSwapchainKHR swap{};
                        result = swapCreate(device, &si, nullptr, &swap);
                        if (result != VK_SUCCESS) {
                            std::fprintf(stderr, "swapchain=%d\n", result);
                            dd(device, nullptr);
                            status = 15;
                            break;
                        }
                        uint32_t images{};
                        result = swapImages(device, swap, &images, nullptr);
                        if (result != VK_SUCCESS || !images) {
                            swapDestroy(device, swap, nullptr);
                            dd(device, nullptr);
                            status = 16;
                            break;
                        }
                        std::printf(
                            "host_control window/surface/swapchain passed width=%u height=%u images=%u; acquired=false presented=false\n",
                            si.imageExtent.width, si.imageExtent.height, images);
                        VkPhysicalDeviceMemoryProperties memory{};
                        memoryProperties(devices[d], &memory);
                        ctrport::DisplayEventHost displayKernel;
                        char tilingDirectory[32768]{};
                        const DWORD tilingLength = GetEnvironmentVariableA("CTR_VIDEO_TILING_SHADER_DIR",
                                                                           tilingDirectory, sizeof(tilingDirectory));
                        if (tilingLength) {
                            if (ctr_tiling_pipeline_control(VK_NULL_HANDLE, nullptr, "fixture", false, true, q,
                                                            memory) != 88 ||
                                ctr_tiling_pipeline_control(VK_NULL_HANDLE, nullptr, "fixture", true, false, q,
                                                            memory) != 88 ||
                                ctr_tiling_pipeline_control(VK_NULL_HANDLE, nullptr, nullptr, true, true, q, memory) !=
                                    88) {
                                swapDestroy(device, swap, nullptr);
                                dd(device, nullptr);
                                status = 150;
                                break;
                            }
                            std::puts("tiling missing scalar/compute/fixture admission rejected88 before device calls");
                            const int tilingStatus =
                                tilingLength >= sizeof(tilingDirectory)
                                    ? 60
                                    : ctr_tiling_pipeline_control(
                                          device, deviceGet, tilingDirectory, enabled12.scalarBlockLayout != 0,
                                          (families[q].queueFlags & VK_QUEUE_COMPUTE_BIT) != 0, q, memory);
                            if (tilingStatus) {
                                swapDestroy(device, swap, nullptr);
                                dd(device, nullptr);
                                status = tilingStatus;
                                break;
                            }
                        }
                        if (tilingLength && tilingLength < sizeof(tilingDirectory)) {
                            const int thinStatus = ctr_tiling_pipeline_control(
                                device, deviceGet, tilingDirectory, enabled12.scalarBlockLayout != 0,
                                (families[q].queueFlags & VK_QUEUE_COMPUTE_BIT) != 0, q, memory, true);
                            if (thinStatus) {
                                swapDestroy(device, swap, nullptr);
                                dd(device, nullptr);
                                status = thinStatus;
                                break;
                            }
                        }
                        if (tilingLength && tilingLength < sizeof(tilingDirectory)) {
                            const int depth0Status = ctr_with_tiling_pipelines(
                                device, deviceGet, tilingDirectory, enabled12.scalarBlockLayout != 0,
                                (families[q].queueFlags & VK_QUEUE_COMPUTE_BIT) != 0, false,
                                [&](VkPipelineLayout layout, const VkPipeline* pipelines,
                                    VkDescriptorSetLayout descriptors) -> int {
                                    std::vector<unsigned char> tiled;
                                    const int rc = ctr_tiling_roundtrip(device, deviceGet, q, memory, layout, pipelines,
                                                                        descriptors, false, 2048, 1152, &tiled);
                                    if (!rc)
                                        std::puts(
                                            "Depth2DThin64 tile0 actual GPU2048x1152 roundtrip9437184bytes0diff guards768 fenced; authored only");
                                    return rc;
                                },
                                4, true);
                            if (depth0Status) {
                                swapDestroy(device, swap, nullptr);
                                dd(device, nullptr);
                                status = depth0Status;
                                break;
                            }
                        }
                        if (ctr_pm4_queue_barrier(device, deviceGet, q)) {
                            swapDestroy(device, swap, nullptr);
                            dd(device, nullptr);
                            status = 143;
                            break;
                        }
                        std::puts(
                            "PM4 real owner queue memory barrier and fence completed; no guest alias coherence or EOP write claimed");
                        PresentationSession session(device, deviceGet, swap, dd, swapDestroy, q, images, si.imageExtent,
                                                    si.imageFormat, si.imageColorSpace, memory, window,
                                                    &closeRequested);
                        if (!CtrTransferMemoryTypeControl()) {
                            status = 144;
                            break;
                        }
                        std::puts(
                            "transfer memory selector cached preference/fallback/compatibility/no-match controls passed");
                        CtrGraphicsProfileV1 createdProfile{};
                        const auto queryProfile = reinterpret_cast<PFN_vkGetPhysicalDeviceProperties2>(
                            get(instance, "vkGetPhysicalDeviceProperties2"));
                        const int profileStatus = CaptureCreatedDeviceShaderProfile(
                            devices[d], queryProfile, app.apiVersion, selectedProperties.apiVersion, createdProfile);
                        CtrGraphicsProfileV1 missingProfile{};
                        missingProfile.version = 99;
                        if (CaptureCreatedDeviceShaderProfile(devices[d], nullptr, app.apiVersion,
                                                              selectedProperties.apiVersion, missingProfile) != 88 ||
                            missingProfile.version != 99 ||
                            CaptureCreatedDeviceShaderProfile(devices[d], queryProfile, VK_API_VERSION_1_0,
                                                              selectedProperties.apiVersion, missingProfile) != 88 ||
                            missingProfile.version != 99) {
                            status = 144;
                            break;
                        }
                        {
                            CtrTransferPhaseTiming identity("selected_device", 0);
                            if (identity.enabled)
                                std::fprintf(stderr,
                                             "CTR_TRANSFER_DEVICE name=%s vendor=%u device=%u type=%u queue=%u\n",
                                             selectedProperties.deviceName, selectedProperties.vendorID,
                                             selectedProperties.deviceID,
                                             static_cast<unsigned>(selectedProperties.deviceType), q);
                        }
                        session.SetGraphicsProfile(createdProfile, profileStatus);
                        session.SetSamplerLimits(selectedProperties.limits);
                        session.SetNullDescriptorAuthority(enabledRobust.nullDescriptor == VK_TRUE);
                        const bool renderingCommands =
                            deviceGet(device, "vkCmdBeginRenderingKHR") && deviceGet(device, "vkCmdEndRenderingKHR");
                        session.SetDynamicRenderingAuthority(enabledRendering.dynamicRendering == VK_TRUE &&
                                                             renderingCommands);
                        std::printf(
                            "dynamic rendering authority extension=%u queried=%u enabled=%u commands=%u status=%d; no graphics pipeline or draw\n",
                            dynamicRenderingExtension ? 1u : 0u, availableRendering.dynamicRendering,
                            enabledRendering.dynamicRendering, renderingCommands ? 1u : 0u,
                            session.DynamicRenderingAuthority());
                        if (enabledRendering.dynamicRendering &&
                            ctr_video_dynamic_rendering_authority_v1(&session) != 0) {
                            status = 167;
                            break;
                        }
                        int renderingWrongOwner = 0;
                        std::thread renderingOwner(
                            [&] { renderingWrongOwner = ctr_video_dynamic_rendering_authority_v1(&session); });
                        renderingOwner.join();
                        if (renderingWrongOwner != 116 || ctr_video_dynamic_rendering_authority_v1(nullptr) != 60) {
                            status = 167;
                            break;
                        }
                        const char* edsCommands[] = {"vkCmdSetCullModeEXT",          "vkCmdSetFrontFaceEXT",
                                                     "vkCmdSetPrimitiveTopologyEXT", "vkCmdSetViewportWithCountEXT",
                                                     "vkCmdSetScissorWithCountEXT",  "vkCmdBindVertexBuffers2EXT",
                                                     "vkCmdSetDepthTestEnableEXT",   "vkCmdSetDepthWriteEnableEXT",
                                                     "vkCmdSetDepthCompareOpEXT",    "vkCmdSetDepthBoundsTestEnableEXT",
                                                     "vkCmdSetStencilTestEnableEXT", "vkCmdSetStencilOpEXT"};
                        const char* eds2Commands[] = {"vkCmdSetRasterizerDiscardEnableEXT",
                                                      "vkCmdSetDepthBiasEnableEXT",
                                                      "vkCmdSetPrimitiveRestartEnableEXT"};
                        bool edsCommandsAvailable = true, eds2CommandsAvailable = true;
                        for (const char* command : edsCommands)
                            edsCommandsAvailable &= deviceGet(device, command) != nullptr;
                        for (const char* command : eds2Commands)
                            eds2CommandsAvailable &= deviceGet(device, command) != nullptr;
                        const uint32_t dynamicMask =
                            (enabledEds.extendedDynamicState && edsCommandsAvailable ? 1u : 0u) |
                            (enabledEds2.extendedDynamicState2 && eds2CommandsAvailable ? 2u : 0u);
                        session.SetMixedDepthAuthority((mixedQueried ? 1u : 0u) | (mixedAmd ? 6u : 0u));
                        uint32_t mixedSnapshot = 99;
                        if (ctr_video_mixed_depth_authority_v1(&session, &mixedSnapshot) ||
                            mixedSnapshot != ((mixedQueried ? 1u : 0u) | (mixedAmd ? 6u : 0u)) ||
                            ctr_video_mixed_depth_authority_v1(nullptr, &mixedSnapshot) != 60 ||
                            ctr_video_mixed_depth_authority_v1(&session, nullptr) != 60) {
                            status = 207;
                            break;
                        }
                        int mixedWrong = 0;
                        std::thread mixedThread(
                            [&] { mixedWrong = ctr_video_mixed_depth_authority_v1(&session, &mixedSnapshot); });
                        mixedThread.join();
                        if (mixedWrong != 116) {
                            status = 207;
                            break;
                        }
                        std::printf(
                            "mixed depth AMD authority queried=%u supported=%u enabled=%u snapshot=%u; no combined draw\n",
                            mixedQueried ? 1u : 0u, mixedAmd ? 1u : 0u, mixedAmd ? 1u : 0u, mixedSnapshot);
                        session.SetGraphicsCoreAuthority(graphicsCoreMask);
                        {
                            unsigned topologies = 0, logicops = 0;
                            for (uint32_t value = 0; value < 32; ++value) {
                                VkPrimitiveTopology topology = VK_PRIMITIVE_TOPOLOGY_MAX_ENUM;
                                const int rc = CtrPm4::GraphicsTopology(value, topology);
                                const bool known = (value >= 1 && value <= 6) || value == 9 ||
                                                   (value >= 10 && value <= 13) || value == 17 || value == 19 ||
                                                   value == 21;
                                if (known ? (rc != 0 || topology == VK_PRIMITIVE_TOPOLOGY_MAX_ENUM)
                                          : (rc != 88 || topology != VK_PRIMITIVE_TOPOLOGY_MAX_ENUM)) {
                                    status = 180;
                                    break;
                                }
                                if (!rc) ++topologies;
                                if (value == 4 && topology != VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST) {
                                    status = 180;
                                    break;
                                }
                                if ((value == 17 || value == 19) && topology != VK_PRIMITIVE_TOPOLOGY_PATCH_LIST) {
                                    status = 180;
                                    break;
                                }
                            }
                            if (status == 180) break;
                            for (uint32_t value = 0; value < 256; ++value) {
                                VkLogicOp op = VK_LOGIC_OP_MAX_ENUM;
                                const int rc = CtrPm4::GraphicsLogicOp(value, op);
                                const bool known = (value & 15) == (value >> 4);
                                if (known ? (rc != 0 || op == VK_LOGIC_OP_MAX_ENUM)
                                          : (rc != 88 || op != VK_LOGIC_OP_MAX_ENUM)) {
                                    status = 180;
                                    break;
                                }
                                if (!rc) ++logicops;
                            }
                            if (status == 180) break;
                            VkPolygonMode polygon = VK_POLYGON_MODE_MAX_ENUM;
                            for (uint32_t value = 0; value < 3; ++value) {
                                if (CtrPm4::GraphicsPolygonMode(value, polygon)) {
                                    status = 180;
                                    break;
                                }
                            }
                            if (status == 180) break;
                            if (polygon != VK_POLYGON_MODE_FILL || CtrPm4::GraphicsPolygonMode(3, polygon) != 88 ||
                                polygon != VK_POLYGON_MODE_FILL) {
                                status = 180;
                                break;
                            }
                            std::printf(
                                "graphics_fixed_state=topologies%u_logicops%u_polygon3_unknownatomic88_noauxshaderadmission\n",
                                topologies, logicops);
                        }
                        {
                            CtrGraphicsKeyColorV1 color{};
                            color.blend_enable = 1;
                            color.write_mask = 15;
                            color.color_src_factor = 4;
                            color.color_dst_factor = 5;
                            VkPipelineColorBlendAttachmentState blend{};
                            if (CtrPm4::GraphicsAttachmentBlend(color, 15, blend) ||
                                blend.srcColorBlendFactor != VK_BLEND_FACTOR_SRC_ALPHA ||
                                blend.dstColorBlendFactor != VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA ||
                                blend.colorWriteMask != 15 || !blend.blendEnable) {
                                status = 179;
                                break;
                            }
                            if (CtrPm4::GraphicsAttachmentBlend(color, 7, blend) ||
                                blend.srcColorBlendFactor != VK_BLEND_FACTOR_ONE ||
                                blend.dstColorBlendFactor != VK_BLEND_FACTOR_ZERO ||
                                blend.srcAlphaBlendFactor != VK_BLEND_FACTOR_SRC_ALPHA) {
                                status = 179;
                                break;
                            }
                            color.separate_alpha_blend = 1;
                            color.alpha_src_factor = 1;
                            color.alpha_dst_factor = 0;
                            color.alpha_func = 4;
                            if (CtrPm4::GraphicsAttachmentBlend(color, 15, blend) ||
                                blend.alphaBlendOp != VK_BLEND_OP_REVERSE_SUBTRACT ||
                                blend.srcAlphaBlendFactor != VK_BLEND_FACTOR_ONE ||
                                blend.dstAlphaBlendFactor != VK_BLEND_FACTOR_ZERO) {
                                status = 179;
                                break;
                            }
                            auto previous = blend;
                            color.color_src_factor = 11;
                            if (CtrPm4::GraphicsAttachmentBlend(color, 15, blend) != 88 ||
                                std::memcmp(&previous, &blend, sizeof(blend))) {
                                status = 179;
                                break;
                            }
                            color.color_src_factor = 4;
                            color.color_func = 2;
                            if (CtrPm4::GraphicsAttachmentBlend(color, 15, blend) != 88 ||
                                std::memcmp(&previous, &blend, sizeof(blend))) {
                                status = 179;
                                break;
                            }
                            unsigned mapped = 0;
                            for (uint32_t factor = 0; factor < 32; ++factor) {
                                VkBlendFactor f = VK_BLEND_FACTOR_MAX_ENUM;
                                const int rc = CtrPm4::GraphicsBlendFactor(factor, f);
                                if ((factor < 21 && factor != 11 && factor != 12)
                                        ? (rc != 0 || f == VK_BLEND_FACTOR_MAX_ENUM)
                                        : (rc != 88 || f != VK_BLEND_FACTOR_MAX_ENUM)) {
                                    status = 179;
                                    break;
                                }
                                if (!rc) ++mapped;
                            }
                            if (status == 179) break;
                            std::printf(
                                "graphics_attachment_blend=19factors_mapped%u_alpha_exportmask_separatealpha_scaledminmax88_atomic passed\n",
                                mapped);
                        }
                        int coreWrong = 0;
                        std::thread coreOwner([&] { coreWrong = ctr_video_graphics_core_authority_v1(&session, 1); });
                        coreOwner.join();
                        if (coreWrong != 116 || ctr_video_graphics_core_authority_v1(nullptr, 1) != 60 ||
                            ctr_video_graphics_core_authority_v1(&session, 0) != 60 ||
                            ctr_video_graphics_core_authority_v1(&session, 8192) != 60) {
                            status = 173;
                            break;
                        }
                        std::printf(
                            "graphics core authority queried_supported_enabled_mask=%u owner116 null60 zero60 invalidbit60; no pipeline/draw\n",
                            graphicsCoreMask);
                        session.SetGraphicsDynamicAuthority(dynamicMask);
                        int dynamicWrong = 0;
                        std::thread dynamicOwner(
                            [&] { dynamicWrong = ctr_video_graphics_dynamic_authority_v1(&session, 3); });
                        dynamicOwner.join();
                        if (dynamicWrong != 116 || ctr_video_graphics_dynamic_authority_v1(nullptr, 3) != 60 ||
                            ctr_video_graphics_dynamic_authority_v1(&session, 4) != 60) {
                            status = 172;
                            break;
                        }
                        std::printf(
                            "graphics dynamic authority eds_query=%u eds_enable=%u commands12=%u eds2_query=%u eds2_enable=%u commands3=%u mask=%u status=%d owner116 null60 invalidmask60; no pipeline/draw\n",
                            availableEds.extendedDynamicState, enabledEds.extendedDynamicState,
                            edsCommandsAvailable ? 1u : 0u, availableEds2.extendedDynamicState2,
                            enabledEds2.extendedDynamicState2, eds2CommandsAvailable ? 1u : 0u, dynamicMask,
                            ctr_video_graphics_dynamic_authority_v1(&session, 3));
                        session.SetTilingAuthority(
                            tilingLength && tilingLength < sizeof(tilingDirectory) ? tilingDirectory : nullptr,
                            enabled12.scalarBlockLayout != 0, (families[q].queueFlags & VK_QUEUE_COMPUTE_BIT) != 0,
                            enabled11.storageBuffer16BitAccess != 0 &&
                                enabledGraphicsCore.shaderStorageImageExtendedFormats != 0);
                        session.SetImageAuthority(devices[d],
                                                  reinterpret_cast<PFN_vkGetPhysicalDeviceFormatProperties>(
                                                      get(instance, "vkGetPhysicalDeviceFormatProperties")),
                                                  reinterpret_cast<PFN_vkGetPhysicalDeviceImageFormatProperties>(
                                                      get(instance, "vkGetPhysicalDeviceImageFormatProperties")));
                        CtrGraphicsVertexInputsV1 vertexInputs{};
                        vertexInputs.version = 1;
                        vertexInputs.size = sizeof(vertexInputs);
                        vertexInputs.count = 3;
                        const uint32_t vertexFormats[3] = {37, 109, 103};
                        for (uint32_t vi = 0; vi < 3; ++vi) {
                            vertexInputs.inputs[vi].location = vi;
                            vertexInputs.inputs[vi].binding = vi;
                            vertexInputs.inputs[vi].stride = 16;
                            vertexInputs.inputs[vi].divisor = 1;
                        }
                        int vertexPreflight =
                            ctr_video_graphics_vertex_inputs_preflight_v1(&session, &vertexInputs, vertexFormats, 3);
                        int vertexWrong = 0;
                        std::thread vertexOwner([&] {
                            vertexWrong = ctr_video_graphics_vertex_inputs_preflight_v1(&session, &vertexInputs,
                                                                                        vertexFormats, 3);
                        });
                        vertexOwner.join();
                        if (vertexPreflight || vertexWrong != 116 ||
                            ctr_video_graphics_vertex_inputs_preflight_v1(nullptr, &vertexInputs, vertexFormats, 3) !=
                                60) {
                            status = 178;
                            break;
                        }
                        vertexInputs.inputs[0].input_rate = 1;
                        vertexInputs.inputs[0].divisor = 7;
                        if (ctr_video_graphics_vertex_inputs_preflight_v1(&session, &vertexInputs, vertexFormats, 3) !=
                            88) {
                            status = 178;
                            break;
                        }
                        vertexInputs.inputs[0].input_rate = 0;
                        vertexInputs.inputs[0].stride = UINT32_MAX;
                        if (ctr_video_graphics_vertex_inputs_preflight_v1(&session, &vertexInputs, vertexFormats, 3) !=
                            88) {
                            status = 178;
                            break;
                        }
                        vertexInputs.inputs[0].stride = 16;
                        vertexInputs.inputs[1].location = 0;
                        if (ctr_video_graphics_vertex_inputs_preflight_v1(&session, &vertexInputs, vertexFormats, 3) !=
                            60) {
                            status = 178;
                            break;
                        }
                        std::printf(
                            "vertex_input_selected_preflight=3formats_status0_owner116_divisor88_stride88_duplicate60\n");

                        {
                            CtrGraphicsImageParametersV1 p{};
                            p.version = 1;
                            p.size = sizeof(p);
                            p.type = 9;
                            p.width = 1920;
                            p.height = 1080;
                            p.depth = 1;
                            p.layers = 1;
                            p.levels = 1;
                            p.samples = 1;
                            p.data_format = 10;
                            uint64_t token = 999;
                            if (ctr_video_sampled_image_create_v1(&session, &p, &token) || !token || token == 999 ||
                                ([&] {
                                    constexpr size_t row = 1920 * 4, pitch = row + 16;
                                    std::vector<unsigned char> pixels(pitch * 1080, 0xab),
                                        output(pitch * 1080 + 256, 0xd3);
                                    for (size_t y = 0; y < 1080; ++y)
                                        for (size_t x = 0; x < row; ++x)
                                            pixels[y * pitch + x] =
                                                static_cast<unsigned char>((y * 73 + x * 19) ^ (x >> 8));
                                    if (ctr_video_sampled_image_readback_linear_v1(&session, token, output.data(),
                                                                                   output.size(), pitch) != 91)
                                        return true;
                                    if (ctr_video_sampled_image_upload_linear_v1(&session, token, pixels.data(),
                                                                                 pitch * 1079 + row - 1, pitch) != 60)
                                        return true;
                                    for (int pass = 0; pass < 2; ++pass) {
                                        pixels[0] ^= 91;
                                        if (ctr_video_sampled_image_upload_linear_v1(&session, token, pixels.data(),
                                                                                     pixels.size(), pitch))
                                            return true;
                                        const auto before = output;
                                        if (ctr_video_sampled_image_readback_linear_v1(
                                                &session, token, output.data(), pitch * 1079 + row - 1, pitch) != 60 ||
                                            output != before)
                                            return true;
                                        if (ctr_video_sampled_image_readback_linear_v1(&session, token, output.data(),
                                                                                       output.size(), pitch))
                                            return true;
                                        for (size_t y = 0; y < 1080; ++y) {
                                            if (std::memcmp(pixels.data() + y * pitch, output.data() + y * pitch, row))
                                                return true;
                                            for (size_t x = row; x < pitch; ++x)
                                                if (output[y * pitch + x] != 0xd3) return true;
                                        }
                                        for (size_t x = pitch * 1080; x < output.size(); ++x)
                                            if (output[x] != 0xd3) return true;
                                    }
                                    return false;
                                })() ||
                                ctr_video_sampled_image_release_v1(&session, token) ||
                                ctr_video_sampled_image_release_v1(&session, token) != 62) {
                                status = 156;
                                break;
                            }
                            token = 999;
                            int wrong = 0;
                            std::thread worker(
                                [&] { wrong = ctr_video_sampled_image_create_v1(&session, &p, &token); });
                            worker.join();
                            p.samples = 2;
                            if (wrong != 116 || ctr_video_sampled_image_create_v1(&session, &p, &token) != 88 ||
                                token != 999 || ctr_video_sampled_image_create_v1(nullptr, &p, &token) != 60) {
                                status = 156;
                                break;
                            }
                            p.samples = 1;
                            p.swizzle[0] = 2;
                            if (ctr_video_sampled_image_create_v1(&session, &p, &token) != 88 || token != 999) {
                                status = 156;
                                break;
                            }
                            p.swizzle[0] = 0;
                            p.width = 0;
                            if (ctr_video_sampled_image_create_v1(&session, &p, &token) != 60 || token != 999) {
                                status = 156;
                                break;
                            }
                            std::puts(
                                "session sampled image actual Vulkan allocation/bind/two linear uploads/readbacks/fence/release stale62 wrongthread116 MSAA88 zero60 partial60 undefined91 rowpadding16 guard256 exact8294400B actual_view reserved_swizzle88; no tiled upload/binding/gameframe");
                        }
                        if (tilingLength && tilingLength < sizeof(tilingDirectory)) {
                            const uint32_t shapes[][4] = {
                                {1920, 1080, 1920, 1088}, {192, 108, 256, 128}, {64, 1, 128, 64}};
                            for (const auto& shape : shapes) {
                                CtrGraphicsImageParametersV1 p{};
                                p.version = 1;
                                p.size = sizeof(p);
                                p.type = 9;
                                p.width = shape[0];
                                p.height = shape[1];
                                p.pitch = shape[2];
                                p.depth = 1;
                                p.layers = 1;
                                p.levels = 1;
                                p.samples = 1;
                                p.data_format = 10;
                                p.tile_mode = 14;
                                p.guest_address = 1;
                                CtrGraphicsImageExtentV1 extent{
                                    1, sizeof(extent), shape[2], shape[3], 1, uint64_t(shape[2]) * shape[3] * 4};
                                std::vector<unsigned char> tiled;
                                const int authored = ctr_with_tiling_pipelines(
                                    device, deviceGet, tilingDirectory, true, true, true,
                                    [&](VkPipelineLayout layout, const VkPipeline* pipelines,
                                        VkDescriptorSetLayout descriptors) -> int {
                                        return ctr_tiling_roundtrip(device, deviceGet, q, memory, layout, pipelines,
                                                                    descriptors, true, extent.pitch,
                                                                    extent.padded_height, &tiled);
                                    });
                                if (authored) {
                                    status = 157;
                                    break;
                                }
                                CtrGraphicsColorRequirementV1 color{
                                    1, sizeof(color), 0, extent.pitch, extent.padded_height, 1, 1, 14, 10, 0,
                                    1, extent.bytes};
                                uint64_t colorToken = 0;
                                if (ctr_video_color_attachment_create_v1(&session, &color, 0, &colorToken)) {
                                    status = 169;
                                    break;
                                }
                                const uint64_t colorPitch = uint64_t(color.pitch) * 4 + 16;
                                std::vector<unsigned char> colorOutput(size_t(colorPitch * color.height) + 256, 0xCD);
                                CtrGraphicsImageParametersV1 alias = p;
                                alias.number_format = 0;
                                uint64_t aliasToken = 0;
                                if (ctr_video_sampled_image_create_v1(&session, &alias, &aliasToken)) {
                                    status = 171;
                                    break;
                                }
                                if (ctr_video_color_to_sampled_v1(&session, colorToken, aliasToken) != 91) {
                                    status = 171;
                                    break;
                                }
                                auto oversizedAlias = alias;
                                oversizedAlias.height = color.height + 1;
                                uint64_t oversizedToken = 0;
                                if (ctr_video_sampled_image_create_v1(&session, &oversizedAlias, &oversizedToken) ||
                                    ctr_video_color_to_sampled_v1(&session, colorToken, oversizedToken) != 88 ||
                                    ctr_video_sampled_image_release_v1(&session, oversizedToken)) {
                                    status = 171;
                                    break;
                                }

                                auto invalidColorExtent = extent;
                                invalidColorExtent.guest_address = 2;
                                if (ctr_video_color_attachment_upload_tiled_v1(
                                        &session, colorToken, tiled.data(), extent.bytes, &invalidColorExtent) != 88 ||
                                    ctr_video_color_attachment_upload_tiled_v1(&session, colorToken, tiled.data(),
                                                                               extent.bytes - 1, &extent) != 60 ||
                                    ctr_video_color_attachment_upload_tiled_v1(&session, colorToken, tiled.data(),
                                                                               extent.bytes, &extent) ||
                                    ctr_video_color_attachment_readback_linear_v1(
                                        &session, colorToken, colorOutput.data(), colorOutput.size(), colorPitch)) {
                                    status = 169;
                                    break;
                                }
                                const uint64_t aliasPitch = uint64_t(alias.width) * 4 + 16;
                                std::vector<unsigned char> aliasOutput(size_t(aliasPitch * alias.height), 0xAB);
                                int aliasWrong = 0;
                                std::thread aliasWorker([&] {
                                    aliasWrong = ctr_video_color_to_sampled_v1(&session, colorToken, aliasToken);
                                });
                                aliasWorker.join();
                                if (aliasWrong != 116 ||
                                    ctr_video_color_to_sampled_v1(&session, colorToken, aliasToken) ||
                                    ctr_video_sampled_image_readback_linear_v1(&session, aliasToken, aliasOutput.data(),
                                                                               aliasOutput.size(), aliasPitch)) {
                                    status = 171;
                                    break;
                                }
                                for (uint32_t y = 0; y < alias.height; ++y)
                                    for (uint64_t x = 0; x < aliasPitch; ++x)
                                        if (aliasOutput[size_t(y * aliasPitch + x)] !=
                                            (x < uint64_t(alias.width) * 4 ? colorOutput[size_t(y * colorPitch + x)]
                                                                           : 0xAB))
                                            status = 171;
                                if (ctr_video_sampled_image_release_v1(&session, aliasToken) ||
                                    ctr_video_color_to_sampled_v1(&session, colorToken, aliasToken) != 62)
                                    status = 171;
                                if (status == 171) break;
                                std::puts(
                                    "color to sampled boundedheight fenced conversion pixel0diff padding16 unchanged oversized88 undefined91 owner116 stale62; authored only");
                                for (uint32_t y = 0; y < color.height; ++y)
                                    for (uint64_t x = 0; x < colorPitch; ++x) {
                                        const uint32_t index = static_cast<uint32_t>(y * extent.pitch + x / 4);
                                        const uint32_t pixel = (index * 2654435761U) ^ (y * 0x9e37U) ^ 0xa5c37129U;
                                        const unsigned char expected =
                                            x < uint64_t(color.pitch) * 4
                                                ? static_cast<unsigned char>(pixel >> ((x % 4) * 8))
                                                : 0xCD;
                                        if (colorOutput[size_t(y * colorPitch + x)] != expected) status = 169;
                                    }
                                for (size_t x = size_t(colorPitch * color.height); x < colorOutput.size(); ++x)
                                    if (colorOutput[x] != 0xCD) status = 169;
                                if (ctr_video_color_attachment_release_v1(&session, colorToken)) status = 169;
                                if (status == 169) break;
                                std::printf(
                                    "color tile14 whole public detile upload/readback exact pitch=%u height=%u padding16 guard256 unchanged; address88 short60 authored only\n",
                                    color.pitch, color.height);

                                const size_t pitch = size_t(p.width) * 4 + 16;
                                std::vector<unsigned char> output(pitch * p.height + 256, 0xd3);
                                uint64_t token = 0;
                                if (ctr_video_sampled_image_create_v1(&session, &p, &token)) {
                                    status = 157;
                                    break;
                                }
                                auto invalid = extent;
                                invalid.padded_height += 64;
                                int wrong = 0;
                                std::thread worker([&] {
                                    wrong = ctr_video_sampled_image_upload_tiled_v1(&session, token, tiled.data(),
                                                                                    extent.bytes, &extent);
                                });
                                worker.join();
                                if (wrong != 116 ||
                                    ctr_video_sampled_image_upload_tiled_v1(&session, token, tiled.data(), extent.bytes,
                                                                            &invalid) != 88 ||
                                    ctr_video_sampled_image_upload_tiled_v1(&session, token, tiled.data(),
                                                                            extent.bytes - 1, &extent) != 60 ||
                                    ctr_video_sampled_image_readback_linear_v1(&session, token, output.data(),
                                                                               output.size(), pitch) != 91 ||
                                    ctr_video_sampled_image_upload_tiled_v1(&session, token, tiled.data(), extent.bytes,
                                                                            &extent) ||
                                    ctr_video_sampled_image_readback_linear_v1(&session, token, output.data(),
                                                                               output.size(), pitch)) {
                                    status = 157;
                                    break;
                                }
                                for (size_t y = 0; y < p.height; ++y)
                                    for (size_t x = 0; x < pitch; ++x) {
                                        const uint32_t index = static_cast<uint32_t>(y * extent.pitch + x / 4);
                                        const uint32_t pixel =
                                            (index * 2654435761U) ^ (static_cast<uint32_t>(y) * 0x9e37U) ^ 0xa5c37129U;
                                        const unsigned char expected =
                                            x < size_t(p.width) * 4 ? static_cast<unsigned char>(pixel >> ((x % 4) * 8))
                                                                    : 0xd3;
                                        if (output[y * pitch + x] != expected) status = 157;
                                    }
                                for (size_t x = pitch * p.height; x < output.size(); ++x)
                                    if (output[x] != 0xd3) status = 157;
                                if (ctr_video_sampled_image_release_v1(&session, token) ||
                                    ctr_video_sampled_image_upload_tiled_v1(&session, token, tiled.data(), extent.bytes,
                                                                            &extent) != 62)
                                    status = 157;
                                if (status == 157 || status == 169 || status == 171) break;
                            }
                            if (status == 157 || status == 169 || status == 171) break;
                            {
                                CtrGraphicsColorRequirementV1 color{};
                                color.version = 1;
                                color.size = sizeof(color);
                                color.pitch = 1920;
                                color.height = 1080;
                                color.layers = 1;
                                color.samples = 1;
                                color.data_format = 10;
                                color.number_format = 9;
                                uint64_t token = 999;
                                if (ctr_video_color_attachment_create_v1(&session, &color, 1, &token) || token == 999) {
                                    status = 166;
                                    break;
                                }
                                const uint64_t colorPitch = uint64_t(color.pitch) * 4 + 16;
                                std::vector<unsigned char> colorSource(size_t(colorPitch * color.height), 0xA5),
                                    colorOutput(colorSource.size(), 0xCD);
                                for (uint32_t y = 0; y < color.height; ++y)
                                    for (uint32_t x = 0; x < color.pitch * 4; ++x)
                                        colorSource[size_t(y * colorPitch + x)] =
                                            static_cast<unsigned char>((x + 17 * y) & 255);
                                if (ctr_video_color_attachment_readback_linear_v1(
                                        &session, token, colorOutput.data(), colorOutput.size(), colorPitch) != 91 ||
                                    ctr_video_color_attachment_upload_linear_v1(&session, token, colorSource.data(), 1,
                                                                                colorPitch) != 60 ||
                                    ctr_video_color_attachment_upload_linear_v1(&session, token, colorSource.data(),
                                                                                colorSource.size(), colorPitch) ||
                                    ctr_video_color_attachment_readback_linear_v1(&session, token, colorOutput.data(),
                                                                                  colorOutput.size(), colorPitch)) {
                                    status = 168;
                                    break;
                                }
                                bool colorExact = true;
                                for (uint32_t y = 0; y < color.height; ++y)
                                    for (uint64_t x = 0; x < colorPitch; ++x)
                                        if (colorOutput[size_t(y * colorPitch + x)] !=
                                            (x < uint64_t(color.pitch) * 4 ? colorSource[size_t(y * colorPitch + x)]
                                                                           : 0xCD))
                                            colorExact = false;
                                if (!colorExact) {
                                    status = 168;
                                    break;
                                }
                                std::puts(
                                    "color linear upload/readback fenced exact1920x1080=true padding16=unchanged undefined91 short60; authored snapshot only");
                                int wrong = 0;
                                std::thread owner(
                                    [&] { wrong = ctr_video_color_attachment_release_v1(&session, token); });
                                owner.join();
                                if (wrong != 116 || ctr_video_color_attachment_release_v1(&session, token) ||
                                    ctr_video_color_attachment_release_v1(&session, token) != 62) {
                                    status = 166;
                                    break;
                                }
                                token = 999;
                                color.data_format = 0;
                                if (ctr_video_color_attachment_create_v1(&session, &color, 1, &token) != 88 ||
                                    token != 999) {
                                    status = 166;
                                    break;
                                }
                                std::puts(
                                    "color actual selected physical COLOR_ATTACHMENT SRGB1920x1080 image/device-local memory/view release0; unsupported88 atomic owner116 stale62; undefinedcontents no LOAD/draw");
                            }
                            for (uint32_t usage = 1; usage <= 4; ++usage) {
                                std::array<unsigned char, 408> source{}, copy{};
                                for (size_t i = 0; i < source.size(); ++i)
                                    source[i] = static_cast<unsigned char>(i * 37 + usage);
                                const auto expected = source;
                                uint64_t token = 999;
                                if (ctr_video_buffer_create_v1(&session, source.data(), source.size(), usage, &token) ||
                                    token == 999) {
                                    status = 158;
                                    break;
                                }
                                source.fill(0);
                                copy.fill(0xa5);
                                const auto saved = copy;
                                int wrong = 0;
                                std::thread worker([&] {
                                    wrong = ctr_video_buffer_copy_initial_v1(&session, token, copy.data(), copy.size());
                                });
                                worker.join();
                                if (wrong != 116 || copy != saved ||
                                    ctr_video_buffer_copy_initial_v1(&session, token, copy.data(), copy.size() - 1) !=
                                        60 ||
                                    copy != saved ||
                                    ctr_video_buffer_copy_initial_v1(&session, token, copy.data(), copy.size()) ||
                                    copy != expected || ctr_video_buffer_release_v1(&session, token) ||
                                    ctr_video_buffer_release_v1(&session, token) != 62) {
                                    status = 158;
                                    break;
                                }
                                token = 999;
                                if (ctr_video_buffer_create_v1(&session, source.data(), source.size(), 5, &token) !=
                                        88 ||
                                    token != 999 ||
                                    ctr_video_buffer_create_v1(&session, source.data(), source.size() - 1, 3, &token) !=
                                        60 ||
                                    token != 999) {
                                    status = 158;
                                    break;
                                }
                            }
                            if (status == 158) break;
                            std::puts(
                                "session immutable storage/index16/index32 buffers actual VkBuffer coherent allocation initialized bytes exact408; source mutation independent, thread116 partial60 stale62 unsupported88 atomic; no GPUbinding/readback claim synthetic=true");
                            for (const auto shape : {CtrPm4::DepthAttachmentShape{1920, 1080, 2, 3, 1},
                                                     CtrPm4::DepthAttachmentShape{1024, 1024, 1, 1, 0},
                                                     CtrPm4::DepthAttachmentShape{960, 540, 1, 3, 1}}) {
                                VkImageCreateInfo depthInfo{};
                                VkImageAspectFlags aspects = 0;
                                const int preflight = session.PreflightDepth(shape, depthInfo, aspects);
                                if (preflight != 0 && preflight != 88) {
                                    status = 159;
                                    break;
                                }
                                if (preflight == 0 &&
                                    (depthInfo.extent.width != shape.width ||
                                     static_cast<uint32_t>(depthInfo.samples) != shape.samples || !aspects)) {
                                    status = 159;
                                    break;
                                }
                                if (preflight == 0 && shape.samples == 2 &&
                                    !(depthInfo.usage & VK_IMAGE_USAGE_SAMPLED_BIT)) {
                                    status = 159;
                                    break;
                                }
                                std::printf(
                                    "depth actual selected physical preflight width=%u height=%u samples=%u status=%d; allocation=false HTILE=false initialized=false\n",
                                    shape.width, shape.height, shape.samples, preflight);
                                if (preflight == 0) {
                                    CtrDepthAttachmentParametersV1 params{1,
                                                                          sizeof(CtrDepthAttachmentParametersV1),
                                                                          shape.width,
                                                                          shape.height,
                                                                          shape.samples,
                                                                          shape.z_format,
                                                                          shape.stencil_format};
                                    uint64_t depthToken = 999;
                                    if (ctr_video_depth_attachment_create_v1(&session, &params, &depthToken) ||
                                        depthToken == 999) {
                                        status = 160;
                                        break;
                                    }
                                    if (ctr_video_depth_attachment_clear_v1(&session, depthToken, 0x3f800000, 0) ||
                                        ctr_video_depth_attachment_clear_v1(&session, depthToken, 0x3e800000, 7) ||
                                        ctr_video_depth_attachment_clear_v1(&session, depthToken, 0x7fc00000, 0) !=
                                            60 ||
                                        ctr_video_depth_attachment_clear_v1(&session, depthToken, 0x3f800000, 256) !=
                                            60) {
                                        status = 161;
                                        break;
                                    }
                                    if (shape.samples == 2) {
                                        if (session.DepthSampledControl(depthToken)) {
                                            status = 215;
                                            break;
                                        }
                                        std::puts(
                                            "MS2 depth sampled layout actual fenced roundtrip descriptor readiness preserved; no sampling dispatch");
                                        status = 0;
                                        const auto load = [](const char* path) {
                                            std::ifstream file(path, std::ios::binary | std::ios::ate);
                                            if (!file) return std::vector<uint32_t>{};
                                            const auto n = file.tellg();
                                            if (n <= 0 || n % 4) return std::vector<uint32_t>{};
                                            std::vector<uint32_t> code(static_cast<size_t>(n) / 4);
                                            file.seekg(0);
                                            file.read(reinterpret_cast<char*>(code.data()), n);
                                            return code;
                                        };
                                        const auto vs = load("build/CTRPort-data/rectlist_control.vert.spv"),
                                                   tcs = load("build/CTRPort-data/rectlist_public.tcs.spv"),
                                                   tes = load("build/CTRPort-data/rectlist_public.tes.spv");
                                        {
                                            const auto cs = load("build/CTRPort-data/compute_public_control.spv");
                                            CtrGraphicsBindingContractV1 binding{};
                                            binding.version = 1;
                                            binding.size = sizeof(binding);
                                            uint64_t computeLayout = 0, computePipeline = 999;
                                            if (ctr_video_compute_layout_create_v1(&session, &binding,
                                                                                   &computeLayout) ||
                                                ctr_video_compute_pipeline_create_v1(&session, computeLayout, cs.data(),
                                                                                     4, &computePipeline) != 60 ||
                                                computePipeline != 999 ||
                                                ctr_video_compute_pipeline_create_v1(&session, computeLayout, vs.data(),
                                                                                     static_cast<uint32_t>(vs.size()),
                                                                                     &computePipeline) != 60 ||
                                                computePipeline != 999 ||
                                                ctr_video_compute_pipeline_create_v1(&session, computeLayout, cs.data(),
                                                                                     static_cast<uint32_t>(cs.size()),
                                                                                     &computePipeline)) {
                                                status = 198;
                                                break;
                                            }
                                            std::shared_ptr<CtrGraphicsLayoutOwner> retainedCompute;
                                            if (session.RetainGraphicsLayout(computeLayout, retainedCompute) ||
                                                !CtrPipelineOwnerControl::RunCompute(device, retainedCompute)) {
                                                status = 204;
                                                break;
                                            }
                                            int wrong = 0;
                                            std::thread computeWorker([&] {
                                                wrong =
                                                    ctr_video_compute_pipeline_release_v1(&session, computePipeline);
                                            });
                                            computeWorker.join();
                                            if (wrong != 116 || session.ReleaseGraphicsLayout(computeLayout) ||
                                                ctr_video_compute_pipeline_release_v1(&session, computePipeline) ||
                                                ctr_video_compute_pipeline_release_v1(&session, computePipeline) !=
                                                    62 ||
                                                ctr_video_compute_pipeline_create_v1(&session, computeLayout, cs.data(),
                                                                                     static_cast<uint32_t>(cs.size()),
                                                                                     &computePipeline) != 62) {
                                                status = 198;
                                                break;
                                            }
                                            std::puts(
                                                "compute real pipeline create/release with whole public authored SPIRV; compute-only layout retained after token release; malformed/noncompute60 owner116 stale62 no dispatch");
                                        }
                                        {
                                            const auto cs = load("build/CTRPort-data/compute_write_control.spv");
                                            CtrGraphicsBindingContractV1 binding{};
                                            binding.version = 1;
                                            binding.size = sizeof(binding);
                                            binding.unified_after = 1;
                                            binding.buffer_after = 1;
                                            binding.descriptor_count = 1;
                                            binding.descriptors[0] = {0, 0, 0, 1, 0, 1};
                                            std::array<uint32_t, 16> source{}, computeReadback{};
                                            source.fill(0xa5a5a5a5);
                                            uint64_t buffer = 0, computeLayout = 0, computePipeline = 0;
                                            if (session.CreateBuffer(source.data(), sizeof(source), 1, &buffer) ||
                                                ctr_video_compute_layout_create_v1(&session, &binding,
                                                                                   &computeLayout) ||
                                                ctr_video_compute_pipeline_create_v1(&session, computeLayout, cs.data(),
                                                                                     static_cast<uint32_t>(cs.size()),
                                                                                     &computePipeline)) {
                                                status = 199;
                                                break;
                                            }
                                            CtrGraphicsPushDataV1 push{};
                                            push.version = 1;
                                            push.size = sizeof(push);
                                            uint32_t value = 123;
                                            std::memcpy(push.bytes, &value, 4);
                                            uint32_t groups[] = {2, 1, 1};
                                            if (ctr_video_compute_dispatch_v1(&session, computePipeline, &push,
                                                                              groups) != 88) {
                                                status = 199;
                                                break;
                                            }
                                            CtrGraphicsBufferWriteV1 write{0, 0, 1, 0, buffer};
                                            if (session.WriteGraphicsBuffers(computeLayout, &write, 1) ||
                                                ctr_video_compute_dispatch_v1(&session, computePipeline, &push,
                                                                              groups) ||
                                                session.CopyBufferInitial(buffer, computeReadback.data(),
                                                                          sizeof(computeReadback))) {
                                                status = 199;
                                                break;
                                            }
                                            for (uint32_t i = 0; i < 16; ++i)
                                                if (computeReadback[i] != 123 + i) status = 199;
                                            if (status) break;
                                            groups[0] = 0;
                                            if (ctr_video_compute_dispatch_v1(&session, computePipeline, &push,
                                                                              groups) != 88) {
                                                status = 199;
                                                break;
                                            }
                                            groups[0] = 2;
                                            value = 456;
                                            std::memcpy(push.bytes, &value, 4);
                                            if (ctr_video_compute_dispatch_v1(&session, computePipeline, &push,
                                                                              groups) ||
                                                session.CopyBufferInitial(buffer, computeReadback.data(),
                                                                          sizeof(computeReadback))) {
                                                status = 199;
                                                break;
                                            }
                                            for (uint32_t i = 0; i < 16; ++i)
                                                if (computeReadback[i] != 456 + i) status = 199;
                                            if (status) break;
                                            if (ctr_video_compute_pipeline_release_v1(&session, computePipeline) ||
                                                session.ReleaseGraphicsLayout(computeLayout) ||
                                                session.ReleaseBuffer(buffer)) {
                                                status = 199;
                                                break;
                                            }
                                            std::puts(
                                                "compute actual SSBO writer two exact dispatch2x1x1 push120B shader->HOST barrier fence GPUmapped16words readback0diff; unbound/zero88 no guest writeback");
                                        }
                                        {
                                            const auto cs = load("build/CTRPort-data/compute_depth_ms_control.spv");
                                            CtrGraphicsBindingContractV1 binding{};
                                            binding.version = 1;
                                            binding.size = sizeof(binding);
                                            binding.unified_after = 2;
                                            binding.buffer_after = 1;
                                            binding.descriptor_count = 2;
                                            binding.descriptors[0] = {0, 0, 0, 1, 0, 1};
                                            binding.descriptors[1] = {1, 0, 1, 1, UINT32_MAX, 0};
                                            std::array<uint32_t, 16> initial{}, readback{};
                                            uint64_t buffer = 0, layout = 0, pipeline = 0, image = 0;
                                            if (session.CreateBuffer(initial.data(), sizeof(initial), 1, &buffer) ||
                                                ctr_video_compute_layout_create_v1(&session, &binding, &layout) ||
                                                ctr_video_compute_pipeline_create_v1(&session, layout, cs.data(),
                                                                                     static_cast<uint32_t>(cs.size()),
                                                                                     &pipeline) ||
                                                ctr_video_depth_sampled_alias_v1(&session, depthToken, &image)) {
                                                status = 216;
                                                break;
                                            }
                                            CtrGraphicsBufferWriteV1 write{0, 0, 1, 0, buffer};
                                            uint32_t imageBinding = 1;
                                            CtrGraphicsPushDataV1 push{};
                                            push.version = 1;
                                            push.size = sizeof(push);
                                            uint32_t groups[] = {2, 1, 1};
                                            if (session.WriteGraphicsBuffers(layout, &write, 1) ||
                                                session.WriteGraphicsImages(layout, &imageBinding, &image, 1) ||
                                                session.ReleaseSampledImage(image) ||
                                                ctr_video_compute_dispatch_v1(&session, pipeline, &push, groups) ||
                                                session.CopyBufferInitial(buffer, readback.data(), sizeof(readback))) {
                                                status = 216;
                                                break;
                                            }
                                            for (auto word : readback)
                                                if (word != 0x3e800000) status = 216;
                                            if (status) break;
                                            if (ctr_video_depth_attachment_restore_v1(&session, depthToken)) {
                                                status = 225;
                                                break;
                                            }
                                            CtrGraphicsBindingContractV1 emptyMask{};
                                            emptyMask.version = 1;
                                            emptyMask.size = sizeof(emptyMask);
                                            uint64_t maskLayout = 0, maskPipeline = 0;
                                            CtrGraphicsKeyV1 maskKey{};
                                            maskKey.version = 1;
                                            maskKey.size = sizeof(maskKey);
                                            maskKey.primitive_type = 17;
                                            maskKey.polygon_mode = 2;
                                            maskKey.logic_op = 0xcc;
                                            maskKey.num_samples = maskKey.depth_samples = 2;
                                            maskKey.z_format = 3;
                                            maskKey.stencil_format = 1;
                                            const auto maskTrace = [](const char* phase, int rc) {
                                                std::printf("mask depth %s rc=%d\n", phase, rc);
                                                return rc;
                                            };
                                            if (maskTrace("layout", session.CreateGraphicsLayout(&emptyMask, &emptyMask,
                                                                                                 &maskLayout)) ||
                                                maskTrace("pipeline",
                                                          session.CreateRectListDepthPipeline(
                                                              &maskKey, maskLayout, depthToken, vs.data(),
                                                              static_cast<uint32_t>(vs.size()), tcs.data(),
                                                              static_cast<uint32_t>(tcs.size()), tes.data(),
                                                              static_cast<uint32_t>(tes.size()), &maskPipeline, 1))) {
                                                status = 225;
                                                break;
                                            }
                                            CtrGraphicsViewportStateV1 maskVp{};
                                            maskVp.version = 1;
                                            maskVp.size = sizeof(maskVp);
                                            maskVp.count = 1;
                                            maskVp.viewports[0] = {0, 1, 64, -1, 0, 1};
                                            maskVp.scissors[0].extent = {64, 1};
                                            CtrGraphicsDynamicStateV1 maskDs{};
                                            maskDs.version = 1;
                                            maskDs.size = sizeof(maskDs);
                                            maskDs.line_width = 1;
                                            maskDs.depth_test = maskDs.depth_write = 1;
                                            maskDs.depth_compare = 7;
                                            CtrGraphicsDrawV1 maskDraw{};
                                            maskDraw.version = 1;
                                            maskDraw.size = sizeof(maskDraw);
                                            maskDraw.vertex_count = 3;
                                            maskDraw.instance_count = 1;
                                            if (maskTrace("draw",
                                                          session.DrawRectListDepth(maskPipeline, depthToken, &maskVp,
                                                                                    &maskDs, &push, &maskDraw)) ||
                                                session.AliasSampledDepth(depthToken, &image) ||
                                                session.ReleaseSampledImage(image) ||
                                                ctr_video_compute_dispatch_v1(&session, pipeline, &push, groups) ||
                                                session.CopyBufferInitial(buffer, readback.data(), sizeof(readback))) {
                                                status = 225;
                                                break;
                                            }
                                            for (uint32_t sample = 0; sample < 16; ++sample)
                                                if (readback[sample] != (sample % 2 ? 0x3e800000u : 0x3f000000u)) {
                                                    std::printf("mask sample%u actual%x\n", sample, readback[sample]);
                                                    status = 225;
                                                }
                                            if (status) break;
                                            if (session.ReleaseGraphicsPipeline(maskPipeline) ||
                                                session.ReleaseGraphicsLayout(maskLayout)) {
                                                status = 225;
                                                break;
                                            }
                                            std::puts(
                                                "MS2 unequal depth sample0half sample1quarter actual masked draw then16word texelFetch0diff no guestpublication");
                                            if (ctr_video_depth_attachment_restore_v1(&session, depthToken) ||
                                                ctr_video_compute_dispatch_v1(&session, pipeline, &push, groups) !=
                                                    91 ||
                                                ctr_video_compute_pipeline_release_v1(&session, pipeline) ||
                                                session.ReleaseGraphicsLayout(layout) ||
                                                session.ReleaseBuffer(buffer)) {
                                                status = 216;
                                                break;
                                            }
                                            if (session.ClearDepthAttachment(depthToken, 0x3e800000, 7)) {
                                                status = 225;
                                                break;
                                            }
                                            std::puts(
                                                "MS2 retained depth actual compute texelFetch both samples16words quarter0diff; alias token released before dispatch; attachment restore rejects stale descriptor91");
                                        }
                                        {
                                            const auto cs =
                                                load("build/CTRPort-data/compute_depth_ms_full_control.spv");
                                            CtrDepthMS2LayoutV1 fullLayout{
                                                1, sizeof(fullLayout), 1920, 1080, 1920, 2, 3, 1, 1, 34559, 0, 0, 0,
                                                0, 73170878464ULL};
                                            CtrGraphicsImageExtentV1 fullExtent{};
                                            if (ctr_video_depth_ms2_extent_v1(&fullLayout, &fullExtent) ||
                                                fullExtent.bytes != 17694720 || fullExtent.padded_height != 1152 ||
                                                fullExtent.pitch != 1920) {
                                                status = 232;
                                                break;
                                            }
                                            CtrDepthBackingSpansV1 spans{};
                                            if (ctr_video_depth_ms2_spans_v1(&fullLayout, 73188573184ULL, &spans) ||
                                                spans.depth_address != fullLayout.guest_address ||
                                                spans.depth_bytes != 17694720 ||
                                                spans.stencil_address != 73188573184ULL ||
                                                spans.stencil_bytes != 2073600) {
                                                status = 234;
                                                break;
                                            }
                                            const auto expectedSpans = spans;
                                            for (const auto address :
                                                 {uint64_t(0), uint64_t(73188573185ULL), uint64_t(UINT64_MAX - 255)})
                                                if (ctr_video_depth_ms2_spans_v1(&fullLayout, address, &spans) != 60 ||
                                                    std::memcmp(&spans, &expectedSpans, sizeof(spans)))
                                                    status = 234;
                                            if (ctr_video_depth_ms2_spans_v1(&fullLayout, fullLayout.guest_address,
                                                                             &spans) != 88 ||
                                                std::memcmp(&spans, &expectedSpans, sizeof(spans)) ||
                                                ctr_video_depth_ms2_spans_v1(&fullLayout, 73188573184ULL, nullptr) !=
                                                    60)
                                                status = 234;
                                            auto invalidSpans = fullLayout;
                                            invalidSpans.samples = 1;
                                            if (ctr_video_depth_ms2_spans_v1(&invalidSpans, 73188573184ULL, &spans) !=
                                                    88 ||
                                                std::memcmp(&spans, &expectedSpans, sizeof(spans)))
                                                status = 234;
                                            if (status) break;
                                            std::puts(
                                                "MS2 exact backing spans depth17694720 stencil2073600 adjacent nonoverlap; invalid alignment overflow60 overlap88 atomic no page padding or stencil mapping");
                                            const auto validFull = fullExtent;
                                            auto invalidFull = fullLayout;
                                            invalidFull.slice_tile_max = 34558;
                                            if (ctr_video_depth_ms2_extent_v1(&invalidFull, &fullExtent) != 88 ||
                                                std::memcmp(&fullExtent, &validFull, sizeof(fullExtent))) {
                                                status = 232;
                                                break;
                                            }
                                            invalidFull = fullLayout;
                                            invalidFull.guest_address = UINT64_MAX - 255;
                                            if (ctr_video_depth_ms2_extent_v1(&invalidFull, &fullExtent) != 60 ||
                                                std::memcmp(&fullExtent, &validFull, sizeof(fullExtent))) {
                                                status = 232;
                                                break;
                                            }
                                            std::puts(
                                                "MS2 extent prewriter1920x1080 pitch1920 padded1152 bytes17694720 public slice34559; bad slice88 overflow60 atomic depth span only");
                                            CtrDepthAttachmentParametersV1 fullDepth{
                                                1, sizeof(fullDepth), 1920, 1080, 2, 3, 1};
                                            uint64_t owner = 0;
                                            CtrGraphicsBindingContractV1 empty{}, binding{};
                                            empty.version = binding.version = 1;
                                            empty.size = sizeof(empty);
                                            binding.size = sizeof(binding);
                                            binding.unified_after = 2;
                                            binding.buffer_after = 1;
                                            binding.descriptor_count = 2;
                                            binding.descriptors[0] = {0, 0, 0, 1, 0, 1};
                                            binding.descriptors[1] = {1, 0, 1, 1, UINT32_MAX, 0};
                                            CtrGraphicsKeyV1 key{};
                                            key.version = 1;
                                            key.size = sizeof(key);
                                            key.primitive_type = 17;
                                            key.polygon_mode = 2;
                                            key.logic_op = 0xcc;
                                            key.num_samples = key.depth_samples = 2;
                                            key.z_format = 3;
                                            key.stencil_format = 1;
                                            uint64_t drawLayout = 0, drawPipeline = 0, buffer = 0, layout = 0,
                                                     pipeline = 0, image = 0;
                                            if (session.CreateDepthAttachment(&fullDepth, &owner) ||
                                                session.ClearDepthAttachment(owner, 0x3e800000, 7) ||
                                                session.CreateGraphicsLayout(&empty, &empty, &drawLayout) ||
                                                session.CreateRectListDepthPipeline(
                                                    &key, drawLayout, owner, vs.data(),
                                                    static_cast<uint32_t>(vs.size()), tcs.data(),
                                                    static_cast<uint32_t>(tcs.size()), tes.data(),
                                                    static_cast<uint32_t>(tes.size()), &drawPipeline, 1)) {
                                                status = 231;
                                                break;
                                            }
                                            CtrGraphicsViewportStateV1 vp{};
                                            vp.version = 1;
                                            vp.size = sizeof(vp);
                                            vp.count = 1;
                                            vp.viewports[0] = {0, 1080, 1920, -1080, 0, 1};
                                            vp.scissors[0].extent = {1920, 1080};
                                            CtrGraphicsDynamicStateV1 ds{};
                                            ds.version = 1;
                                            ds.size = sizeof(ds);
                                            ds.line_width = 1;
                                            ds.depth_test = ds.depth_write = 1;
                                            ds.depth_compare = 7;
                                            CtrGraphicsPushDataV1 push{};
                                            push.version = 1;
                                            push.size = sizeof(push);
                                            CtrGraphicsDrawV1 draw{};
                                            draw.version = 1;
                                            draw.size = sizeof(draw);
                                            draw.vertex_count = 3;
                                            draw.instance_count = 1;
                                            if (session.DrawRectListDepth(drawPipeline, owner, &vp, &ds, &push,
                                                                          &draw)) {
                                                status = 231;
                                                break;
                                            }
                                            uint64_t lease = 999;
                                            int wrongRetain = 0;
                                            std::thread retainWorker([&] {
                                                wrongRetain =
                                                    ctr_video_depth_attachment_retain_v1(&session, owner, &lease);
                                            });
                                            retainWorker.join();
                                            if (wrongRetain != 116 || lease != 999 ||
                                                ctr_video_depth_attachment_retain_v1(&session, owner, nullptr) != 60 ||
                                                ctr_video_depth_attachment_retain_v1(&session, owner, &lease) ||
                                                lease == owner || lease == 999 ||
                                                session.ReleaseGraphicsPipeline(drawPipeline) ||
                                                session.ReleaseGraphicsLayout(drawLayout) ||
                                                ctr_video_depth_attachment_release_v1(&session, owner)) {
                                                status = 233;
                                                break;
                                            }
                                            uint64_t rejected = 999;
                                            if (ctr_video_depth_attachment_retain_v1(&session, owner, &rejected) !=
                                                    62 ||
                                                rejected != 999 ||
                                                ctr_video_depth_attachment_clear_v1(&session, owner, 0x3f800000, 0) !=
                                                    62) {
                                                status = 233;
                                                break;
                                            }
                                            owner = lease;
                                            uint64_t generation = 999;
                                            int wrongRegister = 0;
                                            std::thread registerWorker([&] {
                                                wrongRegister = ctr_video_depth_residency_register_v1(
                                                    &session, owner, &fullLayout, 73188573184ULL, &generation);
                                            });
                                            registerWorker.join();
                                            if (wrongRegister != 116 || generation != 999 ||
                                                ctr_video_depth_residency_register_v1(&session, owner, &fullLayout,
                                                                                      73188573184ULL, &generation) ||
                                                generation == 999 ||
                                                ctr_video_depth_attachment_clear_v1(&session, owner, 0x3f800000, 0) !=
                                                    91 ||
                                                session.ReleaseDepthAttachment(owner)) {
                                                status = 235;
                                                break;
                                            }
                                            rejected = 999;
                                            auto wrongSpans = spans;
                                            wrongSpans.stencil_bytes++;
                                            if (ctr_video_depth_residency_acquire_v1(&session, &wrongSpans, generation,
                                                                                     &rejected) != 88 ||
                                                rejected != 999 ||
                                                ctr_video_depth_residency_acquire_v1(&session, &spans, generation + 1,
                                                                                     &rejected) != 62 ||
                                                rejected != 999 ||
                                                ctr_video_depth_residency_acquire_v1(&session, &spans, generation,
                                                                                     &owner)) {
                                                status = 235;
                                                break;
                                            }

                                            uint64_t duplicateGeneration = 999;
                                            if (ctr_video_depth_residency_register_v1(&session, owner, &fullLayout,
                                                                                      73188573184ULL,
                                                                                      &duplicateGeneration) != 91 ||
                                                duplicateGeneration != 999) {
                                                status = 236;
                                                break;
                                            }
                                            uint64_t otherOwner = 0;
                                            if (session.CreateDepthAttachment(&fullDepth, &otherOwner) ||
                                                session.ClearDepthAttachment(otherOwner, 0x3f800000, 0)) {
                                                status = 236;
                                                break;
                                            }
                                            for (uint32_t collision = 0; collision < 4; ++collision) {
                                                auto overlapLayout = fullLayout;
                                                overlapLayout.guest_address =
                                                    collision < 2
                                                        ? (collision ? spans.stencil_address : spans.depth_address)
                                                        : 0x2000000000ULL;
                                                const uint64_t stencil = collision < 2
                                                                             ? 0x3000000000ULL
                                                                             : (collision == 2 ? spans.depth_address
                                                                                               : spans.stencil_address);
                                                duplicateGeneration = 999;
                                                if (ctr_video_depth_residency_register_v1(&session, otherOwner,
                                                                                          &overlapLayout, stencil,
                                                                                          &duplicateGeneration) != 88 ||
                                                    duplicateGeneration != 999)
                                                    status = 236;
                                            }
                                            if (status || session.ReleaseDepthAttachment(otherOwner)) {
                                                status = 236;
                                                break;
                                            }
                                            int wrongAcquire = 0, wrongRemove = 0;
                                            rejected = 999;
                                            std::thread residencyWorker([&] {
                                                wrongAcquire = ctr_video_depth_residency_acquire_v1(
                                                    &session, &spans, generation, &rejected);
                                                wrongRemove = ctr_video_depth_residency_remove_v1(&session, generation);
                                            });
                                            residencyWorker.join();
                                            if (wrongAcquire != 116 || wrongRemove != 116 || rejected != 999 ||
                                                session.CreateGraphicsLayout(&empty, &empty, &drawLayout) ||
                                                session.CreateRectListDepthPipeline(
                                                    &key, drawLayout, owner, vs.data(),
                                                    static_cast<uint32_t>(vs.size()), tcs.data(),
                                                    static_cast<uint32_t>(tcs.size()), tes.data(),
                                                    static_cast<uint32_t>(tes.size()), &drawPipeline, 1) ||
                                                session.DrawRectListDepth(drawPipeline, owner, &vp, &ds, &push,
                                                                          &draw) != 91) {
                                                status = 236;
                                                break;
                                            }
                                            constexpr size_t bytes = 16588800;
                                            std::vector<unsigned char> initial(bytes), readback(bytes + 32, 0xa7);
                                            if (session.CreateBuffer(initial.data(), bytes, 1, &buffer) ||
                                                ctr_video_compute_layout_create_v1(&session, &binding, &layout) ||
                                                ctr_video_compute_pipeline_create_v1(&session, layout, cs.data(),
                                                                                     static_cast<uint32_t>(cs.size()),
                                                                                     &pipeline) ||
                                                ctr_video_depth_sampled_alias_v1(&session, owner, &image)) {
                                                status = 231;
                                                break;
                                            }
                                            CtrGraphicsBufferWriteV1 write{0, 0, 1, 0, buffer};
                                            uint32_t imageBinding = 1, groups[] = {480, 1080, 1};
                                            if (session.WriteGraphicsBuffers(layout, &write, 1) ||
                                                session.WriteGraphicsImages(layout, &imageBinding, &image, 1) ||
                                                session.ReleaseSampledImage(image) ||
                                                ctr_video_compute_dispatch_v1(&session, pipeline, &push, groups) ||
                                                session.CopyBufferInitial(buffer, readback.data() + 16, bytes)) {
                                                status = 231;
                                                break;
                                            }
                                            for (size_t i = 0; i < bytes / 4; ++i) {
                                                uint32_t word = 0;
                                                std::memcpy(&word, readback.data() + 16 + i * 4, 4);
                                                if (word != (i % 2 ? 0x3e800000u : 0x3f000000u)) {
                                                    std::printf("full MS2 sample%zu actual%x\n", i, word);
                                                    status = 231;
                                                    break;
                                                }
                                            }
                                            for (size_t i = 0; i < 16; ++i)
                                                if (readback[i] != 0xa7 || readback[bytes + 16 + i] != 0xa7)
                                                    status = 231;
                                            if (status) break;
                                            {
                                                std::vector<unsigned char> prior(fullExtent.bytes),
                                                    converted(fullExtent.bytes + 32, 0xa7);
                                                for (size_t i = 0; i < prior.size(); ++i)
                                                    prior[i] = static_cast<unsigned char>((i * 17 + 31) % 251);
                                                int rc = ctr_video_depth_ms2_retile_samples_v1(
                                                    &session, owner, &fullLayout, prior.data(), prior.size(),
                                                    readback.data() + 16, bytes, converted.data() + 16);
                                                if (rc) {
                                                    std::printf("MS2 retile status%d\n", rc);
                                                    status = 237;
                                                    break;
                                                }
                                                rc = ctr_with_tiling_pipelines(
                                                    device, deviceGet, tilingDirectory, true, true, true,
                                                    [&](VkPipelineLayout pl, const VkPipeline* ps,
                                                        VkDescriptorSetLayout dl) -> int {
                                                        std::vector<unsigned char> before, after;
                                                        int r = ctr_thin_detile(device, deviceGet, q, memory, pl, ps[1],
                                                                                dl, 1920, 1152, prior.data(),
                                                                                prior.size(), before, false, 4, 2);
                                                        if (r) return r;
                                                        r = ctr_thin_detile(device, deviceGet, q, memory, pl, ps[1], dl,
                                                                            1920, 1152, converted.data() + 16,
                                                                            prior.size(), after, false, 4, 2);
                                                        if (r) return r;
                                                        if (std::memcmp(after.data(), readback.data() + 16, bytes) ||
                                                            std::memcmp(after.data() + bytes, before.data() + bytes,
                                                                        after.size() - bytes))
                                                            return 112;
                                                        return 0;
                                                    },
                                                    4, false, 2);
                                                if (rc) {
                                                    status = 237;
                                                    break;
                                                }
                                                for (size_t i = 0; i < 16; ++i)
                                                    if (converted[i] != 0xa7 ||
                                                        converted[prior.size() + 16 + i] != 0xa7)
                                                        status = 237;
                                                const auto saved = converted;
                                                auto invalid = fullLayout;
                                                invalid.samples = 1;
                                                if (ctr_video_depth_ms2_retile_samples_v1(
                                                        &session, owner, &invalid, prior.data(), prior.size(),
                                                        readback.data() + 16, bytes, converted.data() + 16) != 88 ||
                                                    converted != saved ||
                                                    ctr_video_depth_ms2_retile_samples_v1(
                                                        &session, owner, &fullLayout, prior.data(), prior.size(),
                                                        readback.data() + 16, bytes - 4, converted.data() + 16) != 60 ||
                                                    converted != saved ||
                                                    ctr_video_depth_ms2_retile_samples_v1(
                                                        &session, UINT64_MAX, &fullLayout, prior.data(), prior.size(),
                                                        readback.data() + 16, bytes, converted.data() + 16) != 62 ||
                                                    converted != saved) {
                                                    status = 237;
                                                    break;
                                                }
                                                if (status) break;
                                                std::puts(
                                                    "MS2 retile actual4147200 samples exact; prior padded72 rows preserved; guards32; invalid88 short60 stale62 atomic no guest publication");
                                            }
                                            if (ctr_video_depth_attachment_restore_v1(&session, owner) ||
                                                ctr_video_compute_dispatch_v1(&session, pipeline, &push, groups) !=
                                                    91 ||
                                                ctr_video_depth_residency_remove_v1(&session, generation) ||
                                                session.ClearDepthAttachment(owner, 0x3e800000, 7) ||
                                                session.DrawRectListDepth(drawPipeline, owner, &vp, &ds, &push,
                                                                          &draw) ||
                                                session.ReleaseGraphicsPipeline(drawPipeline) ||
                                                session.ReleaseGraphicsLayout(drawLayout)) {
                                                status = 236;
                                                break;
                                            }
                                            uint64_t nextGeneration = 999;
                                            if (ctr_video_depth_residency_register_v1(
                                                    &session, owner, &fullLayout, 73188573184ULL, &nextGeneration) ||
                                                nextGeneration <= generation || nextGeneration == 999) {
                                                status = 236;
                                                break;
                                            }
                                            rejected = 999;
                                            if (ctr_video_depth_residency_acquire_v1(&session, &spans, generation,
                                                                                     &rejected) != 62 ||
                                                rejected != 999 ||
                                                ctr_video_depth_residency_acquire_v1(&session, &spans, nextGeneration,
                                                                                     &lease) ||
                                                session.ReleaseDepthAttachment(lease) ||
                                                ctr_video_depth_residency_remove_v1(&session, nextGeneration) ||
                                                ctr_video_compute_pipeline_release_v1(&session, pipeline) ||
                                                session.ReleaseGraphicsLayout(layout) ||
                                                session.ReleaseBuffer(buffer) ||
                                                session.ReleaseDepthAttachment(owner)) {
                                                status = 231;
                                                break;
                                            }
                                            rejected = 999;
                                            if (ctr_video_depth_attachment_retain_v1(&session, owner, &rejected) !=
                                                    62 ||
                                                rejected != 999 ||
                                                ctr_video_depth_attachment_release_v1(&session, owner) != 62) {
                                                status = 233;
                                                break;
                                            }
                                            rejected = 999;
                                            if (ctr_video_depth_residency_acquire_v1(&session, &spans, generation,
                                                                                     &rejected) != 62 ||
                                                rejected != 999 ||
                                                ctr_video_depth_residency_remove_v1(&session, generation) != 62) {
                                                status = 235;
                                                break;
                                            }
                                            std::puts(
                                                "Depth residency generations four plane overlap cases88 atomic duplicate91 acquireRemoveThreads116 drawFrozen91 then clearDraw0 monotonicReregister old62 newAcquire0");
                                            std::puts(
                                                "Depth residency exact address generation retains sole GPU owner after token release; lookup fullMS2 words0diff immutable clear91 stale62 mismatch88 thread116");
                                            std::puts(
                                                "Depth lease actual MS2 survives original token and draw pipeline release; all4147200 sample words0diff thread116 stale62 atomic finalrelease0 same session");
                                            std::puts(
                                                "MS2 full retained1920x1080 actual masked depth draw sample0half sample1quarter compute extraction16588800B all4147200words0diff guards32; restore rejects stale91 no guestpublication");
                                        }
                                        for (const uint32_t width : {2u, 4u}) {
                                            std::vector<unsigned char> data(102 * width);
                                            for (uint32_t i = 0; i < 102; ++i) {
                                                const uint32_t v = i == 101 ? 557u : i;
                                                std::memcpy(data.data() + i * width, &v, width);
                                            }
                                            std::unique_ptr<CtrOwnedBuffer> owner;
                                            if (CtrOwnedBuffer::Create(device, deviceGet, memory, data.data(),
                                                                       data.size(), VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
                                                                       owner, width)) {
                                                status = 242;
                                                break;
                                            }
                                            VkBuffer handle = VK_NULL_HANDLE;
                                            uint32_t lo = 999, hi = 999;
                                            if (owner->IndexBuffer(width, 0, 0, 102, handle, lo, hi) || !handle ||
                                                lo != 0 || hi != 557) {
                                                status = 242;
                                                break;
                                            }
                                            const auto saved = handle;
                                            lo = hi = 999;
                                            if (owner->IndexBuffer(width == 2 ? 4u : 2u, 0, 0, 102, handle, lo, hi) !=
                                                    88 ||
                                                handle != saved || lo != 999 || hi != 999 ||
                                                owner->IndexBuffer(width, 1, 0, 1, handle, lo, hi) != 60 ||
                                                owner->IndexBuffer(width, 0, 1, 102, handle, lo, hi) != 88 ||
                                                owner->IndexBuffer(width, UINT64_MAX, UINT32_MAX, UINT32_MAX, handle,
                                                                   lo, hi) != 60 ||
                                                owner->IndexBuffer(width, 0, 0, 0, handle, lo, hi) != 60 || lo != 999 ||
                                                hi != 999) {
                                                status = 242;
                                                break;
                                            }
                                            if (owner->IndexBuffer(width, width, 1, 99, handle, lo, hi) || lo != 2 ||
                                                hi != 100) {
                                                status = 242;
                                                break;
                                            }
                                            std::vector<unsigned char> copy(data.size());
                                            if (owner->CopyInitialized(copy.data(), copy.size()) || copy != data) {
                                                status = 242;
                                                break;
                                            }
                                        }
                                        if (status) break;
                                        std::puts(
                                            "Immutable index owner U16 U32 actual102 elements range0_557 offsets type bounds atomic no draw claim");
                                        {
                                            const auto cs = load("build/CTRPort-data/compute_sampled_r32_control.spv");
                                            CtrGraphicsImageParametersV1 p{};
                                            p.version = 1;
                                            p.size = sizeof(p);
                                            p.type = 9;
                                            p.width = p.height = p.pitch = 64;
                                            p.depth = p.layers = p.levels = p.samples = 1;
                                            p.data_format = 5;
                                            p.number_format = 7;
                                            p.tile_mode = 13;
                                            p.guest_address = 68857298944ULL;
                                            p.swizzle[0] = 4;
                                            p.swizzle[1] = 5;
                                            p.swizzle[3] = 1;
                                            uint64_t image = 999;
                                            auto bad = p;
                                            bad.number_format = 0;
                                            if (session.CreateSampledImage(&bad, &image) != 88 || image != 999) {
                                                status = 240;
                                                break;
                                            }
                                            bad = p;
                                            bad.swizzle[1] = 0;
                                            if (session.CreateSampledImage(&bad, &image) != 88 || image != 999 ||
                                                session.CreateSampledImage(&p, &image)) {
                                                status = 240;
                                                break;
                                            }
                                            constexpr uint16_t halfBits[] = {0, 0x3400, 0x3800, 0x3c00};
                                            constexpr uint32_t floatBits[] = {0, 0x3e800000, 0x3f000000, 0x3f800000};
                                            std::vector<uint16_t> source(8192), download(8192);
                                            for (size_t i = 0; i < 4096; ++i) {
                                                source[i * 2] = halfBits[i % 4];
                                                source[i * 2 + 1] = halfBits[(i / 4 + 1) % 4];
                                            }
                                            const auto unchanged = source;
                                            constexpr size_t bytes = 4096 * 16;
                                            std::vector<unsigned char> initial(bytes), output(bytes + 32, 0xa7);
                                            CtrGraphicsBindingContractV1 binding{};
                                            binding.version = 1;
                                            binding.size = sizeof(binding);
                                            binding.unified_after = 2;
                                            binding.buffer_after = 1;
                                            binding.descriptor_count = 2;
                                            binding.descriptors[0] = {0, 0, 0, 1, 0, 1};
                                            binding.descriptors[1] = {1, 0, 1, 1, UINT32_MAX, 0};
                                            uint64_t buffer = 0, layout = 0, pipeline = 0;
                                            if (session.UploadSampledImage(image, source.data(), 16383, 256) != 60 ||
                                                session.UploadSampledImage(image, source.data(), 16384, 256) ||
                                                session.CreateBuffer(initial.data(), bytes, 1, &buffer) ||
                                                ctr_video_compute_layout_create_v1(&session, &binding, &layout) ||
                                                ctr_video_compute_pipeline_create_v1(&session, layout, cs.data(),
                                                                                     static_cast<uint32_t>(cs.size()),
                                                                                     &pipeline)) {
                                                status = 240;
                                                break;
                                            }
                                            CtrGraphicsImageExtentV1 tiledExtent{
                                                1,   sizeof(tiledExtent), p.pitch,
                                                64u, p.guest_address,     uint64_t(p.pitch) * (64u) * 4};
                                            std::vector<unsigned char> padded(static_cast<size_t>(tiledExtent.bytes),
                                                                              0xd3),
                                                tiled;
                                            for (size_t i = 0; i < padded.size(); ++i)
                                                padded[i] = static_cast<unsigned char>(i * 37 + 11);
                                            for (uint32_t y = 0; y < p.height; ++y)
                                                std::memcpy(padded.data() + size_t(y) * p.pitch * 4,
                                                            source.data() + size_t(y) * p.width * 2,
                                                            size_t(p.width) * 4);
                                            if (ctr_with_tiling_pipelines(
                                                    device, deviceGet, tilingDirectory, true, true, true,
                                                    [&](VkPipelineLayout l, const VkPipeline* pipes,
                                                        VkDescriptorSetLayout d) -> int {
                                                        return ctr_thin_detile(device, deviceGet, q, memory, l,
                                                                               pipes[0], d, p.pitch,
                                                                               tiledExtent.padded_height, padded.data(),
                                                                               padded.size(), tiled, true, 4, 1, true);
                                                    },
                                                    4, false, 1, true)) {
                                                status = 241;
                                                break;
                                            }
                                            const auto preservedTiled = tiled;
                                            std::vector<unsigned char> tiledOutput(tiled.size() + 32, 0xa7);
                                            auto invalidExtent = tiledExtent;
                                            invalidExtent.padded_height += 8;
                                            if (session.UploadSampledTiled(image, tiled.data(), tiled.size() - 1,
                                                                           &tiledExtent) != 60 ||
                                                session.UploadSampledTiled(image, tiled.data(), tiled.size(),
                                                                           &invalidExtent) != 88 ||
                                                session.UploadSampledTiled(image, tiled.data(), tiled.size(),
                                                                           &tiledExtent)) {
                                                status = 241;
                                                break;
                                            }
                                            const auto untouchedOutput = tiledOutput;
                                            if (ctr_video_image_readback_tiled_v1(&session, image, tiled.data(),
                                                                                  tiled.size() - 1, &tiledExtent,
                                                                                  tiledOutput.data() + 16) != 60 ||
                                                tiledOutput != untouchedOutput ||
                                                ctr_video_image_readback_tiled_v1(&session, image, tiled.data(),
                                                                                  tiled.size(), &invalidExtent,
                                                                                  tiledOutput.data() + 16) != 88 ||
                                                tiledOutput != untouchedOutput ||
                                                ctr_video_image_readback_tiled_v1(&session, image, tiled.data(),
                                                                                  tiled.size(), &tiledExtent,
                                                                                  tiledOutput.data() + 16) ||
                                                std::memcmp(tiled.data(), tiledOutput.data() + 16, tiled.size()) ||
                                                tiled != preservedTiled) {
                                                status = 241;
                                                break;
                                            }
                                            for (size_t i = 0; i < 16; ++i)
                                                if (tiledOutput[i] != 0xa7 ||
                                                    tiledOutput[tiled.size() + 16 + i] != 0xa7)
                                                    status = 241;
                                            if (status) break;
                                            CtrGraphicsBufferWriteV1 write{0, 0, 1, 0, buffer};
                                            uint32_t imageBinding = 1, groups[] = {8, 64, 1};
                                            CtrGraphicsPushDataV1 push{};
                                            push.version = 1;
                                            push.size = sizeof(push);
                                            if (session.WriteGraphicsBuffers(layout, &write, 1) ||
                                                session.WriteGraphicsImages(layout, &imageBinding, &image, 1) ||
                                                ctr_video_compute_dispatch_v1(&session, pipeline, &push, groups) ||
                                                session.CopyBufferInitial(buffer, output.data() + 16, bytes) ||
                                                session.ReadbackSampledImage(image, download.data(), 16384, 256) ||
                                                download != source || source != unchanged) {
                                                status = 240;
                                                break;
                                            }
                                            for (size_t i = 0; i < 4096; ++i) {
                                                uint32_t actual[4]{};
                                                std::memcpy(actual, output.data() + 16 + i * 16, 16);
                                                if (actual[0] != floatBits[i % 4] ||
                                                    actual[1] != floatBits[(i / 4 + 1) % 4] || actual[2] ||
                                                    actual[3] != 0x3f800000) {
                                                    status = 240;
                                                    break;
                                                }
                                            }
                                            for (size_t i = 0; i < 16; ++i)
                                                if (output[i] != 0xa7 || output[bytes + 16 + i] != 0xa7) status = 240;
                                            if (status) break;
                                            if (ctr_video_compute_pipeline_release_v1(&session, pipeline) ||
                                                session.ReleaseGraphicsLayout(layout) ||
                                                session.ReleaseBuffer(buffer) || session.ReleaseSampledImage(image)) {
                                                status = 240;
                                                break;
                                            }
                                            std::puts(
                                                "RG16 tile13 composition actual16384B0diff whole public microtiling true GPU image component16384words0diff guards32 short60 extent88");
                                            std::puts(
                                                "RG16float sampled64x64 exact R_G_ZERO_ONE actual16384 component float words0diff half16 input readback16384B0diff guards32 unsupported88 short60 atomic tile13 composition verified");
                                        }
                                        for (const uint32_t oneWidth : {64u, 1024u}) {
                                            const auto cs = load("build/CTRPort-data/compute_sampled_1d_control.spv");
                                            CtrGraphicsImageParametersV1 p{};
                                            p.version = 1;
                                            p.size = sizeof(p);
                                            p.type = 8;
                                            p.width = oneWidth;
                                            p.height = 1;
                                            p.pitch = oneWidth == 64 ? 128 : 1024;
                                            p.depth = p.layers = p.levels = p.samples = 1;
                                            p.data_format = 10;
                                            p.number_format = 0;
                                            p.tile_mode = 14;
                                            p.guest_address = 73161965568ULL;
                                            p.swizzle[0] = 6;
                                            p.swizzle[1] = 5;
                                            p.swizzle[2] = 4;
                                            p.swizzle[3] = 7;
                                            uint64_t rejected1D = 999;
                                            for (unsigned field1D = 0; field1D < 5; ++field1D) {
                                                auto invalid1D = p;
                                                switch (field1D) {
                                                case 0: invalid1D.width++; break;
                                                case 1: invalid1D.pitch++; break;
                                                case 2: invalid1D.height = 2; break;
                                                case 3: invalid1D.tile_mode = 8; break;
                                                case 4: invalid1D.swizzle[3] = 1; break;
                                                }
                                                if (session.CreateSampledImage(&invalid1D, &rejected1D) != 88 ||
                                                    rejected1D != 999)
                                                    status = 240;
                                            }
                                            if (status) break;
                                            uint64_t image = 999;
                                            auto bad = p;
                                            bad.number_format = 7;
                                            if (session.CreateSampledImage(&bad, &image) != 88 || image != 999) {
                                                status = 240;
                                                break;
                                            }
                                            bad = p;
                                            bad.swizzle[1] = 0;
                                            if (session.CreateSampledImage(&bad, &image) != 88 || image != 999 ||
                                                session.CreateSampledImage(&p, &image)) {
                                                status = 240;
                                                break;
                                            }
                                            std::vector<uint8_t> source(oneWidth * 4), download(oneWidth * 4);
                                            for (size_t i = 0; i < source.size(); ++i)
                                                source[i] = static_cast<uint8_t>((i % 4) * 85);
                                            const auto unchanged = source;
                                            const size_t bytes = oneWidth * 16;
                                            std::vector<unsigned char> initial(bytes), output(bytes + 32, 0xa7);
                                            CtrGraphicsBindingContractV1 binding{};
                                            binding.version = 1;
                                            binding.size = sizeof(binding);
                                            binding.unified_after = 2;
                                            binding.buffer_after = 1;
                                            binding.descriptor_count = 2;
                                            binding.descriptors[0] = {0, 0, 0, 1, 0, 1};
                                            binding.descriptors[1] = {1, 0, 1, 1, UINT32_MAX, 0};
                                            uint64_t buffer = 0, layout = 0, pipeline = 0;
                                            if (session.UploadSampledImage(image, source.data(), source.size() - 1,
                                                                           p.pitch * 4) != 60 ||
                                                session.UploadSampledImage(image, source.data(), source.size(),
                                                                           p.pitch * 4) ||
                                                session.CreateBuffer(initial.data(), bytes, 1, &buffer) ||
                                                ctr_video_compute_layout_create_v1(&session, &binding, &layout) ||
                                                ctr_video_compute_pipeline_create_v1(&session, layout, cs.data(),
                                                                                     static_cast<uint32_t>(cs.size()),
                                                                                     &pipeline)) {
                                                status = 240;
                                                break;
                                            }
                                            CtrGraphicsImageExtentV1 tiledExtent{
                                                1,   sizeof(tiledExtent), p.pitch,
                                                64u, p.guest_address,     uint64_t(p.pitch) * (64u) * 4};
                                            std::vector<unsigned char> padded(static_cast<size_t>(tiledExtent.bytes),
                                                                              0xd3),
                                                tiled;
                                            for (size_t i = 0; i < padded.size(); ++i)
                                                padded[i] = static_cast<unsigned char>(i * 37 + 11);
                                            for (uint32_t y = 0; y < p.height; ++y)
                                                std::memcpy(padded.data() + size_t(y) * p.pitch * 4,
                                                            source.data() + size_t(y) * p.width, size_t(p.width) * 4);
                                            if (ctr_with_tiling_pipelines(
                                                    device, deviceGet, tilingDirectory, true, true, true,
                                                    [&](VkPipelineLayout l, const VkPipeline* pipes,
                                                        VkDescriptorSetLayout d) -> int {
                                                        return ctr_thin_detile(device, deviceGet, q, memory, l,
                                                                               pipes[0], d, p.pitch,
                                                                               tiledExtent.padded_height, padded.data(),
                                                                               padded.size(), tiled, true, 4);
                                                    },
                                                    4)) {
                                                status = 241;
                                                break;
                                            }
                                            const auto preservedTiled = tiled;
                                            std::vector<unsigned char> tiledOutput(tiled.size() + 32, 0xa7);
                                            auto invalidExtent = tiledExtent;
                                            invalidExtent.padded_height += 64;
                                            if (session.UploadSampledTiled(image, tiled.data(), tiled.size() - 1,
                                                                           &tiledExtent) != 60 ||
                                                session.UploadSampledTiled(image, tiled.data(), tiled.size(),
                                                                           &invalidExtent) != 88 ||
                                                session.UploadSampledTiled(image, tiled.data(), tiled.size(),
                                                                           &tiledExtent)) {
                                                status = 241;
                                                break;
                                            }
                                            const auto untouchedOutput = tiledOutput;
                                            if (ctr_video_image_readback_tiled_v1(&session, image, tiled.data(),
                                                                                  tiled.size() - 1, &tiledExtent,
                                                                                  tiledOutput.data() + 16) != 60 ||
                                                tiledOutput != untouchedOutput ||
                                                ctr_video_image_readback_tiled_v1(&session, image, tiled.data(),
                                                                                  tiled.size(), &invalidExtent,
                                                                                  tiledOutput.data() + 16) != 88 ||
                                                tiledOutput != untouchedOutput ||
                                                ctr_video_image_readback_tiled_v1(&session, image, tiled.data(),
                                                                                  tiled.size(), &tiledExtent,
                                                                                  tiledOutput.data() + 16) ||
                                                std::memcmp(tiled.data(), tiledOutput.data() + 16, tiled.size()) ||
                                                tiled != preservedTiled) {
                                                status = 241;
                                                break;
                                            }
                                            for (size_t i = 0; i < 16; ++i)
                                                if (tiledOutput[i] != 0xa7 ||
                                                    tiledOutput[tiled.size() + 16 + i] != 0xa7)
                                                    status = 241;
                                            if (status) break;
                                            CtrGraphicsBufferWriteV1 write{0, 0, 1, 0, buffer};
                                            uint32_t imageBinding = 1, groups[] = {oneWidth / 8, 1, 1};
                                            CtrGraphicsPushDataV1 push{};
                                            push.version = 1;
                                            push.size = sizeof(push);
                                            if (session.WriteGraphicsBuffers(layout, &write, 1) ||
                                                session.WriteGraphicsImages(layout, &imageBinding, &image, 1) ||
                                                ctr_video_compute_dispatch_v1(&session, pipeline, &push, groups) ||
                                                session.CopyBufferInitial(buffer, output.data() + 16, bytes) ||
                                                session.ReadbackSampledImage(image, download.data(), download.size(),
                                                                             p.pitch * 4) ||
                                                download != source || source != unchanged) {
                                                status = 240;
                                                break;
                                            }
                                            for (size_t i = 0; i < oneWidth; ++i) {
                                                uint32_t actual[4]{};
                                                std::memcpy(actual, output.data() + 16 + i * 16, 16);
                                                if (actual[0] != 0x3f2aaaab || actual[1] != 0x3eaaaaab ||
                                                    actual[2] != 0 || actual[3] != 0x3f800000) {
                                                    status = 240;
                                                    break;
                                                }
                                            }
                                            for (size_t i = 0; i < 16; ++i)
                                                if (output[i] != 0xa7 || output[bytes + 16 + i] != 0xa7) status = 240;
                                            if (status) break;
                                            if (ctr_video_compute_pipeline_release_v1(&session, pipeline) ||
                                                session.ReleaseGraphicsLayout(layout) ||
                                                session.ReleaseBuffer(buffer) || session.ReleaseSampledImage(image)) {
                                                status = 240;
                                                break;
                                            }
                                            if (oneWidth == 1024)
                                                std::puts(
                                                    "RGBA8 sampled1D1024 actual texture1D BGRA4096 raw4096B tiled262144B guards32 padding preserved");
                                            if (oneWidth == 64)
                                                std::puts(
                                                    "RGBA8 sampled1D64 actual texture1D BGRA256 component words0diff raw256B0diff guards32 short60 invalid88 tiled32768B0diff padding32512B preserved");
                                        }
                                        for (const bool halfResolution : {false, true}) {
                                            const auto cs = load("build/CTRPort-data/compute_sampled_r32_control.spv");
                                            CtrGraphicsImageParametersV1 p{};
                                            p.version = 1;
                                            p.size = sizeof(p);
                                            p.type = 9;
                                            p.width = halfResolution ? 960u : 1920u;
                                            p.pitch = halfResolution ? 1024u : 1920u;
                                            p.height = halfResolution ? 540u : 1080u;
                                            p.depth = p.layers = p.levels = p.samples = 1;
                                            p.data_format = 4;
                                            p.number_format = 7;
                                            p.tile_mode = 14;
                                            p.guest_address = halfResolution ? 73329541120ULL : 73272328192ULL;
                                            p.swizzle[0] = 4;
                                            p.swizzle[3] = 1;
                                            uint64_t image = 999;
                                            auto bad = p;
                                            bad.number_format = 0;
                                            if (session.CreateSampledImage(&bad, &image) != 88 || image != 999) {
                                                status = 237;
                                                break;
                                            }
                                            bad = p;
                                            bad.swizzle[1] = 5;
                                            if (session.CreateSampledImage(&bad, &image) != 88 || image != 999 ||
                                                session.CreateSampledImage(&p, &image)) {
                                                status = 237;
                                                break;
                                            }
                                            for (uint32_t invalid = 0; invalid < 12; ++invalid) {
                                                auto unsupported = p;
                                                switch (invalid) {
                                                case 0: unsupported.width++; break;
                                                case 1: unsupported.height++; break;
                                                case 2: unsupported.pitch += 128; break;
                                                case 3: unsupported.tile_mode = 0; break;
                                                case 4: unsupported.pow2pad = 1; break;
                                                case 5: unsupported.compression = 1; break;
                                                case 6: unsupported.alt_tile = 1; break;
                                                case 7: unsupported.samples = 2; break;
                                                case 8: unsupported.layers = 2; break;
                                                case 9: unsupported.levels = 2; break;
                                                case 10: unsupported.base_level = 1; break;
                                                default: unsupported.depth = 2; break;
                                                }
                                                uint64_t untouched = 999;
                                                if (session.CreateSampledImage(&unsupported, &untouched) != 88 ||
                                                    untouched != 999)
                                                    status = 239;
                                            }
                                            if (status) break;
                                            const size_t pixels = size_t(p.width) * p.height, bytes = pixels * 16;
                                            std::vector<float> source(pixels);
                                            for (size_t i = 0; i < pixels; ++i) source[i] = float(i % 1024) / 1024.0f;
                                            const auto unchanged = source;
                                            std::vector<unsigned char> initial(bytes), output(bytes + 32, 0xa7);
                                            std::vector<float> downloaded(pixels);
                                            CtrGraphicsBindingContractV1 binding{};
                                            binding.version = 1;
                                            binding.size = sizeof(binding);
                                            binding.unified_after = 2;
                                            binding.buffer_after = 1;
                                            binding.descriptor_count = 2;
                                            binding.descriptors[0] = {0, 0, 0, 1, 0, 1};
                                            binding.descriptors[1] = {1, 0, 1, 1, UINT32_MAX, 0};
                                            uint64_t buffer = 0, layout = 0, pipeline = 0;
                                            if (session.UploadSampledImage(image, source.data(), source.size() * 4 - 1,
                                                                           uint64_t(p.width) * 4) != 60 ||
                                                session.UploadSampledImage(image, source.data(), source.size() * 4,
                                                                           uint64_t(p.width) * 4) ||
                                                session.CreateBuffer(initial.data(), bytes, 1, &buffer) ||
                                                ctr_video_compute_layout_create_v1(&session, &binding, &layout) ||
                                                ctr_video_compute_pipeline_create_v1(&session, layout, cs.data(),
                                                                                     static_cast<uint32_t>(cs.size()),
                                                                                     &pipeline)) {
                                                status = 237;
                                                break;
                                            }
                                            CtrGraphicsImageExtentV1 tiledExtent{
                                                1,
                                                sizeof(tiledExtent),
                                                p.pitch,
                                                halfResolution ? 576u : 1088u,
                                                p.guest_address,
                                                uint64_t(p.pitch) * (halfResolution ? 576u : 1088u) * 4};
                                            std::vector<unsigned char> padded(static_cast<size_t>(tiledExtent.bytes),
                                                                              0xd3),
                                                tiled;
                                            for (size_t i = 0; i < padded.size(); ++i)
                                                padded[i] = static_cast<unsigned char>(i * 37 + 11);
                                            for (uint32_t y = 0; y < p.height; ++y)
                                                std::memcpy(padded.data() + size_t(y) * p.pitch * 4,
                                                            source.data() + size_t(y) * p.width, size_t(p.width) * 4);
                                            if (ctr_with_tiling_pipelines(
                                                    device, deviceGet, tilingDirectory, true, true, true,
                                                    [&](VkPipelineLayout l, const VkPipeline* pipes,
                                                        VkDescriptorSetLayout d) -> int {
                                                        return ctr_thin_detile(device, deviceGet, q, memory, l,
                                                                               pipes[0], d, p.pitch,
                                                                               tiledExtent.padded_height, padded.data(),
                                                                               padded.size(), tiled, true, 4);
                                                    },
                                                    4)) {
                                                status = 238;
                                                break;
                                            }
                                            const auto preservedTiled = tiled;
                                            std::vector<unsigned char> tiledOutput(tiled.size() + 32, 0xa7);
                                            auto invalidExtent = tiledExtent;
                                            invalidExtent.padded_height += 64;
                                            if (session.UploadSampledTiled(image, tiled.data(), tiled.size() - 1,
                                                                           &tiledExtent) != 60 ||
                                                session.UploadSampledTiled(image, tiled.data(), tiled.size(),
                                                                           &invalidExtent) != 88 ||
                                                session.UploadSampledTiled(image, tiled.data(), tiled.size(),
                                                                           &tiledExtent)) {
                                                status = 238;
                                                break;
                                            }
                                            const auto untouchedOutput = tiledOutput;
                                            if (ctr_video_image_readback_tiled_v1(&session, image, tiled.data(),
                                                                                  tiled.size() - 1, &tiledExtent,
                                                                                  tiledOutput.data() + 16) != 60 ||
                                                tiledOutput != untouchedOutput ||
                                                ctr_video_image_readback_tiled_v1(&session, image, tiled.data(),
                                                                                  tiled.size(), &invalidExtent,
                                                                                  tiledOutput.data() + 16) != 88 ||
                                                tiledOutput != untouchedOutput ||
                                                ctr_video_image_readback_tiled_v1(&session, image, tiled.data(),
                                                                                  tiled.size(), &tiledExtent,
                                                                                  tiledOutput.data() + 16) ||
                                                std::memcmp(tiled.data(), tiledOutput.data() + 16, tiled.size()) ||
                                                tiled != preservedTiled) {
                                                status = 238;
                                                break;
                                            }
                                            for (size_t i = 0; i < 16; ++i)
                                                if (tiledOutput[i] != 0xa7 ||
                                                    tiledOutput[tiled.size() + 16 + i] != 0xa7)
                                                    status = 238;
                                            if (status) break;
                                            CtrGraphicsBufferWriteV1 write{0, 0, 1, 0, buffer};
                                            uint32_t imageBinding = 1, groups[] = {p.width / 8, p.height, 1};
                                            CtrGraphicsPushDataV1 push{};
                                            push.version = 1;
                                            push.size = sizeof(push);
                                            if (session.WriteGraphicsBuffers(layout, &write, 1) ||
                                                session.WriteGraphicsImages(layout, &imageBinding, &image, 1) ||
                                                ctr_video_compute_dispatch_v1(&session, pipeline, &push, groups) ||
                                                session.CopyBufferInitial(buffer, output.data() + 16, bytes) ||
                                                session.ReadbackSampledImage(image, downloaded.data(),
                                                                             source.size() * 4,
                                                                             uint64_t(p.width) * 4) ||
                                                downloaded != source || source != unchanged) {
                                                status = 237;
                                                break;
                                            }
                                            for (size_t i = 0; i < pixels; ++i) {
                                                uint32_t expected = 0, actual[4]{};
                                                std::memcpy(&expected, &source[i], 4);
                                                std::memcpy(actual, output.data() + 16 + i * 16, 16);
                                                if (actual[0] != expected || actual[1] || actual[2] ||
                                                    actual[3] != 0x3f800000) {
                                                    status = 237;
                                                    break;
                                                }
                                            }
                                            for (size_t i = 0; i < 16; ++i)
                                                if (output[i] != 0xa7 || output[bytes + 16 + i] != 0xa7) status = 237;
                                            if (status) break;
                                            if (ctr_video_compute_pipeline_release_v1(&session, pipeline) ||
                                                session.ReleaseGraphicsLayout(layout) ||
                                                session.ReleaseBuffer(buffer) || session.ReleaseSampledImage(image)) {
                                                status = 237;
                                                break;
                                            }
                                            if (halfResolution)
                                                std::puts(
                                                    "R32 sampled half960x540 pitch1024 padded576 tiled2359296B0diff padding285696B preserved all2073600componentWords0diff guards32 short60 extent88");
                                            else
                                                std::puts(
                                                    "R32 sampled tile14 composition actual upload readback retile8355840B0diff padding8rows61440B preserved guards32 short60 extent88 atomic full component probes0diff");
                                            std::puts(
                                                "R32 sampled exact admission twelve shape tiling and subresource negatives88 unchanged token; power2 padding explicitly unsupported");
                                            if (!halfResolution)
                                                std::puts(
                                                    "R32 sampled full1920x1080 format4 number7 actual view R_ZERO_ZERO_ONE all8294400 float component words0diff uploadReadback8294400B0diff guards32 unsupported88 short60 atomic no guest tiling claim");
                                        }
                                        {
                                            const auto cs = load("build/CTRPort-data/compute_sampled_control.spv");
                                            CtrGraphicsBindingContractV1 binding{};
                                            binding.version = 1;
                                            binding.size = sizeof(binding);
                                            binding.unified_after = 2;
                                            binding.buffer_after = 1;
                                            binding.descriptor_count = 2;
                                            binding.descriptors[0] = {0, 0, 0, 1, 0, 1};
                                            binding.descriptors[1] = {1, 0, 1, 1, UINT32_MAX, 0};
                                            std::array<uint32_t, 16> initial{}, nullReadback{};
                                            initial.fill(0xa5a5a5a5);
                                            uint64_t buffer = 0, layout = 0, pipeline = 0;
                                            if (session.CreateBuffer(initial.data(), sizeof(initial), 1, &buffer) ||
                                                ctr_video_compute_layout_create_v1(&session, &binding, &layout) ||
                                                ctr_video_compute_pipeline_create_v1(&session, layout, cs.data(),
                                                                                     static_cast<uint32_t>(cs.size()),
                                                                                     &pipeline)) {
                                                status = 241;
                                                break;
                                            }
                                            CtrGraphicsBufferWriteV1 write{0, 0, 1, 0, buffer};
                                            uint32_t ib = 1, semanticNull = 1;
                                            uint64_t zero = 0, stale = UINT64_MAX;
                                            session.SetNullDescriptorAuthority(false);
                                            if (ctr_video_graphics_layout_images_null_v1(&session, layout, &ib, &zero,
                                                                                         &semanticNull, 1) != 88) {
                                                status = 241;
                                                break;
                                            }
                                            session.SetNullDescriptorAuthority(enabledRobust.nullDescriptor == VK_TRUE);
                                            if (!enabledRobust.nullDescriptor) {
                                                status = 241;
                                                break;
                                            }
                                            semanticNull = 2;
                                            if (ctr_video_graphics_layout_images_null_v1(&session, layout, &ib, &zero,
                                                                                         &semanticNull, 1) != 60) {
                                                status = 241;
                                                break;
                                            }
                                            semanticNull = 0;
                                            if (ctr_video_graphics_layout_images_null_v1(&session, layout, &ib, &stale,
                                                                                         &semanticNull, 1) != 62 ||
                                                session.WriteGraphicsImages(layout, &ib, &zero, 1) != 62) {
                                                status = 241;
                                                break;
                                            }
                                            semanticNull = 1;
                                            if (ctr_video_graphics_layout_images_null_v1(&session, layout, &ib, &stale,
                                                                                         &semanticNull, 1) != 60 ||
                                                session.WriteGraphicsBuffers(layout, &write, 1) ||
                                                ctr_video_graphics_layout_images_null_v1(&session, layout, &ib, &zero,
                                                                                         &semanticNull, 1)) {
                                                status = 241;
                                                break;
                                            }
                                            CtrGraphicsPushDataV1 push{};
                                            push.version = 1;
                                            push.size = sizeof(push);
                                            uint32_t bias = 123;
                                            std::memcpy(push.bytes, &bias, 4);
                                            uint32_t groups[] = {2, 1, 1};
                                            if (ctr_video_compute_dispatch_v1(&session, pipeline, &push, groups) ||
                                                session.CopyBufferInitial(buffer, nullReadback.data(),
                                                                          sizeof(nullReadback))) {
                                                status = 241;
                                                break;
                                            }
                                            for (auto value : nullReadback)
                                                if (value != 123) status = 241;
                                            if (status) break;
                                            if (ctr_video_graphics_layout_images_null_v1(&session, layout, &ib, &zero,
                                                                                         &semanticNull, 1) != 91 ||
                                                ctr_video_compute_pipeline_release_v1(&session, pipeline) ||
                                                session.ReleaseGraphicsLayout(layout) ||
                                                session.ReleaseBuffer(buffer)) {
                                                status = 241;
                                                break;
                                            }
                                            std::puts(
                                                "null sampled explicit capability real GPU texelFetch16 zero values bias123 fence readback0diff legacyZero62 stale62 malformed60 disabled88 rewrite91 no dummy image");
                                        }
                                        for (const uint32_t sampledCase : {10u, 3u, 30u}) {
                                            const uint32_t sampledFormat = sampledCase == 30 ? 3u : sampledCase;
                                            const bool green = sampledCase == 30;
                                            const uint32_t texelBytes = sampledFormat == 3 ? 2u : 4u;
                                            const auto cs = load("build/CTRPort-data/compute_sampled_control.spv");
                                            CtrGraphicsBindingContractV1 binding{};
                                            binding.version = 1;
                                            binding.size = sizeof(binding);
                                            binding.unified_after = 2;
                                            binding.buffer_after = 1;
                                            binding.descriptor_count = 2;
                                            binding.descriptors[0] = {0, 0, 0, 1, 0, 1};
                                            binding.descriptors[1] = {1, 0, 1, 1, UINT32_MAX, 0};
                                            std::array<uint32_t, 16> source{}, computeReadback{};
                                            source.fill(0xa5a5a5a5);
                                            uint64_t buffer = 0, computeLayout = 0, computePipeline = 0, image = 0;
                                            CtrGraphicsImageParametersV1 sampledParams{};
                                            sampledParams.version = 1;
                                            sampledParams.size = sizeof(sampledParams);
                                            sampledParams.type = 9;
                                            sampledParams.width = sampledFormat == 3 ? 1920u : 16u;
                                            sampledParams.height = sampledFormat == 3 ? 1080u : 1u;
                                            sampledParams.pitch = sampledParams.width;
                                            sampledParams.depth = sampledParams.layers = sampledParams.levels =
                                                sampledParams.samples = 1;
                                            sampledParams.data_format = sampledFormat;
                                            sampledParams.swizzle[0] = green ? 5u : 4u;
                                            sampledParams.swizzle[1] = 5;
                                            sampledParams.swizzle[2] = 6;
                                            sampledParams.swizzle[3] = 7;
                                            std::vector<unsigned char> pixels(size_t(sampledParams.width) *
                                                                              sampledParams.height * texelBytes);
                                            for (size_t i = 0; i < pixels.size() / texelBytes; ++i) {
                                                pixels[i * texelBytes] = static_cast<unsigned char>(i);
                                                if (sampledFormat == 3)
                                                    pixels[i * texelBytes + 1] = static_cast<unsigned char>(i * 7 + 3);
                                            }
                                            if (session.CreateSampledImage(&sampledParams, &image) ||
                                                session.UploadSampledImage(image, pixels.data(), pixels.size(),
                                                                           sampledParams.pitch * texelBytes)) {
                                                status = 200;
                                                break;
                                            }
                                            if (session.CreateBuffer(source.data(), sizeof(source), 1, &buffer) ||
                                                ctr_video_compute_layout_create_v1(&session, &binding,
                                                                                   &computeLayout) ||
                                                ctr_video_compute_pipeline_create_v1(&session, computeLayout, cs.data(),
                                                                                     static_cast<uint32_t>(cs.size()),
                                                                                     &computePipeline)) {
                                                status = 200;
                                                break;
                                            }
                                            CtrGraphicsPushDataV1 push{};
                                            push.version = 1;
                                            push.size = sizeof(push);
                                            uint32_t value = 123;
                                            std::memcpy(push.bytes, &value, 4);
                                            uint32_t groups[] = {2, 1, 1};
                                            if (ctr_video_compute_dispatch_v1(&session, computePipeline, &push,
                                                                              groups) != 88) {
                                                status = 200;
                                                break;
                                            }
                                            CtrGraphicsBufferWriteV1 write{0, 0, 1, 0, buffer};
                                            uint32_t imageBinding = 1;
                                            if (session.WriteGraphicsBuffers(computeLayout, &write, 1) ||
                                                ctr_video_compute_dispatch_v1(&session, computePipeline, &push,
                                                                              groups) != 88 ||
                                                session.WriteGraphicsImages(computeLayout, &imageBinding, &image, 1) ||
                                                ctr_video_compute_dispatch_v1(&session, computePipeline, &push,
                                                                              groups) ||
                                                session.CopyBufferInitial(buffer, computeReadback.data(),
                                                                          sizeof(computeReadback))) {
                                                status = 200;
                                                break;
                                            }
                                            for (uint32_t i = 0; i < 16; ++i)
                                                if (computeReadback[i] != 123 + (green ? i * 7 + 3 : i)) status = 200;
                                            if (status) break;
                                            groups[0] = 0;
                                            if (ctr_video_compute_dispatch_v1(&session, computePipeline, &push,
                                                                              groups) != 88) {
                                                status = 200;
                                                break;
                                            }
                                            groups[0] = 2;
                                            value = 456;
                                            std::memcpy(push.bytes, &value, 4);
                                            for (size_t i = 0; i < pixels.size() / texelBytes; ++i) {
                                                pixels[i * texelBytes] = static_cast<unsigned char>(i + 17);
                                                if (sampledFormat == 3)
                                                    pixels[i * texelBytes + 1] = static_cast<unsigned char>(i * 7 + 20);
                                            }
                                            if (session.UploadSampledImage(image, pixels.data(), pixels.size(),
                                                                           sampledParams.pitch * texelBytes)) {
                                                status = 200;
                                                break;
                                            }
                                            if (ctr_video_compute_dispatch_v1(&session, computePipeline, &push,
                                                                              groups) ||
                                                session.CopyBufferInitial(buffer, computeReadback.data(),
                                                                          sizeof(computeReadback))) {
                                                status = 200;
                                                break;
                                            }
                                            for (uint32_t i = 0; i < 16; ++i)
                                                if (computeReadback[i] != 456 + (green ? i * 7 + 20 : i + 17))
                                                    status = 200;
                                            if (status) break;
                                            if (sampledFormat == 3) {
                                                std::vector<unsigned char> downloaded(pixels.size());
                                                if (session.ReadbackSampledImage(image, downloaded.data(),
                                                                                 downloaded.size(),
                                                                                 sampledParams.pitch * texelBytes) ||
                                                    downloaded != pixels) {
                                                    status = 218;
                                                    break;
                                                }
                                            }
                                            if (ctr_video_compute_pipeline_release_v1(&session, computePipeline) ||
                                                session.ReleaseGraphicsLayout(computeLayout) ||
                                                session.ReleaseBuffer(buffer) || session.ReleaseSampledImage(image)) {
                                                status = 200;
                                                break;
                                            }
                                            if (sampledFormat == 3) {
                                                if (!green)
                                                    std::puts(
                                                        "RG8 sampled actual texelFetch two GPUdispatches16words0diff changed upload retained descriptor; real2byte transfer");
                                                else
                                                    std::puts(
                                                        "RG8 full1920x1080 both channel swizzle probes and4147200B actual upload/readback0diff");
                                            } else
                                                std::puts(
                                                    "compute sampled texelFetch -> SSBO actual two dispatches, changed image upload after binding observed,16words0diff; unbound image88 no storageimage/guestwriteback");
                                        }
                                        {
                                            const auto cs = load("build/CTRPort-data/compute_storage_control.spv");
                                            CtrGraphicsBindingContractV1 binding{};
                                            binding.version = 1;
                                            binding.size = sizeof(binding);
                                            binding.unified_after = 2;
                                            binding.buffer_after = 1;
                                            binding.descriptor_count = 2;
                                            binding.descriptors[0] = {0, 0, 0, 1, 0, 1};
                                            binding.descriptors[1] = {1, 0, 1, 1, UINT32_MAX, 1};
                                            std::array<uint32_t, 16> source{};
                                            for (uint32_t i = 0; i < 16; ++i) source[i] = i;
                                            uint64_t buffer = 0, computeLayout = 0, computePipeline = 0, image = 0;
                                            CtrGraphicsImageParametersV1 sampledParams{};
                                            sampledParams.version = 1;
                                            sampledParams.size = sizeof(sampledParams);
                                            sampledParams.type = 9;
                                            sampledParams.width = 16;
                                            sampledParams.height = 1;
                                            sampledParams.pitch = 128;
                                            sampledParams.tile_mode = 14;
                                            sampledParams.guest_address = 1;
                                            sampledParams.depth = sampledParams.layers = sampledParams.levels =
                                                sampledParams.samples = 1;
                                            sampledParams.data_format = 4;
                                            sampledParams.number_format = 7;
                                            std::array<float, 16> pixels{};
                                            pixels.fill(-1);
                                            if (ctr_video_storage_image_create_v1(&session, &sampledParams, &image) ||
                                                session.UploadSampledImage(image, pixels.data(), sizeof(pixels), 64)) {
                                                status = 201;
                                                break;
                                            }
                                            if (session.CreateBuffer(source.data(), sizeof(source), 1, &buffer) ||
                                                ctr_video_compute_layout_create_v1(&session, &binding,
                                                                                   &computeLayout) ||
                                                ctr_video_compute_pipeline_create_v1(&session, computeLayout, cs.data(),
                                                                                     static_cast<uint32_t>(cs.size()),
                                                                                     &computePipeline)) {
                                                status = 201;
                                                break;
                                            }
                                            CtrGraphicsPushDataV1 push{};
                                            push.version = 1;
                                            push.size = sizeof(push);
                                            uint32_t value = 123;
                                            std::memcpy(push.bytes, &value, 4);
                                            uint32_t groups[] = {2, 1, 1};
                                            if (ctr_video_compute_dispatch_v1(&session, computePipeline, &push,
                                                                              groups) != 88) {
                                                status = 201;
                                                break;
                                            }
                                            CtrGraphicsBufferWriteV1 write{0, 0, 1, 0, buffer};
                                            uint32_t imageBinding = 1;
                                            if (session.WriteGraphicsBuffers(computeLayout, &write, 1) ||
                                                ctr_video_compute_dispatch_v1(&session, computePipeline, &push,
                                                                              groups) != 88 ||
                                                session.WriteGraphicsImages(computeLayout, &imageBinding, &image, 1) ||
                                                ctr_video_compute_dispatch_v1(&session, computePipeline, &push,
                                                                              groups) ||
                                                session.ReadbackSampledImage(image, pixels.data(), sizeof(pixels),
                                                                             64)) {
                                                status = 201;
                                                break;
                                            }
                                            for (uint32_t i = 0; i < 16; ++i)
                                                if (pixels[i] != float(123 + i)) status = 201;
                                            if (status) break;
                                            groups[0] = 0;
                                            if (ctr_video_compute_dispatch_v1(&session, computePipeline, &push,
                                                                              groups) != 88) {
                                                status = 201;
                                                break;
                                            }
                                            groups[0] = 2;
                                            value = 456;
                                            std::memcpy(push.bytes, &value, 4);
                                            pixels.fill(-7);
                                            if (session.UploadSampledImage(image, pixels.data(), sizeof(pixels), 64)) {
                                                status = 201;
                                                break;
                                            }
                                            if (ctr_video_compute_dispatch_v1(&session, computePipeline, &push,
                                                                              groups) ||
                                                session.ReadbackSampledImage(image, pixels.data(), sizeof(pixels),
                                                                             64)) {
                                                status = 201;
                                                break;
                                            }
                                            for (uint32_t i = 0; i < 16; ++i)
                                                if (pixels[i] != float(456 + i)) status = 201;
                                            if (status) break;
                                            {
                                                std::vector<unsigned char> prior(128 * 64 * 4),
                                                    output(prior.size(), 0xd3);
                                                for (size_t i = 0; i < prior.size(); ++i)
                                                    prior[i] = static_cast<unsigned char>(i * 37 + 11);
                                                CtrGraphicsImageExtentV1 extent{1, sizeof(extent), 128, 64,
                                                                                1, prior.size()};
                                                const auto unchanged = output;
                                                if (ctr_video_image_readback_tiled_v1(&session, image, prior.data(),
                                                                                      prior.size() - 1, &extent,
                                                                                      output.data()) != 60 ||
                                                    output != unchanged ||
                                                    ctr_video_image_readback_tiled_v1(&session, image, prior.data(),
                                                                                      prior.size(), &extent,
                                                                                      output.data())) {
                                                    status = 202;
                                                    break;
                                                }
                                                if (ctr_with_tiling_pipelines(
                                                        device, deviceGet, tilingDirectory, true, true, true,
                                                        [&](VkPipelineLayout tiledLayout, const VkPipeline* pipelines,
                                                            VkDescriptorSetLayout descriptorLayout) -> int {
                                                            std::vector<unsigned char> originalLinear, actualLinear;
                                                            int check = ctr_thin_detile(
                                                                device, deviceGet, q, memory, tiledLayout, pipelines[1],
                                                                descriptorLayout, 128, 64, prior.data(), prior.size(),
                                                                originalLinear);
                                                            if (check) return check;
                                                            check = ctr_thin_detile(
                                                                device, deviceGet, q, memory, tiledLayout, pipelines[1],
                                                                descriptorLayout, 128, 64, output.data(), output.size(),
                                                                actualLinear);
                                                            if (check) return check;
                                                            for (size_t i = 0; i < actualLinear.size(); ++i) {
                                                                if (i < 64) {
                                                                    if (actualLinear[i] !=
                                                                        reinterpret_cast<const unsigned char*>(
                                                                            pixels.data())[i])
                                                                        return 202;
                                                                } else if (actualLinear[i] != originalLinear[i])
                                                                    return 202;
                                                            }
                                                            return 0;
                                                        })) {
                                                    status = 202;
                                                    break;
                                                }
                                                pixels.fill(-9);
                                                if (session.UploadSampledTiled(image, output.data(), output.size(),
                                                                               &extent) ||
                                                    session.ReadbackSampledImage(image, pixels.data(), sizeof(pixels),
                                                                                 64)) {
                                                    status = 202;
                                                    break;
                                                }
                                                for (uint32_t i = 0; i < 16; ++i)
                                                    if (pixels[i] != float(456 + i)) status = 202;
                                                if (status) break;
                                                std::puts(
                                                    "storage tile14 real GPU readback/retile preserves padded128x64 bytes outside16x1; independent public detile compares64writtenbytes/allpadding0diff; short60 atomic");
                                            }
                                            if (ctr_video_compute_pipeline_release_v1(&session, computePipeline) ||
                                                session.ReleaseGraphicsLayout(computeLayout) ||
                                                session.ReleaseBuffer(buffer) || session.ReleaseSampledImage(image)) {
                                                status = 201;
                                                break;
                                            }
                                            std::puts(
                                                "compute actual R32float STORAGE GENERAL image writes123+i then456+i, two fenced dispatches and linear float readbacks0diff, upload/readback restoresGENERAL; unbound88 no guestwriteback");
                                        }
                                        {
                                            CtrGraphicsKeyV1 key{};
                                            key.version = 1;
                                            key.size = sizeof(key);
                                            key.primitive_type = 4;
                                            key.polygon_mode = 2;
                                            {
                                                CtrGraphicsColorRequirementV1 r32{
                                                    1, sizeof(r32), 0, 1920, 1088, 1, 1, 14, 4, 7, 1, 8355840};
                                                uint64_t r32Token = 0;
                                                std::vector<uint32_t> source(1920u * 1088u), download(source.size(), 0);
                                                for (size_t i = 0; i < source.size(); ++i)
                                                    source[i] = 0x3f000000u + static_cast<uint32_t>(i % 65536);
                                                if (session.CreateColorAttachment(&r32, 0, &r32Token) ||
                                                    session.TransferColorAttachment(r32Token, source.data(), nullptr,
                                                                                    8355840, 7680) ||
                                                    session.TransferColorAttachment(r32Token, nullptr, download.data(),
                                                                                    8355840, 7680) ||
                                                    source != download || session.ReleaseColorAttachment(r32Token)) {
                                                    status = 220;
                                                    break;
                                                }
                                                r32.number_format = 0;
                                                r32Token = 999;
                                                if (session.CreateColorAttachment(&r32, 0, &r32Token) != 88 ||
                                                    r32Token != 999) {
                                                    status = 220;
                                                    break;
                                                }
                                                std::puts(
                                                    "R32float color actual1920x1088 upload/readback8355840B0diff unsupported number88 atomic");
                                            }
                                            key.logic_op = 0xcc;
                                            key.num_color_attachments = 2;
                                            key.mrt_mask = 3;
                                            key.num_samples = 1;
                                            key.cb_shader_mask = 0x3f;
                                            key.colors[0].write_mask = 15;
                                            key.colors[1].write_mask = 3;
                                            key.colors[0].samples = key.colors[1].samples = 1;
                                            CtrGraphicsVertexInputsV1 inputs{};
                                            inputs.version = 1;
                                            inputs.size = sizeof(inputs);
                                            CtrGraphicsPipelineState state;
                                            if (state.Prepare(key, inputs, nullptr, VK_FORMAT_R8G8B8A8_UNORM,
                                                              graphicsCoreMask, 0, VK_FORMAT_UNDEFINED,
                                                              VK_FORMAT_R8G8_UNORM) ||
                                                state.rendering.colorAttachmentCount != 2 ||
                                                state.blend.attachmentCount != 2 ||
                                                state.rendering.pColorAttachmentFormats[1] != VK_FORMAT_R8G8_UNORM ||
                                                state.blend.pAttachments[0].colorWriteMask != 15 ||
                                                state.blend.pAttachments[1].colorWriteMask != 3) {
                                                status = 206;
                                                break;
                                            }
                                            if (state.Prepare(key, inputs, nullptr, VK_FORMAT_R8G8B8A8_UNORM,
                                                              graphicsCoreMask & ~16u, 0, VK_FORMAT_UNDEFINED,
                                                              VK_FORMAT_R8G8_UNORM) != 88) {
                                                status = 206;
                                                break;
                                            }
                                            key.colors[0].data_format = 10;
                                            key.colors[1].data_format = 3;
                                            CtrGraphicsBindingContractV1 empty{};
                                            empty.version = 1;
                                            empty.size = sizeof(empty);
                                            uint64_t mrtLayout = 0, mrtPipeline = 999;
                                            uint64_t colorTokens[2]{};
                                            CtrGraphicsColorRequirementV1 demand{
                                                1, sizeof(demand), 0, 1920, 1088, 1, 1, 14, 10, 0, 1, 8355840};
                                            if (session.CreateGraphicsLayout(&empty, &empty, &mrtLayout) ||
                                                session.CreateColorAttachment(&demand, 0, &colorTokens[0])) {
                                                status = 207;
                                                break;
                                            }
                                            demand.slot = 1;
                                            demand.data_format = 3;
                                            demand.height = 1152;
                                            demand.bytes = 4423680;
                                            demand.guest_address = 2;
                                            const auto
                                                mrtVs = load(
                                                    "build/CTRPort-data/tiling-glslang-ninja/pipeline_control.vert.spv"),
                                                mrtPs = load(
                                                    "build/CTRPort-data/tiling-glslang-ninja/pipeline_control.frag.spv");
                                            if (session.CreateColorAttachment(&demand, 0, &colorTokens[1]) ||
                                                ctr_video_graphics_mrt_pipeline_create_v1(
                                                    &session, &key, &inputs, nullptr, 0, mrtLayout, colorTokens, 2,
                                                    mrtVs.data(), static_cast<uint32_t>(mrtVs.size()), mrtPs.data(),
                                                    static_cast<uint32_t>(mrtPs.size()), 0, &mrtPipeline)) {
                                                status = 207;
                                                break;
                                            }
                                            if (session.AllocateColorSampleBacking(colorTokens[0]) ||
                                                session.AllocateColorSampleBacking(colorTokens[1]) ||
                                                session.AllocateColorSampleBacking(colorTokens[0]) ||
                                                session.AllocateColorSampleBacking(0) != 60 ||
                                                session.AllocateColorSampleBacking(UINT64_MAX) != 62) {
                                                status = 208;
                                                break;
                                            }
                                            const auto
                                                copyVs =
                                                    load("build/CTRPort-data/tiling-glslang-ninja/ms_copy_vertex.spv"),
                                                copySingle = load(
                                                    "build/CTRPort-data/tiling-glslang-ninja/ms_copy_from_single.spv"),
                                                copyMulti = load(
                                                    "build/CTRPort-data/tiling-glslang-ninja/ms_copy_from_multi.spv");
                                            {
                                                uint64_t pair[2]{}, pairPipeline = 0;
                                                auto pairKey = key;
                                                pairKey.cb_shader_mask = 0xf1;
                                                pairKey.colors[0].data_format = 4;
                                                pairKey.colors[0].number_format = 7;
                                                pairKey.colors[0].write_mask = 1;
                                                pairKey.colors[1].data_format = 10;
                                                pairKey.colors[1].number_format = 0;
                                                pairKey.colors[1].write_mask = 15;
                                                CtrGraphicsColorRequirementV1 req{
                                                    1, sizeof(req), 0, 1920, 1088, 1, 1, 14, 4, 7, 1, 8355840};
                                                std::vector<uint32_t> prior(1920u * 1088u, 0x3f000000),
                                                    after(prior.size());
                                                if (session.CreateColorAttachment(&req, 0, &pair[0]) ||
                                                    session.TransferColorAttachment(pair[0], prior.data(), nullptr,
                                                                                    8355840, 7680)) {
                                                    status = 222;
                                                    break;
                                                }
                                                req.slot = 1;
                                                req.data_format = 10;
                                                req.number_format = 0;
                                                req.guest_address = 2;
                                                if (session.CreateColorAttachment(&req, 0, &pair[1]) ||
                                                    session.TransferColorAttachment(pair[1], prior.data(), nullptr,
                                                                                    8355840, 7680)) {
                                                    status = 222;
                                                    break;
                                                }
                                                const auto dualPs = load("build/CTRPort-data/mrt_r32_control.frag.spv");
                                                if (ctr_video_graphics_mrt_pipeline_create_v1(
                                                        &session, &pairKey, &inputs, nullptr, 0, mrtLayout, pair, 2,
                                                        copyVs.data(), static_cast<uint32_t>(copyVs.size()),
                                                        dualPs.data(), static_cast<uint32_t>(dualPs.size()), 0,
                                                        &pairPipeline)) {
                                                    status = 222;
                                                    break;
                                                }
                                                CtrGraphicsViewportStateV1 vp{};
                                                vp.version = 1;
                                                vp.size = sizeof(vp);
                                                vp.count = 1;
                                                vp.viewports[0] = {0, 0, 1920, 1080, 0, 1};
                                                vp.scissors[0].extent = {1920, 1080};
                                                CtrGraphicsDynamicStateV1 ds{};
                                                ds.version = 1;
                                                ds.size = sizeof(ds);
                                                ds.line_width = 1;
                                                CtrGraphicsPushDataV1 push{};
                                                push.version = 1;
                                                push.size = sizeof(push);
                                                CtrGraphicsDrawV1 draw{};
                                                draw.version = 1;
                                                draw.size = sizeof(draw);
                                                draw.vertex_count = 3;
                                                draw.instance_count = 1;
                                                if (ctr_video_graphics_mrt_draw_v1(&session, pairPipeline, &vp, &ds,
                                                                                   &push, &draw)) {
                                                    status = 222;
                                                    break;
                                                }
                                                for (uint32_t a = 0; a < 2; ++a) {
                                                    if (session.TransferColorAttachment(pair[a], nullptr, after.data(),
                                                                                        8355840, 7680)) {
                                                        status = 222;
                                                        break;
                                                    }
                                                    for (size_t i = 0; i < after.size(); ++i)
                                                        if (after[i] != (i < 1920u * 1080u
                                                                             ? (a ? 0xffc08040u : 0x3e800000u)
                                                                             : prior[i])) {
                                                            status = 222;
                                                            break;
                                                        }
                                                }
                                                if (status) break;
                                                if (session.ReleaseColorAttachment(pair[0]) ||
                                                    session.ReleaseColorAttachment(pair[1]) ||
                                                    ctr_video_graphics_pipeline_release_v1(&session, pairPipeline)) {
                                                    status = 222;
                                                    break;
                                                }
                                                std::puts(
                                                    "MRT2 R32float/RGBA8 actual1920x1080 draw both8355840B targets0diff padded8rows retained");
                                            }
                                            {
                                                auto singleKey = key;
                                                singleKey.num_color_attachments = singleKey.mrt_mask = 1;
                                                singleKey.cb_shader_mask = 15;
                                                CtrGraphicsBindingContractV1 ps{};
                                                ps.version = 1;
                                                ps.size = sizeof(ps);
                                                ps.unified_after = 2;
                                                ps.descriptor_count = 2;
                                                ps.descriptors[0] = {1, 0, 0, 1, UINT32_MAX, 0};
                                                ps.descriptors[1] = {2, 0, 1, 1, UINT32_MAX, 0};
                                                auto vsContract = empty;
                                                vsContract.unified_before = vsContract.unified_after = 2;
                                                uint64_t sampledLayout = 0, sampledPipeline = 0, image = 0, sampler = 0;
                                                CtrGraphicsImageParametersV1 ip{};
                                                ip.version = 1;
                                                ip.size = sizeof(ip);
                                                ip.type = 9;
                                                ip.width = ip.height = ip.pitch = 1;
                                                ip.depth = ip.layers = ip.levels = ip.samples = 1;
                                                ip.data_format = 10;
                                                for (uint32_t c = 0; c < 4; ++c) ip.swizzle[c] = 4 + c;
                                                CtrGraphicsSamplerParametersV1 sp{};
                                                sp.version = 1;
                                                sp.size = sizeof(sp);
                                                sp.max_anisotropy = 1;
                                                const auto fp =
                                                    load("build/CTRPort-data/sampled_draw_control.frag.spv");
                                                const auto traced = [](const char* name, int rc) {
                                                    std::printf("sampled draw phase %s rc=%d\n", name, rc);
                                                    return rc;
                                                };
                                                if (traced("layout", session.CreateGraphicsLayout(&ps, &vsContract,
                                                                                                  &sampledLayout)) ||
                                                    traced("image", session.CreateSampledImage(&ip, &image)) ||
                                                    traced("sampler", session.CreateSampler(&sp, &sampler)) ||
                                                    ctr_video_graphics_pipeline_create_v1(
                                                        &session, &singleKey, &inputs, nullptr, 0, sampledLayout,
                                                        colorTokens[0], copyVs.data(),
                                                        static_cast<uint32_t>(copyVs.size()), fp.data(),
                                                        static_cast<uint32_t>(fp.size()), 0, &sampledPipeline)) {
                                                    status = 223;
                                                    break;
                                                }
                                                CtrGraphicsViewportStateV1 vp{};
                                                vp.version = 1;
                                                vp.size = sizeof(vp);
                                                vp.count = 1;
                                                vp.viewports[0] = {0, 0, 64, 64, 0, 1};
                                                vp.scissors[0].extent = {64, 64};
                                                CtrGraphicsDynamicStateV1 ds{};
                                                ds.version = 1;
                                                ds.size = sizeof(ds);
                                                ds.line_width = 1;
                                                CtrGraphicsPushDataV1 push{};
                                                push.version = 1;
                                                push.size = sizeof(push);
                                                CtrGraphicsDrawV1 draw{};
                                                draw.version = 1;
                                                draw.size = sizeof(draw);
                                                draw.vertex_count = 3;
                                                draw.instance_count = 1;
                                                std::printf("sampled draw pipeline created\n");
                                                if (ctr_video_graphics_draw_v1(&session, sampledPipeline,
                                                                               colorTokens[0], &vp, &ds, &push,
                                                                               &draw) != 88) {
                                                    status = 223;
                                                    break;
                                                }
                                                std::vector<unsigned char> initialColor(8355840, 0);
                                                if (session.TransferColorAttachment(colorTokens[0], initialColor.data(),
                                                                                    nullptr, initialColor.size(),
                                                                                    7680)) {
                                                    status = 223;
                                                    break;
                                                }
                                                uint32_t ib = 0, sb = 1;
                                                const unsigned char pixel[] = {32, 96, 160, 255};
                                                if (traced("upload", session.UploadSampledImage(image, pixel, 4, 4)) ||
                                                    traced("images", session.WriteGraphicsImages(sampledLayout, &ib,
                                                                                                 &image, 1)) ||
                                                    ctr_video_graphics_layout_samplers_v1(&session, sampledLayout, &sb,
                                                                                          &sampler, 1) ||
                                                    session.ReleaseSampler(sampler) ||
                                                    traced("draw", ctr_video_graphics_draw_v1(&session, sampledPipeline,
                                                                                              colorTokens[0], &vp, &ds,
                                                                                              &push, &draw))) {
                                                    status = 223;
                                                    break;
                                                }
                                                std::vector<unsigned char> after(8355840);
                                                if (session.TransferColorAttachment(colorTokens[0], nullptr,
                                                                                    after.data(), after.size(), 7680)) {
                                                    status = 223;
                                                    break;
                                                }
                                                for (uint32_t y = 0; y < 64; ++y)
                                                    for (uint32_t x = 0; x < 64; ++x)
                                                        for (uint32_t c = 0; c < 4; ++c)
                                                            if (after[size_t(y) * 7680 + x * 4 + c] != pixel[c])
                                                                status = 223;
                                                const unsigned char changedPixel[] = {224, 128, 64, 255};
                                                if (status || session.UploadSampledImage(image, changedPixel, 4, 4) ||
                                                    session.ReleaseSampledImage(image) ||
                                                    ctr_video_graphics_draw_v1(&session, sampledPipeline,
                                                                               colorTokens[0], &vp, &ds, &push,
                                                                               &draw) ||
                                                    session.TransferColorAttachment(colorTokens[0], nullptr,
                                                                                    after.data(), after.size(), 7680)) {
                                                    status = 223;
                                                    break;
                                                }
                                                for (uint32_t y = 0; y < 64; ++y)
                                                    for (uint32_t x = 0; x < 64; ++x)
                                                        for (uint32_t c = 0; c < 4; ++c)
                                                            if (after[size_t(y) * 7680 + x * 4 + c] != changedPixel[c])
                                                                status = 223;
                                                if (status) break;
                                                std::puts(
                                                    "sampled graphics changed upload after binding second draw16384B0diff image+sampler tokens released");
                                                auto noKey = singleKey;
                                                for (auto& c : noKey.colors) {
                                                    c.data_format = 0;
                                                    c.write_mask = 0;
                                                }
                                                noKey.z_format = 1;
                                                uint64_t noPipeline = 0;
                                                if (ctr_video_graphics_no_attachment_pipeline_create_v1(
                                                        &session, &noKey, &inputs, nullptr, 0, sampledLayout,
                                                        copyVs.data(), static_cast<uint32_t>(copyVs.size()), fp.data(),
                                                        static_cast<uint32_t>(fp.size()), &noPipeline)) {
                                                    status = 224;
                                                    break;
                                                }
                                                vp.viewports[0] = {0, 1024, 1024, -1024, 0, 1};
                                                vp.scissors[0].extent = {1024, 1024};
                                                uint64_t visible = 0;
                                                if (session.DrawGraphics(noPipeline, 0, &vp, &ds, &push, &draw, true,
                                                                         &visible) ||
                                                    !visible ||
                                                    ctr_video_graphics_no_attachment_draw_v1(&session, noPipeline, &vp,
                                                                                             &ds, &push, &draw)) {
                                                    status = 224;
                                                    break;
                                                }
                                                ds.depth_write = 1;
                                                if (ctr_video_graphics_no_attachment_draw_v1(&session, noPipeline, &vp,
                                                                                             &ds, &push, &draw) != 88) {
                                                    status = 224;
                                                    break;
                                                }
                                                ds.depth_write = 0;
                                                if (session.ReleaseGraphicsPipeline(noPipeline)) {
                                                    status = 224;
                                                    break;
                                                }
                                                std::puts(
                                                    "No attachment actual1024x1024 sampled VS/FS draw positiveOcclusion realFence CABI0 depthWrite88 unchangedStaleZ1");
                                                if (status ||
                                                    ctr_video_graphics_pipeline_release_v1(&session, sampledPipeline) ||
                                                    session.ReleaseGraphicsLayout(sampledLayout)) {
                                                    status = 223;
                                                    break;
                                                }
                                                std::puts(
                                                    "sampled single-color actual64x64 draw image+sampler retained after release16384B0diff unwritten88");
                                            }
                                            for (uint32_t nullFilter : {0u, 1u}) {
                                                auto nullKey = key;
                                                nullKey.num_color_attachments = nullKey.mrt_mask = 1;
                                                nullKey.cb_shader_mask = 15;
                                                CtrGraphicsBindingContractV1 nullPs{};
                                                nullPs.version = 1;
                                                nullPs.size = sizeof(nullPs);
                                                nullPs.unified_after = 2;
                                                nullPs.descriptor_count = 2;
                                                nullPs.descriptors[0] = {1, 0, 0, 1, UINT32_MAX, 0};
                                                nullPs.descriptors[1] = {2, 0, 1, 1, UINT32_MAX, 0};
                                                auto nullVs = empty;
                                                nullVs.unified_before = nullVs.unified_after = 2;
                                                uint64_t nullLayout = 0, nullPipeline = 0, nullSampler = 0,
                                                         absentImage = 0;
                                                uint32_t imageSlot = 0, samplerSlot = 1, explicitNull = 1;
                                                CtrGraphicsSamplerParametersV1 nullParameters{};
                                                nullParameters.version = 1;
                                                nullParameters.size = sizeof(nullParameters);
                                                nullParameters.max_anisotropy = 1;
                                                nullParameters.mag_filter = nullParameters.min_filter = nullFilter;
                                                const auto nullFragment =
                                                    load("build/CTRPort-data/sampled_draw_control.frag.spv");
                                                if (session.CreateGraphicsLayout(&nullPs, &nullVs, &nullLayout) ||
                                                    session.CreateSampler(&nullParameters, &nullSampler) ||
                                                    ctr_video_graphics_pipeline_create_v1(
                                                        &session, &nullKey, &inputs, nullptr, 0, nullLayout,
                                                        colorTokens[0], copyVs.data(),
                                                        static_cast<uint32_t>(copyVs.size()), nullFragment.data(),
                                                        static_cast<uint32_t>(nullFragment.size()), 0, &nullPipeline) ||
                                                    ctr_video_graphics_layout_images_null_v1(&session, nullLayout,
                                                                                             &imageSlot, &absentImage,
                                                                                             &explicitNull, 1) ||
                                                    ctr_video_graphics_layout_samplers_v1(
                                                        &session, nullLayout, &samplerSlot, &nullSampler, 1) ||
                                                    session.ReleaseSampler(nullSampler)) {
                                                    status = 243;
                                                    break;
                                                }
                                                std::vector<unsigned char> nullPrior(8355840, 0x7b),
                                                    nullAfter(nullPrior.size());
                                                CtrGraphicsViewportStateV1 nullViewport{};
                                                nullViewport.version = 1;
                                                nullViewport.size = sizeof(nullViewport);
                                                nullViewport.count = 1;
                                                nullViewport.viewports[0] = {0, 0, 64, 64, 0, 1};
                                                nullViewport.scissors[0].extent = {64, 64};
                                                CtrGraphicsDynamicStateV1 nullDynamic{};
                                                nullDynamic.version = 1;
                                                nullDynamic.size = sizeof(nullDynamic);
                                                nullDynamic.line_width = 1;
                                                CtrGraphicsPushDataV1 nullPush{};
                                                nullPush.version = 1;
                                                nullPush.size = sizeof(nullPush);
                                                CtrGraphicsDrawV1 nullDraw{};
                                                nullDraw.version = 1;
                                                nullDraw.size = sizeof(nullDraw);
                                                nullDraw.vertex_count = 3;
                                                nullDraw.instance_count = 1;
                                                if (session.TransferColorAttachment(colorTokens[0], nullPrior.data(),
                                                                                    nullptr, nullPrior.size(), 7680) ||
                                                    ctr_video_graphics_draw_v1(&session, nullPipeline, colorTokens[0],
                                                                               &nullViewport, &nullDynamic, &nullPush,
                                                                               &nullDraw) ||
                                                    session.TransferColorAttachment(colorTokens[0], nullptr,
                                                                                    nullAfter.data(), nullAfter.size(),
                                                                                    7680)) {
                                                    status = 243;
                                                    break;
                                                }
                                                for (size_t nullByte = 0; nullByte < nullAfter.size(); ++nullByte) {
                                                    const size_t nullRow = nullByte / 7680,
                                                                 nullColumn = nullByte % 7680;
                                                    const unsigned char expectedNull =
                                                        nullRow < 64 && nullColumn < 256 ? 0 : 0x7b;
                                                    if (nullAfter[nullByte] != expectedNull) {
                                                        status = 243;
                                                        break;
                                                    }
                                                }
                                                if (status || session.ReleaseGraphicsPipeline(nullPipeline) ||
                                                    session.ReleaseGraphicsLayout(nullLayout)) {
                                                    status = 243;
                                                    break;
                                                }
                                                std::printf(
                                                    "null sampled graphics filter%u actual4096 pixels RGBA_ZERO includingAlpha full8355840B readback0diff untouchedOutside samplerRetainedAfterRelease no dummy image\n",
                                                    nullFilter);
                                            }
                                            if (status) break;
                                            for (auto format : {VK_FORMAT_R8G8B8A8_UNORM, VK_FORMAT_R8G8_UNORM}) {
                                                std::unique_ptr<CtrGraphicsPipelineOwner> toMs, toSingle;
                                                if (CreateMsBackingCopyPipeline(
                                                        device, deviceGet, selectedProperties.limits, format, 2,
                                                        copyVs.data(), static_cast<uint32_t>(copyVs.size()),
                                                        copySingle.data(), static_cast<uint32_t>(copySingle.size()),
                                                        graphicsCoreMask, toMs) ||
                                                    CreateMsBackingCopyPipeline(
                                                        device, deviceGet, selectedProperties.limits, format, 1,
                                                        copyVs.data(), static_cast<uint32_t>(copyVs.size()),
                                                        copyMulti.data(), static_cast<uint32_t>(copyMulti.size()),
                                                        graphicsCoreMask, toSingle)) {
                                                    status = 210;
                                                    break;
                                                }
                                            }
                                            if (status) break;
                                            std::puts(
                                                "MRT2 whole-public backing-copy Vulkan pipelines RGBA8/RG8 destination2/single shader variants actual allocation; descriptors unwritten no copy");
                                            for (uint32_t attachment = 0; attachment < 2; ++attachment) {
                                                const uint64_t row = 1920ull * (attachment ? 2ull : 4ull),
                                                               bytes = row * (attachment ? 1152ull : 1088ull);
                                                std::vector<unsigned char> prior(static_cast<size_t>(bytes)),
                                                    after(static_cast<size_t>(bytes), 0);
                                                for (size_t index = 0; index < prior.size(); ++index)
                                                    prior[index] = static_cast<unsigned char>(
                                                        (index * 37 + attachment * 19) & 255);
                                                if (ctr_video_color_attachment_upload_linear_v1(
                                                        &session, colorTokens[attachment], prior.data(), bytes, row) ||
                                                    session.AllocateColorSampleBacking(colorTokens[attachment]) ||
                                                    ctr_video_color_backing_copy_v1(&session, colorTokens[attachment],
                                                                                    0) ||
                                                    ctr_video_color_backing_copy_v1(&session, colorTokens[attachment],
                                                                                    1) ||
                                                    ctr_video_color_attachment_readback_linear_v1(
                                                        &session, colorTokens[attachment], after.data(), bytes, row) ||
                                                    prior != after) {
                                                    status = 209;
                                                    break;
                                                }
                                            }
                                            if (status) break;
                                            {
                                                const auto dualPs =
                                                    load("build/CTRPort-data/mrt_depth_control.frag.spv");
                                                uint64_t colorPipeline = 0;
                                                if (ctr_video_graphics_mrt_pipeline_create_v1(
                                                        &session, &key, &inputs, nullptr, 0, mrtLayout, colorTokens, 2,
                                                        copyVs.data(), static_cast<uint32_t>(copyVs.size()),
                                                        dualPs.data(), static_cast<uint32_t>(dualPs.size()), 0,
                                                        &colorPipeline)) {
                                                    status = 221;
                                                    break;
                                                }
                                                CtrGraphicsViewportStateV1 vp{};
                                                vp.version = 1;
                                                vp.size = sizeof(vp);
                                                vp.count = 1;
                                                vp.viewports[0] = {0, 0, 1920, 1080, 0, 1};
                                                vp.scissors[0].extent = {1920, 1080};
                                                CtrGraphicsDynamicStateV1 ds{};
                                                ds.version = 1;
                                                ds.size = sizeof(ds);
                                                ds.line_width = 1;
                                                CtrGraphicsPushDataV1 push{};
                                                push.version = 1;
                                                push.size = sizeof(push);
                                                CtrGraphicsDrawV1 draw{};
                                                draw.version = 1;
                                                draw.size = sizeof(draw);
                                                draw.vertex_count = 3;
                                                draw.instance_count = 1;
                                                if (ctr_video_graphics_mrt_draw_v1(&session, colorPipeline, &vp, &ds,
                                                                                   &push, &draw)) {
                                                    status = 221;
                                                    break;
                                                }
                                                for (uint32_t attachment = 0; attachment < 2; ++attachment) {
                                                    const uint32_t texelBytes = attachment ? 2u : 4u,
                                                                   height = attachment ? 1152u : 1088u;
                                                    std::vector<unsigned char> pixels(size_t(1920) * height *
                                                                                      texelBytes);
                                                    if (session.TransferColorAttachment(
                                                            colorTokens[attachment], nullptr, pixels.data(),
                                                            pixels.size(), 1920u * texelBytes)) {
                                                        status = 221;
                                                        break;
                                                    }
                                                    const unsigned char rgba[] = {64, 128, 192, 255}, rg[] = {128, 64};
                                                    for (size_t index = 0; index < pixels.size(); ++index) {
                                                        const auto expected =
                                                            index < size_t(1920) * 1080 * texelBytes
                                                                ? (attachment ? rg[index % 2] : rgba[index % 4])
                                                                : static_cast<unsigned char>(
                                                                      (index * 37 + attachment * 19) & 255);
                                                        if (pixels[index] != expected) {
                                                            status = 221;
                                                            break;
                                                        }
                                                    }
                                                }
                                                if (status) break;
                                                auto invalid = ds;
                                                invalid.depth_test = 1;
                                                if (ctr_video_graphics_mrt_draw_v1(&session, colorPipeline, &vp,
                                                                                   &invalid, &push, &draw) != 88 ||
                                                    ctr_video_graphics_mrt_depth_draw_v1(&session, colorPipeline, &vp,
                                                                                         &ds, &push, &draw) != 60 ||
                                                    ctr_video_graphics_pipeline_release_v1(&session, colorPipeline)) {
                                                    status = 221;
                                                    break;
                                                }
                                                std::puts(
                                                    "MRT2 color-only actual1920x1080 single-sample draw both targets/padding0diff depth-request88 wrong-depth-API60");
                                            }
                                            {
                                                auto inactiveKey = key;
                                                inactiveKey.colors[1] = {};
                                                const uint64_t inactiveTokens[] = {colorTokens[0], 0};
                                                const auto dualPs =
                                                    load("build/CTRPort-data/mrt_depth_control.frag.spv");
                                                uint64_t inactivePipeline = 0;
                                                std::vector<unsigned char> prior(8355840, 0x7b), pixels(prior.size());
                                                if (ctr_video_color_attachment_upload_linear_v1(
                                                        &session, colorTokens[0], prior.data(), prior.size(), 7680) ||
                                                    ctr_video_graphics_mrt_pipeline_create_v1(
                                                        &session, &inactiveKey, &inputs, nullptr, 0, mrtLayout,
                                                        inactiveTokens, 2, copyVs.data(),
                                                        static_cast<uint32_t>(copyVs.size()), dualPs.data(),
                                                        static_cast<uint32_t>(dualPs.size()), 0, &inactivePipeline)) {
                                                    status = 251;
                                                    break;
                                                }
                                                CtrGraphicsViewportStateV1 vp{};
                                                vp.version = 1;
                                                vp.size = sizeof(vp);
                                                vp.count = 1;
                                                vp.viewports[0] = {0, 0, 1920, 1080, 0, 1};
                                                vp.scissors[0].extent = {1920, 1080};
                                                CtrGraphicsDynamicStateV1 ds{};
                                                ds.version = 1;
                                                ds.size = sizeof(ds);
                                                ds.line_width = 1;
                                                CtrGraphicsPushDataV1 push{};
                                                push.version = 1;
                                                push.size = sizeof(push);
                                                CtrGraphicsDrawV1 draw{};
                                                draw.version = 1;
                                                draw.size = sizeof(draw);
                                                draw.vertex_count = 3;
                                                draw.instance_count = 1;
                                                if (ctr_video_graphics_mrt_draw_v1(&session, inactivePipeline, &vp, &ds,
                                                                                   &push, &draw) ||
                                                    session.TransferColorAttachment(
                                                        colorTokens[0], nullptr, pixels.data(), pixels.size(), 7680)) {
                                                    status = 251;
                                                    break;
                                                }
                                                const unsigned char rgba[] = {64, 128, 192, 255};
                                                for (size_t i = 0; i < pixels.size(); ++i)
                                                    if (pixels[i] !=
                                                        (i < size_t(1920) * 1080 * 4 ? rgba[i % 4] : 0x7b)) {
                                                        status = 251;
                                                        break;
                                                    }
                                                if (status) break;
                                                for (uint32_t bad = 0; bad < 6; ++bad) {
                                                    auto malformed = inactiveKey;
                                                    uint64_t unchanged = 999;
                                                    uint64_t tokens[] = {colorTokens[0], 0};
                                                    if (bad == 0) malformed.colors[1].write_mask = 15;
                                                    if (bad == 1) malformed.colors[1].samples = 1;
                                                    if (bad == 2) malformed.colors[1].data_format = 10;
                                                    if (bad == 3) tokens[1] = colorTokens[1];
                                                    if (bad == 4) malformed.mrt_mask = 1;
                                                    if (!ctr_video_graphics_mrt_pipeline_create_v1(
                                                            &session, &malformed, &inputs, nullptr, 0, mrtLayout,
                                                            tokens, bad == 5 ? 1u : 2u, copyVs.data(),
                                                            static_cast<uint32_t>(copyVs.size()), dualPs.data(),
                                                            static_cast<uint32_t>(dualPs.size()), 0, &unchanged) ||
                                                        unchanged != 999) {
                                                        status = 251;
                                                        break;
                                                    }
                                                }
                                                auto forbidden = ds;
                                                forbidden.depth_write = 1;
                                                if (status ||
                                                    ctr_video_graphics_mrt_draw_v1(&session, inactivePipeline, &vp,
                                                                                   &forbidden, &push, &draw) != 88 ||
                                                    session.ReleaseGraphicsPipeline(inactivePipeline)) {
                                                    status = 251;
                                                    break;
                                                }
                                                std::puts(
                                                    "inactive MRT slot1 undefined null actualGPU dualExport slot0RGBA full8355840B padding0diff sixAtomicNegatives depthWrite88");
                                            }
                                            {
                                                auto singleDepthKey = key;
                                                singleDepthKey.num_color_attachments = singleDepthKey.mrt_mask = 1;
                                                singleDepthKey.num_samples = singleDepthKey.depth_samples = 1;
                                                singleDepthKey.z_format = 3;
                                                singleDepthKey.stencil_format = 1;
                                                singleDepthKey.colors[0].samples = 1;
                                                singleDepthKey.colors[0].data_format = 4;
                                                singleDepthKey.colors[0].number_format = 7;
                                                singleDepthKey.colors[1] = {};
                                                singleDepthKey.cb_shader_mask = 15;
                                                CtrGraphicsColorRequirementV1 demandR32{
                                                    1, sizeof(demandR32), 0, 1920, 1088, 1, 1, 14, 4, 7, 1, 8355840};
                                                uint64_t singleColor = 0, singleDepth = 0, singlePipeline = 0;
                                                CtrDepthAttachmentParametersV1 depthParameters{
                                                    1, sizeof(depthParameters), 1920, 1080, 1, 3, 1};
                                                std::vector<float> priorR32(size_t(1920) * 1088, 0.5f),
                                                    afterR32(priorR32.size());
                                                const auto psR32 = load("build/CTRPort-data/mrt_r32_control.frag.spv");
                                                if (session.CreateColorAttachment(&demandR32, 0, &singleColor) ||
                                                    session.TransferColorAttachment(singleColor, priorR32.data(),
                                                                                    nullptr, 8355840, 7680) ||
                                                    session.CreateDepthAttachment(&depthParameters, &singleDepth) ||
                                                    session.ClearDepthAttachment(singleDepth, 0x3f800000, 0) ||
                                                    ctr_video_graphics_mrt_depth_pipeline_create_v1(
                                                        &session, &singleDepthKey, &inputs, nullptr, 0, mrtLayout,
                                                        &singleColor, 1, singleDepth, copyVs.data(),
                                                        static_cast<uint32_t>(copyVs.size()), psR32.data(),
                                                        static_cast<uint32_t>(psR32.size()), 0, &singlePipeline)) {
                                                    status = 242;
                                                    break;
                                                }
                                                CtrGraphicsViewportStateV1 vp{};
                                                vp.version = 1;
                                                vp.size = sizeof(vp);
                                                vp.count = 1;
                                                vp.viewports[0] = {0, 0, 1920, 1080, 0, 1};
                                                vp.scissors[0].extent = {1920, 1080};
                                                CtrGraphicsDynamicStateV1 ds{};
                                                ds.version = 1;
                                                ds.size = sizeof(ds);
                                                ds.line_width = 1;
                                                ds.stencil_test = 1;
                                                ds.front.compare_op = ds.back.compare_op = 2;
                                                ds.front.reference = ds.back.reference = 16;
                                                ds.front.compare_mask = ds.back.compare_mask = 16;
                                                ds.front.write_mask = ds.back.write_mask = 16;
                                                CtrGraphicsPushDataV1 push{};
                                                push.version = 1;
                                                push.size = sizeof(push);
                                                CtrGraphicsDrawV1 draw{};
                                                draw.version = 1;
                                                draw.size = sizeof(draw);
                                                draw.vertex_count = 3;
                                                draw.instance_count = 1;
                                                uint64_t visibility = 999;
                                                if (session.DrawMrtDepth(singlePipeline, &vp, &ds, &push, &draw,
                                                                         &visibility) ||
                                                    visibility ||
                                                    session.TransferColorAttachment(singleColor, nullptr,
                                                                                    afterR32.data(), 8355840, 7680) ||
                                                    afterR32 != priorR32 ||
                                                    session.ClearDepthAttachment(singleDepth, 0x3f800000, 16) ||
                                                    session.DrawMrtDepth(singlePipeline, &vp, &ds, &push, &draw,
                                                                         &visibility) ||
                                                    !visibility ||
                                                    session.TransferColorAttachment(singleColor, nullptr,
                                                                                    afterR32.data(), 8355840, 7680)) {
                                                    status = 242;
                                                    break;
                                                }
                                                for (size_t depthPixel = 0; depthPixel < afterR32.size(); ++depthPixel)
                                                    if (afterR32[depthPixel] !=
                                                        (depthPixel < size_t(1920) * 1080 ? 0.25f : 0.5f))
                                                        status = 242;
                                                if (status) break;
                                                auto mismatch = singleDepthKey;
                                                mismatch.depth_samples = 2;
                                                uint64_t unchanged = 999;
                                                if (ctr_video_graphics_mrt_depth_pipeline_create_v1(
                                                        &session, &mismatch, &inputs, nullptr, 0, mrtLayout,
                                                        &singleColor, 1, singleDepth, copyVs.data(),
                                                        static_cast<uint32_t>(copyVs.size()), psR32.data(),
                                                        static_cast<uint32_t>(psR32.size()), 0, &unchanged) != 88 ||
                                                    unchanged != 999 ||
                                                    session.ReleaseGraphicsPipeline(singlePipeline) ||
                                                    session.ReleaseDepthAttachment(singleDepth) ||
                                                    session.ReleaseColorAttachment(singleColor)) {
                                                    status = 242;
                                                    break;
                                                }
                                                std::puts(
                                                    "single R32 retained D32S8 sample1 actual stencil reject0/passPositive GPU color8355840B logical0.25 padded0.5 zeroDiff sampleMismatch88 atomic");
                                            }
                                            {
                                                auto singleDepthKey = key;
                                                singleDepthKey.num_color_attachments = singleDepthKey.mrt_mask = 1;
                                                singleDepthKey.num_samples = singleDepthKey.depth_samples = 1;
                                                singleDepthKey.z_format = 3;
                                                singleDepthKey.stencil_format = 1;
                                                singleDepthKey.colors[0].samples = 1;
                                                singleDepthKey.colors[0].data_format = 10;
                                                singleDepthKey.colors[0].number_format = 0;
                                                singleDepthKey.colors[1] = {};
                                                singleDepthKey.cb_shader_mask = 15;
                                                CtrGraphicsColorRequirementV1 demandR32{
                                                    1, sizeof(demandR32), 0, 1920, 1088, 1, 1, 14, 10, 0, 1, 8355840};
                                                uint64_t singleColor = 0, singleDepth = 0, singlePipeline = 0;
                                                CtrDepthAttachmentParametersV1 depthParameters{
                                                    1, sizeof(depthParameters), 1920, 1080, 1, 3, 1};
                                                std::vector<uint32_t> priorR32(size_t(1920) * 1088, 0x7b7b7b7b),
                                                    afterR32(priorR32.size());
                                                const auto psR32 =
                                                    load("build/CTRPort-data/mrt_depth_control.frag.spv");
                                                if (session.CreateColorAttachment(&demandR32, 0, &singleColor) ||
                                                    session.TransferColorAttachment(singleColor, priorR32.data(),
                                                                                    nullptr, 8355840, 7680) ||
                                                    session.CreateDepthAttachment(&depthParameters, &singleDepth) ||
                                                    session.ClearDepthAttachment(singleDepth, 0x3f800000, 0) ||
                                                    ctr_video_graphics_mrt_depth_pipeline_create_v1(
                                                        &session, &singleDepthKey, &inputs, nullptr, 0, mrtLayout,
                                                        &singleColor, 1, singleDepth, copyVs.data(),
                                                        static_cast<uint32_t>(copyVs.size()), psR32.data(),
                                                        static_cast<uint32_t>(psR32.size()), 0, &singlePipeline)) {
                                                    status = 242;
                                                    break;
                                                }
                                                CtrGraphicsViewportStateV1 vp{};
                                                vp.version = 1;
                                                vp.size = sizeof(vp);
                                                vp.count = 1;
                                                vp.viewports[0] = {0, 0, 1920, 1080, 0, 1};
                                                vp.scissors[0].extent = {1920, 1080};
                                                CtrGraphicsDynamicStateV1 ds{};
                                                ds.version = 1;
                                                ds.size = sizeof(ds);
                                                ds.line_width = 1;
                                                ds.depth_test = ds.depth_write = 1;
                                                ds.depth_compare = 1;
                                                ds.stencil_test = 1;
                                                ds.front.compare_op = ds.back.compare_op = 2;
                                                ds.front.reference = ds.back.reference = 16;
                                                ds.front.compare_mask = ds.back.compare_mask = 16;
                                                ds.front.write_mask = ds.back.write_mask = 16;
                                                CtrGraphicsPushDataV1 push{};
                                                push.version = 1;
                                                push.size = sizeof(push);
                                                CtrGraphicsDrawV1 draw{};
                                                draw.version = 1;
                                                draw.size = sizeof(draw);
                                                draw.vertex_count = 3;
                                                draw.instance_count = 1;
                                                uint64_t visibility = 999;
                                                if (session.DrawMrtDepth(singlePipeline, &vp, &ds, &push, &draw,
                                                                         &visibility) ||
                                                    visibility ||
                                                    session.TransferColorAttachment(singleColor, nullptr,
                                                                                    afterR32.data(), 8355840, 7680) ||
                                                    afterR32 != priorR32 ||
                                                    session.ClearDepthAttachment(singleDepth, 0x3f800000, 16) ||
                                                    session.DrawMrtDepth(singlePipeline, &vp, &ds, &push, &draw,
                                                                         &visibility) ||
                                                    !visibility ||
                                                    session.TransferColorAttachment(singleColor, nullptr,
                                                                                    afterR32.data(), 8355840, 7680)) {
                                                    status = 242;
                                                    break;
                                                }
                                                for (size_t depthPixel = 0; depthPixel < afterR32.size(); ++depthPixel)
                                                    if (afterR32[depthPixel] !=
                                                        (depthPixel < size_t(1920) * 1080 ? 0xffc08040u : 0x7b7b7b7bu))
                                                        status = 242;
                                                if (status) break;
                                                if (session.DrawMrtDepth(singlePipeline, &vp, &ds, &push, &draw,
                                                                         &visibility) ||
                                                    visibility) {
                                                    status = 249;
                                                    break;
                                                }
                                                // Retail8094: depthcompare3/write0, stencilAlways Replace3/ref1/masks255.
                                                ds.depth_write = 0;
                                                ds.depth_compare = 3;
                                                ds.front.compare_op = ds.back.compare_op = 7;
                                                ds.front.pass_op = ds.back.pass_op = 3;
                                                ds.front.reference = ds.back.reference = 1;
                                                ds.front.compare_mask = ds.back.compare_mask = 255;
                                                ds.front.write_mask = ds.back.write_mask = 255;
                                                if (session.ClearDepthAttachment(singleDepth, 0x3f800000, 0) ||
                                                    session.DrawMrtDepth(singlePipeline, &vp, &ds, &push, &draw,
                                                                         &visibility) ||
                                                    !visibility) {
                                                    status = 249;
                                                    break;
                                                }
                                                ds.front.compare_op = ds.back.compare_op = 2;
                                                ds.front.pass_op = ds.back.pass_op = 0;
                                                ds.depth_compare = 1;
                                                if (session.DrawMrtDepth(singlePipeline, &vp, &ds, &push, &draw,
                                                                         &visibility) ||
                                                    !visibility ||
                                                    session.DrawMrtDepth(singlePipeline, &vp, &ds, &push, &draw,
                                                                         &visibility) ||
                                                    !visibility) {
                                                    status = 249;
                                                    break;
                                                }
                                                ds.front.reference = ds.back.reference = 0;
                                                if (session.DrawMrtDepth(singlePipeline, &vp, &ds, &push, &draw,
                                                                         &visibility) ||
                                                    visibility) {
                                                    status = 249;
                                                    break;
                                                }

                                                auto mismatch = singleDepthKey;
                                                mismatch.depth_samples = 2;
                                                uint64_t unchanged = 999;
                                                if (ctr_video_graphics_mrt_depth_pipeline_create_v1(
                                                        &session, &mismatch, &inputs, nullptr, 0, mrtLayout,
                                                        &singleColor, 1, singleDepth, copyVs.data(),
                                                        static_cast<uint32_t>(copyVs.size()), psR32.data(),
                                                        static_cast<uint32_t>(psR32.size()), 0, &unchanged) != 88 ||
                                                    unchanged != 999 ||
                                                    session.ReleaseGraphicsPipeline(singlePipeline) ||
                                                    session.ReleaseDepthAttachment(singleDepth) ||
                                                    session.ReleaseColorAttachment(singleColor)) {
                                                    status = 242;
                                                    break;
                                                }
                                                std::puts(
                                                    "single RGBA8 retained D32S8 sample1 stencilReject0 passPositive depthWriteLESSreject0 retailAlwaysReplace1 stencilEqualPass/ref0Reject depthNoWritePreserved full8355840B logicalRGBA padding0diff sampleMismatch88 atomic");
                                            }
                                            {
                                                auto combinedKey = key;
                                                combinedKey.num_samples = combinedKey.depth_samples = 2;
                                                combinedKey.z_format = 3;
                                                combinedKey.stencil_format = 1;
                                                combinedKey.colors[0].samples = combinedKey.colors[1].samples = 2;
                                                uint64_t combinedPipeline = 999;
                                                const auto dualPs =
                                                    load("build/CTRPort-data/mrt_depth_control.frag.spv");
                                                if (ctr_video_graphics_mrt_depth_pipeline_create_v1(
                                                        &session, &combinedKey, &inputs, nullptr, 0, mrtLayout,
                                                        colorTokens, 2, depthToken, copyVs.data(),
                                                        static_cast<uint32_t>(copyVs.size()), dualPs.data(),
                                                        static_cast<uint32_t>(dualPs.size()), 0, &combinedPipeline)) {
                                                    status = 211;
                                                    break;
                                                }
                                                CtrGraphicsViewportStateV1 vp{};
                                                vp.version = 1;
                                                vp.size = sizeof(vp);
                                                vp.count = 1;
                                                vp.viewports[0] = {0, 0, 1920, 1080, 0, 1};
                                                vp.scissors[0].extent = {1920, 1080};
                                                CtrGraphicsDynamicStateV1 ds{};
                                                ds.version = 1;
                                                ds.size = sizeof(ds);
                                                ds.line_width = 1;
                                                ds.stencil_test = 1;
                                                ds.front.compare_op = ds.back.compare_op = 2;
                                                ds.front.reference = ds.back.reference = 16;
                                                ds.front.compare_mask = ds.back.compare_mask = 16;
                                                ds.front.write_mask = ds.back.write_mask = 16;
                                                CtrGraphicsPushDataV1 push{};
                                                push.version = 1;
                                                push.size = sizeof(push);
                                                CtrGraphicsDrawV1 draw{};
                                                draw.version = 1;
                                                draw.size = sizeof(draw);
                                                draw.vertex_count = 3;
                                                draw.instance_count = 1;
                                                uint64_t visible = 999;
                                                int combinedRc = session.DrawMrtDepth(combinedPipeline, &vp, &ds, &push,
                                                                                      &draw, &visible);
                                                std::printf("combined reject rc=%d visible=%llu\n", combinedRc,
                                                            static_cast<unsigned long long>(visible));
                                                if (combinedRc || visible ||
                                                    session.ClearDepthAttachment(depthToken, 0x3f800000, 16)) {
                                                    status = 212;
                                                    break;
                                                }
                                                combinedRc = session.DrawMrtDepth(combinedPipeline, &vp, &ds, &push,
                                                                                  &draw, &visible);
                                                std::printf("combined pass rc=%d visible=%llu\n", combinedRc,
                                                            static_cast<unsigned long long>(visible));
                                                if (combinedRc || !visible) {
                                                    status = 212;
                                                    break;
                                                }
                                                int wrongThread = 0;
                                                std::thread combinedWorker([&] {
                                                    wrongThread = ctr_video_graphics_mrt_depth_draw_v1(
                                                        &session, combinedPipeline, &vp, &ds, &push, &draw);
                                                });
                                                combinedWorker.join();
                                                auto missingVertex = draw;
                                                missingVertex.vertex_buffer_count = 1;
                                                auto imageWriter = draw;
                                                imageWriter.written_images = 1;
                                                if (ctr_video_graphics_mrt_depth_draw_v1(&session, UINT64_MAX, &vp, &ds,
                                                                                         &push, &draw) != 62 ||
                                                    ctr_video_graphics_mrt_depth_draw_v1(&session, combinedPipeline,
                                                                                         &vp, &ds, &push,
                                                                                         &missingVertex) != 60 ||
                                                    ctr_video_graphics_mrt_depth_draw_v1(&session, combinedPipeline,
                                                                                         &vp, &ds, &push,
                                                                                         &imageWriter) != 88) {
                                                    status = 214;
                                                    break;
                                                }
                                                auto invalidStencil = ds;
                                                invalidStencil.front.compare_op = 8;
                                                auto unsupportedStencil = ds;
                                                unsupportedStencil.back.fail_op = 2;
                                                if (wrongThread != 116 ||
                                                    ctr_video_graphics_mrt_depth_draw_v1(nullptr, combinedPipeline, &vp,
                                                                                         &ds, &push, &draw) != 60 ||
                                                    ctr_video_graphics_mrt_depth_draw_v1(
                                                        &session, combinedPipeline, &vp, &ds, nullptr, &draw) != 60 ||
                                                    ctr_video_graphics_mrt_depth_draw_v1(&session, combinedPipeline,
                                                                                         &vp, &invalidStencil, &push,
                                                                                         &draw) != 60 ||
                                                    ctr_video_graphics_mrt_depth_draw_v1(&session, combinedPipeline,
                                                                                         &vp, &unsupportedStencil,
                                                                                         &push, &draw) != 88 ||
                                                    ctr_video_graphics_mrt_depth_draw_v1(&session, combinedPipeline,
                                                                                         &vp, &ds, &push, &draw)) {
                                                    status = 214;
                                                    break;
                                                }
                                                {
                                                    auto single = combinedKey;
                                                    single.num_color_attachments = 1;
                                                    single.mrt_mask = 1;
                                                    single.colors[1] = {};
                                                    uint64_t singlePipeline = 999;
                                                    const int singleRc = session.CreateMrtPipeline(
                                                        &single, &inputs, nullptr, 0, mrtLayout, colorTokens, 1,
                                                        copyVs.data(), static_cast<uint32_t>(copyVs.size()),
                                                        dualPs.data(), static_cast<uint32_t>(dualPs.size()), 0,
                                                        &singlePipeline, depthToken);
                                                    std::printf("single MS2 create status%d\n", singleRc);
                                                    if (singleRc) {
                                                        status = 245;
                                                        break;
                                                    }
                                                    if (session.ClearDepthAttachment(depthToken, 0x3f800000, 0) ||
                                                        session.DrawMrtDepth(singlePipeline, &vp, &ds, &push, &draw,
                                                                             &visible) ||
                                                        visible ||
                                                        session.ClearDepthAttachment(depthToken, 0x3f800000, 16) ||
                                                        session.DrawMrtDepth(singlePipeline, &vp, &ds, &push, &draw,
                                                                             &visible) ||
                                                        !visible ||
                                                        ctr_video_graphics_mrt_depth_draw_v1(&session, singlePipeline,
                                                                                             &vp, &ds, &push, &draw)) {
                                                        status = 247;
                                                        break;
                                                    }
                                                    std::vector<unsigned char> nonindexedReadback(8355840);
                                                    if (ctr_video_color_backing_copy_v1(&session, colorTokens[0], 1) ||
                                                        session.TransferColorAttachment(
                                                            colorTokens[0], nullptr, nonindexedReadback.data(),
                                                            nonindexedReadback.size(), 7680)) {
                                                        status = 247;
                                                        break;
                                                    }
                                                    const unsigned char nonindexedPixel[] = {64, 128, 192, 255};
                                                    for (size_t nonindexedByte = 0;
                                                         nonindexedByte < nonindexedReadback.size(); ++nonindexedByte) {
                                                        const auto expectedNonindexed =
                                                            nonindexedByte < 1920ull * 1080 * 4
                                                                ? nonindexedPixel[nonindexedByte % 4]
                                                                : static_cast<unsigned char>((nonindexedByte * 37) &
                                                                                             255);
                                                        if (nonindexedReadback[nonindexedByte] != expectedNonindexed) {
                                                            status = 247;
                                                            break;
                                                        }
                                                    }
                                                    if (status) break;
                                                    std::puts(
                                                        "Nonindexed color1 MS2 retained D32S8 actual TriangleList stencilReject0 stencilPassPositive CABI0 sample0Readback8355840B zeroDiff paddedRowsPreserved exact6309 shape");
                                                    {
                                                        uint64_t inactivePipeline = 0;
                                                        if (ctr_video_graphics_mrt_pipeline_create_v1(
                                                                &session, &single, &inputs, nullptr, 0, mrtLayout,
                                                                colorTokens, 1, copyVs.data(),
                                                                static_cast<uint32_t>(copyVs.size()), dualPs.data(),
                                                                static_cast<uint32_t>(dualPs.size()), 0,
                                                                &inactivePipeline)) {
                                                            status = 250;
                                                            break;
                                                        }
                                                        auto inactiveDynamic = ds;
                                                        inactiveDynamic.depth_test = inactiveDynamic.depth_write =
                                                            inactiveDynamic.stencil_test = 0;
                                                        if (ctr_video_graphics_mrt_draw_v1(&session, inactivePipeline,
                                                                                           &vp, &inactiveDynamic, &push,
                                                                                           &draw)) {
                                                            status = 250;
                                                            break;
                                                        }
                                                        auto forbiddenDepth = inactiveDynamic;
                                                        forbiddenDepth.depth_test = 1;
                                                        auto forbiddenStencil = inactiveDynamic;
                                                        forbiddenStencil.stencil_test = 1;
                                                        if (ctr_video_graphics_mrt_draw_v1(&session, inactivePipeline,
                                                                                           &vp, &forbiddenDepth, &push,
                                                                                           &draw) != 88 ||
                                                            ctr_video_graphics_mrt_draw_v1(&session, inactivePipeline,
                                                                                           &vp, &forbiddenStencil,
                                                                                           &push, &draw) != 88 ||
                                                            ctr_video_graphics_mrt_depth_draw_v1(
                                                                &session, inactivePipeline, &vp, &inactiveDynamic,
                                                                &push, &draw) != 60 ||
                                                            ctr_video_color_backing_copy_v1(&session, colorTokens[0],
                                                                                            1) ||
                                                            session.TransferColorAttachment(
                                                                colorTokens[0], nullptr, nonindexedReadback.data(),
                                                                nonindexedReadback.size(), 7680)) {
                                                            status = 250;
                                                            break;
                                                        }
                                                        for (size_t inactiveByte = 0;
                                                             inactiveByte < nonindexedReadback.size(); ++inactiveByte) {
                                                            const auto expectedInactive =
                                                                inactiveByte < 1920ull * 1080 * 4
                                                                    ? nonindexedPixel[inactiveByte % 4]
                                                                    : static_cast<unsigned char>((inactiveByte * 37) &
                                                                                                 255);
                                                            if (nonindexedReadback[inactiveByte] != expectedInactive) {
                                                                status = 250;
                                                                break;
                                                            }
                                                        }
                                                        if (status ||
                                                            session.ReleaseGraphicsPipeline(inactivePipeline)) {
                                                            status = 250;
                                                            break;
                                                        }
                                                        std::puts(
                                                            "Inactive depth color1MS2 exact stale z3s1 key actual no depth attachment GPUdraw CABI0 sample0Readback8355840B0diff paddingPreserved activeDepth88 stencil88 wrongAPI60 no fabricated depth6309");
                                                    }
                                                    for (const uint32_t width : {2u, 4u}) {
                                                        const uint32_t indexValues[] = {0, 1, 2};
                                                        std::vector<unsigned char> indexData(3 * width);
                                                        for (size_t i = 0; i < 3; ++i)
                                                            std::memcpy(indexData.data() + i * width, &indexValues[i],
                                                                        width);
                                                        uint64_t index = 0;
                                                        if (session.CreateBuffer(indexData.data(), indexData.size(),
                                                                                 width == 2 ? 2u : 3u, &index)) {
                                                            status = 245;
                                                            break;
                                                        }
                                                        CtrGraphicsIndexedDrawV1 indexed{};
                                                        indexed.version = 1;
                                                        indexed.size = sizeof(indexed);
                                                        indexed.index_count = 3;
                                                        indexed.instance_count = 1;
                                                        indexed.index_token = index;
                                                        indexed.index_bytes = width;
                                                        auto actual = [&] {
                                                            return session.DrawMrtDepth(singlePipeline, &vp, &ds, &push,
                                                                                        &draw, &visible, true, index,
                                                                                        width);
                                                        };
                                                        int wrongIndexedThread = 0;
                                                        std::thread iw([&] {
                                                            wrongIndexedThread =
                                                                ctr_video_graphics_indexed_depth_draw_v1(
                                                                    &session, singlePipeline, &vp, &ds, &push,
                                                                    &indexed);
                                                        });
                                                        iw.join();
                                                        auto badIndexed = indexed;
                                                        badIndexed.index_bytes = 1;
                                                        if (wrongIndexedThread != 116 ||
                                                            ctr_video_graphics_indexed_depth_draw_v1(
                                                                nullptr, singlePipeline, &vp, &ds, &push, &indexed) !=
                                                                60 ||
                                                            ctr_video_graphics_indexed_depth_draw_v1(
                                                                &session, singlePipeline, &vp, &ds, &push,
                                                                &badIndexed) != 88 ||
                                                            ctr_video_graphics_indexed_depth_draw_v1(
                                                                &session, singlePipeline, &vp, &ds, &push, &indexed)) {
                                                            status = 246;
                                                            break;
                                                        }
                                                        if (session.ClearDepthAttachment(depthToken, 0x3f800000, 0) ||
                                                            actual() || visible ||
                                                            session.ClearDepthAttachment(depthToken, 0x3f800000, 16) ||
                                                            actual() || !visible || session.ReleaseBuffer(index)) {
                                                            status = 245;
                                                            break;
                                                        }
                                                        if (actual() != 62 || ctr_video_graphics_indexed_depth_draw_v1(
                                                                                  &session, singlePipeline, &vp, &ds,
                                                                                  &push, &indexed) != 62) {
                                                            status = 245;
                                                            break;
                                                        }
                                                    }
                                                    if (status == 245 || status == 246 ||
                                                        session.ReleaseGraphicsPipeline(singlePipeline)) {
                                                        status = 245;
                                                        break;
                                                    }
                                                    std::puts(
                                                        "Indexed color1 MS2 retained D32S8 stencil actual U16 U32 reject0 passPositive occlusion fenced depthTest0 depthWrite0 stale62 exactKey no disableStencil");
                                                }
                                                std::puts(
                                                    "MRT2 combined CABI actual draw fenced; wrongthread116 null60 invalid stencil60 unsupported stencil88");
                                                for (uint32_t attachment = 0; attachment < 2; ++attachment) {
                                                    const uint64_t texel = attachment ? 2ull : 4ull, row = 1920 * texel,
                                                                   bytes = row * (attachment ? 1152ull : 1088ull);
                                                    std::vector<unsigned char> pixels(static_cast<size_t>(bytes), 0);
                                                    if (ctr_video_color_backing_copy_v1(&session,
                                                                                        colorTokens[attachment], 1) ||
                                                        ctr_video_color_attachment_readback_linear_v1(
                                                            &session, colorTokens[attachment], pixels.data(), bytes,
                                                            row)) {
                                                        status = 213;
                                                        break;
                                                    }
                                                    const unsigned char rgba[] = {64, 128, 192, 255}, rg[] = {128, 64};
                                                    for (size_t index = 0; index < pixels.size(); ++index) {
                                                        const auto expected =
                                                            index < row * 1080
                                                                ? (attachment ? rg[index % 2] : rgba[index % 4])
                                                                : static_cast<unsigned char>(
                                                                      (index * 37 + attachment * 19) & 255);
                                                        if (pixels[index] != expected) {
                                                            std::printf(
                                                                "combined pixel target=%u index=%llu actual=%u expected=%u\n",
                                                                attachment, static_cast<unsigned long long>(index),
                                                                pixels[index], expected);
                                                            status = 213;
                                                            break;
                                                        }
                                                    }
                                                    if (status) break;
                                                }
                                                if (status) break;
                                                if (session.ClearDepthAttachment(depthToken, 0x3e800000, 7) ||
                                                    ctr_video_graphics_pipeline_release_v1(&session,
                                                                                           combinedPipeline)) {
                                                    status = 211;
                                                    break;
                                                }
                                                std::puts(
                                                    "MRT2 actual combined full1920x1080 draw color2/2 depth2 stencilEqual16 rejects7 accepts16; both target sample0 bytes expected0diff and padded rows preserved; no guest draw");
                                            }
                                            std::puts(
                                                "MRT2 full-size actual shader single-to-MS2-to-sample0 copy8355840/4423680bytes0diff fenced; source COLOR restored; no combined guest draw");
                                            std::puts(
                                                "MRT2 actual retained sample2 color backing RGBA8/RG8 allocation; sampled physical usage checked; duplicate idempotent; UNDEFINED no copy/draw");
                                            if (session.ReleaseColorAttachment(colorTokens[0]) ||
                                                session.ReleaseColorAttachment(colorTokens[1]) ||
                                                session.ReleaseGraphicsLayout(mrtLayout) ||
                                                ctr_video_graphics_pipeline_release_v1(&session, mrtPipeline)) {
                                                status = 207;
                                                break;
                                            }
                                            std::puts(
                                                "MRT2 actual Vulkan pipeline RGBA8/RG8 allocation/release; two color owners and layout retained after token release; no MRT draw");
                                            key.mrt_mask = 1;
                                            if (state.Prepare(key, inputs, nullptr, VK_FORMAT_R8G8B8A8_UNORM,
                                                              graphicsCoreMask, 0, VK_FORMAT_UNDEFINED,
                                                              VK_FORMAT_R8G8_UNORM) != 88) {
                                                status = 206;
                                                break;
                                            }
                                            std::puts(
                                                "MRT2 public pipeline state RGBA8/RG8 exact two formats and independent write masks15/3; missingIndependentBlend88 inconsistentMRT88; state only no GPU MRT pipeline");
                                        }
                                        {
                                            CtrGraphicsColorRequirementV1 rg8{1,
                                                                              sizeof(rg8),
                                                                              1,
                                                                              1920,
                                                                              1152,
                                                                              1,
                                                                              1,
                                                                              14,
                                                                              3,
                                                                              0,
                                                                              uint64_t(286087552) << 8,
                                                                              4423680};
                                            uint64_t token = 999;
                                            auto invalid = rg8;
                                            invalid.number_format = 7;
                                            if (session.CreateColorAttachment(&invalid, 0, &token) != 88 ||
                                                token != 999 || session.CreateColorAttachment(&rg8, 0, &token)) {
                                                status = 205;
                                                break;
                                            }
                                            const uint64_t row = 1920 * 2, pitch = row + 16;
                                            std::vector<unsigned char> source(static_cast<size_t>(pitch * 1152), 0xa7),
                                                actual(source.size(), 0xd3);
                                            for (uint32_t y = 0; y < 1152; ++y)
                                                for (uint32_t x = 0; x < 1920; ++x) {
                                                    source[size_t(y * pitch) + x * 2] = static_cast<unsigned char>(x);
                                                    source[size_t(y * pitch) + x * 2 + 1] =
                                                        static_cast<unsigned char>(y);
                                                }
                                            const auto preserved = actual;
                                            if (session.TransferColorAttachment(token, source.data(), nullptr,
                                                                                source.size(), pitch) ||
                                                session.TransferColorAttachment(token, nullptr, actual.data(), row - 1,
                                                                                pitch) != 60 ||
                                                actual != preserved ||
                                                session.TransferColorAttachment(token, nullptr, actual.data(),
                                                                                actual.size(), pitch)) {
                                                status = 205;
                                                break;
                                            }
                                            for (uint32_t y = 0; y < 1152; ++y)
                                                for (uint64_t x = 0; x < pitch; ++x)
                                                    if (actual[size_t(y * pitch + x)] !=
                                                        (x < row ? source[size_t(y * pitch + x)] : 0xd3))
                                                        status = 205;
                                            if (status) break;
                                            std::vector<unsigned char> tiled(4423680), expected;
                                            for (size_t i = 0; i < tiled.size(); ++i)
                                                tiled[i] = static_cast<unsigned char>(i * 37 + 11);
                                            CtrGraphicsImageExtentV1 rgExtent{1,    sizeof(rgExtent),  1920,
                                                                              1152, rg8.guest_address, 4423680};
                                            if (session.UploadColorTiled(token, tiled.data(), tiled.size(),
                                                                         &rgExtent) ||
                                                session.TransferColorAttachment(token, nullptr, actual.data(),
                                                                                actual.size(), pitch)) {
                                                status = 205;
                                                break;
                                            }
                                            if (ctr_with_tiling_pipelines(
                                                    device, deviceGet, tilingDirectory, true, true, true,
                                                    [&](VkPipelineLayout l, const VkPipeline* pipes,
                                                        VkDescriptorSetLayout d) -> int {
                                                        return ctr_thin_detile(device, deviceGet, q, memory, l,
                                                                               pipes[1], d, 1920, 1152, tiled.data(),
                                                                               tiled.size(), expected, false, 2);
                                                    },
                                                    2)) {
                                                status = 205;
                                                break;
                                            }
                                            for (uint32_t y = 0; y < 1152; ++y)
                                                for (uint64_t x = 0; x < pitch; ++x)
                                                    if (actual[size_t(y * pitch + x)] !=
                                                        (x < row ? expected[size_t(y * row + x)] : 0xd3))
                                                        status = 205;
                                            if (status || session.ReleaseColorAttachment(token)) {
                                                status = 205;
                                                break;
                                            }
                                            std::puts(
                                                "RG8 color attachment1920x1152 real upload/fence/readback4423680B exact rowpadding16B preserved; short60 atomic format88; tiled initialization separate public detile0diff; no MRT draw");
                                        }
                                        for (const uint32_t storageCase : {4u, 10u, 3u, 40u, 100u}) {
                                            const bool display10 = storageCase == 100, depth0 = storageCase == 40;
                                            const uint32_t storageFormat = display10 ? 10u
                                                                           : depth0  ? 4u
                                                                                     : storageCase,
                                                           pitch = depth0 ? 2048u : 1920u;
                                            const auto cs =
                                                load(storageFormat == 4
                                                         ? "build/CTRPort-data/compute_storage_full_control.spv"
                                                     : storageFormat == 3
                                                         ? "build/CTRPort-data/compute_storage_rg8_control.spv"
                                                         : "build/CTRPort-data/compute_storage_rgba_control.spv");
                                            CtrGraphicsImageParametersV1 storageParams{};
                                            storageParams.version = 1;
                                            storageParams.size = sizeof(storageParams);
                                            storageParams.type = 9;
                                            storageParams.width = 1920;
                                            storageParams.pitch = pitch;
                                            storageParams.height = 1080;
                                            storageParams.depth = storageParams.layers = storageParams.levels =
                                                storageParams.samples = 1;
                                            storageParams.data_format = storageFormat;
                                            storageParams.number_format = storageFormat == 4 ? 7u : 0u;
                                            storageParams.tile_mode = display10 ? 10u : depth0 ? 0u : 14u;
                                            storageParams.guest_address = 1;
                                            uint64_t fullImage = 999;
                                            storageParams.number_format = 6;
                                            if (ctr_video_storage_image_create_v1(&session, &storageParams,
                                                                                  &fullImage) != 88 ||
                                                fullImage != 999) {
                                                status = 203;
                                                break;
                                            }
                                            storageParams.number_format = storageFormat == 4 ? 7u : 0u;
                                            if (ctr_video_storage_image_create_v1(&session, &storageParams,
                                                                                  &fullImage)) {
                                                status = 203;
                                                break;
                                            }
                                            const uint32_t texelBytes = storageFormat == 3 ? 2u : 4u,
                                                           paddedHeight = depth0 || storageFormat == 3 ? 1152u : 1088u;
                                            CtrGraphicsImageExtentV1 extent{
                                                1,     sizeof(extent),
                                                pitch, paddedHeight,
                                                1,     uint64_t(pitch) * paddedHeight * texelBytes};
                                            std::vector<unsigned char> prior(static_cast<size_t>(extent.bytes)),
                                                output(prior.size(), 0xd3);
                                            for (size_t i = 0; i < prior.size(); ++i)
                                                prior[i] = static_cast<unsigned char>(i * 37 + 11);
                                            if (depth0) {
                                                auto invalidExtent = extent;
                                                invalidExtent.padded_height = 1088;
                                                const auto unchanged = output;
                                                if (session.UploadSampledTiled(fullImage, prior.data(),
                                                                               prior.size() - 1, &extent) != 60 ||
                                                    session.UploadSampledTiled(fullImage, prior.data(), prior.size(),
                                                                               &invalidExtent) != 88 ||
                                                    ctr_video_image_readback_tiled_v1(&session, fullImage, prior.data(),
                                                                                      prior.size() - 1, &extent,
                                                                                      output.data()) != 60 ||
                                                    output != unchanged ||
                                                    ctr_video_image_readback_tiled_v1(&session, fullImage, prior.data(),
                                                                                      prior.size(), &invalidExtent,
                                                                                      output.data()) != 88 ||
                                                    output != unchanged) {
                                                    status = 217;
                                                    break;
                                                }
                                            }
                                            if (session.UploadSampledTiled(fullImage, prior.data(), prior.size(),
                                                                           &extent)) {
                                                status = 203;
                                                break;
                                            }
                                            CtrGraphicsBindingContractV1 bindings{};
                                            bindings.version = 1;
                                            bindings.size = sizeof(bindings);
                                            bindings.unified_after = 2;
                                            bindings.buffer_after = 1;
                                            bindings.descriptor_count = 2;
                                            bindings.descriptors[0] = {0, 8, 0, 1, 0, 1};
                                            bindings.descriptors[1] = {1, 0, 1, 1, UINT32_MAX, 1};
                                            const uint32_t source[] = {7, 11, 13, 17};
                                            uint64_t fullBuffer = 0, fullLayout = 0, fullPipeline = 0;
                                            if (session.CreateBuffer(source, sizeof(source), 1, &fullBuffer) ||
                                                ctr_video_compute_layout_create_v1(&session, &bindings, &fullLayout) ||
                                                ctr_video_compute_pipeline_create_v1(&session, fullLayout, cs.data(),
                                                                                     static_cast<uint32_t>(cs.size()),
                                                                                     &fullPipeline)) {
                                                status = 203;
                                                break;
                                            }
                                            CtrGraphicsBufferWriteV1 write{0, 0, 1, 0, fullBuffer};
                                            const uint32_t imageBinding = 1;
                                            CtrGraphicsPushDataV1 push{};
                                            push.version = 1;
                                            push.size = sizeof(push);
                                            const uint32_t value = 789;
                                            std::memcpy(push.bytes, &value, 4);
                                            const uint32_t groups[] = {240, 135, 1};
                                            if (session.WriteGraphicsBuffers(fullLayout, &write, 1) ||
                                                session.WriteGraphicsImages(fullLayout, &imageBinding, &fullImage, 1) ||
                                                ctr_video_compute_dispatch_v1(&session, fullPipeline, &push, groups) ||
                                                ctr_video_image_readback_tiled_v1(&session, fullImage, prior.data(),
                                                                                  prior.size(), &extent,
                                                                                  output.data())) {
                                                status = 203;
                                                break;
                                            }
                                            if (ctr_with_tiling_pipelines(
                                                    device, deviceGet, tilingDirectory, true, true, !display10,
                                                    [&](VkPipelineLayout tiledLayout, const VkPipeline* pipelines,
                                                        VkDescriptorSetLayout descriptorLayout) -> int {
                                                        std::vector<unsigned char> originalLinear, actualLinear;
                                                        int check = ctr_thin_detile(
                                                            device, deviceGet, q, memory, tiledLayout, pipelines[1],
                                                            descriptorLayout, pitch, paddedHeight, prior.data(),
                                                            prior.size(), originalLinear, false, texelBytes);
                                                        if (check) return check;
                                                        check = ctr_thin_detile(
                                                            device, deviceGet, q, memory, tiledLayout, pipelines[1],
                                                            descriptorLayout, pitch, paddedHeight, output.data(),
                                                            output.size(), actualLinear, false, texelBytes);
                                                        if (check) return check;
                                                        for (uint32_t y = 0; y < 1080; ++y)
                                                            for (uint32_t x = 0; x < 1920; ++x) {
                                                                const auto* pixel =
                                                                    actualLinear.data() +
                                                                    (size_t(y) * pitch + x) * texelBytes;
                                                                if (storageFormat == 4) {
                                                                    float actual = 0;
                                                                    std::memcpy(&actual, pixel, 4);
                                                                    if (actual !=
                                                                        float(789 + x + y * 1920 + source[(x + y) & 3]))
                                                                        return 203;
                                                                } else if (storageFormat == 3) {
                                                                    if (pixel[0] != (x & 255) || pixel[1] != (y & 255))
                                                                        return 203;
                                                                } else if (pixel[0] != (x & 255) ||
                                                                           pixel[1] != (y & 255) ||
                                                                           pixel[2] !=
                                                                               ((789 + source[(x + y) & 3]) & 255) ||
                                                                           pixel[3] != 255)
                                                                    return 203;
                                                            }
                                                        for (uint32_t y = 0; y < paddedHeight; ++y)
                                                            for (uint32_t x = 0; x < pitch; ++x)
                                                                if (y >= 1080 || x >= 1920)
                                                                    for (uint32_t c = 0; c < texelBytes; ++c) {
                                                                        const size_t i =
                                                                            (size_t(y) * pitch + x) * texelBytes + c;
                                                                        if (actualLinear[i] != originalLinear[i])
                                                                            return 203;
                                                                    }
                                                        return 0;
                                                    },
                                                    texelBytes, depth0)) {
                                                status = 203;
                                                break;
                                            }
                                            if (ctr_video_compute_pipeline_release_v1(&session, fullPipeline) ||
                                                session.ReleaseGraphicsLayout(fullLayout) ||
                                                session.ReleaseBuffer(fullBuffer) ||
                                                session.ReleaseSampledImage(fullImage)) {
                                                status = 203;
                                                break;
                                            }
                                            if (display10) {
                                                CtrGraphicsColorRequirementV1 demand{
                                                    1, sizeof(demand), 0, 1920, 1088, 1, 1, 10, 10, 0, 1, 8355840};
                                                uint64_t color = 0;
                                                if (session.CreateColorAttachment(&demand, 0, &color)) {
                                                    status = 256;
                                                    break;
                                                }
                                                auto wrong = extent;
                                                wrong.guest_address = 2;
                                                if (session.UploadColorTiled(color, output.data(), output.size(),
                                                                             &wrong) != 88 ||
                                                    session.UploadColorTiled(color, output.data(), output.size() - 1,
                                                                             &extent) != 60 ||
                                                    session.UploadColorTiled(color, output.data(), output.size(),
                                                                             &extent)) {
                                                    status = 256;
                                                    break;
                                                }
                                                std::vector<unsigned char> actual(output.size()), expected;
                                                if (session.TransferColorAttachment(color, nullptr, actual.data(),
                                                                                    actual.size(), 1920 * 4) ||
                                                    ctr_with_tiling_pipelines(
                                                        device, deviceGet, tilingDirectory, true, true, false,
                                                        [&](VkPipelineLayout l, const VkPipeline* pp,
                                                            VkDescriptorSetLayout d) -> int {
                                                            return ctr_thin_detile(device, deviceGet, q, memory, l,
                                                                                   pp[1], d, 1920, 1088, output.data(),
                                                                                   output.size(), expected);
                                                        }) ||
                                                    actual != expected || session.ReleaseColorAttachment(color)) {
                                                    status = 256;
                                                    break;
                                                }
                                                std::puts(
                                                    "Display2DThin tile10 color upload actual1920x1088 full8355840B GPUdetile attachment readback0diff wrongAddress88 short60");
                                            }
                                            if (display10)
                                                std::puts(
                                                    "Display2DThin tile10 RGBA8 storage1920x1080 pitch1920 padded1088 GPUdispatch full8355840B writeback logical/allpadding0diff");
                                            else if (depth0)
                                                std::puts(
                                                    "Depth0 R32float actual1920x1080 pitch2048 padded1152 GPUstorage tiled writeback9437184B logical/row/bottom padding0diff");
                                            else
                                                std::printf(
                                                    "storage dataformat%u original755/869 shape authored1920x1080/pitch1920/bounded padded extent readonly16B/workgroup8x8/groups240x135 GPUdispatch;2073600pixels/allpadding0diff real tiled writeback snapshot; unsupportedformat88 atomic no guestwrite\n",
                                                    storageFormat);
                                        }
                                        if (ctr_with_tiling_pipelines(
                                                device, deviceGet, tilingDirectory, true, true, true,
                                                [&](VkPipelineLayout tiledLayout, const VkPipeline* pipelines,
                                                    VkDescriptorSetLayout descriptorLayout) -> int {
                                                    std::vector<unsigned char> linear(2097152), tiled, roundtrip;
                                                    for (size_t i = 0; i < linear.size(); ++i)
                                                        linear[i] = static_cast<unsigned char>((i * 37) ^ (i >> 8));
                                                    int rc =
                                                        ctr_thin_detile(device, deviceGet, q, memory, tiledLayout,
                                                                        pipelines[0], descriptorLayout, 1024, 1024,
                                                                        linear.data(), linear.size(), tiled, true, 2);
                                                    if (rc) return rc;
                                                    rc = ctr_thin_detile(device, deviceGet, q, memory, tiledLayout,
                                                                         pipelines[1], descriptorLayout, 1024, 1024,
                                                                         tiled.data(), tiled.size(), roundtrip, false,
                                                                         2);
                                                    if (rc) return rc;
                                                    return roundtrip == linear && tiled != linear ? 0 : 227;
                                                },
                                                2, true)) {
                                            status = 227;
                                            break;
                                        }
                                        std::puts(
                                            "Depth0 D16 actual1024x1024 public GPU inverse2097152B0diff guards512 unchanged no image download or guestpublication");
                                        if (ctr_with_tiling_pipelines(
                                                device, deviceGet, tilingDirectory, true, true, true,
                                                [&](VkPipelineLayout l, const VkPipeline* p,
                                                    VkDescriptorSetLayout d) -> int {
                                                    std::vector<unsigned char> linear(17694720), tiled, roundtrip;
                                                    for (size_t pixel = 0; pixel < 1920u * 1152u; ++pixel) {
                                                        uint32_t a = static_cast<uint32_t>(pixel) ^ 0x13579bdfu, b = ~a;
                                                        std::memcpy(linear.data() + pixel * 8, &a, 4);
                                                        std::memcpy(linear.data() + pixel * 8 + 4, &b, 4);
                                                    }
                                                    int rc = ctr_thin_detile(device, deviceGet, q, memory, l, p[0], d,
                                                                             1920, 1152, linear.data(), linear.size(),
                                                                             tiled, true, 4, 2);
                                                    if (rc) return rc;
                                                    rc = ctr_thin_detile(device, deviceGet, q, memory, l, p[1], d, 1920,
                                                                         1152, tiled.data(), tiled.size(), roundtrip,
                                                                         false, 4, 2);
                                                    if (rc) return rc;
                                                    return roundtrip == linear && tiled != linear ? 0 : 230;
                                                },
                                                4, false, 2)) {
                                            status = 230;
                                            break;
                                        }
                                        std::puts(
                                            "Depth1 MS2 full1920x1152 interleaved unequal sample pair17694720B public address functions GPU inverse0diff guards512; no depth image export or guestpublication");
                                        if (ctr_with_tiling_pipelines(
                                                device, deviceGet, tilingDirectory, true, true, true,
                                                [&](VkPipelineLayout l, const VkPipeline* pp,
                                                    VkDescriptorSetLayout d) -> int {
                                                    // Block grids include all census base levels and the second64x64 mip.
                                                    const uint32_t grids[][2] = {{8, 8},     {216, 280}, {16, 16},
                                                                                 {8, 8},     {256, 256}, {64, 64},
                                                                                 {344, 136}, {32, 32}};
                                                    for (const auto& grid : grids) {
                                                        const uint32_t w = grid[0], h = grid[1];
                                                        std::vector<unsigned char> linear(size_t(w) * h * 16),
                                                            expected(linear.size()), tiled, roundtrip;
                                                        for (uint32_t y = 0; y < h; ++y)
                                                            for (uint32_t x = 0; x < w; ++x) {
                                                                const uint32_t morton =
                                                                    (x & 1) | ((y & 1) << 1) | ((x & 2) << 1) |
                                                                    ((y & 2) << 2) | ((x & 4) << 2) | ((y & 4) << 3);
                                                                const size_t target =
                                                                    ((size_t(y / 8) * (w / 8) + x / 8) * 64 + morton) *
                                                                    16;
                                                                for (uint32_t c = 0; c < 4; ++c) {
                                                                    const uint32_t value =
                                                                        (y * w + x) * 0x9e3779b9u + c * 0x13579bdu;
                                                                    std::memcpy(linear.data() +
                                                                                    (size_t(y) * w + x) * 16 + c * 4,
                                                                                &value, 4);
                                                                    std::memcpy(expected.data() + target + c * 4,
                                                                                &value, 4);
                                                                }
                                                            }
                                                        int rc = ctr_thin_detile(device, deviceGet, q, memory, l, pp[0],
                                                                                 d, w, h, linear.data(), linear.size(),
                                                                                 tiled, true, 16, 1, true);
                                                        if (rc) return rc;
                                                        if (tiled != expected) return 242;
                                                        rc = ctr_thin_detile(device, deviceGet, q, memory, l, pp[1], d,
                                                                             w, h, expected.data(), expected.size(),
                                                                             roundtrip, false, 16, 1, true);
                                                        if (rc) return rc;
                                                        if (roundtrip != linear) return 242;
                                                        const auto saved = roundtrip;
                                                        if (ctr_thin_detile(device, deviceGet, q, memory, l, pp[1], d,
                                                                            w, h, expected.data(), expected.size() - 1,
                                                                            roundtrip, false, 16, 1, true) != 60 ||
                                                            roundtrip != saved)
                                                            return 242;
                                                    }
                                                    return 0;
                                                },
                                                16, false, 1, true)) {
                                            status = 242;
                                            break;
                                        }
                                        std::puts(
                                            "BC3 public128 micro13 GPU8 block grids full address permutation and detile0diff guards512 short60atomic; compressed image upload still missing");
                                        if (ctr_with_tiling_pipelines(
                                                device, deviceGet, tilingDirectory, true, true, true,
                                                [&](VkPipelineLayout l, const VkPipeline* pp,
                                                    VkDescriptorSetLayout d) -> int {
                                                    // All distinct padded block grids in the exact BC1 mip chains.
                                                    const uint32_t grids[][2] = {{64, 64}, {32, 32}, {16, 16}, {8, 8}};
                                                    for (const auto& grid : grids) {
                                                        const uint32_t w = grid[0], h = grid[1];
                                                        std::vector<unsigned char> linear(size_t(w) * h * 8),
                                                            expected(linear.size()), tiled, roundtrip;
                                                        for (uint32_t y = 0; y < h; ++y)
                                                            for (uint32_t x = 0; x < w; ++x) {
                                                                const uint32_t morton =
                                                                    (x & 1) | ((y & 1) << 1) | ((x & 2) << 1) |
                                                                    ((y & 2) << 2) | ((x & 4) << 2) | ((y & 4) << 3);
                                                                const size_t target =
                                                                    ((size_t(y / 8) * (w / 8) + x / 8) * 64 + morton) *
                                                                    8;
                                                                for (uint32_t c = 0; c < 2; ++c) {
                                                                    const uint32_t value =
                                                                        (y * w + x) * 0x9e3779b9u + c * 0x13579bdu;
                                                                    std::memcpy(linear.data() +
                                                                                    (size_t(y) * w + x) * 8 + c * 4,
                                                                                &value, 4);
                                                                    std::memcpy(expected.data() + target + c * 4,
                                                                                &value, 4);
                                                                }
                                                            }
                                                        int rc = ctr_thin_detile(device, deviceGet, q, memory, l, pp[0],
                                                                                 d, w, h, linear.data(), linear.size(),
                                                                                 tiled, true, 8, 1, true);
                                                        if (rc) return rc;
                                                        if (tiled != expected) return 242;
                                                        rc = ctr_thin_detile(device, deviceGet, q, memory, l, pp[1], d,
                                                                             w, h, expected.data(), expected.size(),
                                                                             roundtrip, false, 8, 1, true);
                                                        if (rc) return rc;
                                                        if (roundtrip != linear) return 242;
                                                        const auto saved = roundtrip;
                                                        if (ctr_thin_detile(device, deviceGet, q, memory, l, pp[1], d,
                                                                            w, h, expected.data(), expected.size() - 1,
                                                                            roundtrip, false, 8, 1, true) != 60 ||
                                                            roundtrip != saved)
                                                            return 242;
                                                    }
                                                    return 0;
                                                },
                                                8, false, 1, true)) {
                                            status = 242;
                                            break;
                                        }
                                        std::puts(
                                            "BC1 public64 micro13 GPU4 block grids full address permutation and detile0diff guards512 short60atomic; compressed image upload still missing");
                                        {
                                            const uint32_t bc3Shapes[][5] = {
                                                {4, 4, 32, 1, 0},      {860, 1120, 864, 1, 0},
                                                {64, 64, 64, 2, 1},    {1024, 1024, 1024, 1, 0},
                                                {256, 256, 256, 1, 0}, {1360, 520, 1376, 1, 0},
                                                {120, 120, 128, 1, 0}};
                                            for (const auto& bc3Shape : bc3Shapes) {
                                                CtrGraphicsImageParametersV1 b{};
                                                b.version = 1;
                                                b.size = sizeof(b);
                                                b.type = 9;
                                                b.width = bc3Shape[0];
                                                b.height = bc3Shape[1];
                                                b.pitch = bc3Shape[2];
                                                b.levels = bc3Shape[3];
                                                b.pow2pad = bc3Shape[4];
                                                b.depth = b.layers = b.samples = 1;
                                                b.data_format = 37;
                                                b.tile_mode = 13;
                                                b.guest_address = 0x100000;
                                                for (uint32_t c = 0; c < 4; ++c) b.swizzle[c] = c + 4;
                                                CtrGraphicsImageExtentV1 e{1, sizeof(e), 0, 0, b.guest_address, 0};
                                                std::vector<unsigned char> source, expected;
                                                for (uint32_t mip = 0; mip < b.levels; ++mip) {
                                                    uint32_t w = std::max(((b.pitch >> mip) + 3) / 4, 1u),
                                                             h = std::max(((b.height >> mip) + 3) / 4, 1u);
                                                    if (b.pow2pad) {
                                                        uint32_t a = 1, z = 1;
                                                        while (a < w) a *= 2;
                                                        while (z < h) z *= 2;
                                                        w = a;
                                                        h = z;
                                                    }
                                                    w = (w + 7) & ~7u;
                                                    h = (h + 7) & ~7u;
                                                    if (!mip) {
                                                        e.pitch = w * 4;
                                                        e.padded_height = h * 4;
                                                    }
                                                    const size_t offset = source.size(), size = size_t(w) * h * 16;
                                                    source.resize(offset + size);
                                                    expected.resize(offset + size, static_cast<unsigned char>(0x5a));
                                                    for (size_t pos = 0; pos < size; ++pos)
                                                        source[offset + pos] =
                                                            static_cast<unsigned char>((pos * 19 + mip * 71 + 3) & 255);
                                                    const uint32_t activeW = (std::max(b.width >> mip, 1u) + 3) / 4,
                                                                   activeH = (std::max(b.height >> mip, 1u) + 3) / 4;
                                                    for (uint32_t y = 0; y < activeH; ++y)
                                                        for (uint32_t x = 0; x < activeW; ++x) {
                                                            const uint32_t morton = (x & 1) | ((y & 1) << 1) |
                                                                                    ((x & 2) << 1) | ((y & 2) << 2) |
                                                                                    ((x & 4) << 2) | ((y & 4) << 3);
                                                            const size_t address =
                                                                offset +
                                                                ((size_t(y / 8) * (w / 8) + x / 8) * 64 + morton) * 16;
                                                            std::memcpy(expected.data() + address,
                                                                        source.data() + address, 16);
                                                        }
                                                }
                                                e.bytes = source.size();
                                                uint64_t image = 0;
                                                if (!enabledGraphicsCore.textureCompressionBC ||
                                                    session.CreateSampledImage(&b, &image)) {
                                                    status = 243;
                                                    break;
                                                }
                                                auto wrong = e;
                                                wrong.guest_address++;
                                                if (session.UploadSampledTiled(image, source.data(), source.size() - 1,
                                                                               &e) != 60 ||
                                                    session.UploadSampledTiled(image, source.data(), source.size(),
                                                                               &wrong) != 88 ||
                                                    session.UploadSampledTiled(image, source.data(), source.size(),
                                                                               &e)) {
                                                    status = 243;
                                                    break;
                                                }
                                                std::vector<unsigned char> prior(source.size(),
                                                                                 static_cast<unsigned char>(0x5a)),
                                                    out(source.size(), static_cast<unsigned char>(0xab));
                                                if (session.ReadbackImageTiled(image, prior.data(), prior.size(), &e,
                                                                               out.data()) ||
                                                    out != expected) {
                                                    status = 244;
                                                    break;
                                                }
                                                std::fill(out.begin(), out.end(), static_cast<unsigned char>(0xab));
                                                const auto saved = out;
                                                if (session.ReadbackImageTiled(image, prior.data(), prior.size() - 1,
                                                                               &e, out.data()) != 60 ||
                                                    out != saved ||
                                                    session.ReadbackImageTiled(image, prior.data(), prior.size(),
                                                                               &wrong, out.data()) != 88 ||
                                                    out != saved ||
                                                    session.ReadbackSampledImage(image, out.data(), out.size(),
                                                                                 e.pitch * 4) != 88 ||
                                                    session.ReleaseSampledImage(image)) {
                                                    status = 245;
                                                    break;
                                                }
                                            }
                                            if (status) break;
                                            std::puts(
                                                "BC3 compressed image7 shapes mip2 actual GPU upload/readback full tiled active blocks0diff prior padding preserved short60/address88 atomic linear88");
                                        }
                                        {
                                            for (uint32_t cube = 0; cube < 2; ++cube) {
                                                CtrGraphicsImageParametersV1 b{};
                                                b.version = 1;
                                                b.size = sizeof(b);
                                                b.type = cube ? 11u : 9u;
                                                b.width = b.height = b.pitch = cube ? 128u : 256u;
                                                b.depth = b.samples = 1;
                                                b.layers = cube ? 8u : 1u;
                                                b.levels = cube ? 8u : 5u;
                                                b.last_array = cube ? 5u : 0u;
                                                b.pow2pad = 1;
                                                b.data_format = 35;
                                                b.tile_mode = 13;
                                                b.guest_address = 0x100000;
                                                for (uint32_t c = 0; c < 4; ++c) b.swizzle[c] = c + 4;
                                                CtrPm4::Bc1Layout plan{};
                                                if (CtrPm4::Bc1LayoutFor(b, plan) ||
                                                    plan.bytes != (cube ? 106496u : 44032u)) {
                                                    status = 247;
                                                    break;
                                                }
                                                CtrGraphicsImageExtentV1 e{1,
                                                                           sizeof(e),
                                                                           plan.mips[0].pitch * 4,
                                                                           plan.mips[0].height * 4,
                                                                           b.guest_address,
                                                                           plan.bytes};
                                                std::vector<unsigned char> source(size_t(plan.bytes)),
                                                    prior(size_t(plan.bytes), 0x5a), expected = prior,
                                                                                     out(size_t(plan.bytes) + 32, 0xab);
                                                for (uint32_t mip = 0; mip < b.levels; ++mip)
                                                    for (uint32_t layer = 0; layer < b.layers; ++layer) {
                                                        const auto& ml = plan.mips[mip];
                                                        const uint32_t active = std::max(1u, (b.width >> mip) + 3) / 4;
                                                        const uint32_t blocks = std::max(active, 1u);
                                                        for (uint32_t y = 0; y < ml.height; ++y)
                                                            for (uint32_t x = 0; x < ml.pitch; ++x) {
                                                                const uint32_t morton =
                                                                    (x & 1) | ((y & 1) << 1) | ((x & 2) << 1) |
                                                                    ((y & 2) << 2) | ((x & 4) << 2) | ((y & 4) << 3);
                                                                const size_t index =
                                                                    size_t(ml.offset + layer * ml.slice_bytes) +
                                                                    ((size_t(y / 8) * (ml.pitch / 8) + x / 8) * 64 +
                                                                     morton) *
                                                                        8;
                                                                for (uint32_t c = 0; c < 8; ++c) {
                                                                    const auto value = static_cast<unsigned char>(
                                                                        (mip * 29 + layer * 47 + y * 13 + x * 7 +
                                                                         c * 31) &
                                                                        255);
                                                                    source[index + c] = value;
                                                                    if (x < blocks && y < blocks)
                                                                        expected[index + c] = value;
                                                                }
                                                            }
                                                    }
                                                for (uint32_t field = 0; field < 10; ++field) {
                                                    auto invalid = b;
                                                    switch (field) {
                                                    case 0: invalid.width++; break;
                                                    case 1: invalid.height++; break;
                                                    case 2: invalid.pitch++; break;
                                                    case 3: invalid.layers++; break;
                                                    case 4: invalid.levels++; break;
                                                    case 5: invalid.last_array++; break;
                                                    case 6: invalid.swizzle[0] = 0; break;
                                                    case 7: invalid.tile_mode = 14; break;
                                                    case 8: invalid.pow2pad = 0; break;
                                                    default: invalid.base_array = 1; break;
                                                    }
                                                    uint64_t untouched = 999;
                                                    if (session.CreateSampledImage(&invalid, &untouched) != 88 ||
                                                        untouched != 999)
                                                        status = 247;
                                                }
                                                if (status) break;
                                                uint64_t image = 0;
                                                auto wrong = e;
                                                wrong.guest_address++;
                                                if (session.CreateSampledImage(&b, &image) ||
                                                    session.UploadSampledTiled(image, source.data(), source.size() - 1,
                                                                               &e) != 60 ||
                                                    session.UploadSampledTiled(image, source.data(), source.size(),
                                                                               &wrong) != 88 ||
                                                    session.UploadSampledTiled(image, source.data(), source.size(),
                                                                               &e) ||
                                                    session.ReadbackImageTiled(image, prior.data(), prior.size(), &e,
                                                                               out.data()) ||
                                                    !std::equal(expected.begin(), expected.end(), out.begin()) ||
                                                    !std::all_of(out.end() - 32, out.end(),
                                                                 [](unsigned char c) { return c == 0xab; })) {
                                                    status = 247;
                                                    break;
                                                }
                                                const auto savedOut = out;
                                                if (session.ReadbackImageTiled(image, prior.data(), prior.size() - 1,
                                                                               &e, out.data()) != 60 ||
                                                    out != savedOut ||
                                                    session.ReadbackImageTiled(image, prior.data(), prior.size(),
                                                                               &wrong, out.data()) != 88 ||
                                                    out != savedOut) {
                                                    status = 247;
                                                    break;
                                                }
                                                // Endpoint0 at every block: exact primary/white UNORM results, independent of BC1 interpolation rounding.
                                                for (uint32_t mip = 0; mip < b.levels; ++mip)
                                                    for (uint32_t layer = 0; layer < b.layers; ++layer) {
                                                        const auto& ml = plan.mips[mip];
                                                        const uint16_t colors[] = {0xf800, 0x07e0, 0x001f, 0xffff};
                                                        const uint16_t color = colors[(mip + layer) % 4];
                                                        for (size_t at = size_t(ml.offset + layer * ml.slice_bytes);
                                                             at < ml.offset + (layer + 1) * ml.slice_bytes; at += 8) {
                                                            std::memcpy(source.data() + at, &color, 2);
                                                            std::memcpy(source.data() + at + 2, &color, 2);
                                                            std::memset(source.data() + at + 4, 0, 4);
                                                        }
                                                    }
                                                const auto cs =
                                                    load(cube ? "build/CTRPort-data/compute_sampled_bc1_1.spv"
                                                              : "build/CTRPort-data/compute_sampled_bc1_0.spv");
                                                std::vector<unsigned char> initial(6 * 8 * 16, 0),
                                                    sampled(initial.size() + 32, 0xa7);
                                                CtrGraphicsBindingContractV1 binding{};
                                                binding.version = 1;
                                                binding.size = sizeof(binding);
                                                binding.unified_after = 2;
                                                binding.buffer_after = 1;
                                                binding.descriptor_count = 2;
                                                binding.descriptors[0] = {0, 0, 0, 1, 0, 1};
                                                binding.descriptors[1] = {1, 0, 1, 1, UINT32_MAX, 0};
                                                uint64_t buffer = 0, layout = 0, pipeline = 0;
                                                if (session.UploadSampledTiled(image, source.data(), source.size(),
                                                                               &e) ||
                                                    session.CreateBuffer(initial.data(), initial.size(), 1, &buffer) ||
                                                    ctr_video_compute_layout_create_v1(&session, &binding, &layout) ||
                                                    ctr_video_compute_pipeline_create_v1(
                                                        &session, layout, cs.data(), static_cast<uint32_t>(cs.size()),
                                                        &pipeline)) {
                                                    status = 248;
                                                    break;
                                                }
                                                CtrGraphicsBufferWriteV1 write{0, 0, 1, 0, buffer};
                                                uint32_t imageBinding = 1, groups[] = {1, cube ? 6u : 1u, 1};
                                                CtrGraphicsPushDataV1 push{};
                                                push.version = 1;
                                                push.size = sizeof(push);
                                                if (session.WriteGraphicsBuffers(layout, &write, 1) ||
                                                    session.WriteGraphicsImages(layout, &imageBinding, &image, 1) ||
                                                    ctr_video_compute_dispatch_v1(&session, pipeline, &push, groups) ||
                                                    session.CopyBufferInitial(buffer, sampled.data() + 16,
                                                                              initial.size())) {
                                                    status = 248;
                                                    break;
                                                }
                                                for (uint32_t layer = 0; layer < (cube ? 6u : 1u); ++layer)
                                                    for (uint32_t mip = 0; mip < b.levels; ++mip) {
                                                        uint32_t actual[4]{};
                                                        std::memcpy(actual,
                                                                    sampled.data() + 16 + (layer * 8 + mip) * 16, 16);
                                                        const uint32_t color = (mip + layer) % 4;
                                                        for (uint32_t c = 0; c < 4; ++c)
                                                            if (actual[c] != ((c == 3 || color == 3 || color == c)
                                                                                  ? 0x3f800000u
                                                                                  : 0u))
                                                                status = 248;
                                                    }
                                                for (size_t i = 0; i < 16; ++i)
                                                    if (sampled[i] != 0xa7 || sampled[initial.size() + 16 + i] != 0xa7)
                                                        status = 248;
                                                if (status ||
                                                    ctr_video_compute_pipeline_release_v1(&session, pipeline) ||
                                                    session.ReleaseGraphicsLayout(layout) ||
                                                    session.ReleaseBuffer(buffer) ||
                                                    session.ReleaseSampledImage(image)) {
                                                    status = 248;
                                                    break;
                                                }
                                            }
                                            if (status) break;
                                            std::puts(
                                                "BC1 full mip layered GPU transfer flat44032 cube106496 active0diff priorpadding guards32 short60 address88; sampled5mips and6faces8mips exactRGBA endpoint0 guards32");
                                        }
                                        {
                                            CtrGraphicsImageParametersV1 rgbaMicro{};
                                            rgbaMicro.version = 1;
                                            rgbaMicro.size = sizeof(rgbaMicro);
                                            rgbaMicro.type = 9;
                                            rgbaMicro.width = rgbaMicro.pitch = 1024;
                                            rgbaMicro.height = 2308;
                                            rgbaMicro.depth = rgbaMicro.layers = rgbaMicro.samples = rgbaMicro.levels =
                                                1;
                                            rgbaMicro.data_format = 10;
                                            rgbaMicro.tile_mode = 13;
                                            rgbaMicro.guest_address = 0x100000;
                                            for (uint32_t c = 0; c < 4; ++c) rgbaMicro.swizzle[c] = c + 4;
                                            const CtrGraphicsImageExtentV1 microExtent{
                                                1, sizeof(microExtent), 1024, 2312, rgbaMicro.guest_address, 9469952};
                                            std::vector<unsigned char> linear(9469952), tiled,
                                                prior(9469952, static_cast<unsigned char>(0x5a)), expected,
                                                download(9469952, static_cast<unsigned char>(0x5a));
                                            for (size_t pos = 0; pos < linear.size(); ++pos)
                                                linear[pos] =
                                                    static_cast<unsigned char>((pos * 13 + pos / 4096 * 37 + 9) & 255);
                                            int microRc = ctr_with_tiling_pipelines(
                                                device, deviceGet, tilingDirectory, true, true, true,
                                                [&](VkPipelineLayout l, const VkPipeline* pp,
                                                    VkDescriptorSetLayout d) -> int {
                                                    int result = ctr_thin_detile(
                                                        device, deviceGet, q, memory, l, pp[0], d, 1024, 2312,
                                                        linear.data(), linear.size(), tiled, true, 4, 1, true);
                                                    if (result) return result;
                                                    std::vector<unsigned char> expectedLinear = prior;
                                                    std::memcpy(expectedLinear.data(), linear.data(),
                                                                size_t(1024) * 2308 * 4);
                                                    return ctr_thin_detile(device, deviceGet, q, memory, l, pp[0], d,
                                                                           1024, 2312, expectedLinear.data(),
                                                                           expectedLinear.size(), expected, true, 4, 1,
                                                                           true);
                                                },
                                                4, false, 1, true);
                                            uint64_t microImage = 0;
                                            auto invalid = microExtent;
                                            invalid.guest_address++;
                                            if (microRc || session.CreateSampledImage(&rgbaMicro, &microImage) ||
                                                session.UploadSampledTiled(microImage, tiled.data(), tiled.size() - 1,
                                                                           &microExtent) != 60 ||
                                                session.UploadSampledTiled(microImage, tiled.data(), tiled.size(),
                                                                           &invalid) != 88 ||
                                                session.UploadSampledTiled(microImage, tiled.data(), tiled.size(),
                                                                           &microExtent) ||
                                                session.ReadbackSampledImage(microImage, download.data(),
                                                                             download.size(), 4096)) {
                                                status = 246;
                                                break;
                                            }
                                            if (std::memcmp(download.data(), linear.data(), size_t(1024) * 2308 * 4) ||
                                                !std::all_of(download.begin() + size_t(1024) * 2308 * 4, download.end(),
                                                             [](unsigned char c) { return c == 0x5a; })) {
                                                status = 246;
                                                break;
                                            }
                                            std::vector<unsigned char> output(9469952,
                                                                              static_cast<unsigned char>(0xab));
                                            const auto untouched = output;
                                            if (session.ReadbackImageTiled(microImage, prior.data(), prior.size() - 1,
                                                                           &microExtent, output.data()) != 60 ||
                                                output != untouched ||
                                                session.ReadbackImageTiled(microImage, prior.data(), prior.size(),
                                                                           &invalid, output.data()) != 88 ||
                                                output != untouched ||
                                                session.ReadbackImageTiled(microImage, prior.data(), prior.size(),
                                                                           &microExtent, output.data()) ||
                                                output != expected || session.ReleaseSampledImage(microImage)) {
                                                status = 246;
                                                break;
                                            }
                                            std::puts(
                                                "RGBA8 micro13 actual1024x2308 padded2312 upload9469952B active rows0diff padding4 preserved GPUretile0diff short60/address88atomic");
                                        }
                                        CtrGraphicsBindingContractV1 ps{}, vertex{};
                                        ps.version = vertex.version = 1;
                                        ps.size = vertex.size = sizeof(ps);
                                        uint64_t layout = 0, pipeline = 999;
                                        CtrGraphicsKeyV1 key{};
                                        key.version = 1;
                                        key.size = sizeof(key);
                                        key.primitive_type = 17;
                                        key.polygon_mode = 2;
                                        key.logic_op = 0xcc;
                                        key.num_samples = key.depth_samples = 2;
                                        key.z_format = 3;
                                        key.stencil_format = 1;
                                        if (session.CreateGraphicsLayout(&ps, &vertex, &layout)) {
                                            status = 195;
                                            break;
                                        }
                                        const auto createRectDepth = [&](uint64_t depthOwner, uint32_t vertexWords) {
                                            return ctr_video_graphics_rect_depth_pipeline_create_v1(
                                                &session, &key, layout, depthOwner, vs.data(), vertexWords, tcs.data(),
                                                static_cast<uint32_t>(tcs.size()), tes.data(),
                                                static_cast<uint32_t>(tes.size()), &pipeline);
                                        };
                                        key.num_samples = 1;
                                        if (createRectDepth(depthToken, static_cast<uint32_t>(vs.size())) != 88 ||
                                            pipeline != 999) {
                                            status = 195;
                                            break;
                                        }
                                        key.num_samples = 2;
                                        if (createRectDepth(depthToken, 4) != 60 || pipeline != 999 ||
                                            createRectDepth(UINT64_MAX, static_cast<uint32_t>(vs.size())) != 62 ||
                                            pipeline != 999) {
                                            status = 195;
                                            break;
                                        }
                                        int wrong = 0;
                                        std::thread worker([&] {
                                            wrong = createRectDepth(depthToken, static_cast<uint32_t>(vs.size()));
                                        });
                                        worker.join();
                                        if (wrong != 116 || pipeline != 999 ||
                                            createRectDepth(depthToken, static_cast<uint32_t>(vs.size())) ||
                                            pipeline == 999) {
                                            status = 195;
                                            break;
                                        }
                                        CtrGraphicsViewportStateV1 vp{};
                                        vp.version = 1;
                                        vp.size = sizeof(vp);
                                        vp.count = 1;
                                        vp.viewports[0] = {0, 1, 64, -1, 0, 1};
                                        vp.scissors[0].extent = {64, 1};
                                        CtrGraphicsDynamicStateV1 ds{};
                                        ds.version = 1;
                                        ds.size = sizeof(ds);
                                        ds.line_width = 1;
                                        ds.depth_test = ds.depth_write = ds.stencil_test = 1;
                                        ds.depth_compare = 1;
                                        ds.front.compare_op = ds.back.compare_op = 2;
                                        ds.front.reference = ds.back.reference = 7;
                                        ds.front.compare_mask = ds.back.compare_mask = 255;
                                        ds.front.write_mask = ds.back.write_mask = 255;
                                        CtrGraphicsPushDataV1 push{};
                                        push.version = 1;
                                        push.size = sizeof(push);
                                        CtrGraphicsDrawV1 draw{};
                                        draw.version = 1;
                                        draw.size = sizeof(draw);
                                        draw.vertex_count = 3;
                                        draw.instance_count = 1;
                                        uint64_t visible = 999;
                                        // Previous clear0.25/stencil7: authored VS depth0.5 fails LESS.
                                        if (session.DrawRectListDepth(pipeline, depthToken, &vp, &ds, &push, &draw,
                                                                      &visible) ||
                                            visible != 0) {
                                            status = 196;
                                            break;
                                        }
                                        if (session.ClearDepthAttachment(depthToken, 0x3f800000, 7)) {
                                            status = 196;
                                            break;
                                        }
                                        visible = 0;
                                        if (session.DrawRectListDepth(pipeline, depthToken, &vp, &ds, &push, &draw,
                                                                      &visible) ||
                                            !visible) {
                                            status = 196;
                                            break;
                                        }
                                        if (ctr_video_queue_memory_visibility_v1(&session) ||
                                            ctr_video_queue_memory_visibility_v1(nullptr) != 60) {
                                            status = 197;
                                            break;
                                        }
                                        int visibilityWrongThread = 0;
                                        std::thread visibilityWorker([&] {
                                            visibilityWrongThread = ctr_video_queue_memory_visibility_v1(&session);
                                        });
                                        visibilityWorker.join();
                                        if (visibilityWrongThread != 116) {
                                            status = 197;
                                            break;
                                        }
                                        // Depth ALWAYS, stencil EQUAL8 against preserved7 must reject all samples.
                                        ds.depth_compare = 7;
                                        ds.front.reference = ds.back.reference = 8;
                                        visible = 999;
                                        if (session.DrawRectListDepth(pipeline, depthToken, &vp, &ds, &push, &draw,
                                                                      &visible) ||
                                            visible != 0) {
                                            status = 196;
                                            break;
                                        }
                                        ds.depth_compare = 1;
                                        ds.front.reference = ds.back.reference = 7;
                                        visible = 999;
                                        if (session.DrawRectListDepth(pipeline, depthToken, &vp, &ds, &push, &draw,
                                                                      &visible) ||
                                            visible != 0) {
                                            status = 196;
                                            break;
                                        }
                                        // Public C ABI draw writes stencil9, then a real query observes it through LOAD.
                                        ds.depth_compare = 7;
                                        ds.front.compare_op = ds.back.compare_op = 7;
                                        ds.front.reference = ds.back.reference = 9;
                                        ds.front.pass_op = ds.back.pass_op = 4;
                                        if (ctr_video_graphics_rect_depth_draw_v1(&session, pipeline, depthToken, &vp,
                                                                                  &ds, &push, &draw)) {
                                            status = 196;
                                            break;
                                        }
                                        ds.front.compare_op = ds.back.compare_op = 2;
                                        ds.front.pass_op = ds.back.pass_op = 0;
                                        visible = 0;
                                        if (session.DrawRectListDepth(pipeline, depthToken, &vp, &ds, &push, &draw,
                                                                      &visible) ||
                                            !visible) {
                                            status = 196;
                                            break;
                                        }
                                        ds.front.pass_op = 10;
                                        if (ctr_video_graphics_rect_depth_draw_v1(&session, pipeline, depthToken, &vp,
                                                                                  &ds, &push, &draw) != 88) {
                                            status = 196;
                                            break;
                                        }
                                        if (session.ReleaseGraphicsPipeline(pipeline) ||
                                            session.ReleaseGraphicsLayout(layout)) {
                                            status = 195;
                                            break;
                                        }
                                        std::puts(
                                            "RectList_depth_draw=six_real_fences_occlusion_boolean_depthLESS_fail0_passPositive_persistedWrite_fail0_stencilEQUAL_fail0_CABI_replace9_thenPositive_unsupportedbitwise88_persistentLOAD authored_noRetail_noHTILE");
                                        pipeline = 999;
                                        if (createRectDepth(depthToken, static_cast<uint32_t>(vs.size())) != 62 ||
                                            pipeline != 999) {
                                            status = 195;
                                            break;
                                        }
                                        {
                                            CtrDepthAttachmentParametersV1 rectSingle{
                                                1, sizeof(rectSingle), 960, 540, 1, 3, 1};
                                            uint64_t rectOwner = 0;
                                            key.num_samples = key.depth_samples = 1;
                                            key.z_format = 3;
                                            key.stencil_format = 1;
                                            if (session.CreateGraphicsLayout(&ps, &vertex, &layout) ||
                                                session.CreateDepthAttachment(&rectSingle, &rectOwner) ||
                                                session.ClearDepthAttachment(rectOwner, 0x3f800000, 0) ||
                                                createRectDepth(rectOwner, static_cast<uint32_t>(vs.size()))) {
                                                status = 248;
                                                break;
                                            }
                                            vp.viewports[0] = {0, 0, 16384, 16384, 0, 1};
                                            vp.scissors[0].extent = {960, 540};
                                            ds.depth_test = 1;
                                            ds.depth_write = 0;
                                            ds.stencil_test = 1;
                                            ds.depth_compare = 7;
                                            ds.front.compare_op = ds.back.compare_op = 2;
                                            ds.front.reference = ds.back.reference = 7;
                                            ds.front.pass_op = ds.back.pass_op = 0;
                                            visible = 999;
                                            if (session.DrawRectListDepth(pipeline, rectOwner, &vp, &ds, &push, &draw,
                                                                          &visible) ||
                                                visible || session.ClearDepthAttachment(rectOwner, 0x3f800000, 7) ||
                                                session.DrawRectListDepth(pipeline, rectOwner, &vp, &ds, &push, &draw,
                                                                          &visible) ||
                                                !visible ||
                                                ctr_video_graphics_rect_depth_draw_v1(&session, pipeline, rectOwner,
                                                                                      &vp, &ds, &push, &draw)) {
                                                status = 248;
                                                break;
                                            }
                                            ds.front.compare_op = ds.back.compare_op = 7;
                                            ds.front.reference = ds.back.reference = 9;
                                            ds.front.pass_op = ds.back.pass_op = 4;
                                            if (ctr_video_graphics_rect_depth_draw_v1(&session, pipeline, rectOwner,
                                                                                      &vp, &ds, &push, &draw)) {
                                                status = 248;
                                                break;
                                            }
                                            ds.front.compare_op = ds.back.compare_op = 2;
                                            ds.front.pass_op = ds.back.pass_op = 0;
                                            visible = 0;
                                            if (session.DrawRectListDepth(pipeline, rectOwner, &vp, &ds, &push, &draw,
                                                                          &visible) ||
                                                !visible || session.ReleaseGraphicsPipeline(pipeline) ||
                                                session.ReleaseGraphicsLayout(layout)) {
                                                status = 248;
                                                break;
                                            }
                                            CtrDepthSingleLayoutV1 singleLayout{
                                                1, sizeof(singleLayout), 960, 540, 1024, 1, 3, 1, 0, 10239, 0, 0, 0,
                                                0, 73371320320ULL};
                                            CtrDepthBackingSpansV1 singleSpans{};
                                            if (ctr_video_depth_single_spans_v1(&singleLayout, 73373941760ULL,
                                                                                &singleSpans) ||
                                                singleSpans.depth_bytes != 2621440 ||
                                                singleSpans.stencil_bytes != 552960) {
                                                status = 249;
                                                break;
                                            }
                                            const auto preservedSingle = singleSpans;
                                            auto badSingle = singleLayout;
                                            badSingle.samples = 2;
                                            if (ctr_video_depth_single_spans_v1(&badSingle, 73373941760ULL,
                                                                                &singleSpans) != 88 ||
                                                std::memcmp(&singleSpans, &preservedSingle, sizeof(singleSpans)) ||
                                                ctr_video_depth_single_spans_v1(
                                                    &singleLayout, singleLayout.guest_address, &singleSpans) != 88 ||
                                                std::memcmp(&singleSpans, &preservedSingle, sizeof(singleSpans)) ||
                                                ctr_video_depth_single_spans_v1(&singleLayout, 1, &singleSpans) != 60 ||
                                                std::memcmp(&singleSpans, &preservedSingle, sizeof(singleSpans))) {
                                                status = 249;
                                                break;
                                            }
                                            uint64_t singleGeneration = 0, singleLease = 0;
                                            if (ctr_video_depth_residency_register_v1(&session, rectOwner,
                                                                                      &singleLayout, 73373941760ULL,
                                                                                      &singleGeneration) != 88 ||
                                                singleGeneration ||
                                                ctr_video_depth_single_residency_register_v1(
                                                    &session, rectOwner, &singleLayout, 73373941760ULL,
                                                    &singleGeneration) ||
                                                !singleGeneration || session.ReleaseDepthAttachment(rectOwner) ||
                                                ctr_video_depth_residency_acquire_v1(&session, &singleSpans,
                                                                                     singleGeneration, &singleLease)) {
                                                status = 249;
                                                break;
                                            }
                                            {
                                                const auto singleCs =
                                                    load("build/CTRPort-data/compute_sampled_r32_control.spv");
                                                CtrGraphicsBindingContractV1 singleBindings{};
                                                singleBindings.version = 1;
                                                singleBindings.size = sizeof(singleBindings);
                                                singleBindings.unified_after = 2;
                                                singleBindings.buffer_after = 1;
                                                singleBindings.descriptor_count = 2;
                                                singleBindings.descriptors[0] = {0, 0, 0, 1, 0, 1};
                                                singleBindings.descriptors[1] = {1, 0, 1, 1, UINT32_MAX, 0};
                                                std::vector<uint32_t> singleInitial(960 * 540 * 4),
                                                    singleReadback(singleInitial.size());
                                                uint64_t singleBuffer = 0, singleComputeLayout = 0,
                                                         singleComputePipeline = 0, singleAlias = 0;
                                                if (session.CreateBuffer(singleInitial.data(), singleInitial.size() * 4,
                                                                         1, &singleBuffer) ||
                                                    ctr_video_compute_layout_create_v1(&session, &singleBindings,
                                                                                       &singleComputeLayout) ||
                                                    ctr_video_compute_pipeline_create_v1(
                                                        &session, singleComputeLayout, singleCs.data(),
                                                        static_cast<uint32_t>(singleCs.size()),
                                                        &singleComputePipeline) ||
                                                    ctr_video_depth_sampled_alias_v1(&session, singleLease,
                                                                                     &singleAlias)) {
                                                    status = 250;
                                                    break;
                                                }
                                                CtrGraphicsBufferWriteV1 singleWrite{0, 0, 1, 0, singleBuffer};
                                                uint32_t singleImageBinding = 1, singleGroups[] = {120, 540, 1};
                                                if (session.WriteGraphicsBuffers(singleComputeLayout, &singleWrite,
                                                                                 1) ||
                                                    session.WriteGraphicsImages(singleComputeLayout,
                                                                                &singleImageBinding, &singleAlias, 1) ||
                                                    session.ReleaseSampledImage(singleAlias) ||
                                                    ctr_video_compute_dispatch_v1(&session, singleComputePipeline,
                                                                                  &push, singleGroups) ||
                                                    session.CopyBufferInitial(singleBuffer, singleReadback.data(),
                                                                              singleReadback.size() * 4)) {
                                                    status = 250;
                                                    break;
                                                }
                                                for (size_t singleIndex = 0; singleIndex < singleReadback.size();
                                                     ++singleIndex)
                                                    if (singleReadback[singleIndex] !=
                                                        (singleIndex % 4 == 0 || singleIndex % 4 == 3 ? 0x3f800000u
                                                                                                      : 0u)) {
                                                        status = 250;
                                                        break;
                                                    }
                                                if (status) break;
                                                if (ctr_video_depth_attachment_restore_v1(&session, singleLease) ||
                                                    ctr_video_compute_dispatch_v1(&session, singleComputePipeline,
                                                                                  &push, singleGroups) != 91 ||
                                                    ctr_video_compute_pipeline_release_v1(&session,
                                                                                          singleComputePipeline) ||
                                                    session.ReleaseGraphicsLayout(singleComputeLayout) ||
                                                    session.ReleaseBuffer(singleBuffer)) {
                                                    status = 250;
                                                    break;
                                                }
                                                std::puts(
                                                    "singleDepth sampled alias real960x540 D32S8 sample1 after originalRelease resident lease full518400 RGBA texelFetch0diff layout retains alias afterRelease restore then dispatch91");
                                            }
                                            if (session.ClearDepthAttachment(singleLease, 0x3f800000, 0) != 91 ||
                                                ctr_video_depth_residency_remove_v1(&session, singleGeneration) ||
                                                session.ClearDepthAttachment(singleLease, 0x3f800000, 0) ||
                                                session.ReleaseDepthAttachment(singleLease)) {
                                                status = 249;
                                                break;
                                            }
                                            std::puts(
                                                "singleDepth residency real960x540 D32S8 lease survives ownerRelease exact2621440depth552960stencil register MS2reject88 malformed60 overlap88 atomic residentClear91 remove then actualGPUclear0 no guest/page/HTILE authority");
                                            std::puts(
                                                "RectList D32S8 sample1 actual VS publicTCS TES noFragment depthTest1 depthWrite0 compareAlways stencilReject0 passPositive CABIreplace9 persistedLOAD positive realFences exact5851 shape");
                                        }
                                        CtrDepthAttachmentParametersV1 d16{1, sizeof(d16), 1024, 1024, 1, 1, 0};
                                        uint64_t d16Token = 0;
                                        key.num_samples = key.depth_samples = 1;
                                        key.z_format = 1;
                                        key.stencil_format = 0;
                                        if (session.CreateDepthAttachment(&d16, &d16Token) ||
                                            session.CreateGraphicsLayout(&ps, &vertex, &layout) ||
                                            ctr_video_depth_attachment_clear_v1(&session, d16Token, 0x3e800000, 0) ||
                                            createRectDepth(d16Token, static_cast<uint32_t>(vs.size()))) {
                                            status = 218;
                                            break;
                                        }
                                        vp.viewports[0] = {0, 1024, 1024, -1024, 0, 1};
                                        vp.scissors[0].extent = {1024, 1024};
                                        ds.stencil_test = 0;
                                        ds.front.pass_op = 0;
                                        ds.depth_test = ds.depth_write = 1;
                                        ds.depth_compare = 1;
                                        visible = 999;
                                        if (session.DrawRectListDepth(pipeline, d16Token, &vp, &ds, &push, &draw,
                                                                      &visible) ||
                                            visible) {
                                            status = 218;
                                            break;
                                        }
                                        if (ctr_video_depth_attachment_clear_v1(&session, d16Token, 0x3f800000, 0)) {
                                            status = 218;
                                            break;
                                        }
                                        std::vector<unsigned char> d16Snapshot(2097152 + 32, 0xa7);
                                        if (ctr_video_depth_d16_read_linear_v1(
                                                &session, d16Token, d16Snapshot.data() + 16, 2097152, 2048)) {
                                            status = 228;
                                            break;
                                        }
                                        for (size_t i = 16; i < 2097152 + 16; ++i)
                                            if (d16Snapshot[i] != 255) {
                                                status = 228;
                                                break;
                                            }
                                        if (status == 228) break;
                                        const auto d16Before = d16Snapshot;
                                        if (ctr_video_depth_d16_read_linear_v1(
                                                &session, d16Token, d16Snapshot.data() + 16, 2097151, 2048) != 60 ||
                                            d16Snapshot != d16Before ||
                                            ctr_video_depth_d16_read_linear_v1(
                                                &session, depthToken, d16Snapshot.data() + 16, 2097152, 2048) != 88) {
                                            status = 228;
                                            break;
                                        }
                                        for (size_t i = 0; i < 16; ++i)
                                            if (d16Snapshot[i] != 0xa7 || d16Snapshot[2097152 + 16 + i] != 0xa7) {
                                                status = 228;
                                                break;
                                            }
                                        if (status == 228) break;
                                        std::puts(
                                            "D16 actual full1024x1024 linear GPU download2097152B clear1 all65535 guards32 unchanged restored attachment; no guestpublication");
                                        visible = 0;
                                        if (session.DrawRectListDepth(pipeline, d16Token, &vp, &ds, &push, &draw,
                                                                      &visible) ||
                                            !visible) {
                                            status = 218;
                                            break;
                                        }
                                        visible = 999;
                                        if (session.DrawRectListDepth(pipeline, d16Token, &vp, &ds, &push, &draw,
                                                                      &visible) ||
                                            visible) {
                                            status = 218;
                                            break;
                                        }
                                        if (ctr_video_depth_d16_read_linear_v1(
                                                &session, d16Token, d16Snapshot.data() + 16, 2097152, 2048)) {
                                            status = 228;
                                            break;
                                        }
                                        size_t halfDepth = 0;
                                        for (size_t i = 16; i < 2097152 + 16; i += 2) {
                                            uint16_t value = 0;
                                            std::memcpy(&value, d16Snapshot.data() + i, 2);
                                            // Fixture bounds half-depth quantization to one adjacent D16 code; no exact floating-depth equivalence claim.
                                            if (value == 32767 || value == 32768)
                                                ++halfDepth;
                                            else if (value != 65535) {
                                                std::printf("D16 draw unexpected word=%u index=%zu\n", value,
                                                            (i - 16) / 2);
                                                status = 228;
                                                break;
                                            }
                                        }
                                        if (status == 228 || !halfDepth) {
                                            status = 228;
                                            break;
                                        }
                                        for (size_t i = 0; i < 16; ++i)
                                            if (d16Snapshot[i] != 0xa7 || d16Snapshot[2097152 + 16 + i] != 0xa7) {
                                                status = 228;
                                                break;
                                            }
                                        if (status == 228) break;
                                        std::puts(
                                            "D16 actual drawn depth GPU download half32767or32768 retained65535 only guards32 unchanged attachment restored");
                                        CtrDepthD16LayoutV1 d16Layout{
                                            1, sizeof(d16Layout), 1024, 1024, 1024, 1, 1, 0, 0, 16383, 0, 0, 0,
                                            0, 73290416128ULL};
                                        CtrGraphicsImageExtentV1 d16Extent{};
                                        if (ctr_video_depth_d16_extent_v1(&d16Layout, &d16Extent) ||
                                            d16Extent.bytes != 2097152 ||
                                            d16Extent.guest_address != d16Layout.guest_address) {
                                            status = 229;
                                            break;
                                        }
                                        const auto correctExtent = d16Extent;
                                        auto invalidD16 = d16Layout;
                                        invalidD16.slice_tile_max = 16382;
                                        if (ctr_video_depth_d16_extent_v1(&invalidD16, &d16Extent) != 88 ||
                                            std::memcmp(&d16Extent, &correctExtent, sizeof(d16Extent))) {
                                            status = 229;
                                            break;
                                        }
                                        invalidD16 = d16Layout;
                                        invalidD16.guest_address = UINT64_MAX - 255;
                                        if (ctr_video_depth_d16_extent_v1(&invalidD16, &d16Extent) != 60 ||
                                            std::memcmp(&d16Extent, &correctExtent, sizeof(d16Extent))) {
                                            status = 229;
                                            break;
                                        }
                                        std::vector<unsigned char> d16Prior(2097152, 0x39),
                                            d16Tiled(2097152 + 32, 0xa7), d16Linear;
                                        const auto unchangedD16 = d16Tiled;
                                        invalidD16 = d16Layout;
                                        invalidD16.bank_swizzle = 1;
                                        if (ctr_video_depth_d16_read_tiled_v1(&session, d16Token, &invalidD16,
                                                                              d16Prior.data(), 2097152,
                                                                              d16Tiled.data() + 16) != 88 ||
                                            d16Tiled != unchangedD16 ||
                                            ctr_video_depth_d16_read_tiled_v1(&session, d16Token, &d16Layout,
                                                                              d16Prior.data(), 2097151,
                                                                              d16Tiled.data() + 16) != 60 ||
                                            d16Tiled != unchangedD16 ||
                                            ctr_video_depth_d16_read_tiled_v1(&session, d16Token, &d16Layout,
                                                                              d16Prior.data(), 2097152,
                                                                              d16Tiled.data() + 16)) {
                                            status = 229;
                                            break;
                                        }
                                        if (ctr_with_tiling_pipelines(
                                                device, deviceGet, tilingDirectory, true, true, true,
                                                [&](VkPipelineLayout l, const VkPipeline* p,
                                                    VkDescriptorSetLayout d) -> int {
                                                    return ctr_thin_detile(device, deviceGet, q, memory, l, p[1], d,
                                                                           1024, 1024, d16Tiled.data() + 16, 2097152,
                                                                           d16Linear, false, 2);
                                                },
                                                2, true) ||
                                            d16Linear.size() != 2097152 ||
                                            std::memcmp(d16Linear.data(), d16Snapshot.data() + 16, 2097152)) {
                                            status = 229;
                                            break;
                                        }
                                        for (size_t i = 0; i < 16; ++i)
                                            if (d16Tiled[i] != 0xa7 || d16Tiled[2097152 + 16 + i] != 0xa7) {
                                                status = 229;
                                                break;
                                            }
                                        if (status == 229) break;
                                        std::puts(
                                            "D16 actual tiled download2097152B postdraw detile versus linear0diff guards32 extent/short/swizzle/overflow atomic; host only no guestpublication");
                                        ds.stencil_test = 1;
                                        if (session.DrawRectListDepth(pipeline, d16Token, &vp, &ds, &push, &draw) !=
                                                88 ||
                                            session.ReleaseGraphicsPipeline(pipeline) ||
                                            session.ReleaseGraphicsLayout(layout) ||
                                            ctr_video_depth_attachment_release_v1(&session, d16Token)) {
                                            status = 218;
                                            break;
                                        }
                                        key.num_color_attachments = key.mrt_mask = 0;
                                        if (session.CreateGraphicsLayout(&ps, &vertex, &layout) ||
                                            ctr_video_graphics_rect_no_attachment_pipeline_create_v1(
                                                &session, &key, layout, vs.data(), static_cast<uint32_t>(vs.size()),
                                                tcs.data(), static_cast<uint32_t>(tcs.size()), tes.data(),
                                                static_cast<uint32_t>(tes.size()), &pipeline)) {
                                            status = 226;
                                            break;
                                        }
                                        ds.depth_test = ds.depth_write = ds.stencil_test = 0;
                                        visible = 0;
                                        if (session.DrawGraphics(pipeline, 0, &vp, &ds, &push, &draw, true, &visible) ||
                                            !visible ||
                                            ctr_video_graphics_no_attachment_draw_v1(&session, pipeline, &vp, &ds,
                                                                                     &push, &draw)) {
                                            status = 226;
                                            break;
                                        }
                                        vp.viewports[0] = {0, 0, 16384, 16384, 0, 1};
                                        visible = 0;
                                        if (session.DrawGraphics(pipeline, 0, &vp, &ds, &push, &draw, true, &visible) ||
                                            !visible || session.ReleaseGraphicsPipeline(pipeline) ||
                                            session.ReleaseGraphicsLayout(layout)) {
                                            status = 226;
                                            break;
                                        }
                                        std::puts(
                                            "RectList no attachment actual viewport16384x16384 scissor1024x1024 positiveOcclusion realFence no clamp");
                                        std::puts(
                                            "RectList no attachment actual1024x1024 VS_publicTCS_TES noFragment positiveOcclusion realFence CABI0 unchangedStaleZ1");
                                        std::puts(
                                            "RectList D16 sample1 actual1024x1024 occlusion fail0 passPositive persistedDepthLESSfail0 stencilUnsupported88");
                                        std::puts(
                                            "RectList_vertex_only_depth_MSAA2=actual_VkPipeline_VS_publicTCS_publicTES_noFragment_patch3_depth1920x1080_z3s1 allocation_release0 authored_noDraw");
                                    }
                                    std::puts(
                                        "depth full logical aspect clear twice explicit1.0/0.25 stencil0/7 real GPU fences persistent attachment layout; NaN/stencil256 rejected60; no readback or guest HTILE claim");
                                    int wrongClear = 0;
                                    std::thread clearWorker([&] {
                                        wrongClear =
                                            ctr_video_depth_attachment_clear_v1(&session, depthToken, 0x3f800000, 0);
                                    });
                                    clearWorker.join();
                                    if (wrongClear != 116) {
                                        status = 161;
                                        break;
                                    }
                                    int wrongRelease = 0;
                                    std::thread worker([&] {
                                        wrongRelease = ctr_video_depth_attachment_release_v1(&session, depthToken);
                                    });
                                    worker.join();
                                    if (wrongRelease != 116 ||
                                        ctr_video_depth_attachment_release_v1(&session, depthToken) ||
                                        ctr_video_depth_attachment_release_v1(&session, depthToken) != 62) {
                                        status = 160;
                                        break;
                                    }
                                    if (ctr_video_depth_attachment_clear_v1(&session, depthToken, 0x3f800000, 0) !=
                                        62) {
                                        status = 161;
                                        break;
                                    }
                                    depthToken = 999;
                                    params.version = 2;
                                    if (ctr_video_depth_attachment_create_v1(&session, &params, &depthToken) != 60 ||
                                        depthToken != 999) {
                                        status = 160;
                                        break;
                                    }
                                    params.version = 1;
                                    params.samples = 3;
                                    if (ctr_video_depth_attachment_create_v1(&session, &params, &depthToken) != 88 ||
                                        depthToken != 999) {
                                        status = 160;
                                        break;
                                    }
                                    std::puts(
                                        "depth actual image device-local memory bound aspect-view release0 stale62 thread116 version60 unsupported88 atomic; undefined contents no GPUdraw");
                                }
                                VkImageCreateInfo sentinel{};
                                sentinel.flags = 123;
                                VkImageAspectFlags mask = 123;
                                int wrong = 0;
                                std::thread worker([&] { wrong = session.PreflightDepth(shape, sentinel, mask); });
                                worker.join();
                                auto bad = shape;
                                bad.samples = 3;
                                if (wrong != 116 || session.PreflightDepth(bad, sentinel, mask) != 88 ||
                                    sentinel.flags != 123 || mask != 123) {
                                    status = 159;
                                    break;
                                }
                                bad = shape;
                                bad.width = 0;
                                if (session.PreflightDepth(bad, sentinel, mask) != 60 || sentinel.flags != 123 ||
                                    mask != 123) {
                                    status = 159;
                                    break;
                                }
                            }
                            if (status) break;
                            {
                                {
                                    CtrGraphicsViewportStateV1 viewport{};
                                    viewport.version = 1;
                                    viewport.size = sizeof(viewport);
                                    viewport.count = 1;
                                    viewport.viewports[0] = {0, 1, 64, -1, 0, 1};
                                    viewport.scissors[0] = {{0, 0}, {64, 1}};
                                    CtrGraphicsDynamicStateV1 dynamic{};
                                    dynamic.version = 1;
                                    dynamic.size = sizeof(dynamic);
                                    dynamic.cull_mode = 2;
                                    dynamic.line_width = 1;
                                    auto dynamicCheck = [&] {
                                        return ctr_video_graphics_dynamic_preflight_v1(&session, &viewport, &dynamic);
                                    };
                                    if (dynamicCheck() || viewport.viewports[0].height != -1) {
                                        status = 189;
                                        break;
                                    }
                                    int preflightWrong = 0;
                                    std::thread preflightWorker([&] { preflightWrong = dynamicCheck(); });
                                    preflightWorker.join();
                                    if (preflightWrong != 116) {
                                        status = 189;
                                        break;
                                    }
                                    viewport.count = 2;
                                    if (dynamicCheck() != 88) {
                                        status = 189;
                                        break;
                                    }
                                    viewport.count = 1;
                                    dynamic.line_width = 2;
                                    if (dynamicCheck() != 88) {
                                        status = 189;
                                        break;
                                    }
                                    dynamic.line_width = 1;
                                    viewport.scissors[0].offset.x = -1;
                                    if (dynamicCheck() != 88) {
                                        status = 189;
                                        break;
                                    }
                                    viewport.scissors[0].offset.x = 0;
                                    viewport.viewports[0].width = std::numeric_limits<float>::quiet_NaN();
                                    if (dynamicCheck() != 60) {
                                        status = 189;
                                        break;
                                    }
                                    viewport.viewports[0].width = 64;
                                    if (dynamicCheck()) {
                                        status = 189;
                                        break;
                                    }
                                    std::puts(
                                        "graphics_dynamic_preflight=actualdevice_negativeheight_preserved_multiview_wideline_scissor88_NaN60_owner116 noGPUdraw");
                                    auto read = [](const char* suffix) {
                                        char directory[32768]{};
                                        if (!GetEnvironmentVariableA("CTR_VIDEO_TILING_SHADER_DIR", directory, 32768))
                                            return std::vector<uint32_t>{};
                                        std::ifstream file(std::string(directory) + suffix,
                                                           std::ios::binary | std::ios::ate);
                                        if (!file) return std::vector<uint32_t>{};
                                        const auto size = file.tellg();
                                        if (size <= 0 || size % 4) return std::vector<uint32_t>{};
                                        std::vector<uint32_t> words(static_cast<size_t>(size) / 4);
                                        file.seekg(0);
                                        file.read(reinterpret_cast<char*>(words.data()), size);
                                        return words;
                                    };
                                    const auto vertexCode = read("/pipeline_control.vert.spv"),
                                               fragmentCode = read("/pipeline_control.frag.spv");
                                    CtrGraphicsBindingContractV1 authoredPs{}, authoredVs{};
                                    authoredPs.version = authoredVs.version = 1;
                                    authoredPs.size = authoredVs.size = sizeof(authoredPs);
                                    authoredPs.unified_after = authoredPs.buffer_after = 1;
                                    authoredPs.descriptor_count = 1;
                                    authoredPs.descriptors[0] = {0, 0, 0, 1, 0, 1};
                                    authoredVs.unified_before = authoredVs.buffer_before = 1;
                                    authoredVs.unified_after = authoredVs.buffer_after = 3;
                                    authoredVs.descriptor_count = 2;
                                    authoredVs.descriptors[0] = {0, 0, 1, 1, 1, 1};
                                    authoredVs.descriptors[1] = {0, 4, 2, 1, 2, 1};
                                    uint64_t layout = 0, colorOwner = 0, pipeline = 999;
                                    CtrGraphicsColorRequirementV1 color{};
                                    color.version = 1;
                                    color.size = sizeof(color);
                                    color.pitch = 64;
                                    color.height = 64;
                                    color.layers = 1;
                                    color.samples = 1;
                                    color.data_format = 10;
                                    color.guest_address = 1;
                                    CtrGraphicsKeyV1 key{};
                                    key.version = 1;
                                    key.size = sizeof(key);
                                    key.mrt_mask = 1;
                                    key.num_color_attachments = 1;
                                    key.num_samples = key.colors[0].samples = 1;
                                    key.colors[0].data_format = 10;
                                    key.colors[0].write_mask = 15;
                                    key.cb_shader_mask = 15;
                                    key.primitive_type = 4;
                                    key.polygon_mode = 2;
                                    key.logic_op = 0xcc;
                                    CtrGraphicsVertexInputsV1 inputs{};
                                    inputs.version = 1;
                                    inputs.size = sizeof(inputs);
                                    if (session.CreateGraphicsLayout(&authoredPs, &authoredVs, &layout) ||
                                        session.CreateColorAttachment(&color, 0, &colorOwner)) {
                                        status = 183;
                                        break;
                                    }
                                    inputs.count = 2;
                                    inputs.inputs[0].stride = 20;
                                    inputs.inputs[0].buffer_size = 60;
                                    inputs.inputs[1] = inputs.inputs[0];
                                    inputs.inputs[1].binding = inputs.inputs[1].location = 1;
                                    inputs.inputs[1].offset = 0;
                                    const uint32_t authoredFormats[] = {106, 103};
                                    struct AuthoredVertex {
                                        float position[3];
                                        float padding[2];
                                    };
                                    const AuthoredVertex triangleVertices[] = {
                                        {{-1, -1, 0}, {0, 0}}, {{3, -1, 0}, {0, 0}}, {{-1, 3, 0}, {0, 0}}};
                                    const AuthoredVertex colorVertices[] = {
                                        {{1, 0, 0}, {0, 0}}, {{1, 0, 0}, {0, 0}}, {{1, 0, 0}, {0, 0}}};
                                    uint64_t triangleBuffer = 0, triangleColorBuffer = 0;
                                    if (session.CreateBuffer(triangleVertices, sizeof(triangleVertices), 4,
                                                             &triangleBuffer) ||
                                        session.CreateBuffer(colorVertices, sizeof(colorVertices), 4,
                                                             &triangleColorBuffer)) {
                                        status = 191;
                                        break;
                                    }
                                    auto createAuthoredPipeline = [&] {
                                        return ctr_video_graphics_pipeline_create_v1(
                                            &session, &key, &inputs, authoredFormats, 2, layout, colorOwner,
                                            vertexCode.data(), static_cast<uint32_t>(vertexCode.size()),
                                            fragmentCode.data(), static_cast<uint32_t>(fragmentCode.size()), 0,
                                            &pipeline);
                                    };
                                    key.primitive_type = 17;
                                    if (createAuthoredPipeline() != 88 || pipeline != 999) {
                                        status = 183;
                                        break;
                                    }
                                    key.primitive_type = 4;
                                    key.colors[0].number_format = 9;
                                    if (createAuthoredPipeline() != 88 || pipeline != 999) {
                                        status = 183;
                                        break;
                                    }
                                    key.colors[0].number_format = 0;
                                    key.num_samples = 2;
                                    if (createAuthoredPipeline() != 88 || pipeline != 999) {
                                        status = 183;
                                        break;
                                    }
                                    key.num_samples = 1;
                                    int pipelineWrongOwner = 0;
                                    std::thread pipelineWorker([&] { pipelineWrongOwner = createAuthoredPipeline(); });
                                    pipelineWorker.join();
                                    if (pipelineWrongOwner != 116 || pipeline != 999) {
                                        status = 183;
                                        break;
                                    }
                                    if (createAuthoredPipeline() || pipeline == 999) {
                                        status = 191;
                                        break;
                                    }
                                    std::vector<uint8_t> initialDraw(64 * 64 * 4, 0),
                                        drawReadback(initialDraw.size(), 77);
                                    CtrGraphicsPushDataV1 drawPush{};
                                    drawPush.version = 1;
                                    drawPush.size = sizeof(drawPush);
                                    dynamic.cull_mode = 0;
                                    CtrGraphicsDrawV1 drawArgs{};
                                    drawArgs.version = 1;
                                    drawArgs.size = sizeof(drawArgs);
                                    drawArgs.vertex_count = 3;
                                    drawArgs.instance_count = 1;
                                    drawArgs.vertex_buffer_count = 2;
                                    drawArgs.vertex_buffers[0] = triangleBuffer;
                                    drawArgs.vertex_buffers[1] = triangleColorBuffer;
                                    auto drawGpu = [&] {
                                        return ctr_video_graphics_draw_v1(&session, pipeline, colorOwner, &viewport,
                                                                          &dynamic, &drawPush, &drawArgs);
                                    };
                                    if (drawGpu() != 88) {
                                        status = 192;
                                        break;
                                    }
                                    const float descriptorData[3][4] = {{1, 1, 1, 1}, {0, 0, 0, 0}, {1, 1, 1, 1}};
                                    uint64_t descriptorTokens[3]{};
                                    CtrGraphicsBufferWriteV1 descriptorWrites[3]{};
                                    for (uint32_t i = 0; i < 3; ++i) {
                                        if (session.CreateBuffer(descriptorData[i], 16, 1, &descriptorTokens[i]))
                                            status = 192;
                                        descriptorWrites[i] = {i, i, 1, 0, descriptorTokens[i]};
                                    }
                                    if (status == 192 || session.WriteGraphicsBuffers(layout, descriptorWrites, 3)) {
                                        status = 192;
                                        break;
                                    }
                                    for (auto token : descriptorTokens)
                                        if (session.ReleaseBuffer(token)) status = 192;
                                    if (status == 192) break;
                                    if (drawGpu() != 91 ||
                                        session.TransferColorAttachment(colorOwner, initialDraw.data(), nullptr,
                                                                        initialDraw.size(), 64 * 4) ||
                                        drawGpu()) {
                                        status = 191;
                                        break;
                                    }
                                    viewport.viewports[0].y = 2;
                                    viewport.scissors[0].offset.y = 1;
                                    if (drawGpu() ||
                                        session.TransferColorAttachment(colorOwner, nullptr, drawReadback.data(),
                                                                        drawReadback.size(), 64 * 4)) {
                                        status = 193;
                                        break;
                                    }
                                    viewport.viewports[0].y = 1;
                                    viewport.scissors[0].offset.y = 0;
                                    for (size_t pixel = 0; pixel < 64 * 64; ++pixel) {
                                        const uint8_t expected[4] = {255, 0, 0, 255};
                                        for (size_t channel = 0; channel < 4; ++channel)
                                            if (drawReadback[pixel * 4 + channel] !=
                                                (pixel < 128 ? expected[channel] : 0))
                                                status = 191;
                                    }
                                    if (status == 191) break;
                                    std::puts(
                                        "graphics_draw=actualGPU_authored_triangle_negativeheight_twoOrderedRows_red128_LOAD_preserves_remaining3968_fenced_readback0diff uninitialized91 actualVertexFormats106and103_twoOwners_stride20_threeRetainedStorageReads_unwritten88 retail0");
                                    for (const uint32_t width : {2u, 4u}) {
                                        const uint32_t values[] = {99, 99, 2, 0, 1};
                                        std::vector<unsigned char> indices(5 * width);
                                        for (size_t i = 0; i < 5; ++i)
                                            std::memcpy(indices.data() + i * width, &values[i], width);
                                        uint64_t indexToken = 0;
                                        if (session.CreateBuffer(indices.data(), indices.size(), width == 2 ? 2u : 3u,
                                                                 &indexToken)) {
                                            status = 243;
                                            break;
                                        }
                                        auto indexed = [&](uint32_t bytes, uint32_t first, int32_t offset) {
                                            return session.DrawGraphics(pipeline, colorOwner, &viewport, &dynamic,
                                                                        &drawPush, &drawArgs, false, nullptr,
                                                                        indexToken, bytes, width, first, offset);
                                        };
                                        if (indexed(width == 2 ? 4u : 2u, 1, 0) != 88 || indexed(width, 0, 0) != 88 ||
                                            indexed(width, 1, -1) != 88 ||
                                            session.TransferColorAttachment(colorOwner, initialDraw.data(), nullptr,
                                                                            initialDraw.size(), 64 * 4) ||
                                            indexed(width, 1, 0) ||
                                            session.TransferColorAttachment(colorOwner, nullptr, drawReadback.data(),
                                                                            drawReadback.size(), 64 * 4)) {
                                            status = 243;
                                            break;
                                        }
                                        for (size_t pixel = 0; pixel < 64 * 64; ++pixel) {
                                            const uint8_t expected[4] = {255, 0, 0, 255};
                                            for (size_t channel = 0; channel < 4; ++channel)
                                                if (drawReadback[pixel * 4 + channel] !=
                                                    (pixel < 64 ? expected[channel] : 0))
                                                    status = 243;
                                        }
                                        if (session.ReleaseBuffer(indexToken) || indexed(width, 1, 0) != 62) {
                                            status = 243;
                                            break;
                                        }
                                        const uint32_t shiftedValues[] = {3, 1, 2};
                                        indices.resize(3 * width);
                                        for (size_t i = 0; i < 3; ++i)
                                            std::memcpy(indices.data() + i * width, &shiftedValues[i], width);
                                        if (session.CreateBuffer(indices.data(), indices.size(), width == 2 ? 2u : 3u,
                                                                 &indexToken)) {
                                            status = 244;
                                            break;
                                        }
                                        auto shifted = [&](int32_t base) {
                                            return session.DrawGraphics(pipeline, colorOwner, &viewport, &dynamic,
                                                                        &drawPush, &drawArgs, false, nullptr,
                                                                        indexToken, width, 0, 0, base);
                                        };
                                        if (shifted(0) != 88 || shifted(INT32_MIN) != 88 || shifted(INT32_MAX) != 88 ||
                                            session.TransferColorAttachment(colorOwner, initialDraw.data(), nullptr,
                                                                            initialDraw.size(), 64 * 4) ||
                                            shifted(-1) ||
                                            session.TransferColorAttachment(colorOwner, nullptr, drawReadback.data(),
                                                                            drawReadback.size(), 64 * 4)) {
                                            status = 244;
                                            break;
                                        }
                                        for (size_t pixel = 0; pixel < 64 * 64; ++pixel) {
                                            const uint8_t expected[4] = {255, 0, 0, 255};
                                            for (size_t channel = 0; channel < 4; ++channel)
                                                if (drawReadback[pixel * 4 + channel] !=
                                                    (pixel < 64 ? expected[channel] : 0))
                                                    status = 244;
                                        }
                                        if (session.ReleaseBuffer(indexToken)) {
                                            status = 244;
                                            break;
                                        }
                                    }
                                    if (status == 243 || status == 244) break;
                                    viewport.viewports[0].y = 2;
                                    viewport.scissors[0].offset.y = 1;
                                    if (drawGpu() ||
                                        session.TransferColorAttachment(colorOwner, nullptr, drawReadback.data(),
                                                                        drawReadback.size(), 64 * 4)) {
                                        status = 243;
                                        break;
                                    }
                                    viewport.viewports[0].y = 1;
                                    viewport.scissors[0].offset.y = 0;
                                    std::puts(
                                        "Indexed actual U16 U32 triangle signed base minus1 shifted3_1_2 equals2_0_1 vkCmdDrawIndexed offset firstIndex true red64pixels remaining4032preserved0diff wrongtype bounds negativebase stale rejected fenced");
                                    { // Actual Vk83 half-float fetch: max index3 needs exactly124B at stride40.
                                        auto halfInputs = inputs;
                                        halfInputs.inputs[0].stride = halfInputs.inputs[1].stride = 40;
                                        halfInputs.inputs[0].buffer_size = 132;
                                        halfInputs.inputs[1].buffer_size = 124;
                                        const uint32_t halfFormats[] = {106, 83};
                                        std::vector<unsigned char> positions(132), halfColors(124);
                                        const float xyz[4][3] = {{-1, -1, 0}, {3, -1, 0}, {0, 0, 0}, {-1, 3, 0}};
                                        const uint16_t redHalf[2] = {0x3c00, 0};
                                        for (size_t i = 0; i < 4; ++i) {
                                            std::memcpy(positions.data() + i * 40, xyz[i], 12);
                                            std::memcpy(halfColors.data() + i * 40, redHalf, 4);
                                        }
                                        const uint16_t halfIndices[] = {0, 1, 3};
                                        uint64_t hp = 0, hv = 0, hc = 0, hi = 0;
                                        auto makeHalf = [&] {
                                            return ctr_video_graphics_pipeline_create_v1(
                                                &session, &key, &halfInputs, halfFormats, 2, layout, colorOwner,
                                                vertexCode.data(), static_cast<uint32_t>(vertexCode.size()),
                                                fragmentCode.data(), static_cast<uint32_t>(fragmentCode.size()), 0,
                                                &hp);
                                        };
                                        if (session.CreateBuffer(positions.data(), positions.size(), 4, &hv) ||
                                            session.CreateBuffer(halfColors.data(), halfColors.size(), 4, &hc) ||
                                            session.CreateBuffer(halfIndices, sizeof(halfIndices), 2, &hi) ||
                                            makeHalf()) {
                                            status = 251;
                                            break;
                                        }
                                        auto hd = drawArgs;
                                        hd.vertex_buffers[0] = hv;
                                        hd.vertex_buffers[1] = hc;
                                        auto halfDraw = [&] {
                                            CtrGraphicsIndexedDrawV1 id{};
                                            id.version = 1;
                                            id.size = sizeof(id);
                                            id.index_count = 3;
                                            id.instance_count = 1;
                                            id.index_bytes = 2;
                                            id.index_token = hi;
                                            id.vertex_buffer_count = 2;
                                            std::memcpy(id.vertex_buffers, hd.vertex_buffers,
                                                        sizeof(id.vertex_buffers));
                                            return ctr_video_graphics_indexed_color_draw_v1(
                                                &session, hp, colorOwner, &viewport, &dynamic, &drawPush, &id);
                                        };
                                        if (session.TransferColorAttachment(colorOwner, initialDraw.data(), nullptr,
                                                                            initialDraw.size(), 64 * 4) ||
                                            halfDraw() ||
                                            session.TransferColorAttachment(colorOwner, nullptr, drawReadback.data(),
                                                                            drawReadback.size(), 64 * 4)) {
                                            status = 251;
                                            break;
                                        }
                                        for (size_t pixel = 0; pixel < 4096; ++pixel) {
                                            const uint8_t expected[4] = {255, 0, 0, 255};
                                            for (size_t c = 0; c < 4; ++c)
                                                if (drawReadback[pixel * 4 + c] != (pixel < 64 ? expected[c] : 0))
                                                    status = 251;
                                        }
                                        if (status == 251 || session.ReleaseGraphicsPipeline(hp)) {
                                            status = 251;
                                            break;
                                        }
                                        halfInputs.inputs[1].buffer_size = 123;
                                        if (session.ReleaseBuffer(hc) ||
                                            session.CreateBuffer(halfColors.data(), 123, 4, &hc)) {
                                            status = 251;
                                            break;
                                        }
                                        hd.vertex_buffers[1] = hc;
                                        if (makeHalf() || halfDraw() != 88 ||
                                            session.TransferColorAttachment(colorOwner, nullptr, initialDraw.data(),
                                                                            initialDraw.size(), 64 * 4) ||
                                            initialDraw != drawReadback) {
                                            status = 251;
                                            break;
                                        }
                                        if (session.ReleaseGraphicsPipeline(hp) || session.ReleaseBuffer(hv) ||
                                            session.ReleaseBuffer(hc) || session.ReleaseBuffer(hi)) {
                                            status = 251;
                                            break;
                                        }
                                        std::fill(initialDraw.begin(), initialDraw.end(), uint8_t{0});
                                        std::puts(
                                            "Vk83 actual half-float indexed max3 stride40 exact124B red64 pixels short123B reject88 framebuffer unchanged");
                                    }
                                    viewport.viewports[0].y = 2;
                                    viewport.scissors[0].offset.y = 1;
                                    if (drawGpu() ||
                                        session.TransferColorAttachment(colorOwner, nullptr, drawReadback.data(),
                                                                        drawReadback.size(), 64 * 4)) {
                                        status = 251;
                                        break;
                                    }
                                    viewport.viewports[0].y = 1;
                                    viewport.scissors[0].offset.y = 0;
                                    { // Authored four-vertex strip; preserve the prior two-row fixture.
                                        const auto savedPixels = drawReadback;
                                        auto stripInputs = inputs;
                                        stripInputs.inputs[0].buffer_size = stripInputs.inputs[1].buffer_size = 80;
                                        const AuthoredVertex stripPositions[] = {{{-1, -1, 0}, {0, 0}},
                                                                                 {{1, -1, 0}, {0, 0}},
                                                                                 {{-1, 1, 0}, {0, 0}},
                                                                                 {{1, 1, 0}, {0, 0}}};
                                        const AuthoredVertex stripColors[] = {{{1, 0, 0}, {0, 0}},
                                                                              {{1, 0, 0}, {0, 0}},
                                                                              {{1, 0, 0}, {0, 0}},
                                                                              {{1, 0, 0}, {0, 0}}};
                                        uint64_t sp = 0, sv = 0, sc = 0;
                                        auto stripKey = key;
                                        stripKey.primitive_type = 6;
                                        if (session.CreateBuffer(stripPositions, sizeof(stripPositions), 4, &sv) ||
                                            session.CreateBuffer(stripColors, sizeof(stripColors), 4, &sc) ||
                                            ctr_video_graphics_pipeline_create_v1(
                                                &session, &stripKey, &stripInputs, authoredFormats, 2, layout,
                                                colorOwner, vertexCode.data(), static_cast<uint32_t>(vertexCode.size()),
                                                fragmentCode.data(), static_cast<uint32_t>(fragmentCode.size()), 0,
                                                &sp)) {
                                            status = 253;
                                            break;
                                        }
                                        auto sd = drawArgs;
                                        sd.vertex_count = 4;
                                        sd.vertex_buffers[0] = sv;
                                        sd.vertex_buffers[1] = sc;
                                        if (session.TransferColorAttachment(colorOwner, initialDraw.data(), nullptr,
                                                                            initialDraw.size(), 64 * 4) ||
                                            ctr_video_graphics_draw_v1(&session, sp, colorOwner, &viewport, &dynamic,
                                                                       &drawPush, &sd) ||
                                            session.TransferColorAttachment(colorOwner, nullptr, drawReadback.data(),
                                                                            drawReadback.size(), 64 * 4)) {
                                            status = 253;
                                            break;
                                        }
                                        for (size_t pixel = 0; pixel < 4096; ++pixel) {
                                            const uint8_t expected[4] = {255, 0, 0, 255};
                                            for (size_t c = 0; c < 4; ++c)
                                                if (drawReadback[pixel * 4 + c] != (pixel < 64 ? expected[c] : 0))
                                                    status = 253;
                                        }
                                        sd.vertex_count = 5;
                                        if (ctr_video_graphics_draw_v1(&session, sp, colorOwner, &viewport, &dynamic,
                                                                       &drawPush, &sd) != 88)
                                            status = 253;
                                        if (status == 253 || session.ReleaseGraphicsPipeline(sp) ||
                                            session.ReleaseBuffer(sv) || session.ReleaseBuffer(sc) ||
                                            session.TransferColorAttachment(colorOwner, savedPixels.data(), nullptr,
                                                                            savedPixels.size(), 64 * 4)) {
                                            status = 253;
                                            break;
                                        }
                                        drawReadback = savedPixels;
                                        std::puts(
                                            "TriangleStrip6 actual four vertices red64pixels remaining4032preserved fenced readback0diff fifth vertex reject88");
                                    }
                                    {
                                        CtrDepthSingleLayoutV1 dl{1, sizeof(dl), 2048, 8192, 2048,
                                                                  1, 1,          0,    0,    262143,
                                                                  0, 0,          0,    0,    73298870272ULL};
                                        CtrDepthBackingSpansV1 spans{};
                                        if (ctr_video_depth_single_spans_v1(&dl, 0, &spans) ||
                                            spans.depth_bytes != 33554432 || spans.stencil_address ||
                                            spans.stencil_bytes) {
                                            status = 255;
                                            break;
                                        }
                                        const auto kept = spans;
                                        auto bad = dl;
                                        bad.slice_tile_max--;
                                        if (ctr_video_depth_single_spans_v1(&bad, 0, &spans) != 88 ||
                                            std::memcmp(&kept, &spans, sizeof(spans)) ||
                                            ctr_video_depth_single_spans_v1(&dl, 256, &spans) != 88 ||
                                            std::memcmp(&kept, &spans, sizeof(spans))) {
                                            status = 255;
                                            break;
                                        }
                                        for (unsigned field = 0; field < 10; ++field) {
                                            auto wrong = dl;
                                            switch (field) {
                                            case 0: wrong.width--; break;
                                            case 1: wrong.height--; break;
                                            case 2: wrong.pitch--; break;
                                            case 3: wrong.samples = 2; break;
                                            case 4: wrong.z_format = 3; break;
                                            case 5: wrong.stencil_format = 1; break;
                                            case 6: wrong.tile_mode = 1; break;
                                            case 7: wrong.slice_start = 1; break;
                                            case 8: wrong.alt_tile = 1; break;
                                            case 9: wrong.bank_swizzle = 1; break;
                                            }
                                            if (ctr_video_depth_single_spans_v1(&wrong, 0, &spans) != 88 ||
                                                std::memcmp(&kept, &spans, sizeof(spans)))
                                                status = 255;
                                        }
                                        bad = dl;
                                        bad.guest_address = UINT64_MAX - 255;
                                        if (status == 255 || ctr_video_depth_single_spans_v1(&bad, 0, &spans) != 60 ||
                                            std::memcmp(&kept, &spans, sizeof(spans))) {
                                            status = 255;
                                            break;
                                        }
                                        CtrDepthAttachmentParametersV1 dp{1, sizeof(dp), 2048, 8192, 1, 1, 0};
                                        uint64_t dt = 0, gen = 0, lease = 0, alias = 0;
                                        if (ctr_video_depth_attachment_create_v1(&session, &dp, &dt) ||
                                            session.ClearDepthAttachment(dt, 0x3e800000, 0) ||
                                            ctr_video_depth_single_residency_register_v1(&session, dt, &dl, 0, &gen) ||
                                            !gen || session.ReleaseDepthAttachment(dt) ||
                                            ctr_video_depth_residency_acquire_v1(&session, &spans, gen, &lease) ||
                                            session.ClearDepthAttachment(lease, 0, 0) != 91 ||
                                            ctr_video_depth_sampled_alias_v1(&session, lease, &alias)) {
                                            status = 255;
                                            break;
                                        }
                                        const auto dcs = read("/../compute_r8_channels_control.spv");
                                        CtrGraphicsBindingContractV1 dbc{};
                                        dbc.version = 1;
                                        dbc.size = sizeof(dbc);
                                        dbc.unified_after = 2;
                                        dbc.buffer_after = 1;
                                        dbc.descriptor_count = 2;
                                        dbc.descriptors[0] = {0, 0, 0, 1, 0, 1};
                                        dbc.descriptors[1] = {1, 0, 1, 1, UINT32_MAX, 0};
                                        uint32_t d16Result[16]{};
                                        uint64_t db = 0, dcl = 0, dcp = 0;
                                        if (session.CreateBuffer(d16Result, sizeof(d16Result), 1, &db) ||
                                            ctr_video_compute_layout_create_v1(&session, &dbc, &dcl) ||
                                            ctr_video_compute_pipeline_create_v1(
                                                &session, dcl, dcs.data(), static_cast<uint32_t>(dcs.size()), &dcp)) {
                                            status = 255;
                                            break;
                                        }
                                        CtrGraphicsBufferWriteV1 dw{0, 0, 1, 0, db};
                                        uint32_t dib = 1, dgroups[] = {2, 1, 1};
                                        CtrGraphicsPushDataV1 dpush{};
                                        dpush.version = 1;
                                        dpush.size = sizeof(dpush);
                                        if (session.WriteGraphicsBuffers(dcl, &dw, 1) ||
                                            session.WriteGraphicsImages(dcl, &dib, &alias, 1) ||
                                            session.ReleaseSampledImage(alias) ||
                                            ctr_video_compute_dispatch_v1(&session, dcp, &dpush, dgroups) ||
                                            session.CopyBufferInitial(db, d16Result, sizeof(d16Result))) {
                                            status = 255;
                                            break;
                                        }
                                        for (auto v : d16Result)
                                            if (v != 0x40404040u) status = 255;
                                        if (status == 255 || ctr_video_depth_attachment_restore_v1(&session, lease) ||
                                            ctr_video_compute_dispatch_v1(&session, dcp, &dpush, dgroups) != 91 ||
                                            ctr_video_depth_residency_remove_v1(&session, gen) ||
                                            ctr_video_depth_residency_acquire_v1(&session, &spans, gen, &dt) != 62 ||
                                            session.ClearDepthAttachment(lease, 0, 0) ||
                                            session.ReleaseDepthAttachment(lease) ||
                                            ctr_video_compute_pipeline_release_v1(&session, dcp) ||
                                            session.ReleaseGraphicsLayout(dcl) || session.ReleaseBuffer(db)) {
                                            status = 255;
                                            break;
                                        }
                                        std::puts(
                                            "D16 tall retained33554432 noStencil lease survivesRelease sampled16 quarterDepth replicatedR40404040 restore91 stale62 actualGPUclear0 malformed12 overflow atomic");
                                        CtrGraphicsImageParametersV1 r{};
                                        r.version = 1;
                                        r.size = sizeof(r);
                                        r.type = 9;
                                        r.width = r.pitch = 2048;
                                        r.height = 8192;
                                        r.depth = r.layers = r.levels = r.samples = 1;
                                        r.data_format = 2;
                                        r.guest_address = 0x100000;
                                        for (auto& c : r.swizzle) c = 4;
                                        uint64_t image = 0;
                                        if (session.CreateSampledImage(&r, &image)) {
                                            status = 254;
                                            break;
                                        }
                                        const uint64_t count16 = 33554432;
                                        std::vector<unsigned char> src16(count16), out16(count16 + 32, 0xa7);
                                        for (size_t i = 0; i < src16.size(); ++i)
                                            src16[i] = static_cast<unsigned char>((i * 17 + i / 2048) % 251);
                                        CtrGraphicsImageExtentV1 ex{1,    sizeof(ex),      2048,
                                                                    8192, r.guest_address, count16};
                                        auto bad16 = ex;
                                        bad16.padded_height -= 128;
                                        const int badRc16 =
                                                      session.UploadSampledTiled(image, src16.data(), count16, &bad16),
                                                  shortRc16 =
                                                      session.UploadSampledTiled(image, src16.data(), count16 - 1, &ex),
                                                  uploadRc16 =
                                                      session.UploadSampledTiled(image, src16.data(), count16, &ex);
                                        const int readRc16 =
                                            uploadRc16 ? uploadRc16
                                                       : session.ReadbackImageTiled(image, src16.data(), count16, &ex,
                                                                                    out16.data() + 16);
                                        const bool same16 = std::equal(src16.begin(), src16.end(), out16.begin() + 16);
                                        if (badRc16 != 88 || shortRc16 == 0 || uploadRc16 || readRc16 || !same16) {
                                            std::fprintf(
                                                stderr, "R16 tall control bad=%d short=%d upload=%d read=%d equal=%u\n",
                                                badRc16, shortRc16, uploadRc16, readRc16, same16 ? 1u : 0u);
                                            status = 254;
                                            break;
                                        }
                                        for (size_t i = 0; i < 16; ++i)
                                            if (out16[i] != 0xa7 || out16[count16 + 16 + i] != 0xa7) status = 254;
                                        for (size_t i = 0; i < src16.size() / 2; ++i) {
                                            const uint16_t v = static_cast<uint16_t>((i % 256) * 257);
                                            std::memcpy(src16.data() + i * 2, &v, 2);
                                        }
                                        if (session.UploadSampledImage(image, src16.data(), count16, 4096)) {
                                            status = 254;
                                            break;
                                        }
                                        const auto cs = read("/../compute_r8_channels_control.spv");
                                        CtrGraphicsBindingContractV1 bc{};
                                        bc.version = 1;
                                        bc.size = sizeof(bc);
                                        bc.unified_after = 2;
                                        bc.buffer_after = 1;
                                        bc.descriptor_count = 2;
                                        bc.descriptors[0] = {0, 0, 0, 1, 0, 1};
                                        bc.descriptors[1] = {1, 0, 1, 1, UINT32_MAX, 0};
                                        uint32_t raw[16]{};
                                        uint64_t buf = 0, cl = 0, cp = 0;
                                        if (session.CreateBuffer(raw, sizeof(raw), 1, &buf) ||
                                            ctr_video_compute_layout_create_v1(&session, &bc, &cl) ||
                                            ctr_video_compute_pipeline_create_v1(
                                                &session, cl, cs.data(), static_cast<uint32_t>(cs.size()), &cp)) {
                                            status = 254;
                                            break;
                                        }
                                        CtrGraphicsBufferWriteV1 bw{0, 0, 1, 0, buf};
                                        uint32_t ib = 1, groups[] = {2, 1, 1};
                                        CtrGraphicsPushDataV1 pushR{};
                                        pushR.version = 1;
                                        pushR.size = sizeof(pushR);
                                        if (session.WriteGraphicsBuffers(cl, &bw, 1) ||
                                            session.WriteGraphicsImages(cl, &ib, &image, 1) ||
                                            ctr_video_compute_dispatch_v1(&session, cp, &pushR, groups) ||
                                            session.CopyBufferInitial(buf, raw, sizeof(raw))) {
                                            status = 254;
                                            break;
                                        }
                                        for (size_t i = 0; i < 16; ++i)
                                            if (raw[i] != (uint32_t(i) * 0x01010101u)) status = 254;
                                        if (status == 252 || ctr_video_compute_pipeline_release_v1(&session, cp) ||
                                            session.ReleaseGraphicsLayout(cl) || session.ReleaseBuffer(buf)) {
                                            status = 254;
                                            break;
                                        }
                                        auto invalid16 = r;
                                        invalid16.height = 8193;
                                        uint64_t rejected16 = 999;
                                        if (session.CreateSampledImage(&invalid16, &rejected16) != 88 ||
                                            rejected16 != 999) {
                                            status = 254;
                                            break;
                                        }
                                        invalid16 = r;
                                        invalid16.swizzle[3] = 1;
                                        if (session.CreateSampledImage(&invalid16, &rejected16) != 88 ||
                                            rejected16 != 999) {
                                            status = 254;
                                            break;
                                        }
                                        if (status == 254 || session.ReleaseSampledImage(image)) {
                                            status = 254;
                                            break;
                                        }
                                        std::puts(
                                            "R16 tile0 tall2048x8192 actual GPU tiled33554432B roundtrip guards32 sampled16 replicatedR malformed short shape swizzle reject");
                                    }
                                    for (uint32_t side : {1024u, 2048u}) {
                                        CtrGraphicsImageParametersV1 r{};
                                        r.version = 1;
                                        r.size = sizeof(r);
                                        r.type = 9;
                                        r.width = r.height = r.pitch = side;
                                        r.depth = r.layers = r.levels = r.samples = 1;
                                        r.data_format = 1;
                                        r.tile_mode = 8;
                                        r.guest_address = 0x100000;
                                        r.swizzle[3] = 4;
                                        uint64_t image = 0;
                                        if (session.CreateSampledImage(&r, &image)) {
                                            status = 252;
                                            break;
                                        }
                                        const size_t r8Bytes = size_t(side) * side;
                                        std::vector<unsigned char> source(r8Bytes), out(r8Bytes + 32, 0xa7);
                                        for (size_t i = 0; i < r8Bytes; ++i)
                                            source[i] = static_cast<unsigned char>((i * 17 + i / side) % 251);
                                        CtrGraphicsImageExtentV1 e{1, sizeof(e), side, side, r.guest_address, r8Bytes};
                                        auto bad = e;
                                        bad.pitch++;
                                        if (session.UploadSampledTiled(image, source.data(), r8Bytes, &bad) != 88 ||
                                            session.UploadSampledTiled(image, source.data(), r8Bytes - 1, &e) != 88 ||
                                            session.UploadSampledTiled(image, source.data(), r8Bytes, &e) ||
                                            session.ReadbackSampledImage(image, out.data() + 16, r8Bytes, side) ||
                                            !std::equal(source.begin(), source.end(), out.begin() + 16)) {
                                            status = 252;
                                            break;
                                        }
                                        for (size_t i = 0; i < 16; ++i)
                                            if (out[i] != 0xa7 || out[r8Bytes + 16 + i] != 0xa7) status = 252;
                                        const auto cs = read("/../compute_r8_channels_control.spv");
                                        CtrGraphicsBindingContractV1 bc{};
                                        bc.version = 1;
                                        bc.size = sizeof(bc);
                                        bc.unified_after = 2;
                                        bc.buffer_after = 1;
                                        bc.descriptor_count = 2;
                                        bc.descriptors[0] = {0, 0, 0, 1, 0, 1};
                                        bc.descriptors[1] = {1, 0, 1, 1, UINT32_MAX, 0};
                                        uint32_t raw[16]{};
                                        uint64_t buf = 0, cl = 0, cp = 0;
                                        if (session.CreateBuffer(raw, sizeof(raw), 1, &buf) ||
                                            ctr_video_compute_layout_create_v1(&session, &bc, &cl) ||
                                            ctr_video_compute_pipeline_create_v1(
                                                &session, cl, cs.data(), static_cast<uint32_t>(cs.size()), &cp)) {
                                            status = 252;
                                            break;
                                        }
                                        CtrGraphicsBufferWriteV1 bw{0, 0, 1, 0, buf};
                                        uint32_t ib = 1, groups[] = {2, 1, 1};
                                        CtrGraphicsPushDataV1 pushR{};
                                        pushR.version = 1;
                                        pushR.size = sizeof(pushR);
                                        if (session.WriteGraphicsBuffers(cl, &bw, 1) ||
                                            session.WriteGraphicsImages(cl, &ib, &image, 1) ||
                                            ctr_video_compute_dispatch_v1(&session, cp, &pushR, groups) ||
                                            session.CopyBufferInitial(buf, raw, sizeof(raw))) {
                                            status = 252;
                                            break;
                                        }
                                        for (size_t i = 0; i < 16; ++i)
                                            if (raw[i] != (uint32_t(source[i]) << 24)) status = 252;
                                        if (status == 252 || ctr_video_compute_pipeline_release_v1(&session, cp) ||
                                            session.ReleaseGraphicsLayout(cl) || session.ReleaseBuffer(buf)) {
                                            status = 252;
                                            break;
                                        }
                                        auto invalid = r;
                                        invalid.tile_mode = 14;
                                        uint64_t rejected = 999;
                                        if (session.CreateSampledImage(&invalid, &rejected) != 88 || rejected != 999 ||
                                            session.ReleaseSampledImage(image)) {
                                            status = 252;
                                            break;
                                        }
                                    }
                                    if (status == 252) break;
                                    std::puts(
                                        "R8 tile8 actual1024 and2048 linear GPU upload readback byte exact alphaR sampled16 guards32 malformed extent short reject88");
                                    CtrGraphicsImageParametersV1 writerAlias{};
                                    writerAlias.version = 1;
                                    writerAlias.size = sizeof(writerAlias);
                                    writerAlias.type = 9;
                                    writerAlias.width = writerAlias.height = writerAlias.pitch = 64;
                                    writerAlias.depth = writerAlias.layers = writerAlias.levels = writerAlias.samples =
                                        1;
                                    writerAlias.data_format = 10;
                                    writerAlias.guest_address = 1;
                                    uint64_t writerAliasToken = 0;
                                    std::vector<uint8_t> writerAliasOutput(64 * 64 * 4, 99);
                                    if (session.CreateSampledImage(&writerAlias, &writerAliasToken) ||
                                        session.CopyColorToSampled(colorOwner, writerAliasToken)) {
                                        status = 194;
                                        break;
                                    }
                                    viewport.viewports[0].y = 3;
                                    viewport.scissors[0].offset.y = 2;
                                    if (drawGpu() ||
                                        session.ReadbackSampledImage(writerAliasToken, writerAliasOutput.data(),
                                                                     writerAliasOutput.size(), 64 * 4) ||
                                        writerAliasOutput != drawReadback) {
                                        status = 194;
                                        break;
                                    }
                                    // A sampled snapshot remains the previous writer until an explicit real refresh.
                                    if (session.CopyColorToSampled(colorOwner, writerAliasToken) ||
                                        session.ReadbackSampledImage(writerAliasToken, writerAliasOutput.data(),
                                                                     writerAliasOutput.size(), 64 * 4)) {
                                        status = 194;
                                        break;
                                    }
                                    for (size_t pixel = 0; pixel < 64 * 64; ++pixel) {
                                        const uint8_t expected[4] = {255, 0, 0, 255};
                                        for (size_t channel = 0; channel < 4; ++channel)
                                            if (writerAliasOutput[pixel * 4 + channel] !=
                                                (pixel < 192 ? expected[channel] : 0))
                                                status = 194;
                                    }
                                    if (status == 194 || session.ReleaseSampledImage(writerAliasToken)) {
                                        status = 194;
                                        break;
                                    }
                                    viewport.viewports[0].y = 1;
                                    viewport.scissors[0].offset.y = 0;
                                    std::puts(
                                        "GPU_color_writer_to_sampled=twoRows_snapshot_then_thirdDraw_snapshot_stays_old_explicit_fenced_refresh192red3904preserved0diff authored_noGuestWriteback");
                                    drawArgs.first_vertex = 1;
                                    if (drawGpu() != 88) {
                                        status = 191;
                                        break;
                                    }
                                    drawArgs.first_vertex = 0;
                                    drawArgs.written_buffers = 1;
                                    if (drawGpu() != 88) {
                                        status = 191;
                                        break;
                                    }
                                    drawArgs.written_buffers = 0;
                                    if (session.ReleaseBuffer(triangleBuffer) ||
                                        session.ReleaseBuffer(triangleColorBuffer) || drawGpu() != 62 ||
                                        session.ReleaseGraphicsLayout(layout) ||
                                        session.ReleaseColorAttachment(colorOwner) ||
                                        ctr_video_graphics_pipeline_release_v1(&session, pipeline) ||
                                        ctr_video_graphics_pipeline_release_v1(&session, pipeline) != 62) {
                                        status = 183;
                                        break;
                                    }
                                    pipeline = 999;
                                    key.primitive_type = 17;
                                    // Released layout is rejected before state construction; explicit stale token.
                                    if (createAuthoredPipeline() != 62 || pipeline != 999) {
                                        status = 183;
                                        break;
                                    }
                                    std::puts(
                                        "graphics_pipeline_actual=paired_authored_SPIRV_triangle_sample1_colorUNORM_dynamic18plusdepthbounds real_create_release0 stale62 noGPUdraw");
                                }
                                CtrGraphicsBindingContractV1 only{};
                                only.version = 1;
                                only.size = sizeof(only);
                                only.descriptor_count = 1;
                                only.unified_after = 1;
                                only.buffer_after = 1;
                                only.descriptors[0] = {0, 12, 0, 1, 0, 1};
                                uint64_t onlyToken = 999;
                                if (ctr_video_graphics_vertex_layout_create_v1(&session, &only, &onlyToken) ||
                                    onlyToken == 999) {
                                    status = 170;
                                    break;
                                }
                                CtrGraphicsBufferWriteV1 clip{0, 0, 1, 1, 0};
                                if (ctr_video_graphics_layout_buffers_v1(&session, onlyToken, &clip, 1) ||
                                    ctr_video_graphics_layout_samplers_v1(&session, onlyToken, nullptr, nullptr, 0) ||
                                    ctr_video_graphics_layout_images_v1(&session, onlyToken, nullptr, nullptr, 0) ||
                                    ctr_video_graphics_layout_release_v1(&session, onlyToken)) {
                                    status = 170;
                                    break;
                                }
                                only.unified_before = 1;
                                onlyToken = 999;
                                if (ctr_video_graphics_vertex_layout_create_v1(&session, &only, &onlyToken) != 60 ||
                                    onlyToken != 999) {
                                    status = 170;
                                    break;
                                }
                                std::puts(
                                    "vertex-only actual descriptor/pipeline layout set update/release0; sharedorigin0 nonzero60 atomic, no fragment binding/push stage; no pipeline/draw");
                                CtrGraphicsBindingContractV1 ps{}, vs{};
                                ps.version = vs.version = 1;
                                ps.size = vs.size = sizeof(ps);
                                ps.descriptor_count = 3;
                                ps.unified_after = 4;
                                ps.buffer_after = 1;
                                ps.descriptors[0] = {0, 0, 0, 1, 0, 1};
                                ps.descriptors[1] = {1, 4, 1, 2, UINT32_MAX, 0};
                                ps.descriptors[2] = {2, 8, 3, 1, UINT32_MAX, 0};
                                vs.unified_before = 4;
                                vs.unified_after = 5;
                                vs.buffer_before = 1;
                                vs.buffer_after = 2;
                                vs.descriptor_count = 1;
                                vs.descriptors[0] = {0, 12, 4, 1, 1, 1};
                                uint64_t layoutToken = 999;
                                if (ctr_video_graphics_layout_create_v1(&session, &ps, &vs, &layoutToken) ||
                                    layoutToken == 999) {
                                    status = 162;
                                    break;
                                }
                                std::array<uint32_t, 4> initialized{1, 2, 3, 4};
                                uint64_t storageToken = 0;
                                if (ctr_video_buffer_create_v1(&session, initialized.data(), sizeof(initialized), 1,
                                                               &storageToken)) {
                                    status = 163;
                                    break;
                                }
                                CtrGraphicsBufferWriteV1 writes[2] = {{0, 0, 1, 0, storageToken}, {4, 1, 1, 1, 0}};
                                if (ctr_video_graphics_layout_buffers_v1(&session, layoutToken, writes, 1) != 60) {
                                    status = 163;
                                    break;
                                }
                                writes[0].buffer_slot = 7;
                                if (ctr_video_graphics_layout_buffers_v1(&session, layoutToken, writes, 2) != 60) {
                                    status = 163;
                                    break;
                                }
                                writes[0].buffer_slot = 0;
                                int wrongWrite = 0;
                                std::thread writer([&] {
                                    wrongWrite = ctr_video_graphics_layout_buffers_v1(&session, layoutToken, writes, 2);
                                });
                                writer.join();
                                if (wrongWrite != 116 ||
                                    ctr_video_graphics_layout_buffers_v1(&session, layoutToken, writes, 2) ||
                                    ctr_video_graphics_layout_buffers_v1(&session, layoutToken, writes, 2) != 91 ||
                                    ctr_video_buffer_release_v1(&session, storageToken)) {
                                    status = 163;
                                    break;
                                }
                                std::puts(
                                    "paired storage descriptor update real buffer+explicit null ClipPlanes; owner retained after token release; count/slot60 owner116 rewrite91; no pipeline/draw");
                                CtrGraphicsSamplerParametersV1 samplerParameters{};
                                samplerParameters.version = 1;
                                samplerParameters.size = sizeof(samplerParameters);
                                samplerParameters.max_anisotropy = 1;
                                uint64_t samplerToken = 0;
                                uint32_t samplerBinding = 3;
                                if (ctr_video_sampler_create_v1(&session, &samplerParameters, &samplerToken)) {
                                    status = 164;
                                    break;
                                }
                                samplerBinding = 2;
                                if (ctr_video_graphics_layout_samplers_v1(&session, layoutToken, &samplerBinding,
                                                                          &samplerToken, 1) != 60) {
                                    status = 164;
                                    break;
                                }
                                samplerBinding = 3;
                                if (ctr_video_graphics_layout_samplers_v1(&session, layoutToken, &samplerBinding,
                                                                          &samplerToken, 1) ||
                                    ctr_video_graphics_layout_samplers_v1(&session, layoutToken, &samplerBinding,
                                                                          &samplerToken, 1) != 91 ||
                                    ctr_video_sampler_release_v1(&session, samplerToken)) {
                                    status = 164;
                                    break;
                                }
                                std::puts(
                                    "paired sampler descriptor update real sampler retained after token release; badbinding60 rewrite91; no imagewrites/pipeline/draw");
                                std::shared_ptr<CtrGraphicsLayoutOwner> retainedLayout;
                                if (session.RetainGraphicsLayout(layoutToken, retainedLayout) || !retainedLayout ||
                                    !retainedLayout->PipelineLayout() || !retainedLayout->DescriptorSet()) {
                                    status = 181;
                                    break;
                                }
                                const auto retainedHandle = retainedLayout->PipelineLayout();
                                std::weak_ptr<CtrGraphicsLayoutOwner> weakLayout = retainedLayout;
                                int wrong = 0;
                                std::thread worker(
                                    [&] { wrong = ctr_video_graphics_layout_release_v1(&session, layoutToken); });
                                worker.join();
                                if (wrong != 116 || ctr_video_graphics_layout_release_v1(&session, layoutToken) ||
                                    ctr_video_graphics_layout_release_v1(&session, layoutToken) != 62) {
                                    status = 162;
                                    break;
                                }
                                std::shared_ptr<CtrGraphicsLayoutOwner> missingLayout;
                                if (weakLayout.expired() || retainedLayout->PipelineLayout() != retainedHandle ||
                                    session.RetainGraphicsLayout(layoutToken, missingLayout) != 62 || missingLayout) {
                                    status = 181;
                                    break;
                                }
                                if (!CtrPipelineOwnerControl::Run(device, retainedLayout)) {
                                    status = 182;
                                    break;
                                }
                                std::puts(
                                    "graphics_pipeline_owner_injection=shader1_shader2_partialpipeline_failure_cleanup_success_atomic60 noGPUcreation");
                                retainedLayout.reset();
                                if (!weakLayout.expired()) {
                                    status = 181;
                                    break;
                                }
                                std::puts(
                                    "graphics_layout_retention=real_layout_set_survive_public_release_last_owner_expires stale62 noGPUdraw");
                                CtrGraphicsBindingContractV1 imagePs{}, imageVs{};
                                imagePs.version = imageVs.version = 1;
                                imagePs.size = imageVs.size = sizeof(imagePs);
                                imagePs.descriptor_count = 1;
                                imagePs.unified_after = 1;
                                imagePs.descriptors[0] = {1, 0, 0, 1, UINT32_MAX, 0};
                                imageVs.unified_before = imageVs.unified_after = 1;
                                uint64_t imageLayout = 0, imageToken = 0;
                                uint32_t imageBinding = 0;
                                CtrGraphicsImageParametersV1 imageParameters{};
                                imageParameters.version = 1;
                                imageParameters.size = sizeof(imageParameters);
                                imageParameters.type = 9;
                                imageParameters.width = imageParameters.height = imageParameters.pitch = 4;
                                imageParameters.depth = imageParameters.layers = imageParameters.levels =
                                    imageParameters.samples = 1;
                                imageParameters.data_format = 10;
                                imageParameters.swizzle[0] = 4;
                                imageParameters.swizzle[1] = 5;
                                imageParameters.swizzle[2] = 6;
                                imageParameters.swizzle[3] = 7;
                                std::array<uint32_t, 16> pixels{};
                                if (ctr_video_graphics_layout_create_v1(&session, &imagePs, &imageVs, &imageLayout) ||
                                    ctr_video_sampled_image_create_v1(&session, &imageParameters, &imageToken) ||
                                    ctr_video_graphics_layout_images_v1(&session, imageLayout, &imageBinding,
                                                                        &imageToken, 1) != 91 ||
                                    ctr_video_sampled_image_upload_linear_v1(&session, imageToken, pixels.data(),
                                                                             sizeof(pixels), 16) ||
                                    ctr_video_graphics_layout_images_v1(&session, imageLayout, &imageBinding,
                                                                        &imageToken, 1) ||
                                    ctr_video_graphics_layout_images_v1(&session, imageLayout, &imageBinding,
                                                                        &imageToken, 1) != 91 ||
                                    ctr_video_sampled_image_release_v1(&session, imageToken) ||
                                    ctr_video_graphics_layout_release_v1(&session, imageLayout)) {
                                    status = 165;
                                    break;
                                }
                                std::puts(
                                    "paired sampled image descriptor write uploaded4x4 retained after token release; undefined91 rewrite91; no shaderpipeline/draw");
                                layoutToken = 999;
                                vs.descriptors[0].binding = 3;
                                if (ctr_video_graphics_layout_create_v1(&session, &ps, &vs, &layoutToken) != 60 ||
                                    layoutToken != 999) {
                                    status = 162;
                                    break;
                                }
                                vs.descriptors[0].binding = 4;
                                vs.userdata_after = 17;
                                if (ctr_video_graphics_layout_create_v1(&session, &ps, &vs, &layoutToken) != 60 ||
                                    layoutToken != 999) {
                                    status = 162;
                                    break;
                                }
                                std::puts(
                                    "graphics paired actual descriptor set layout/pipeline layout128/pool/unwritten set allocation release0; array2 gap preserved PS-first/VS, malformed60 atomic owner116 stale62; no resource binding/shaderpipeline/draw");
                            }

                            std::puts(
                                "session tiled upload CABI three extents exact nonuniform authored pixels; real detile/upload/readback fences, wrongthread116 extent88 partial60 undefined91 stale62 padding16 guard256; synthetic=true R3=false");
                        }
                        {
                            CtrGraphicsSamplerParametersV1 p{};
                            p.version = 1;
                            p.size = sizeof(p);
                            p.clamp_x = 2;
                            p.clamp_y = 2;
                            p.max_lod = 15.99609375f;
                            p.max_anisotropy = 4;
                            uint64_t token = 999;
                            if (ctr_video_sampler_create_v1(&session, &p, &token) || !token || token == 999 ||
                                ctr_video_sampler_release_v1(&session, token) ||
                                ctr_video_sampler_release_v1(&session, token) != 62) {
                                status = 155;
                                break;
                            }
                            int wrong = 0;
                            std::thread worker([&] { wrong = ctr_video_sampler_create_v1(&session, &p, &token); });
                            worker.join();
                            p.mag_filter = 3;
                            p.anisotropic = 1;
                            token = 999;
                            if (wrong != 116 || ctr_video_sampler_create_v1(&session, &p, &token) != 88 ||
                                token != 999 || ctr_video_sampler_create_v1(nullptr, &p, &token) != 60) {
                                status = 155;
                                break;
                            }
                            std::puts(
                                "session sampler actual Vulkan create/release/stale62 wrongthread116 disabledaniso88 atomic; no binding or gameframe");
                        }

                        {
                            CtrVideoBufferAttributesV1 attributes{0x80000000U, 0, 0, 1920, 1080, 1920, 0, 0, 0};
                            std::unique_ptr<CtrTiledSurface> first, second;
                            if (CtrTiledSurface::Create(device, deviceGet, memory, attributes, 73110945792ULL, first) ||
                                CtrTiledSurface::Create(device, deviceGet, memory, attributes, 73119301632ULL,
                                                        second) ||
                                !first->Image() || !second->Image() || first->Image() == second->Image() ||
                                first->guestBytes != 8355840 || first->paddedHeight != 1088) {
                                status = 134;
                                break;
                            }
                            CtrTiledSurfaceRegistry registry;
                            const uint64_t addresses[2] = {73110945792ULL, 73119301632ULL};
                            uint32_t group = 999;
                            if (registry.Register(device, deviceGet, memory, 0, 2, attributes, addresses, group) ||
                                group != 0 || !registry.Surface(0) || !registry.Surface(1)) {
                                status = 135;
                                break;
                            }
                            std::array<uint64_t, 16> labels{};
                            uint64_t generation = 0;
                            registry.LabelSnapshot(labels, generation);
                            if (generation != 1) {
                                status = 135;
                                break;
                            }
                            for (const auto label : labels)
                                if (label) status = 135;
                            uint32_t rejected = 999;
                            if (registry.Register(device, deviceGet, memory, 1, 2, attributes, addresses, rejected) !=
                                    109 ||
                                rejected != 999 ||
                                registry.Register(device, deviceGet, memory, 15, 2, attributes, addresses, rejected) !=
                                    60 ||
                                registry.Unregister(0) || registry.Surface(0) || registry.Surface(1) ||
                                registry.Unregister(0) != 62) {
                                status = 135;
                                break;
                            }
                            const uint64_t invalid[2] = {addresses[0], UINT64_MAX - 10};
                            if (registry.Register(device, deviceGet, memory, 0, 2, attributes, invalid, rejected) !=
                                    112 ||
                                registry.Surface(0) || registry.Surface(1) || rejected != 999) {
                                status = 135;
                                break;
                            }
                            registry.LabelSnapshot(labels, generation);
                            if (generation != 1) {
                                status = 135;
                                break;
                            }
                            for (uint32_t gi = 0; gi < 4; ++gi)
                                if (registry.Register(device, deviceGet, memory, gi, 1, attributes, addresses, group) ||
                                    group != gi)
                                    status = 135;
                            if (registry.Register(device, deviceGet, memory, 4, 1, attributes, addresses, rejected) !=
                                    108 ||
                                rejected != 999)
                                status = 135;
                            for (uint32_t gi = 0; gi < 4; ++gi)
                                if (registry.Unregister(gi)) status = 135;
                            if (status == 135) break;
                            std::puts(
                                "tiled registry real images passed group4_slot16=true atomic_second_failure=true overlap109=true full108=true labels128B_zero_generation=true unregister_releases=true guest_label_mapping_missing=true");
                            if (!registry.LabelAddress() || registry.PublishLabel(16, 1) != 60 ||
                                registry.WaitLabel(0, 1, std::chrono::milliseconds(0)) != 52) {
                                status = 136;
                                break;
                            }
                            auto waitLabel = std::async(std::launch::async, [&registry] {
                                return registry.WaitLabel(15, 1, std::chrono::milliseconds(500));
                            });
                            if (registry.PublishLabel(15, 1) || waitLabel.get() ||
                                registry.LabelSnapshot(labels, generation) || labels[15] != 1 || generation != 6) {
                                status = 136;
                                break;
                            }
                            auto labelOwner = registry.RetainLabelMemory();
                            auto labelCopy = labelOwner->api();
                            uint64_t lastLabel = 0;
                            if (labelCopy.read(labelCopy.context, registry.LabelAddress() + 120, &lastLabel, 8) ||
                                lastLabel != 1 || registry.WaitLabel(16, 0, std::chrono::milliseconds(0)) != 60) {
                                status = 136;
                                break;
                            }
                            std::puts(
                                "tiled labels mapped128B passed checked_last_slot=true publication_before_notify=true waiter_predicate=true timeout52=true invalid60=true no_GPU_completion_claim=true");
                            {
                                constexpr uint64_t span = 8355840ULL * 2;
                                auto backing = std::make_shared<ctrport::DirectMemoryBacking>(span);
                                std::weak_ptr<ctrport::DirectMemoryBacking> weak = backing;
                                int64_t physical = -1;
                                void* mapped = nullptr;
                                if (backing->allocate(0, span, span, 0, 0, &physical) ||
                                    backing->map(&mapped, span, 51, 0, physical, 0)) {
                                    status = 137;
                                    break;
                                }
                                const uint64_t mappedAddresses[2] = {reinterpret_cast<uint64_t>(mapped),
                                                                     reinterpret_cast<uint64_t>(mapped) + 8355840};
                                uint32_t mappedGroup = 999;
                                if (registry.RegisterMapped(device, deviceGet, memory, 0, 2, attributes,
                                                            mappedAddresses, backing, mappedGroup)) {
                                    status = 137;
                                    break;
                                }
                                backing.reset();
                                if (weak.expired() || registry.Unregister(mappedGroup) || !weak.expired()) {
                                    status = 137;
                                    break;
                                }
                                std::puts(
                                    "tiled mapped registration lifetime passed two_full8355840B_leases=true owner_retained_until_unregister=true release_after_images=true no_coherence_claim=true");
                            }
                            const auto retained = first.get();
                            attributes.tiling = 1;
                            if (CtrTiledSurface::Create(device, deviceGet, memory, attributes, 1, first) != 88 ||
                                first.get() != retained) {
                                status = 134;
                                break;
                            }
                            attributes.tiling = 0;
                            attributes.pitch_pixels = 1919;
                            if (CtrTiledSurface::Create(device, deviceGet, memory, attributes, 1, first) != 89 ||
                                first.get() != retained) {
                                status = 134;
                                break;
                            }
                            attributes.pitch_pixels = 1920;
                            if (CtrTiledSurface::Create(device, deviceGet, memory, attributes, UINT64_MAX - 10,
                                                        first) != 112 ||
                                first.get() != retained) {
                                status = 134;
                                break;
                            }
                            std::puts(
                                "tiled surface actual Vulkan backing passed two1920x1080_images=true guest_span8355840=true negative_preserves=true contents_undefined=true guest_registration_missing=true R3=false");
                        }
                        if (body) {
                            CtrVideoSessionApiV1 api{};
                            status = ctr_video_session_api_v1(&session, &api);
                            if (status) break;
                            invoked = true;
                            try {
                                status = body(&api, si.imageExtent.width, si.imageExtent.height,
                                              static_cast<uint32_t>(si.imageFormat), user);
                            } catch (...) {
                                status = 90;
                            }
                            break;
                        }
                        // Padded host snapshot proves row-pitch handling independent of GPU extent.
                        const size_t pitch = size_t(si.imageExtent.width) * 4 + 16;
                        std::vector<unsigned char> pixels(pitch * si.imageExtent.height, 0xA5);
                        {
                            PresentationSession::HostBufferInput inputs[2] = {{pixels.data(), pixels.size(), pitch},
                                                                              {nullptr, 0, pitch}};
                            uint32_t group = 999;
                            if (session.RegisterHostGroup(14, 2, inputs, group) != 61 || group != 999 ||
                                session.CaptureHostSlot(14)) {
                                status = 110;
                                break;
                            }
                            inputs[1] = inputs[0];
                            if (session.RegisterHostGroup(14, 2, inputs, group) || group != 0) {
                                status = 110;
                                break;
                            }
                            auto retained = session.CaptureHostSlot(14);
                            uint32_t untouched = 999;
                            if (!retained || session.RegisterHostGroup(15, 1, inputs, untouched) != 109 ||
                                untouched != 999 || session.UnregisterHostFrame(14, retained->token) ||
                                !session.UnregisterHostGroup(group) || session.CaptureHostSlot(14) ||
                                session.CaptureHostSlot(15) || session.UnregisterHostGroup(group) ||
                                retained->bytes[0] != 0xA5) {
                                status = 110;
                                break;
                            }
                            uint32_t groups[4]{};
                            for (uint32_t groupIndex = 0; groupIndex < 4; ++groupIndex)
                                if (session.RegisterHostGroup(10 + groupIndex, 1, inputs, groups[groupIndex]))
                                    status = 110;
                            if (session.RegisterHostGroup(9, 1, inputs, untouched) != 108 || untouched != 999)
                                status = 110;
                            for (auto g : groups)
                                if (!session.UnregisterHostGroup(g)) status = 110;
                            if (status == 110) break;
                            std::puts(
                                "host buffer groups passed atomic_rollback=true overlap_rejected=true capacity4=true retained_snapshot=true");
                        }
                        for (uint32_t y = 0; y < si.imageExtent.height; ++y)
                            for (uint32_t x = 0; x < si.imageExtent.width; ++x) {
                                const size_t offset = size_t(y) * pitch + size_t(x) * 4;
                                const unsigned char value = ((x / 32 + y / 32) & 1) ? 192 : 48;
                                pixels[offset] = value;
                                pixels[offset + 1] = value;
                                pixels[offset + 2] = value;
                                pixels[offset + 3] = 255;
                            }
                        auto frame = [&](const void* data, size_t length, size_t stride) {
                            return session.Present(data, length, stride);
                        };
                        if (frame(nullptr, pixels.size(), pitch) != 41 || frame(pixels.data(), 1, pitch) != 41 ||
                            frame(pixels.data(), pixels.size(), 1) != 41 ||
                            frame(pixels.data(), pixels.size(), SIZE_MAX) != 41) {
                            status = 42;
                            break;
                        }
                        // Reject unsupported surface metadata before any acquire/submit.
                        if (present_control(device, deviceGet, swap, q, images, {0, 1}, si.imageFormat, memory,
                                            pixels.data(), pixels.size(), pitch) != 34 ||
                            present_control(device, deviceGet, swap, q, images, {16385, 1}, si.imageFormat, memory,
                                            pixels.data(), pixels.size(), pitch) != 34 ||
                            present_control(device, deviceGet, swap, q, images, si.imageExtent,
                                            VK_FORMAT_R16G16B16A16_SFLOAT, memory, pixels.data(), pixels.size(),
                                            pitch) != 35) {
                            status = 43;
                            break;
                        }
                        std::printf("host snapshot input rejection passed=7 padded_pitch=%zu\n", pitch);
                        CtrVideoSessionApiV1 api{};
                        if (ctr_video_session_api_v1(&session, &api) || api.version != 1 || api.size != sizeof(api)) {
                            status = 66;
                            break;
                        }
                        {
                            {
                                ctrport::DirectMemoryBacking direct(65536);
                                int64_t physical = -1;
                                void* command = nullptr;
                                if (direct.allocate(0, 65536, 16384, 0, 0, &physical) ||
                                    direct.map(&command, 16384, 51, 0, physical, 0)) {
                                    status = 131;
                                    break;
                                }
                                const auto commandCopy = ctrport::make_direct_copy_api(direct);
                                const auto commandAddress = reinterpret_cast<uint64_t>(command);
                                std::memset(command, 0x63, 16384);
                                uint32_t consumed = 999;
                                if (ctr_gnm_draw_init_350(&commandCopy, commandAddress, 1048548, 0, &consumed) ||
                                    consumed != 256 || static_cast<uint32_t*>(command)[0] != 0xc0012800u ||
                                    static_cast<uint32_t*>(command)[255] != 0x63636363u ||
                                    static_cast<unsigned char*>(command)[1024] != 0x63) {
                                    status = 131;
                                    break;
                                }
                                consumed = 999;
                                if (ctr_gnm_draw_init_350(&commandCopy, commandAddress + 16384 - 512, 256, 0,
                                                          &consumed) != 112 ||
                                    consumed != 999 ||
                                    ctr_gnm_draw_init_350(&commandCopy, commandAddress, 256, -1, &consumed) != 60 ||
                                    consumed != 999) {
                                    status = 131;
                                    break;
                                }
                                std::puts(
                                    "Gnm init350 direct ledger composition passed real_mapped_authority=true exact256DWORD=true untouched_payload=true gap112=true negative_policy60=true no_GPU_coherence=true");
                            }
                            void* mapped = VirtualAlloc(nullptr, 4096, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
                            if (!mapped) {
                                status = 114;
                                break;
                            }
                            auto owner = std::shared_ptr<void>(
                                mapped, [](void* pointer) { VirtualFree(pointer, 0, MEM_RELEASE); });
                            ctrport::GuestCopyRegion region(owner, 4096);
                            const auto copy = ctrport::make_guest_copy_api(region);
                            const auto address = reinterpret_cast<uint64_t>(mapped);
                            {
                                uint32_t initResult = 999;
                                if (ctr_gnm_draw_init_350(nullptr, 0, 255, 0, &initResult) || initResult != 0) {
                                    status = 130;
                                    break;
                                }
                                for (uint32_t neo = 0; neo < 2; ++neo) {
                                    std::memset(mapped, 0x71, 4096);
                                    initResult = 999;
                                    if (ctr_gnm_draw_init_350(&copy, address, 1048548, neo, &initResult) ||
                                        initResult != 256) {
                                        status = 130;
                                        break;
                                    }
                                    const auto* words = static_cast<uint32_t*>(mapped);
                                    const size_t sequenceWords = neo ? 134 : 124;
                                    const auto* sequence = neo ? Libraries::GnmDriver::InitSequence350Neo.data()
                                                               : Libraries::GnmDriver::InitSequence350.data();
                                    const size_t used = 12 + sequenceWords;
                                    if (words[0] != 0xc0012800u || words[11] != 10 ||
                                        std::memcmp(words + 12, sequence + 2, sequenceWords * 4) ||
                                        words[used] != (0xc0001000u | (static_cast<uint32_t>(254 - used) << 16)) ||
                                        words[used + 1] != 0 || words[255] != 0x71717171u ||
                                        static_cast<unsigned char*>(mapped)[1024] != 0x71) {
                                        status = 130;
                                        break;
                                    }
                                }
                                if (status == 130) break;
                                initResult = 999;
                                if (ctr_gnm_draw_init_350(&copy, UINT64_MAX - 8, 256, 0, &initResult) != 112 ||
                                    initResult != 999 ||
                                    ctr_gnm_draw_init_350(&copy, address, 256, 2, &initResult) != 60 ||
                                    initResult != 999) {
                                    status = 130;
                                    break;
                                }
                                DWORD old = 0, ignored = 0;
                                if (!VirtualProtect(mapped, 4096, PAGE_READONLY, &old) ||
                                    ctr_gnm_draw_init_350(&copy, address, 256, 0, &initResult) != 112 ||
                                    initResult != 999 || !VirtualProtect(mapped, 4096, old, &ignored)) {
                                    status = 130;
                                    break;
                                }
                                std::puts(
                                    "Gnm init350 control passed base_neo_sequences=true exact256DWORD=true untouched_tail=true small_no_copy=true invalid_policy60=true readonly_overflow112=true initResult_preserved=true synthetic_only=true");
                            }
                            {
                                std::memset(mapped, 0x71, 4096);
                                if (ctr_video_write_gamma(&copy, address, 1.25f)) {
                                    status = 127;
                                    break;
                                }
                                float copiedGamma = 0;
                                std::memcpy(&copiedGamma, mapped, 4);
                                if (copiedGamma != 1.25f || static_cast<unsigned char*>(mapped)[4] != 0x71 ||
                                    ctr_video_write_gamma(&copy, UINT64_MAX - 1, 1.25f) != 112 ||
                                    ctr_video_write_gamma(&copy, address + 4094, 1.25f) != 112) {
                                    status = 127;
                                    break;
                                }
                                unsigned char before[16];
                                std::memcpy(before, mapped, 16);
                                DWORD oldProtection = 0, ignoredProtection = 0;
                                if (!VirtualProtect(mapped, 4096, PAGE_READONLY, &oldProtection) ||
                                    ctr_video_write_gamma(&copy, address, 1.5f) != 112 ||
                                    std::memcmp(before, mapped, 16) ||
                                    !VirtualProtect(mapped, 4096, oldProtection, &ignoredProtection)) {
                                    status = 127;
                                    break;
                                }
                                std::puts(
                                    "gamma owned mapping passed exact4=true reserved_untouched=true readonly_overflow_span112=true unchanged=true synthetic_mapping=true");
                            }
                            {
                                CtrVideoResolutionConfigV1 config{};
                                config.size = sizeof(config);
                                config.version = 1;
                                config.configured = 1;
                                config.value.full_width = 1920;
                                config.value.full_height = 1080;
                                config.value.pane_width = 1280;
                                config.value.pane_height = 720;
                                std::memset(mapped, 0x71, 4096);
                                ctrport::OwnerDispatch resolutionDispatch;
                                const auto transport = ctrport::make_owner_dispatch_api(resolutionDispatch);
                                auto response = std::async(std::launch::async, [&] {
                                    return ctr_video_dispatch_resolution(&transport, &config, &copy, address);
                                });
                                const bool pumped = resolutionDispatch.pump(1000);
                                if (!pumped) resolutionDispatch.close();
                                int resolutionResult = 90;
                                try {
                                    resolutionResult = response.get();
                                } catch (...) {
                                    resolutionResult = 90;
                                }
                                resolutionDispatch.close();
                                if (!pumped || resolutionResult || std::memcmp(mapped, &config.value, 48) ||
                                    static_cast<unsigned char*>(mapped)[48] != 0x71 ||
                                    ctr_video_dispatch_resolution(&transport, &config, &copy, address) != 115 ||
                                    ctr_video_copy_resolution(&config, &copy, UINT64_MAX - 8) != 112) {
                                    status = 126;
                                    break;
                                }
                                std::puts(
                                    "resolution owner integration passed worker_C_ABI=true real_owned_write48=true sentinel48=true closed115=true overflow112=true synthetic_config=true");
                            }
                            std::memset(mapped, 0x71, 4096);
                            ctrport::OwnerDispatch dispatch;
                            const auto dispatcherApi = ctrport::make_owner_dispatch_api(dispatch);
                            auto caller = std::async(std::launch::async, [&] {
                                return ctr_video_dispatch_public_status(&dispatcherApi, &api, &copy, address, 0);
                            });
                            const bool pumped = dispatch.pump(1000);
                            if (!pumped) dispatch.close();
                            int copyResult = 90;
                            try {
                                copyResult = caller.get();
                            } catch (...) {
                                copyResult = 90;
                            }
                            dispatch.close();
                            if (!pumped || copyResult ||
                                ctr_video_dispatch_public_status(&dispatcherApi, &api, &copy, address, 0) != 115 ||
                                ctr_video_dispatch_public_status(nullptr, &api, &copy, address, 0) != 60) {
                                status = 114;
                                break;
                            }
                            CtrVideoPublicFlipStatus copied{};
                            std::memcpy(&copied, mapped, sizeof(copied));
                            if (copied.count || copied.flip_arg != -1 || copied.current_buffer != -1 ||
                                static_cast<unsigned char*>(mapped)[64] != 0x71) {
                                status = 114;
                                break;
                            }
                            DWORD previous = 0;
                            if (!VirtualProtect(mapped, 4096, PAGE_READONLY, &previous) ||
                                ctr_video_copy_public_status(&api, &copy, address, 0) != 112 ||
                                ctr_video_copy_public_status(&api, &copy, UINT64_MAX - 8, 0) != 112) {
                                status = 114;
                                break;
                            }
                            region.revoke();
                            if (ctr_video_copy_public_status(&api, &copy, address, 0) != 112) {
                                status = 114;
                                break;
                            }
                            CtrVideoPublicFlipStatus unchanged{};
                            std::memcpy(&unchanged, mapped, sizeof(unchanged));
                            if (std::memcmp(&copied, &unchanged, sizeof(copied))) {
                                status = 114;
                                break;
                            }
                            std::puts(
                                "owned status copy integration passed C_ABI_worker_marshaled=true closed_transport115=true exact64=true readonly_overflow_revoked112=true preserved=true retail_mapping=false");
                        }
                        pixels[0] = 17;
                        pixels[1] = 34;
                        pixels[2] = 51;
                        pixels[3] = 68;
                        CtrVideoLinearFrameV1 linear{sizeof(CtrVideoLinearFrameV1),
                                                     0x80000000U,
                                                     si.imageExtent.width,
                                                     si.imageExtent.height,
                                                     static_cast<uint32_t>(pitch / 4),
                                                     pixels.data(),
                                                     pixels.size()};
                        uint64_t linearToken = 0;
                        if (api.register_linear_frame(api.context, 1, &linear, &linearToken)) {
                            status = 91;
                            break;
                        }
                        const auto linearCaptured = session.CaptureHostFrame(1, linearToken);
                        const bool bgra =
                            si.imageFormat == VK_FORMAT_B8G8R8A8_UNORM || si.imageFormat == VK_FORMAT_B8G8R8A8_SRGB;
                        if (!linearCaptured || linearCaptured->bytes[0] != (bgra ? 17 : 51) ||
                            linearCaptured->bytes[3] != 68) {
                            status = 92;
                            break;
                        }
                        pixels[0] = 99;
                        if (api.refresh_linear_frame(api.context, 1, linearToken, &linear) ||
                            session.CaptureHostFrame(1, linearToken) == linearCaptured ||
                            linearCaptured->bytes[0] != (bgra ? 17 : 51) ||
                            session.CaptureHostFrame(1, linearToken)->bytes[0] != (bgra ? 99 : 51)) {
                            status = 119;
                            break;
                        }
                        const auto updatedLinear = session.CaptureHostFrame(1, linearToken);
                        if (api.refresh_linear_frame(api.context, 1, linearToken + 1, &linear) != 77 ||
                            session.CaptureHostFrame(1, linearToken) != updatedLinear) {
                            status = 119;
                            break;
                        }
                        pixels[0] = 17;
                        std::puts(
                            "linear refresh passed same_token=true old_snapshot_retained=true stale77_preserves=true");
                        linear.format = 0x88060000U;
                        uint64_t preserved = 999;
                        if (api.register_linear_frame(api.context, 2, &linear, &preserved) != 88 || preserved != 999) {
                            status = 93;
                            break;
                        }
                        std::printf(
                            "host linear VideoOut byte-layout bridge passed HDR_missing88=true output_preserved=true\n");
                        {
                            CtrVideo::DisplayBufferAttributes attributes{0x80000000U,
                                                                         1,
                                                                         0,
                                                                         si.imageExtent.width,
                                                                         si.imageExtent.height,
                                                                         static_cast<uint32_t>(pitch / 4),
                                                                         0,
                                                                         0,
                                                                         0};
                            PresentationSession::HostBufferInput inputs[2] = {{pixels.data(), pixels.size(), pitch},
                                                                              {nullptr, 0, pitch}};
                            uint32_t group = 999;
                            if (session.RegisterLinearGroup(12, 2, attributes, inputs, group) != 89 || group != 999 ||
                                session.CaptureHostSlot(12)) {
                                status = 116;
                                break;
                            }
                            inputs[1] = inputs[0];
                            if (session.RegisterLinearGroup(12, 2, attributes, inputs, group)) {
                                status = 116;
                                break;
                            }
                            CtrVideo::DisplayBufferAttributes stored{};
                            if (!session.LinearGroupAttributes(group, stored) ||
                                std::memcmp(&stored, &attributes, sizeof(stored))) {
                                status = 116;
                                break;
                            }
                            const auto converted = session.CaptureHostSlot(12);
                            if (!converted || converted->bytes[0] != (bgra ? 17 : 51) || converted->bytes[3] != 68 ||
                                !session.UnregisterHostGroup(group)) {
                                status = 116;
                                break;
                            }
                            attributes.tiling = 0;
                            group = 999;
                            if (session.RegisterLinearGroup(12, 2, attributes, inputs, group) != 88 || group != 999) {
                                status = 116;
                                break;
                            }
                            attributes.tiling = 1;
                            const size_t fixtureSize = 56 + pixels.size() * 2;
                            void* captureMemory =
                                VirtualAlloc(nullptr, fixtureSize, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
                            if (!captureMemory) {
                                status = 118;
                                break;
                            }
                            auto captureOwner = std::shared_ptr<void>(
                                captureMemory, [](void* pointer) { VirtualFree(pointer, 0, MEM_RELEASE); });
                            ctrport::GuestCopyRegion captureRegion(captureOwner, fixtureSize);
                            const auto captureApi = ctrport::make_guest_copy_api(captureRegion);
                            const auto base = reinterpret_cast<uint64_t>(captureMemory);
                            const uint64_t addresses[2] = {base + 56, base + 56 + pixels.size()};
                            if (ctr_video_write_buffer_attributes(&captureApi, base, attributes.format, 1, 0,
                                                                  attributes.width, attributes.height,
                                                                  attributes.pitch) ||
                                std::memcmp(captureMemory, &attributes, 40)) {
                                status = 128;
                                break;
                            }
                            std::memcpy(static_cast<unsigned char*>(captureMemory) + 40, addresses, 16);
                            std::memcpy(static_cast<unsigned char*>(captureMemory) + 56, pixels.data(), pixels.size());
                            std::memcpy(static_cast<unsigned char*>(captureMemory) + 56 + pixels.size(), pixels.data(),
                                        pixels.size());
                            CtrVideoLinearRegistration registration{};
                            uint64_t captureTokens[2] = {777, 888};
                            group = 999;
                            if (ctr_video_capture_linear_group(api, captureApi, 12, 2, base, base + 40, group,
                                                               captureTokens, 2, &registration) ||
                                !session.CaptureHostFrame(12, captureTokens[0])) {
                                status = 118;
                                break;
                            }
                            if (registration.count != 2 || registration.first != 12 || registration.group != group ||
                                registration.addresses[1] != addresses[1] ||
                                registration.tokens[0] != captureTokens[0] ||
                                registration.attributes.format != attributes.format) {
                                status = 118;
                                break;
                            }
                            const auto savedRegistration = registration;
                            const auto beforeRefresh = session.CaptureHostFrame(12, captureTokens[0]);
                            static_cast<unsigned char*>(captureMemory)[56] = 201;
                            if (ctr_video_refresh_captured_linear(api, captureApi, registration, 0) ||
                                session.CaptureHostFrame(12, captureTokens[0]) == beforeRefresh ||
                                beforeRefresh->bytes[0] != (bgra ? 17 : 51) ||
                                session.CaptureHostFrame(12, captureTokens[0])->bytes[0] != (bgra ? 201 : 51)) {
                                status = 120;
                                break;
                            }
                            const auto afterRefresh = session.CaptureHostFrame(12, captureTokens[0]);
                            captureRegion.revoke();
                            if (ctr_video_refresh_captured_linear(api, captureApi, registration, 0) != 112 ||
                                session.CaptureHostFrame(12, captureTokens[0]) != afterRefresh ||
                                !session.UnregisterHostGroup(group)) {
                                status = 120;
                                break;
                            }
                            std::puts(
                                "checked linear refresh passed changed_owned_source=true same_token=true old_snapshot=true revoked112_preserves=true");
                            if (ctr_video_write_buffer_attributes(&captureApi, base, 0, 0, 0, 0, 0, 0) != 112 ||
                                std::memcmp(captureMemory, &attributes, 40)) {
                                status = 128;
                                break;
                            }
                            std::puts(
                                "attribute write capture composition passed real_owned_metadata40=true register_refresh=true revoked112_preserves_metadata=true synthetic_only=true");
                            captureRegion.revoke();
                            group = 999;
                            if (ctr_video_capture_linear_group(api, captureApi, 12, 2, base, base + 40, group,
                                                               captureTokens, 2, &registration) != 112 ||
                                group != 999 || session.CaptureHostSlot(12) ||
                                registration.tokens != savedRegistration.tokens ||
                                registration.addresses != savedRegistration.addresses ||
                                registration.count != savedRegistration.count) {
                                status = 118;
                                break;
                            }
                            std::puts(
                                "owned linear group capture passed metadata_array_pixels=true provenance_retained=true failure_preserves_record=true revoked112=true no_guest_deref=true synthetic_mapping=true");
                            std::puts(
                                "public linear group passed conversion=true partial_rollback=true tiled_missing88=true host_only=true");
                        }
                        pixels[3] = 255;
                        uint64_t token = 0;
                        if (api.register_frame(api.context, 0, pixels.data(), pixels.size(), pitch, &token)) {
                            status = 67;
                            break;
                        }
                        for (unsigned flip = 0; flip < 3; ++flip)
                            if (api.enqueue(api.context, 0, token, 100 + flip)) {
                                status = 68;
                                break;
                            }
                        if (status == 68) break;
                        const auto captured = session.CaptureHostFrame(0, token);
                        pixels[0] = 255;
                        if (api.refresh_frame(api.context, 0, token, pixels.data(), pixels.size(), pitch) ||
                            session.CaptureHostFrame(0, token) == captured || captured->bytes[0] == 255 ||
                            session.CaptureHostFrame(0, token)->bytes[0] != 255) {
                            status = 78;
                            break;
                        }
                        const auto refreshed = session.CaptureHostFrame(0, token);
                        if (api.refresh_frame(api.context, 0, token, pixels.data(), 1, pitch) != 77 ||
                            session.CaptureHostFrame(0, token) != refreshed) {
                            status = 79;
                            break;
                        }
                        std::printf(
                            "host refresh snapshot passed old_queued_bytes_retained=true invalid_preserves_slot=true\n");
                        if (!token || !captured || session.RegisterHostFrame(0, pixels.data(), pixels.size(), pitch) ||
                            session.RegisterHostFrame(16, pixels.data(), pixels.size(), pitch) ||
                            api.unregister_frame(api.context, 0, token) != 0 || session.CaptureHostFrame(0, token) ||
                            session.UnregisterHostFrame(0, token) || session.PresentHostFrame({}) != 44) {
                            status = 45;
                            break;
                        }
                        std::fill(pixels.begin(), pixels.end(),
                                  static_cast<unsigned char>(0)); // Captured bytes must remain independent.
                        if (captured->bytes[3] != 255 || captured->pitch != size_t(si.imageExtent.width) * 4) {
                            status = 46;
                            break;
                        }
                        const auto foreign = std::make_shared<PresentationSession::HostFrame>();
                        if (session.EnqueueHostFrame(foreign, 0) || session.PresentHostFrame(foreign) != 44) {
                            status = 48;
                            break;
                        }
                        const auto displayQueue = displayKernel.queues.create();
                        const auto kernelEventsApi = ctrport::display_event_api(displayKernel);
                        if (api.attach_events(api.context, &kernelEventsApi, displayQueue, 123, 456) ||
                            api.attach_events(api.context, &kernelEventsApi, displayQueue, 123, 456) != 91) {
                            status = 91;
                            break;
                        }
                        if (api.start_pacing(api.context) || api.start_pacing(api.context) != 64) {
                            status = 53;
                            break;
                        }
                        const auto extraQueue = displayKernel.queues.create();
                        if (api.add_event(api.context, &kernelEventsApi, extraQueue, 7, 789)) {
                            status = 96;
                            break;
                        }
                        const auto pacingStart = std::chrono::steady_clock::now();
                        int frameStatus = 0;
                        for (unsigned flip = 0; flip < 3; ++flip) {
                            CtrVideoCompletionV1 completion{};
                            frameStatus = api.present_next(api.context, 500, &completion);
                            if (!frameStatus && (completion.count != flip + 1 || completion.token != token ||
                                                 completion.argument != 100 + flip))
                                frameStatus = 50;
                            if (frameStatus) break;
                        }
                        std::printf(
                            "host registered snapshot lifetime passed unregistered_before_present=true borrowed_source_overwritten=true\n");
                        if (frameStatus) {
                            status = frameStatus;
                            break;
                        }
                        PresentationSession::Completion untouched{9, 8, 7};
                        if (session.PresentNext(untouched) != 47 || untouched.count != 9 || untouched.argument != 7) {
                            status = 51;
                            break;
                        }
                        const auto pacingMs = std::chrono::duration_cast<std::chrono::milliseconds>(
                                                  std::chrono::steady_clock::now() - pacingStart)
                                                  .count();
                        if (api.stop_pacing(api.context) || api.stop_pacing(api.context) != 65 ||
                            !session.EnqueueHostFrame(captured, 200)) {
                            status = 54;
                            break;
                        }
                        CtrVideoProgressV1 timedProgress{};
                        if (api.get_progress(api.context, &timedProgress) || timedProgress.timing_valid != 3 ||
                            !timedProgress.submit_tsc || !timedProgress.completion_tsc ||
                            !timedProgress.completion_microseconds) {
                            status = 101;
                            break;
                        }
                        unsigned char packedFlip[65];
                        std::memset(packedFlip, 0xA5, sizeof(packedFlip));
                        auto invalidTiming = timedProgress;
                        invalidTiming.timing_valid = 0;
                        if (CtrVideo::PackCpuFlip(invalidTiming, packedFlip, 64) != 91 || packedFlip[0] != 0xA5 ||
                            api.get_public_flip(api.context, packedFlip, 64) || packedFlip[64] != 0xA5) {
                            status = 104;
                            break;
                        }
                        CtrVideoPublicFlipStatus flipValue{};
                        std::memcpy(&flipValue, packedFlip, 64);
                        if (flipValue.count != 3 || flipValue.flip_arg != 102 || flipValue.current_buffer != 0 ||
                            flipValue.flip_pending_num != 1 || flipValue.gc_queue_num || flipValue.reserved0 ||
                            flipValue.reserved1) {
                            status = 105;
                            break;
                        }
                        std::printf(
                            "public status packaging passed bounded40_64=true invalid_timing_preserves_output=true CPU_only=true\n");
                        if (session.PresentNextAtVblank(std::chrono::milliseconds(2), untouched) != 52 ||
                            untouched.count != 9) {
                            status = 55;
                            break;
                        }
                        CtrVideoVblankV1 blankStatus{};
                        if (api.get_vblank(api.context, &blankStatus) || blankStatus.count < 3 ||
                            !blankStatus.process_microseconds || !blankStatus.native_tsc) {
                            status = 100;
                            break;
                        }
                        unsigned char packedBlank[41];
                        std::memset(packedBlank, 0xA5, sizeof(packedBlank));
                        if (CtrVideo::PackVblank(blankStatus, packedBlank, 39) != 60 || packedBlank[0] != 0xA5 ||
                            api.get_public_vblank(api.context, packedBlank, 40) || packedBlank[40] != 0xA5) {
                            status = 102;
                            break;
                        }
                        CtrVideoPublicVblankStatus packedValue{};
                        std::memcpy(&packedValue, packedBlank, 40);
                        if (packedValue.count != blankStatus.count || packedValue.tsc != blankStatus.native_tsc ||
                            packedValue.reserved || packedValue.flags) {
                            status = 103;
                            break;
                        }
                        const auto displayEvents = displayKernel.queues.poll(displayQueue, 2);
                        if (displayEvents.size() != 2 || displayEvents[0].ident != 6 ||
                            displayEvents[0].data >> 16 != 102 || ((displayEvents[0].data >> 12) & 15) != 3 ||
                            displayEvents[0].udata != 123 || displayEvents[1].ident != 7 ||
                            displayEvents[1].udata != 456 || api.detach_events(api.context)) {
                            status = 92;
                            break;
                        }
                        const auto extraEvents = displayKernel.queues.poll(extraQueue, 2);
                        if (extraEvents.size() != 1 || extraEvents[0].ident != 7 || extraEvents[0].udata != 789 ||
                            api.remove_event(api.context, extraQueue, 7)) {
                            status = 97;
                            break;
                        }
                        displayKernel.queues.destroy(extraQueue);
                        std::printf(
                            "session independent event C ABI passed extra_vblank_queue=true added_while_clock_live=true\n");
                        displayKernel.queues.destroy(displayQueue);
                        const auto failingQueue = displayKernel.queues.create();
                        if (api.attach_events(api.context, &kernelEventsApi, failingQueue, 0, 0) ||
                            api.start_pacing(api.context)) {
                            status = 94;
                            break;
                        }
                        displayKernel.queues.destroy(failingQueue);
                        if (session.PresentNextAtVblank(std::chrono::milliseconds(500), untouched) != 91 ||
                            untouched.count != 9 || session.PresentNext(untouched) != 91 ||
                            api.stop_pacing(api.context) || api.detach_events(api.context) != 91) {
                            status = 95;
                            break;
                        }
                        CtrVideoProgressV1 failedBefore{}, failedAfter{};
                        const auto failedSource = session.CaptureHostFrame(1, linearToken);
                        if (api.get_progress(api.context, &failedBefore) ||
                            api.submit_linear_frame(api.context, 1, linearToken, &linear, 8888) != 91 ||
                            api.enqueue(api.context, 1, linearToken, 8889) != 63 ||
                            session.CaptureHostFrame(1, linearToken) != failedSource ||
                            api.get_progress(api.context, &failedAfter) ||
                            failedBefore.pending != failedAfter.pending ||
                            failedBefore.submit_tsc != failedAfter.submit_tsc) {
                            status = 123;
                            break;
                        }
                        const auto replacementQueue = displayKernel.queues.create();
                        const int reattached = api.attach_events(api.context, &kernelEventsApi, replacementQueue, 0, 0);
                        displayKernel.queues.destroy(replacementQueue);
                        if (reattached != 91 || session.PresentNext(untouched) != 91) {
                            status = 124;
                            break;
                        }
                        std::puts("sticky event failure reattach rejected91=true explicit_new_session_required=true");
                        std::puts(
                            "sticky event failure admission passed submit91=true enqueue63=true source_pending_timestamp_preserved=true");
                        std::printf(
                            "session deleted kernel queue failure passed sticky91=true timeout_not_reported=true output_unchanged=true\n");
                        std::printf(
                            "session kernel events passed GPU_completed_flips=3 final_argument=102 real_vblank=true detach_after_join=true\n");
                        std::printf(
                            "host vblank paced presents=3 elapsed_ms=%lld owned_clock_stopped_preserves_request=true\n",
                            static_cast<long long>(pacingMs));
                        std::printf(
                            "host FIFO completion passed count=3 args=100,101,102 empty_preserves_output=true foreign_rejected=true\n");
                        if (api.pump_messages(api.context) || !PostMessageW(window, WM_CLOSE, 0, 0) ||
                            api.pump_messages(api.context) != 80 || !IsWindow(window)) {
                            status = 81;
                            break;
                        }
                        std::printf("host window close pump passed close_defers_destroy_until_gpu_cleanup=true\n");
                        std::printf("host persistent device/swapchain session passed frames=3 owner_thread=true\n");
                        VkPhysicalDeviceProperties p{};
                        properties(devices[d], &p);
                        std::printf(
                            "host_control instance/device passed gpu=%s vendor=%u device=%u api=%u queue=%u; host_upload_present=true visual_observation=false R3=false\n",
                            p.deviceName, p.vendorID, p.deviceID, p.apiVersion, q);
                        status = 0;
                        break;
                    }
                    if (status == 0 || invoked) break;
                }
        }
    }
    surfaceDestroy(instance, surface, nullptr);
    if (destroy) destroy(instance, nullptr);
    FreeLibrary(loader);
    return status;
}

extern "C" int ctr_video_run_session_v1(CtrVideoSessionBodyV1 body, void* user) {
    if (!body) return 60;
    try {
        return run_session(body, user);
    } catch (...) {
        return 90;
    }
}
extern "C" int ctr_video_device_control() {
    return run_session(nullptr, nullptr);
}

extern "C" int ctr_video_run_window_session_v1(const CtrVideoWindowOptionsV1* options, CtrVideoSessionBodyV1 body,
                                               void* user) {
    if (!options || !body || options->size != sizeof(*options) || options->version != 1 || options->visible > 1 ||
        options->reserved || options->width < 64 || options->height < 64 || options->width > 16384 ||
        options->height > 16384)
        return 60;
    try {
        return run_session(body, user, options);
    } catch (...) {
        return 90;
    }
}

static int checked_submit_body(const CtrVideoSessionApiV1* api, uint32_t width, uint32_t height, uint32_t format,
                               void*) {
    (void)format;
    const size_t bytes = size_t(width) * height * 4;
    const size_t extent = 48 + bytes;
    void* mapped = VirtualAlloc(nullptr, extent, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
    if (!mapped) return 121;
    auto owner = std::shared_ptr<void>(mapped, [](void* pointer) { VirtualFree(pointer, 0, MEM_RELEASE); });
    ctrport::GuestCopyRegion region(owner, extent);
    const auto memory = ctrport::make_guest_copy_api(region);
    const auto base = reinterpret_cast<uint64_t>(mapped);
    const uint64_t source = base + 48;
    const CtrVideoBufferAttributesV1 attributes{0x80000000U, 1, 0, width, height, width, 0, 0, 0};
    std::memcpy(mapped, &attributes, 40);
    std::memcpy(static_cast<unsigned char*>(mapped) + 40, &source, 8);
    std::memset(static_cast<unsigned char*>(mapped) + 48, 17, bytes);
    uint32_t group = 999;
    uint64_t token = 0;
    CtrVideoLinearRegistration registration;
    if (ctr_video_capture_linear_group(*api, memory, 0, 1, base, base + 40, group, &token, 1, &registration))
        return 121;
    std::memset(static_cast<unsigned char*>(mapped) + 48, 201, bytes);
    ctrport::OwnerDispatch dispatch;
    const auto dispatcherApi = ctrport::make_owner_dispatch_api(dispatch);
    auto caller = std::async(std::launch::async, [&] {
        return ctr_video_dispatch_captured_submit(dispatcherApi, *api, memory, registration, 0, 5678);
    });
    const bool pumped = dispatch.pump(1000);
    if (!pumped) dispatch.close();
    int submitted = 90;
    try {
        submitted = caller.get();
    } catch (...) {
        submitted = 90;
    }
    dispatch.close();
    if (!pumped || submitted ||
        ctr_video_dispatch_captured_submit(dispatcherApi, *api, memory, registration, 0, 9999) != 115)
        return 121;
    region.revoke();
    CtrVideoProgressV1 progress{};
    if (ctr_video_submit_captured_linear(*api, memory, registration, 0, 9999) != 112 ||
        api->get_progress(api->context, &progress) || progress.pending != 1 || progress.completed ||
        api->unregister_group(api->context, group) || api->start_pacing(api->context))
        return 121;
    CtrVideoCompletionV1 completion{};
    const int presented = api->present_next(api->context, 500, &completion);
    const int stopped = api->stop_pacing(api->context);
    if (presented || stopped || completion.count != 1 || completion.token != token || completion.argument != 5678)
        return 121;
    // Saturated admission must preserve the current immutable source and FIFO.
    CtrVideoLinearFrameV1 saturated{sizeof(CtrVideoLinearFrameV1),
                                    0x80000000U,
                                    width,
                                    height,
                                    width,
                                    static_cast<unsigned char*>(mapped) + 48,
                                    bytes};
    uint64_t saturatedToken = 0;
    if (api->register_linear_frame(api->context, 0, &saturated, &saturatedToken)) return 122;
    auto* session = static_cast<PresentationSession*>(api->context);
    const auto before = session->CaptureHostFrame(0, saturatedToken);
    for (uint64_t i = 0; i < 16; ++i)
        if (api->enqueue(api->context, 0, saturatedToken, 7000 + i)) return 122;
    std::memset(static_cast<unsigned char*>(mapped) + 48, 33, bytes);
    if (api->submit_linear_frame(api->context, 0, saturatedToken, &saturated, 8000) != 63 ||
        session->CaptureHostFrame(0, saturatedToken) != before || api->get_progress(api->context, &progress) ||
        progress.pending != 16 || progress.completed != 1 || progress.argument != 5678)
        return 122;
    std::puts(
        "saturated submit passed rejected63=true immutable_source_preserved=true FIFO16_preserved=true completion_preserved=true synthetic_only=true");
    std::puts(
        "checked submit pipeline passed worker_dispatch_C_ABI=true closed115=true owned_capture_refresh_enqueue_GPU=true revoked_no_extra_request=true retained_after_group_remove=true synthetic_only=true");
    return 0;
}
extern "C" int ctr_video_checked_submit_control() {
    return ctr_video_run_session_v1(checked_submit_body, nullptr);
}

static int gnm_vs_emit_control() {
    ctrport::DirectMemoryBacking backing(16384);
    int64_t physical = -1;
    void* mapped = nullptr;
    if (backing.allocate(0, 16384, 16384, 0, 0, &physical) || backing.map(&mapped, 16384, 51, 0, physical, 0))
        return 146;
    const auto memory = ctrport::make_direct_copy_api(backing);
    const uint64_t base = reinterpret_cast<uint64_t>(mapped);
    const uint32_t source[7] = {0x12345, 0, 0x81234567, 0x98765432, 0x11111111, 0x22222222, 0x33333333};
    if (memory.write(memory.context, base + 512, source, sizeof(source))) return 146;
    for (const uint32_t modifier : {0U, 0x03000000U}) {
        std::array<uint32_t, 30> words;
        words.fill(0x63636363);
        if (memory.write(memory.context, base, words.data(), sizeof(words))) return 146;
        int32_t result = 999;
        if (ctr_gnm_set_vs_shader(&memory, base, 29, base + 512, modifier, &result) || result ||
            memory.read(memory.context, base, words.data(), sizeof(words)))
            return 146;
        std::vector<CtrPm4::Packet> packets;
        if (CtrPm4::DecodePackets(words.data(), 29, packets) || packets.size() != 6 || packets.back().word != 17 ||
            packets.back().words != 12 || words[18] || words[29] != 0x63636363)
            return 146;
        for (size_t i = 19; i < 29; ++i)
            if (words[i] != 0x63636363) return 146;
        std::vector<CtrPm4::InitOperation> plan;
        if (CtrPm4::PrepareInit(words.data(), 29, plan)) return 146;
        CtrPm4::Registers state;
        for (size_t i = 0; i < 5; ++i)
            if (CtrPm4::ApplyRegister(plan[i], state)) return 146;
        const uint32_t expected = modifier ? 0x83234427U : 0x81234567U;
        if (state.words[0x2C48] != source[0] || state.words[0x2C49] || state.words[0x2C4A] != expected ||
            state.words[0x2C4B] != source[3] || state.words[0xA207] != source[6] || state.words[0xA1B1] != source[4] ||
            state.words[0xA1C3] != source[5])
            return 146;
    }
    int32_t result = 999;
    if (ctr_gnm_set_vs_shader(nullptr, 0, 29, base + 512, 0, &result) || result != -1 ||
        ctr_gnm_set_vs_shader(nullptr, base, 28, base + 512, 0, &result) || result != -1 ||
        ctr_gnm_set_vs_shader(nullptr, base, 29, 0, 0, &result) || result != -1 ||
        ctr_gnm_set_vs_shader(nullptr, base, 29, base + 512, 1, &result) || result != -1)
        return 146;
    result = 999;
    if (ctr_gnm_set_vs_shader(&memory, base + 16384 - 64, 29, base + 512, 0, &result) != 112 || result != 999 ||
        ctr_gnm_set_vs_shader(&memory, base, 29, base + 16384 - 4, 0, &result) != 112 || result != 999)
        return 146;
    if (ctr_gnm_set_vs_shader(&memory, base, 29, base + 4, 0, &result) != 88 || result != 999) return 146;
    const uint32_t high = 1;
    if (memory.write(memory.context, base + 516, &high, 4)) return 146;
    if (ctr_gnm_set_vs_shader(&memory, base, 29, base + 512, 0, &result) || result != -1) return 146;
    std::puts(
        "whole public Gnm SetVsShader emitter passed7sourceDWORD_29destinationDWORD=true modifier0_and03000000=true packetdecode_registereffects=true NOPtail10preserved=true public_minus1_and_checked112=true no_shader_execution=true");
    return 0;
}
static int gnm_update_vs_control() {
    ctrport::DirectMemoryBacking backing(16384);
    int64_t physical = -1;
    void* mapped = nullptr;
    if (backing.allocate(0, 16384, 16384, 0, 0, &physical) || backing.map(&mapped, 16384, 51, 0, physical, 0))
        return 153;
    const auto memory = ctrport::make_direct_copy_api(backing);
    const auto base = reinterpret_cast<uint64_t>(mapped);
    const uint32_t source[7] = {0x12345, 0, 0x81234567, 3, 4, 5, 6};
    if (memory.write(memory.context, base + 512, source, sizeof(source))) return 153;
    for (uint32_t modifier : {0U, 0x03000080U}) {
        std::array<uint32_t, 30> words;
        words.fill(0x63636363);
        int32_t result = 999;
        if (memory.write(memory.context, base, words.data(), sizeof(words)) ||
            ctr_gnm_update_vs_shader(&memory, base, 29, base + 512, modifier, &result) || result ||
            memory.read(memory.context, base, words.data(), sizeof(words)))
            return 153;
        const uint32_t expected[] = {0xc0027600,
                                     0x48,
                                     0x12345,
                                     0,
                                     0xc0027600,
                                     0x4a,
                                     modifier ? 0x832344a7U : 0x81234567U,
                                     3,
                                     0xc0011000,
                                     0xc01e0207,
                                     6,
                                     0xc0011000,
                                     0xc01e01b1,
                                     4,
                                     0xc0011000,
                                     0xc01e01c3,
                                     5,
                                     0xc00a1000,
                                     0};
        if (std::memcmp(words.data(), expected, sizeof(expected))) return 153;
        for (size_t i = 19; i < 30; ++i)
            if (words[i] != 0x63636363) return 153;
        std::vector<CtrPm4::Packet> packets;
        if (CtrPm4::DecodePackets(words.data(), 29, packets) || packets.size() != 6 || packets[2].opcode != 0x10 ||
            packets[3].opcode != 0x10 || packets[4].opcode != 0x10)
            return 153;
    }
    int32_t result = 999;
    if (ctr_gnm_update_vs_shader(nullptr, 0, 29, base + 512, 0, &result) || result != -1 ||
        ctr_gnm_update_vs_shader(nullptr, base, 28, base + 512, 0, &result) || result != -1 ||
        ctr_gnm_update_vs_shader(nullptr, base, 29, 0, 0, &result) || result != -1 ||
        ctr_gnm_update_vs_shader(nullptr, base, 29, base + 512, 1, &result) || result != -1)
        return 153;
    result = 999;
    if (ctr_gnm_update_vs_shader(&memory, base, 29, base + 4, 0, &result) != 88 || result != 999 ||
        ctr_gnm_update_vs_shader(&memory, base + 16384 - 4, 29, base + 512, 0, &result) != 112 || result != 999 ||
        ctr_gnm_update_vs_shader(&memory, base, 29, base + 16384 - 4, 0, &result) != 112 || result != 999)
        return 153;
    const uint32_t high = 1;
    if (memory.write(memory.context, base + 516, &high, 4)) return 153;
    if (ctr_gnm_update_vs_shader(&memory, base, 29, base + 512, 0, &result) || result != -1) return 153;
    std::puts(
        "whole public UpdateVsShader emitter passed modifier03000080=true exact19written_tail10guard=true three_NOP_update_hints=true bounds_alias_negatives=true no_shader_execution=true");
    return 0;
}
static int gnm_ps_emit_control() {
    ctrport::DirectMemoryBacking backing(16384);
    int64_t physical = -1;
    void* mapped = nullptr;
    if (backing.allocate(0, 16384, 16384, 0, 0, &physical) || backing.map(&mapped, 16384, 51, 0, physical, 0))
        return 149;
    const auto memory = ctrport::make_direct_copy_api(backing);
    const uint64_t base = reinterpret_cast<uint64_t>(mapped);
    const uint32_t source[12] = {0x12345, 0, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11};
    if (memory.write(memory.context, base + 512, source, sizeof(source))) return 149;
    for (bool enabled : {false, true}) {
        std::array<uint32_t, 41> words;
        words.fill(0x63636363);
        int32_t result = 999;
        if (memory.write(memory.context, base, words.data(), sizeof(words)) ||
            ctr_gnm_set_ps_shader_350(&memory, base, 40, enabled ? base + 512 : 0, &result) || result ||
            memory.read(memory.context, base, words.data(), sizeof(words)))
            return 149;
        std::vector<CtrPm4::Packet> packets;
        if (CtrPm4::DecodePackets(words.data(), 40, packets) || packets.size() != (enabled ? 9U : 4U) ||
            packets.back().word != (enabled ? 28U : 10U))
            return 149;
        for (size_t i = enabled ? 30 : 12; i < 41; ++i)
            if (words[i] != 0x63636363) return 149;
        std::vector<CtrPm4::InitOperation> plan;
        if (CtrPm4::PrepareInit(words.data(), 40, plan)) return 149;
        CtrPm4::Registers state;
        for (size_t i = 0; i + 1 < plan.size(); ++i)
            if (CtrPm4::ApplyRegister(plan[i], state)) return 149;
        if (state.words[0x2c08] != (enabled ? source[0] : 0) || state.words[0x2c09] ||
            state.words[0xa203] != (enabled ? 10U : 0U) || state.words[0xa08f] != (enabled ? 11U : 15U))
            return 149;
        if (enabled && (state.words[0x2c0a] != 2 || state.words[0x2c0b] != 3 || state.words[0xa1c4] != 4 ||
                        state.words[0xa1c5] != 5 || state.words[0xa1b3] != 6 || state.words[0xa1b4] != 7 ||
                        state.words[0xa1b6] != 8 || state.words[0xa1b8] != 9))
            return 149;
    }
    int32_t result = 999;
    if (ctr_gnm_set_ps_shader_350(nullptr, 0, 40, 0, &result) || result != -1 ||
        ctr_gnm_set_ps_shader_350(nullptr, base, 39, 0, &result) || result != -1)
        return 149;
    result = 999;
    if (ctr_gnm_set_ps_shader_350(&memory, base, 40, base + 4, &result) != 88 || result != 999 ||
        ctr_gnm_set_ps_shader_350(&memory, base + 16384 - 4, 40, 0, &result) != 112 || result != 999 ||
        ctr_gnm_set_ps_shader_350(&memory, base, 40, base + 16384 - 4, &result) != 112 || result != 999)
        return 149;
    const uint32_t high = 1;
    if (memory.write(memory.context, base + 516, &high, 4)) return 149;
    if (ctr_gnm_set_ps_shader_350(&memory, base, 40, base + 512, &result) || result != -1) return 149;
    std::puts(
        "whole public SetPsShader350 emitter passed null_and12source=true destination40=true complete_registereffects=true tail_and_guard_preserved=true negatives=true no_shader_execution=true");
    return 0;
}
static int gnm_update_ps_control() {
    ctrport::DirectMemoryBacking backing(16384);
    int64_t physical = -1;
    void* mapped = nullptr;
    if (backing.allocate(0, 16384, 16384, 0, 0, &physical) || backing.map(&mapped, 16384, 51, 0, physical, 0))
        return 149;
    const auto memory = ctrport::make_direct_copy_api(backing);
    const uint64_t base = reinterpret_cast<uint64_t>(mapped);
    const uint32_t source[12] = {0x12345, 0, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11};
    if (memory.write(memory.context, base + 512, source, sizeof(source))) return 149;
    for (bool enabled : {false, true}) {
        std::array<uint32_t, 41> words;
        words.fill(0x63636363);
        int32_t result = 999;
        if (memory.write(memory.context, base, words.data(), sizeof(words)) ||
            ctr_gnm_update_ps_shader_350(&memory, base, 40, enabled ? base + 512 : 0, &result) || result ||
            memory.read(memory.context, base, words.data(), sizeof(words)))
            return 149;
        std::vector<CtrPm4::Packet> packets;
        if (CtrPm4::DecodePackets(words.data(), 40, packets) || packets.size() != (enabled ? 9U : 4U) ||
            packets.back().word != (enabled ? 28U : 10U))
            return 149;
        for (size_t i = enabled ? 30 : 12; i < 41; ++i)
            if (words[i] != 0x63636363) return 149;
        if (enabled) {
            const uint32_t expected[] = {0xc0027600, 8,          0x12345,    0,          0xc0027600, 10,
                                         2,          3,          0xc0021000, 0xc01e01c4, 4,          5,
                                         0xc0021000, 0xc01e01b3, 6,          7,          0xc0011000, 0xc01e01b6,
                                         8,          0xc0011000, 0xc01e01b8, 9,          0xc0011000, 0xc01e0203,
                                         10,         0xc0011000, 0xc01e008f, 11,         0xc00a1000, 0};
            if (std::memcmp(words.data(), expected, sizeof(expected))) return 149;
        } else {
            const uint32_t expected[] = {0xc0027600, 8,          0,    0,  0xc0011000, 0xc01e0203,
                                         0,          0xc0016900, 0x8f, 15, 0xc01c1000, 0};
            if (std::memcmp(words.data(), expected, sizeof(expected))) return 149;
        }
    }
    int32_t result = 999;
    if (ctr_gnm_update_ps_shader_350(nullptr, 0, 40, 0, &result) || result != -1 ||
        ctr_gnm_update_ps_shader_350(nullptr, base, 39, 0, &result) || result != -1)
        return 149;
    result = 999;
    if (ctr_gnm_update_ps_shader_350(&memory, base, 40, base + 4, &result) != 88 || result != 999 ||
        ctr_gnm_update_ps_shader_350(&memory, base + 16384 - 4, 40, 0, &result) != 112 || result != 999 ||
        ctr_gnm_update_ps_shader_350(&memory, base, 40, base + 16384 - 4, &result) != 112 || result != 999)
        return 149;
    const uint32_t high = 1;
    if (memory.write(memory.context, base + 516, &high, 4)) return 149;
    if (ctr_gnm_update_ps_shader_350(&memory, base, 40, base + 512, &result) || result != -1) return 149;
    std::puts(
        "whole public UpdatePsShader350 emitter passed null_and12source=true destination40=true complete_NOP_update_hints=true tail_and_guard_preserved=true negatives=true no_shader_execution=true");
    return 0;
}
static int gnm_draw_auto_emit_control() {
    ctrport::DirectMemoryBacking backing(16384);
    int64_t physical = -1;
    void* mapped = nullptr;
    if (backing.allocate(0, 16384, 16384, 0, 0, &physical) || backing.map(&mapped, 16384, 51, 0, physical, 0))
        return 152;
    const auto memory = ctrport::make_direct_copy_api(backing);
    const auto base = reinterpret_cast<uint64_t>(mapped);
    for (int neo : {0, 1})
        for (uint32_t flags : {0U, 1U, 0xe0000000U, 0xe0000001U}) {
            std::array<uint32_t, 8> words;
            words.fill(0x63636363);
            int32_t result = 999;
            if (memory.write(memory.context, base, words.data(), sizeof(words)) ||
                ctr_gnm_draw_index_auto(&memory, base, 7, 0xffffffffU, flags, neo, &result) || result ||
                memory.read(memory.context, base, words.data(), sizeof(words)))
                return 152;
            std::vector<CtrPm4::Packet> packets;
            if (CtrPm4::DecodePackets(words.data(), 7, packets) || packets.size() != 2 || words[0] != 0xc0012d00 ||
                words[1] != 0xffffffffU || words[2] != (neo ? ((flags & 0xe0000000U) | 2U) : 2U) ||
                words[3] != 0xc0021000 || words[4] || words[5] != 0x63636363 || words[6] != 0x63636363 ||
                words[7] != 0x63636363)
                return 152;
            std::vector<CtrPm4::InitOperation> plan;
            if (CtrPm4::PrepareInit(words.data(), 7, plan) != 88)
                return 152; // Emission never enables unimplemented draw execution.
        }
    int32_t result = 999;
    for (uint32_t flag : {2U, 0x10000000U})
        if (ctr_gnm_draw_index_auto(nullptr, base, 7, 0, flag, 0, &result) || result != -1) return 152;
    if (ctr_gnm_draw_index_auto(nullptr, 0, 7, 0, 0, 0, &result) || result != -1 ||
        ctr_gnm_draw_index_auto(nullptr, base, 8, 0, 0, 0, &result) || result != -1)
        return 152;
    result = 999;
    if (ctr_gnm_draw_index_auto(&memory, base, 7, 0, 0, 2, &result) != 60 || result != 999 ||
        ctr_gnm_draw_index_auto(&memory, base + 16384 - 4, 7, 0, 0, 0, &result) != 112 || result != 999)
        return 152;
    std::puts(
        "whole public DrawIndexAuto emitter passed8cases standard_neo=true maxcount=true flagmask=true tail_guard=true negatives=true execution_still_rejects88=true synthetic_only=true");
    return 0;
}
static int gnm_vgt_emit_control() {
    ctrport::DirectMemoryBacking backing(16384);
    int64_t physical = -1;
    void* mapped = nullptr;
    if (backing.allocate(0, 16384, 16384, 0, 0, &physical) || backing.map(&mapped, 16384, 51, 0, physical, 0))
        return 156;
    const auto memory = ctrport::make_direct_copy_api(backing);
    const auto base = reinterpret_cast<uint64_t>(mapped);
    for (int neo : {0, 1})
        for (int compat : {0, 1})
            for (uint32_t partial : {0U, 1U})
                for (uint32_t eop : {0U, 1U}) {
                    std::array<uint32_t, 4> words;
                    words.fill(0x63636363);
                    int32_t result = 999;
                    if (memory.write(memory.context, base, words.data(), sizeof(words)) ||
                        ctr_gnm_set_vgt_control(&memory, base, 3, 255, partial, eop, neo, compat, &result) || result ||
                        memory.read(memory.context, base, words.data(), sizeof(words)))
                        return 156;
                    const uint32_t baseValue =
                        neo ? (eop ? (partial ? 0x1500ffU : 0x1400ffU) : 0xd00ffU) : (partial ? 0x100ffU : 0xffU);
                    if (words[0] != (neo && !compat ? 0xc0017900U : 0xc0016900U) ||
                        words[1] != (neo ? (compat ? 0x100002aaU : 0x40000258U) : 0x2aaU) ||
                        words[2] != (neo && !compat ? baseValue | 0x600000U : baseValue) || words[3] != 0x63636363)
                        return 156;
                }
    int32_t result = 999;
    if (ctr_gnm_set_vgt_control(nullptr, 0, 3, 0, 0, 0, 0, 0, &result) || result != -1 ||
        ctr_gnm_set_vgt_control(nullptr, base, 2, 0, 0, 0, 0, 0, &result) || result != -1 ||
        ctr_gnm_set_vgt_control(nullptr, base, 3, 256, 0, 0, 0, 0, &result) || result != -1 ||
        ctr_gnm_set_vgt_control(nullptr, base, 3, 0, 2, 0, 0, 0, &result) || result != -1 ||
        ctr_gnm_set_vgt_control(nullptr, base, 3, 0, 0, 2, 0, 0, &result) || result != -1)
        return 156;
    result = 999;
    if (ctr_gnm_set_vgt_control(&memory, base, 3, 0, 0, 0, 2, 0, &result) != 60 || result != 999 ||
        ctr_gnm_set_vgt_control(&memory, base, 3, 0, 0, 0, 0, 2, &result) != 60 || result != 999 ||
        ctr_gnm_set_vgt_control(&memory, base + 16384 - 4, 3, 0, 0, 0, 0, 0, &result) != 112 || result != 999)
        return 156;
    std::puts(
        "whole public SetVgtControl emitter passed16modecases=true exact3DWORD_guard=true public_minus1=true unknownpolicy60_copy112=true synthetic_only=true");
    return 0;
}
static int gnm_cs_emit_control() {
    ctrport::DirectMemoryBacking backing(16384);
    int64_t physical = -1;
    void* mapped = nullptr;
    if (backing.allocate(0, 16384, 16384, 0, 0, &physical) || backing.map(&mapped, 16384, 51, 0, physical, 0))
        return 157;
    const auto memory = ctrport::make_direct_copy_api(backing);
    const auto base = reinterpret_cast<uint64_t>(mapped);
    const uint32_t source[7] = {0x12345, 0, 0x81234567, 3, 64, 2, 1};
    if (memory.write(memory.context, base + 512, source, sizeof(source))) return 157;
    for (uint32_t modifier : {0U, 0x3c0U}) {
        std::array<uint32_t, 26> words;
        words.fill(0x63636363);
        int32_t result = 999;
        if (memory.write(memory.context, base, words.data(), sizeof(words)) ||
            ctr_gnm_set_cs_shader_modifier(&memory, base, 25, base + 512, modifier, &result) || result ||
            memory.read(memory.context, base, words.data(), sizeof(words)))
            return 157;
        const uint32_t expected[] = {
            0xc0027600, 0x20c, 0x12345, 0, 0xc0027600, 0x212, modifier ? 0x812347e7U : 0x81234567U, 3, 0xc0037600,
            0x207,      64,    2,       1, 0xc00a1000, 0};
        if (std::memcmp(words.data(), expected, sizeof(expected))) return 157;
        for (size_t i = 15; i < 26; ++i)
            if (words[i] != 0x63636363) return 157;
        std::vector<CtrPm4::InitOperation> plan;
        if (CtrPm4::PrepareInit(words.data(), 25, plan) || plan.size() != 4) return 157;
        CtrPm4::Registers state;
        for (size_t i = 0; i < 3; ++i)
            if (CtrPm4::ApplyRegister(plan[i], state)) return 157;
        if (state.compute[12] != 0x12345 || state.compute[13] || state.compute[18] != expected[6] ||
            state.compute[19] != 3 || state.compute[7] != 64 || state.compute[8] != 2 || state.compute[9] != 1)
            return 157;
    }
    int32_t result = 999;
    if (ctr_gnm_set_cs_shader_modifier(nullptr, 0, 25, base + 512, 0, &result) || result != -1 ||
        ctr_gnm_set_cs_shader_modifier(nullptr, base, 24, base + 512, 0, &result) || result != -1 ||
        ctr_gnm_set_cs_shader_modifier(nullptr, base, 25, 0, 0, &result) || result != -1 ||
        ctr_gnm_set_cs_shader_modifier(nullptr, base, 25, base + 512, 1, &result) || result != -1)
        return 157;
    result = 999;
    if (ctr_gnm_set_cs_shader_modifier(&memory, base, 25, base + 4, 0, &result) != 88 || result != 999 ||
        ctr_gnm_set_cs_shader_modifier(&memory, base + 16384 - 4, 25, base + 512, 0, &result) != 112 || result != 999 ||
        ctr_gnm_set_cs_shader_modifier(&memory, base, 25, base + 16384 - 4, 0, &result) != 112 || result != 999)
        return 157;
    const uint32_t high = 1;
    if (memory.write(memory.context, base + 516, &high, 4)) return 157;
    if (ctr_gnm_set_cs_shader_modifier(&memory, base, 25, base + 512, 0, &result) || result != -1) return 157;
    CtrPm4::Registers registers;
    CtrPm4::ComputeShaderState captured;
    captured.codeAddress = 999;
    if (CtrPm4::CaptureComputeShaderState(registers, captured) != 88 || captured.codeAddress != 999) return 157;
    registers.compute[12] = 0x12345678;
    registers.compute[13] = 0xffffffab;
    registers.compute[7] = 0x12340040;
    registers.compute[8] = 0x56780002;
    registers.compute[9] = 0x9abc0001;
    registers.compute[18] = 63;
    registers.compute[19] = (511U << 15) | (5U << 7) | (16U << 1);
    for (uint32_t i = 0; i < 16; ++i) registers.compute[64 + i] = 0x7abc0000 + i;
    if (CtrPm4::CaptureComputeShaderState(registers, captured) || captured.codeAddress != 0xab1234567800ULL ||
        captured.workgroup != std::array<uint32_t, 3>{64, 2, 1} ||
        captured.partial != std::array<uint32_t, 3>{0x1234, 0x5678, 0x9abc} || captured.userRegisters != 16 ||
        captured.allocatedVgprs != 252 || captured.sharedBytes != 261632 ||
        captured.tgid != std::array<bool, 3>{true, false, true})
        return 157;
    for (uint32_t i = 0; i < 16; ++i)
        if (captured.userData[i] != 0x7abc0000 + i) return 157;
    registers.compute[19] = (17U << 1);
    if (CtrPm4::CaptureComputeShaderState(registers, captured) != 88 || captured.userRegisters != 16) return 157;
    std::array<unsigned char, 128> binary{};
    const auto put = [&](size_t at, uint32_t value) { std::memcpy(binary.data() + at, &value, 4); };
    put(0, 0xbeeb03ff);
    put(4, 7);
    std::memcpy(binary.data() + 64, "OrbShdr", 7);
    put(72, 32U << 8);
    const uint64_t hash = 0x123456789abcdef0ULL;
    std::memcpy(binary.data() + 80, &hash, 8);
    CtrPm4::ShaderBinaryMetadata metadata;
    if (CtrPm4::FindShaderBinaryMetadata(binary.data(), binary.size(), metadata) || metadata.footer != 64 ||
        metadata.codeBytes != 32 || metadata.hash != hash)
        return 157;
    put(0, 0);
    if (CtrPm4::FindShaderBinaryMetadata(binary.data(), binary.size(), metadata) || metadata.footer != 64) return 157;
    put(72, 129U << 8);
    if (CtrPm4::FindShaderBinaryMetadata(binary.data(), binary.size(), metadata) != 60 || metadata.codeBytes != 32)
        return 157;
    put(72, 32U << 8);
    if (CtrPm4::FindShaderBinaryMetadata(binary.data(), 91, metadata) != 88 || metadata.codeBytes != 32) return 157;
    put(0, 0xbeeb03ff);
    put(4, 0xffffffff);
    if (CtrPm4::FindShaderBinaryMetadata(binary.data(), binary.size(), metadata) || metadata.footer != 64) return 157;
    binary[64] = 0;
    if (CtrPm4::FindShaderBinaryMetadata(binary.data(), binary.size(), metadata) != 88 || metadata.codeBytes != 32)
        return 157;
    binary[64] = 'O';
    put(4, 7);
    if (memory.write(memory.context, base + 1024, binary.data(), binary.size())) return 157;
    CtrPm4::OwnedShaderBinary owned;
    if (CtrPm4::CaptureShaderBinary(&memory, base + 1024, 128, owned) || owned.guestAddress != base + 1024 ||
        owned.CodeWords() != 8 || owned.words.size() != 32 || owned.metadata.hash != hash)
        return 157;
    const auto originalWords = owned.words;
    const uint32_t mutation = 0x98765432;
    if (memory.write(memory.context, base + 1024, &mutation, 4) || owned.words != originalWords) return 157;
    if (CtrPm4::CaptureShaderBinary(&memory, base + 16384 - 4, 128, owned) != 112 || owned.words != originalWords ||
        CtrPm4::CaptureShaderBinary(&memory, UINT64_MAX - 3, 128, owned) != 112 || owned.words != originalWords ||
        CtrPm4::CaptureShaderBinary(&memory, base + 1025, 128, owned) != 60 || owned.words != originalWords ||
        CtrPm4::CaptureShaderBinary(&memory, base + 1024, 7, owned) != 60 || owned.words != originalWords)
        return 157;
    binary[64] = 0;
    if (memory.write(memory.context, base + 1024, binary.data(), binary.size())) return 157;
    if (CtrPm4::CaptureShaderBinary(&memory, base + 1024, 128, owned) != 88 || owned.words != originalWords) return 157;
    put(72, 132U << 8);
    binary[64] = 'O';
    if (memory.write(memory.context, base + 1024, binary.data(), binary.size()) ||
        CtrPm4::CaptureShaderBinary(&memory, base + 1024, 128, owned) != 60 || owned.words != originalWords)
        return 157;
    std::puts(
        "owned shader code capture passed actual_checked_mapping=true immutable_after_guestmutation=true exact_code_extent=true atomic_failures=true payload_never_persisted=true synthetic_only=true");
    std::puts(
        "bounded public shader footer metadata passed prefix_and_scan=true truncated_missing88=true invalidlength60=true hugeprefix_no_overread=true preservedfailure=true host_snapshot_only=true");
    std::puts(
        "compute compiler metadata passed full_vs_partial_threads=true address40bits_shift8=true userdata16=true LDSunits512=true tgidbits=true atomic_missing88=true no_guestdereference=true");
    std::puts(
        "whole public SetCsShaderWithModifier passed modifier0_and3c0=true source7_dest25=true compute_registers=true tail10guard=true negativecases=true no_dispatch_execution=true");
    return 0;
}
static int shader_input_capture_control() {
    auto registers = std::make_unique<CtrPm4::Registers>();
    registers->words.fill(0);
    for (bool vertex : {false, true}) {
        const size_t at = vertex ? 0x2c48 : 0x2c08;
        registers->words[at] = 0x12345678;
        registers->words[at + 1] = 0xab;
        registers->words[at + 2] = 63 | (3U << 24) | (1U << 12) | (2U << 14) | (3U << 16) | (1U << 18);
        registers->words[at + 3] = (16U << 1) | 1 | 128;
        for (size_t i = 0; i < 16; ++i) registers->words[at + 4 + i] = uint32_t(i + 19);
        CtrPm4::GraphicsShaderState state;
        if (CtrPm4::CaptureGraphicsShaderState(*registers, vertex, state) || state.codeAddress != 0xab1234567800ULL ||
            state.userRegisters != 16 || state.allocatedVgprs != 256 || state.inputVgprs != 3 || state.fpRound32 != 1 ||
            state.fpRound64 != 2 || state.fpDenorm32 != 3 || state.fpDenorm64 != 1 || !state.scratch || !state.ocLds ||
            state.userData[15] != 34)
            return 161;
        registers->words[at + 3] = 17U << 1;
        if (CtrPm4::CaptureGraphicsShaderState(*registers, vertex, state) != 88 || state.userRegisters != 16)
            return 161;
        registers->words[at] = registers->words[at + 1] = 0;
        if (CtrPm4::CaptureGraphicsShaderState(*registers, vertex, state) != 88 || state.userRegisters != 16)
            return 161;
    }
    std::puts(
        "VS PS public ShaderProgram metadata passed exact_offsets=true address40shift8=true graphicsVGPR_plus1=true fp_modes_userdata16=true atomicmissing88=true no_pipeline_admission=true");

    auto backing = std::make_shared<ctrport::DirectMemoryBacking>(16384);
    int64_t physical = -1;
    void* mapped = nullptr;
    if (backing->allocate(0, 16384, 16384, 0, 0, &physical) || backing->map(&mapped, 16384, 51, 0, physical, 0))
        return 161;
    const auto memory = ctrport::make_direct_copy_api(*backing);
    const auto base = reinterpret_cast<uint64_t>(mapped);
    std::array<unsigned char, 128> binary{};
    const uint32_t prefix = 0xbeeb03ff, offset = 7, length = 32U << 8;
    std::memcpy(binary.data(), &prefix, 4);
    std::memcpy(binary.data() + 4, &offset, 4);
    std::memcpy(binary.data() + 64, "OrbShdr", 7);
    std::memcpy(binary.data() + 72, &length, 4);
    if (memory.write(memory.context, base, binary.data(), binary.size())) return 161;
    ctrport::ShaderSnapshotOwner owner(backing);
    auto inputs = owner.api();
    CtrPm4::OwnedShaderBinary owned;
    if (CtrPm4::CaptureShaderBinaryInputs(&inputs, base, 128, owned) || owned.CodeWords() != 8) return 161;
    const auto preserved = owned.words;
    binary[64] = 0;
    if (memory.write(memory.context, base, binary.data(), binary.size()) ||
        CtrPm4::CaptureShaderBinaryInputs(&inputs, base, 128, owned) != 88 || owned.words != preserved ||
        CtrPm4::CaptureShaderBinaryInputs(&inputs, base + 16384 - 4, 128, owned) != 112 || owned.words != preserved)
        return 161;
    if (ctrport::shader_snapshot_owner_control()) return 161;
    std::puts(
        "runtime shader input capture compiler-owned bridge passed actual_direct_owner=true capture_release_lifetime=true immutable_output=true missing_and_bounds_atomic=true synthetic_only=true");
    return 0;
}
static int gnm_draw_index_control() {
    ctrport::DirectMemoryBacking backing(16384);
    int64_t physical = -1;
    void* mapped = nullptr;
    if (backing.allocate(0, 16384, 16384, 0, 0, &physical) || backing.map(&mapped, 16384, 51, 0, physical, 0))
        return 160;
    const auto memory = ctrport::make_direct_copy_api(backing);
    const auto base = reinterpret_cast<uint64_t>(mapped);
    for (int neo : {0, 1})
        for (uint32_t flags : {0U, 1U, 0xe0000000U, 0xe0000001U})
            for (uint32_t type : {1U, UINT32_MAX}) {
                std::array<uint32_t, 11> words;
                words.fill(0x91919191);
                int32_t result = 999;
                const uint64_t indices = 0x1234567802;
                if (memory.write(memory.context, base, words.data(), sizeof(words)) ||
                    ctr_gnm_draw_index(&memory, base, 10, 102, indices, flags, type, neo, &result) || result ||
                    memory.read(memory.context, base, words.data(), sizeof(words)))
                    return 160;
                const std::array<uint32_t, 11> expected = {
                    0xc0042700, 102, 0x34567802, 0x12,       102,       neo ? flags & 0xe0000000U : 0,
                    0xc0021000, 0,   0x91919191, 0x91919191, 0x91919191};
                if (words != expected) return 160;
                std::vector<CtrPm4::InitOperation> plan;
                if (CtrPm4::PrepareInit(words.data(), 10, plan) != 88) return 160;
            }
    int32_t result = 999;
    for (uint64_t indices : {uint64_t(0), uint64_t(1), uint64_t(3)})
        if (ctr_gnm_draw_index(nullptr, base, 10, 102, indices, 0, 1, 0, &result) || result != -1) return 160;
    for (uint32_t flags : {2U, 0x10000000U})
        if (ctr_gnm_draw_index(nullptr, base, 10, 102, 2, flags, 1, 0, &result) || result != -1) return 160;
    result = 999;
    if (ctr_gnm_draw_index(&memory, base + 16384 - 4, 10, 102, 2, 0, 1, 0, &result) != 112 || result != 999 ||
        ctr_gnm_draw_index(&memory, base, 10, 102, 2, 0, 1, 2, &result) != 60 || result != 999)
        return 160;
    std::puts(
        "whole DrawIndex emitter passed16cases=true standard_neo_explicit=true type_unused_public=true address64_even_only=true tail2guard=true negatives=true no_index_read_or_draw_execution=true");
    return 0;
}
static int gnm_marker_emit_control() {
    ctrport::DirectMemoryBacking backing(16384);
    int64_t physical = -1;
    void* mapped = nullptr;
    if (backing.allocate(0, 16384, 16384, 0, 0, &physical) || backing.map(&mapped, 16384, 51, 0, physical, 0))
        return 159;
    const auto memory = ctrport::make_direct_copy_api(backing);
    const auto base = reinterpret_cast<uint64_t>(mapped);
    for (size_t len : {size_t(0), size_t(1), size_t(7), size_t(8), size_t(23), size_t(24), size_t(25)}) {
        std::vector<unsigned char> text(len + 1, 'x');
        text[len] = 0;
        const uint32_t packet = uint32_t(((len + 8) >> 2) + ((len + 12) >> 3) * 2), size = packet + 2;
        std::vector<uint32_t> expected(size + 2, 0x91919191), actual = expected;
        expected[0] = 0xc0001000U | (packet << 16);
        expected[1] = 0x68750001;
        std::memset(expected.data() + 2, 0, packet * 4);
        std::memcpy(expected.data() + 2, text.data(), text.size());
        if (memory.write(memory.context, base, actual.data(), actual.size() * 4) ||
            memory.write(memory.context, base + 1024, text.data(), text.size()))
            return 159;
        int32_t result = 999;
        if (ctr_gnm_insert_push_marker(&memory, base, size, base + 1024, &result) || result ||
            memory.read(memory.context, base, actual.data(), actual.size() * 4) || actual != expected)
            return 159;
        expected[1] = 0x68750003;
        if (ctr_gnm_insert_set_marker(&memory, base, size, base + 1024, &result) || result ||
            memory.read(memory.context, base, actual.data(), actual.size() * 4) || actual != expected)
            return 159;
        result = 999;
        if (ctr_gnm_insert_push_marker(&memory, base, size + 1, base + 1024, &result) || result != -1) return 159;
    }
    int32_t result = 999;
    const unsigned char c = 0;
    if (memory.write(memory.context, base + 1024, &c, 1) ||
        ctr_gnm_insert_push_marker(&memory, base + 16384 - 20, 6, base + 1024, &result) != 112 || result != 999 ||
        ctr_gnm_insert_push_marker(&memory, base, 6, base + 16384, &result) != 112 || result != 999 ||
        ctr_gnm_insert_push_marker(&memory, base, 6, base, &result) != 88 || result != 999)
        return 159;
    std::vector<unsigned char> unterminated(28, 'x');
    if (memory.write(memory.context, base + 1024, unterminated.data(), unterminated.size()) ||
        ctr_gnm_insert_push_marker(&memory, base, 6, base + 1024, &result) != 88 || result != 999)
        return 159;
    std::array<uint32_t, 7> pop;
    pop.fill(0x91919191);
    if (memory.write(memory.context, base, pop.data(), sizeof(pop)) ||
        ctr_gnm_insert_pop_marker(&memory, base, 6, &result) || result ||
        memory.read(memory.context, base, pop.data(), sizeof(pop)))
        return 159;
    const std::array<uint32_t, 7> popExpected = {0xc0041000, 0x68750002, 0, 0, 0, 0, 0x91919191};
    if (pop != popExpected) return 159;
    for (uint32_t invalid : {0U, 5U, 7U, UINT32_MAX}) {
        result = 999;
        if (ctr_gnm_insert_pop_marker(&memory, base, invalid, &result) || result != -1) return 159;
    }
    result = 999;
    if (ctr_gnm_insert_pop_marker(&memory, base + 16384 - 20, 6, &result) != 112 || result != 999 ||
        ctr_gnm_insert_pop_marker(&memory, UINT64_MAX - 3, 6, &result) != 112 || result != 999)
        return 159;
    std::puts(
        "whole set and pop markers passed exact_selectors=true seven_set_lengths=true pop6DWORD_guard=true negative_sizes_and_bounds=true emission_only=true");
    std::puts(
        "whole push marker passed seven_lengths=true exact_public_padding=true guard_preserved=true bounded_terminator=true missing112_alias88=true emission_only=true");
    return 0;
}
static int gnm_dispatch_emit_control() {
    ctrport::DirectMemoryBacking backing(16384);
    int64_t physical = -1;
    void* mapped = nullptr;
    if (backing.allocate(0, 16384, 16384, 0, 0, &physical) || backing.map(&mapped, 16384, 51, 0, physical, 0))
        return 158;
    const auto memory = ctrport::make_direct_copy_api(backing);
    const auto base = reinterpret_cast<uint64_t>(mapped);
    for (uint32_t flags : {0U, 1U, 8U, 16U, 24U, 0xffffffffU})
        for (uint32_t x : {0U, 0x7fffffffU}) {
            std::array<uint32_t, 10> words;
            words.fill(0x63636363);
            int32_t result = 999;
            if (memory.write(memory.context, base, words.data(), sizeof(words)) ||
                ctr_gnm_dispatch_direct(&memory, base, 9, x, 2, 3, flags, &result) || result ||
                memory.read(memory.context, base, words.data(), sizeof(words)))
                return 158;
            if (words[0] != (0xc0031502U | (flags & 1)) || words[1] != x || words[2] != 2 || words[3] != 3 ||
                words[4] != (flags & 24) + 1 || words[5] != 0xc0021000 || words[6] || words[7] != 0x63636363 ||
                words[8] != 0x63636363 || words[9] != 0x63636363)
                return 158;
            CtrPm4::DispatchDirect decoded;
            if (CtrPm4::DecodeDispatchDirect(words.data(), 5, decoded) || decoded.x != x || decoded.y != 2 ||
                decoded.z != 3 || decoded.initiator != (flags & 24) + 1 || !decoded.compute ||
                decoded.predicate != ((flags & 1) != 0))
                return 158;
            auto malformed = words;
            malformed[0] |= 4;
            if (CtrPm4::DecodeDispatchDirect(malformed.data(), 5, decoded) != 88 || decoded.x != x ||
                CtrPm4::DecodeDispatchDirect(words.data(), 4, decoded) != 60 || decoded.x != x)
                return 158;
            std::vector<CtrPm4::InitOperation> plan;
            if (CtrPm4::PrepareInit(words.data(), 9, plan) != 88) return 158;
        }
    int32_t result = 999;
    if (ctr_gnm_dispatch_direct(nullptr, 0, 9, 1, 1, 1, 0, &result) || result != -1 ||
        ctr_gnm_dispatch_direct(nullptr, base, 8, 1, 1, 1, 0, &result) || result != -1)
        return 158;
    for (uint32_t axis = 0; axis < 3; ++axis)
        if (ctr_gnm_dispatch_direct(nullptr, base, 9, axis == 0 ? 0x80000000U : 1, axis == 1 ? 0x80000000U : 1,
                                    axis == 2 ? 0x80000000U : 1, 0, &result) ||
            result != -1)
            return 158;
    result = 999;
    if (ctr_gnm_dispatch_direct(&memory, base + 16384 - 4, 9, 1, 1, 1, 0, &result) != 112 || result != 999) return 158;
    std::puts(
        "DispatchDirect metadata passed12cases=true predicate_compute_and_initiator_retained=true malformed_atomic=true no_execution=true; whole public DispatchDirect emitter passed12cases=true zero_maxsigned_dimensions=true predicate_orderedflags=true tail2guard=true negatives=true executor_still88=true");
    return 0;
}
static int pm4_init_plan_control() {
    const uint32_t mixed[] = {0x80000000, 0xc0001000, 0, 0xbfffffff};
    std::vector<CtrPm4::Packet> decoded;
    if (CtrPm4::DecodePackets(mixed, 4, decoded) || decoded.size() != 3 || decoded[0].word || decoded[0].words != 1 ||
        decoded[0].opcode != 258 || decoded[1].word != 1 || decoded[1].words != 2 || decoded[1].opcode != 16 ||
        decoded[2].word != 3 || decoded[2].opcode != 258)
        return 142;
    const uint32_t truncated[] = {0x80000000, 0xc0011000, 0};
    if (CtrPm4::DecodePackets(truncated, 3, decoded) != 60 || decoded.size() != 3) return 142;
    const uint32_t unknownType[] = {0x80000000, 0x40000000};
    if (CtrPm4::DecodePackets(unknownType, 2, decoded) != 88 || decoded.size() != 3) return 142;
    std::vector<CtrPm4::InitOperation> unsupportedPadding;
    if (CtrPm4::PrepareInit(mixed, 4, unsupportedPadding) != 88 || !unsupportedPadding.empty()) return 142;
    std::puts(
        "PM4 mixed type2/type3 metadata passed ordered_exact_extents=true truncation_and_unknown_atomic=true init_execution_still88=true");

    const uint32_t input[] = {0xc0001200, 0,   0xc0012800, 0x80000000, 0x80000000,
                              0xc0016900, 0xd, 0xabcdef12, 0xc0002f00, 7};
    std::vector<CtrPm4::InitOperation> plan;
    if (CtrPm4::PrepareInit(input, 10, plan) || plan.size() != 4) return 142;
    CtrPm4::Registers state;
    state.words.fill(UINT32_MAX);
    if (CtrPm4::ApplyRegister(plan[0], state) || state.words[0xA00D] != 0x40004000 || state.words[0xA317] != 16 ||
        state.words[0xA080] != 0 || state.words[0xA081] != 0x80000000 || state.words[0])
        return 142;
    if (CtrPm4::ApplyRegister(plan[1], state) != 88 || CtrPm4::ApplyRegister(plan[2], state) ||
        CtrPm4::ApplyRegister(plan[3], state) || state.words[0xA00D] != 0xabcdef12 || state.words[0xC24D] != 7)
        return 142;
    const uint32_t compute[] = {0xc0017600, 0x216, 5};
    std::vector<CtrPm4::InitOperation> computePlan;
    if (CtrPm4::PrepareInit(compute, 3, computePlan) || CtrPm4::ApplyRegister(computePlan[0], state) ||
        state.compute[22] != 5 || state.words[0x2E16])
        return 142;
    const uint32_t computeBad[] = {0xc0017600, 0x250, 5};
    const uint32_t indexed[] = {0xc0016900, 0x10000000, 5};
    const uint32_t unknown[] = {0xc000ff00, 0};
    if (CtrPm4::PrepareInit(computeBad, 3, plan) != 88 || CtrPm4::PrepareInit(indexed, 3, plan) != 60 ||
        CtrPm4::PrepareInit(unknown, 2, plan) != 88 || plan.size() != 4 || plan[2].payload[1] != 0xabcdef12)
        return 142;
    CtrPm4::InitOperation overflow{0x79, {0xfff, 1, 2}};
    if (CtrPm4::ApplyRegister(overflow, state) != 60 || state.words[0xCFFF]) return 142;
    const uint32_t ordinary[] = {0xc0021000, 0x12345678, 8, 9};
    const uint32_t flip[] = {0xc0021000, 0x68750776, 8, 9};
    if (CtrPm4::PrepareInit(ordinary, 4, computePlan) || computePlan[0].payload.size() != 3 ||
        CtrPm4::PrepareInit(flip, 4, computePlan) != 88 || computePlan[0].payload[0] != 0x12345678)
        return 142;
    std::puts("PM4 separate compute bank and ordinary extended NOP passed; special marker rejected; no execution");
    std::puts(
        "PM4 init preparation passed whole_public_ClearState_defaults=true retained_registers=true atomic_plan=true unknown_outofrange_indexed_reject=true GPU_execution=false synthetic=true");
    return 0;
}
static int pm4_decode_control() {
    uint32_t packet[6] = {0xc0044700, 0x514, 0x34567890, 0x42001234, 0x76543210, 0xfedcba98};
    CtrPm4::Eop decoded{};
    if (CtrPm4::DecodeEop(packet, 6, decoded) || decoded.eventType != 20 || decoded.eventIndex != 5 ||
        decoded.address != 0x123434567890ULL || decoded.value != 0xfedcba9876543210ULL || decoded.writeBytes != 8 ||
        decoded.interruptSelect != 2)
        return 141;
    packet[3] = 0x22001234;
    if (CtrPm4::DecodeEop(packet, 6, decoded) || decoded.value != 0x76543210 || decoded.writeBytes != 4) return 141;
    packet[3] = 0x01000000;
    if (CtrPm4::DecodeEop(packet, 6, decoded) || decoded.writeBytes || decoded.interruptSelect != 1) return 141;
    const auto saved = decoded;
    const auto unchanged = [&]() {
        return decoded.address == saved.address && decoded.value == saved.value &&
               decoded.writeBytes == saved.writeBytes && decoded.interruptSelect == saved.interruptSelect;
    };
    for (const uint32_t control : {0x60000000U, 0x80000000U, 0xa0000000U, 0x03000000U, 0x21000000U}) {
        packet[3] = control;
        if (CtrPm4::DecodeEop(packet, 6, decoded) != 88 || !unchanged()) return 141;
    }
    packet[3] = 0x20000000;
    packet[2] = 1;
    if (CtrPm4::DecodeEop(packet, 6, decoded) != 60 || !unchanged() || CtrPm4::DecodeEop(packet, 5, decoded) != 60 ||
        CtrPm4::DecodeEop(nullptr, 6, decoded) != 60)
        return 141;
    packet[2] = 0;
    packet[0] = 0xc0044800;
    if (CtrPm4::DecodeEop(packet, 6, decoded) != 60 || !unchanged()) return 141;
    std::vector<CtrPm4::Packet> packets{{99, 99, 99}};
    const uint32_t stream[] = {0xc0001000, 0, 0xc0044700, 0, 0, 0x40000000, 1, 2};
    if (CtrPm4::DecodePackets(stream, 8, packets) || packets.size() != 2 || packets[1].word != 2 ||
        packets[1].words != 6 || packets[1].opcode != 71)
        return 141;
    if (CtrPm4::DecodePackets(stream, 7, packets) != 60 || packets.size() != 2 ||
        CtrPm4::DecodePackets(nullptr, 1, packets) != 60 || CtrPm4::DecodePackets(stream, 0, packets) != 60 ||
        CtrPm4::DecodePackets(stream, 4 * 1024 * 1024 + 1, packets) != 60 || packets[1].word != 2)
        return 141;
    const uint32_t bad[] = {0, 0};
    if (CtrPm4::DecodePackets(bad, 2, packets) != 88 || packets.size() != 2) return 141;
    std::puts(
        "PM4 bounded decoder passed EOP32_64_IRQonly=true unknownclock_rejected=true atomic_failures=true packet_bounds=true execution=false synthetic=true");
    return 0;
}
static int tiled_mapped_session_body(const CtrVideoSessionApiV1* api, uint32_t, uint32_t, uint32_t, void*) {
    if (ctr_video_hide_system_splash_v1(nullptr) != 60 ||
        static_cast<PresentationSession*>(api->context)->SplashLifecycleControl())
        return 154;
    const int emitted = gnm_vs_emit_control();
    if (emitted) return emitted;
    const int updateVs = gnm_update_vs_control();
    if (updateVs) return updateVs;
    const int psEmitted = gnm_ps_emit_control();
    if (psEmitted) return psEmitted;
    const int updatePs = gnm_update_ps_control();
    if (updatePs) return updatePs;
    const int shaderInputs = shader_input_capture_control();
    if (shaderInputs) return shaderInputs;
    const int indexEmitted = gnm_draw_index_control();
    if (indexEmitted) return indexEmitted;
    const int markerEmitted = gnm_marker_emit_control();
    if (markerEmitted) return markerEmitted;
    const int dispatchEmitted = gnm_dispatch_emit_control();
    if (dispatchEmitted) return dispatchEmitted;
    const int csEmitted = gnm_cs_emit_control();
    if (csEmitted) return csEmitted;
    const int vgtEmitted = gnm_vgt_emit_control();
    if (vgtEmitted) return vgtEmitted;
    const int drawEmitted = gnm_draw_auto_emit_control();
    if (drawEmitted) return drawEmitted;
    const int planned = pm4_init_plan_control();
    if (planned) return planned;
    const int decoded = pm4_decode_control();
    if (decoded) return decoded;
    CtrVideoOutputModeV1 mode;
    std::memset(&mode, 255, sizeof(mode));
    mode.size = 32;
    const auto configure = [&](const CtrVideoOutputModeV1& value, uint32_t reserved = 0, const void* options = nullptr,
                               uint32_t size = 32, uint32_t optionsSize = 16) {
        return ctr_video_configure_output_any_v1(api->context, reserved, &value, options, size, optionsSize);
    };
    if (configure(mode) || configure(mode, 1) != 60 || configure(mode, 0, nullptr, 31) != 60 ||
        configure(mode, 0, &mode) != 88 || configure(mode, 0, nullptr, 32, 0) != 88 ||
        ctr_video_configure_output_any_v1(nullptr, 0, &mode, nullptr, 32, 16) != 60 ||
        ctr_video_configure_output_any_v1(api->context, 0, nullptr, nullptr, 32, 16) != 60)
        return 139;
    auto rejected = mode;
    rejected.size = 31;
    if (configure(rejected) != 60) return 139;
    rejected = mode;
    rejected.colorimetry = 12;
    if (configure(rejected) != 88) return 139;
    rejected = mode;
    rejected.refresh_rate = 60;
    if (configure(rejected) != 88) return 139;
    rejected = mode;
    rejected.resolution = 0;
    if (configure(rejected) != 88) return 139;
    rejected = mode;
    rejected.encoding = 0;
    if (configure(rejected) != 88) return 139;
    rejected = mode;
    rejected.range = 0;
    if (configure(rejected) != 88) return 139;
    rejected = mode;
    rejected.depth = 8;
    if (configure(rejected) != 88) return 139;
    rejected = mode;
    rejected.reserved[7] = 0;
    if (configure(rejected) != 88 || configure(mode)) return 139;
    std::puts(
        "output Any admission passed actual_live_SDR=true exact_retail_mode32=true negative_cases14=true no_resize_or_HDR=true R3=false");
    constexpr uint64_t span = 8355840ULL * 2;
    auto backing = std::make_shared<ctrport::DirectMemoryBacking>(span);
    std::weak_ptr<ctrport::DirectMemoryBacking> weak = backing;
    int64_t physical = -1;
    void* mapped = nullptr;
    if (backing->allocate(0, span, span, 0, 0, &physical) || backing->map(&mapped, span, 51, 0, physical, 0))
        return 138;
    uint64_t address = 999;
    if (ctr_video_tiled_label_address_v1(api->context, &address) != 91 || address != 999) return 138;
    const uint64_t addresses[2] = {reinterpret_cast<uint64_t>(mapped), reinterpret_cast<uint64_t>(mapped) + 8355840};
    CtrVideoBufferAttributesV1 attributes{0x80000000U, 0, 0, 1920, 1080, 1920, 0, 0, 0};
    uint32_t group = 999;
    if (ctr_video_register_tiled_mapped_v1(nullptr, &backing, 0, 2, &attributes, addresses, &group) != 60 ||
        group != 999 ||
        ctr_video_register_tiled_mapped_v1(api->context, &backing, 0, 2, &attributes, addresses, &group) ||
        group != 0 || ctr_video_tiled_label_address_v1(api->context, &address) || !address)
        return 138;
    if (ctr_video_admit_memory_change_v1(api->context, addresses[0], 8) != 88 ||
        ctr_video_admit_memory_change_v1(api->context, addresses[0] - 1, 2) != 88 ||
        ctr_video_admit_memory_change_v1(api->context, addresses[0] + 8355840 - 1, 2) != 88 ||
        ctr_video_admit_memory_change_v1(api->context, addresses[1] + 8355840, 8) ||
        ctr_video_admit_memory_change_v1(api->context, address, 128) != 88 ||
        ctr_video_admit_memory_change_v1(api->context, UINT64_MAX, 8) != 60 ||
        ctr_video_admit_memory_change_v1(api->context, addresses[0], 0) != 60 ||
        ctr_video_admit_memory_change_v1(nullptr, addresses[0], 8) != 60)
        return 145;
    std::puts(
        "renderer memory-change admission passed retained_images_and_labels_overlap_reject88=true adjacent_allowed=true zero_overflow_null60=true no_fake_invalidation=true");
    const auto copy = ctrport::make_direct_copy_api(*backing);
    const uint64_t eopAddress = addresses[0] + 4096;
    std::vector<uint32_t> init = {0xc0001200,
                                  0,
                                  0xc0012800,
                                  0x80000000,
                                  0x80000000,
                                  0xc0017600,
                                  0x216,
                                  5,
                                  0xc0055800,
                                  2294546432U,
                                  1,
                                  0,
                                  0,
                                  0,
                                  10,
                                  0xc0044700,
                                  0x504,
                                  static_cast<uint32_t>(eopAddress),
                                  0x40000000U | static_cast<uint32_t>(eopAddress >> 32),
                                  0x76543210,
                                  0xfedcba98,
                                  0xc0031000,
                                  0x68753000,
                                  0,
                                  0,
                                  0};
    uint64_t observed = 0;
    if (copy.write(copy.context, eopAddress, &observed, 8) ||
        ctr_video_submit_init_v1(api->context, &backing, init.data(), init.size()) ||
        copy.read(copy.context, eopAddress, &observed, 8) || observed != 0xfedcba9876543210ULL)
        return 144;
    CtrGraphicsProfileV1 deviceProfile{};
    if (ctr_video_capture_graphics_profile_v1(api->context, &deviceProfile) || deviceProfile.version != 1 ||
        deviceProfile.size != sizeof(deviceProfile) || !deviceProfile.max_ubo_size || !deviceProfile.subgroup_size ||
        deviceProfile.supported_spirv != 0x10500 || deviceProfile.support_int64 || deviceProfile.support_float64 ||
        deviceProfile.supports_fragment_shader_barycentric || deviceProfile.depth_clip_control)
        return 144;
    const auto preservedProfile = deviceProfile;
    int profileWorker = 0;
    std::thread graphicsWorker(
        [&] { profileWorker = ctr_video_capture_graphics_profile_v1(api->context, &deviceProfile); });
    graphicsWorker.join();
    if (profileWorker != 116 || std::memcmp(&deviceProfile, &preservedProfile, sizeof(deviceProfile)) ||
        ctr_video_capture_graphics_profile_v1(api->context, nullptr) != 60)
        return 144;
    std::printf(
        "actual enabled-device shader profile passed ubo=%llu subgroup=%u spirv=%x optional_int64_float64_barycentric_disabled=true wrongthread_atomic116=true null60=true\n",
        static_cast<unsigned long long>(deviceProfile.max_ubo_size), deviceProfile.subgroup_size,
        deviceProfile.supported_spirv);
    auto capturedState = std::make_unique<CtrPm4::Registers>();
    capturedState->words.fill(0x91919191);
    uint64_t initCount = 999;
    if (ctr_video_capture_pm4_state_v1(api->context, capturedState->words.data(), 0xD000, capturedState->compute.data(),
                                       80, &initCount) ||
        initCount != 1 || capturedState->compute[22] != 5 || capturedState->words[0xA00D] != 0x40004000)
        return 144;
    const auto preservedFirst = capturedState->words[0];
    initCount = 999;
    if (ctr_video_capture_pm4_state_v1(api->context, capturedState->words.data(), 0xCFFF, capturedState->compute.data(),
                                       80, &initCount) != 60 ||
        initCount != 999 || capturedState->words[0] != preservedFirst)
        return 144;
    int captureWorker = 0;
    std::thread stateWorker([&] {
        captureWorker = ctr_video_capture_pm4_state_v1(api->context, capturedState->words.data(), 0xD000,
                                                       capturedState->compute.data(), 80, &initCount);
    });
    stateWorker.join();
    if (captureWorker != 116 || initCount != 999) return 144;
    std::puts(
        "retained PM4 init state capture passed fullD000_compute80=true actual_defaults_and_updates=true count1=true invalid_size_atomic60=true wrongthread116=true host_only=true");
    init[21] = 0x11223344; // Unknown trailing opcode must reject BEFORE EOP write.
    if (ctr_video_submit_init_v1(api->context, &backing, init.data(), init.size()) != 88 ||
        copy.read(copy.context, eopAddress, &observed, 8) || observed != 0xfedcba9876543210ULL)
        return 144;
    init[21] = 0xc0031000;
    init[17] = 1;
    init[18] = 0x40000000; // unmapped EOP destination
    if (ctr_video_submit_init_v1(api->context, &backing, init.data(), init.size()) != 60 ||
        ctr_video_submit_init_v1(nullptr, &backing, init.data(), init.size()) != 60)
        return 144;
    init[17] = 0;
    if (ctr_video_submit_init_v1(api->context, &backing, init.data(), init.size()) != 112 ||
        copy.read(copy.context, eopAddress, &observed, 8) || observed != 0xfedcba9876543210ULL)
        return 144;
    init[17] = static_cast<uint32_t>(eopAddress);
    init[18] = 0x40000000U | static_cast<uint32_t>(eopAddress >> 32);
    init[19] = 0x10203040;
    init[20] = 0x50607080;
    if (ctr_video_submit_init_v1(api->context, &backing, init.data(), init.size()) ||
        copy.read(copy.context, eopAddress, &observed, 8) || observed != 0x5060708010203040ULL)
        return 144;
    std::puts(
        "PM4 init owner execution passed real_queue_fence_then_checked_EOP64=true atomic_unknown_tail=true invalid_target_reject=true synthetic=true no_draws=true R3=false");
    // Ordered standalone wait: prefix publication occurs once, suffix only after satisfaction.
    const uint64_t waitAddress = eopAddress + 16;
    uint32_t waitValue = 1;
    if (copy.write(copy.context, waitAddress, &waitValue, 4)) return 144;
    std::vector<uint32_t> waitInit = {0xc0044700,
                                      0x504,
                                      static_cast<uint32_t>(eopAddress),
                                      0x40000000U | static_cast<uint32_t>(eopAddress >> 32),
                                      0x1234,
                                      0,
                                      0xc0053c00,
                                      0x13,
                                      static_cast<uint32_t>(waitAddress),
                                      static_cast<uint32_t>(waitAddress >> 32),
                                      0,
                                      UINT32_MAX,
                                      10,
                                      0xc0044700,
                                      0x504,
                                      static_cast<uint32_t>(eopAddress + 8),
                                      0x40000000U | static_cast<uint32_t>((eopAddress + 8) >> 32),
                                      0x5678,
                                      0};
    auto invalidWait = waitInit;
    invalidWait[15] = 0;
    invalidWait[16] = 0x40000000;
    if (ctr_video_submit_init_v1(api->context, &backing, invalidWait.data(), invalidWait.size()) != 112 ||
        copy.read(copy.context, eopAddress, &observed, 8) || observed != 0x5060708010203040ULL)
        return 144;
    if (ctr_video_submit_init_v1(api->context, &backing, waitInit.data(), waitInit.size()) != 88 ||
        copy.read(copy.context, eopAddress, &observed, 8) || observed != 0x1234)
        return 144;
    observed = 0x9876;
    if (copy.write(copy.context, eopAddress, &observed, 8)) return 144;
    auto differentWait = waitInit;
    differentWait[10] = 1;
    if (ctr_video_submit_init_v1(api->context, &backing, differentWait.data(), differentWait.size()) != 91 ||
        ctr_video_submit_init_v1(api->context, &backing, waitInit.data(), waitInit.size()) != 88 ||
        copy.read(copy.context, eopAddress, &observed, 8) || observed != 0x9876)
        return 144;
    waitValue = 0;
    if (copy.write(copy.context, waitAddress, &waitValue, 4) ||
        ctr_video_submit_init_v1(api->context, &backing, waitInit.data(), waitInit.size()) ||
        copy.read(copy.context, eopAddress, &observed, 8) || observed != 0x9876 ||
        copy.read(copy.context, eopAddress + 8, &observed, 8) || observed != 0x5678)
        return 144;
    uint64_t realLabelAddress = 0;
    if (ctr_video_tiled_label_address_v1(api->context, &realLabelAddress)) return 144;
    uint32_t labelWait[] = {
        0xc0053c00, 0x13, static_cast<uint32_t>(realLabelAddress), static_cast<uint32_t>(realLabelAddress >> 32), 0,
        UINT32_MAX, 10};
    if (ctr_video_submit_init_v1(api->context, &backing, labelWait, 7)) return 144;
    labelWait[2] = static_cast<uint32_t>(realLabelAddress + 128);
    labelWait[3] = static_cast<uint32_t>((realLabelAddress + 128) >> 32);
    if (ctr_video_submit_init_v1(api->context, &backing, labelWait, 7) != 112) return 144;
    std::puts(
        "PM4 standalone wait passed pending_cursor_retained=true prefix_not_replayed=true late_invalid_EOP_atomic=true changed_input91=true real_owned_read=true synthetic=true R3=false");
    // No queue/event registration: real host clock alone must make first flip status valid.
    ctrport::DisplayEventHost clockHost;
    const auto realClock = ctrport::display_event_api(clockHost);
    auto invalidClock = realClock;
    invalidClock.clock = nullptr;
    if (ctr_video_bind_clock_v1(api->context, &invalidClock) != 91 ||
        ctr_video_bind_clock_v1(nullptr, &realClock) != 60 || ctr_video_bind_clock_v1(api->context, nullptr) != 60)
        return 254;
    int clockThread = 0;
    std::thread clockWorker([&] { clockThread = ctr_video_bind_clock_v1(api->context, &realClock); });
    clockWorker.join();
    if (clockThread != 116 || ctr_video_bind_clock_v1(api->context, &realClock) ||
        ctr_video_bind_clock_v1(api->context, &realClock))
        return 254;
    CtrVideoColorSettingsV1 identityColor{1.0f, {7, 8, 9}}, unsupportedColor{1.5f, {0, 0, 0}};
    if (ctr_video_adjust_color_v1(nullptr, &identityColor) != 60 ||
        ctr_video_adjust_color_v1(api->context, nullptr) != 60 ||
        ctr_video_adjust_color_v1(api->context, &unsupportedColor) != 89)
        return 255;
    int colorThread = 0;
    std::thread colorWorker([&] { colorThread = ctr_video_adjust_color_v1(api->context, &identityColor); });
    colorWorker.join();
    if (colorThread != 116 || ctr_video_adjust_color_v1(api->context, &identityColor)) return 255;
    identityColor.gamma = 1.5f; // provider must retain a copy, not the caller's settings pointer.
    std::vector<unsigned char> flipSnapshot(8355840, 0x71);
    CtrVideoProgressV1 beforeFlip{};
    if (api->get_progress(api->context, &beforeFlip) || beforeFlip.pending || beforeFlip.completed) return 251;
    int flipThread = 0;
    std::thread flipWorker([&] {
        flipThread = ctr_video_submit_tiled_snapshot_v1(api->context, 0, addresses[0], flipSnapshot.data(),
                                                        flipSnapshot.size(), 77);
    });
    flipWorker.join();
    if (flipThread != 116 ||
        ctr_video_submit_tiled_snapshot_v1(nullptr, 0, addresses[0], flipSnapshot.data(), flipSnapshot.size(), 77) !=
            60 ||
        ctr_video_submit_tiled_snapshot_v1(api->context, 15, addresses[0], flipSnapshot.data(), flipSnapshot.size(),
                                           77) != 62 ||
        ctr_video_submit_tiled_snapshot_v1(api->context, 0, addresses[0] + 256, flipSnapshot.data(),
                                           flipSnapshot.size(), 77) != 88 ||
        ctr_video_submit_tiled_snapshot_v1(api->context, 0, addresses[0], flipSnapshot.data(), flipSnapshot.size() - 1,
                                           77) != 88)
        return 251;
    CtrVideoProgressV1 rejectedFlip{};
    if (api->get_progress(api->context, &rejectedFlip) || rejectedFlip.pending || rejectedFlip.completed) return 251;
    if (ctr_video_submit_tiled_snapshot_v1(api->context, 0, addresses[0], flipSnapshot.data(), flipSnapshot.size(), 77))
        return 251;
    const auto firstFlipFrame = static_cast<PresentationSession*>(api->context)->CaptureHostSlot(0);
    if (!firstFlipFrame || firstFlipFrame->bytes.empty()) return 251;
    for (auto byte : firstFlipFrame->bytes)
        if (byte != 0x71) return 251;
    std::fill(flipSnapshot.begin(), flipSnapshot.end(), static_cast<unsigned char>(0x29));
    if (ctr_video_submit_tiled_snapshot_v1(api->context, 0, addresses[0], flipSnapshot.data(), flipSnapshot.size(), 78))
        return 251;
    const auto secondFlipFrame = static_cast<PresentationSession*>(api->context)->CaptureHostSlot(0);
    if (!secondFlipFrame || firstFlipFrame == secondFlipFrame || firstFlipFrame->token != secondFlipFrame->token)
        return 251;
    for (auto byte : firstFlipFrame->bytes)
        if (byte != 0x71) return 251;
    for (auto byte : secondFlipFrame->bytes)
        if (byte != 0x29) return 251;
    uint64_t flipLabelAddress = 0;
    if (ctr_video_tiled_label_address_v1(api->context, &flipLabelAddress)) return 252;
    auto* ownedFlipLabels = reinterpret_cast<uint64_t*>(static_cast<uintptr_t>(flipLabelAddress));
    ownedFlipLabels[0] = 1;
    CtrVideoCompletionV1 flipDone{}, invalidFlip{1, 999, 77};
    if (ctr_video_complete_tiled_flip_v1(api->context, 0, &invalidFlip) != 88 || ownedFlipLabels[0] != 1) return 252;
    if (api->start_pacing(api->context) || api->present_next(api->context, 500, &flipDone) || flipDone.count != 1 ||
        flipDone.argument != 77 || !flipDone.token)
        return 251;
    unsigned char realFlipStatus[64]{};
    uint64_t submitTicks = 0, completeMicros = 0, completeTicks = 0;
    uint32_t clockValid = 0;
    static_cast<PresentationSession*>(api->context)->FlipTiming(submitTicks, completeMicros, completeTicks, clockValid);
    if (clockValid != 3 || !submitTicks || !completeTicks || completeTicks < submitTicks ||
        api->get_public_flip(api->context, realFlipStatus, 64) ||
        ctr_video_bind_clock_v1(api->context, &realClock) != 91)
        return 254;
    std::puts(
        "mapped tiled noEvent clock actual first present publicFlip0 submitCompleteValid3 realTSC no event registration invalid91 null60 thread116 late91");
    std::puts(
        "mapped tiled identity gamma actual GPU present byte preservation copied settings unsupported89 null60 thread116");
    invalidFlip = flipDone;
    invalidFlip.argument = 999;
    if (ctr_video_complete_tiled_flip_v1(api->context, 0, &invalidFlip) != 88 || ownedFlipLabels[0] != 1 ||
        ctr_video_complete_tiled_flip_v1(api->context, 0, &flipDone) || ownedFlipLabels[0] != 1 ||
        ctr_video_complete_tiled_flip_v1(api->context, 0, &flipDone) != 91 || ownedFlipLabels[0] != 1)
        return 252;
    uint64_t endedFrame = 999;
    if (ctr_video_end_tiled_frame_v1(api->context, &flipDone, &endedFrame) != 63 || endedFrame != 999) return 253;
    // Drain second immutable frame, then end a frame boundary covering both actual presentations.
    if (api->present_next(api->context, 500, &flipDone) || flipDone.count != 2 || flipDone.argument != 78 ||
        ctr_video_complete_tiled_flip_v1(api->context, 0, &flipDone) || ownedFlipLabels[0] != 0 ||
        api->stop_pacing(api->context))
        return 252;
    invalidFlip = flipDone;
    invalidFlip.token = 999;
    if (ctr_video_end_tiled_frame_v1(api->context, &invalidFlip, &endedFrame) != 88 || endedFrame != 999 ||
        ctr_video_end_tiled_frame_v1(api->context, nullptr, &endedFrame) != 60 ||
        ctr_video_end_tiled_frame_v1(api->context, &flipDone, &endedFrame) || endedFrame != 1 ||
        ownedFlipLabels[0] != 0)
        return 253;
    endedFrame = 999;
    if (ctr_video_end_tiled_frame_v1(api->context, &flipDone, &endedFrame) != 91 || endedFrame != 999) return 253;
    std::puts(
        "mapped tiled frame boundary actual barrier fence deviceIdle ordinal1 pending63 wrongIdentity88 duplicate91 atomic labels unchanged no syntheticIRQ");
    std::puts(
        "mapped tiled completion public previousSlot semantics first keepsCurrentLock1 second retiresPrevious0 actual presents count1/2 wrongIdentity88 duplicate91 atomic no syntheticIRQ");
    std::puts(
        "mapped tiled snapshot slot0 actual Display2DThin detile8355840B immutable enqueue two refreshed snapshots realGPU presents completion77/78 invalidAddressBytes88 missing62 thread116 atomic no game frame");
    backing.reset();
    if (weak.expired() || ctr_video_unregister_tiled_v1(api->context, 0) || !weak.expired() ||
        ctr_video_unregister_tiled_v1(api->context, 0) != 62)
        return 138;
    CtrVideoProgressV1 progress{};
    if (api->get_progress(api->context, &progress) || progress.completed != 2 || progress.pending) return 138;
    std::puts(
        "tiled session companion passed real_resource_registration=true lease_retention=true checked_label_address=true unregister_release=true GPU_flipcount0=true R3=false");
    return 0;
}
extern "C" int ctr_video_tiled_session_control() {
    return ctr_video_run_session_v1(tiled_mapped_session_body, nullptr);
}
extern "C" int ctr_video_graphics_draw_v1(void* context, uint64_t pipeline, uint64_t color,
                                          const CtrGraphicsViewportStateV1* viewport,
                                          const CtrGraphicsDynamicStateV1* dynamic, const CtrGraphicsPushDataV1* push,
                                          const CtrGraphicsDrawV1* draw) {
    if (!context) return 60;
    try {
        return static_cast<PresentationSession*>(context)->DrawGraphics(pipeline, color, viewport, dynamic, push, draw);
    } catch (...) {
        return 90;
    }
}

extern "C" int ctr_video_graphics_mrt_pipeline_create_v1(void* context, const CtrGraphicsKeyV1* key,
                                                         const CtrGraphicsVertexInputsV1* vertices,
                                                         const uint32_t* formats, uint32_t formatCount, uint64_t layout,
                                                         const uint64_t* colors, uint32_t colorCount,
                                                         const uint32_t* vs, uint32_t vsWords, const uint32_t* ps,
                                                         uint32_t psWords, uint32_t sampleShading, uint64_t* token) {
    if (!context) return 60;
    try {
        return static_cast<PresentationSession*>(context)->CreateMrtPipeline(key, vertices, formats, formatCount,
                                                                             layout, colors, colorCount, vs, vsWords,
                                                                             ps, psWords, sampleShading, token);
    } catch (...) {
        return 90;
    }
}

extern "C" int ctr_video_mixed_depth_authority_v1(void* context, uint32_t* output) {
    if (!context || !output) return 60;
    return static_cast<PresentationSession*>(context)->MixedDepthAuthority(output);
}

extern "C" int ctr_video_color_backing_copy_v1(void* context, uint64_t token, uint32_t direction) {
    if (!context) return 60;
    return static_cast<PresentationSession*>(context)->CopyColorBackingPublic(token, direction);
}

extern "C" int ctr_video_graphics_mrt_depth_pipeline_create_v1(void* context, const CtrGraphicsKeyV1* key,
                                                               const CtrGraphicsVertexInputsV1* vertices,
                                                               const uint32_t* formats, uint32_t formatCount,
                                                               uint64_t layout, const uint64_t* colors,
                                                               uint32_t colorCount, uint64_t depth, const uint32_t* vs,
                                                               uint32_t vsWords, const uint32_t* ps, uint32_t psWords,
                                                               uint32_t sampleShading, uint64_t* token) {
    if (!context) return 60;
    try {
        return static_cast<PresentationSession*>(context)->CreateMrtPipeline(key, vertices, formats, formatCount,
                                                                             layout, colors, colorCount, vs, vsWords,
                                                                             ps, psWords, sampleShading, token, depth);
    } catch (...) {
        return 90;
    }
}

extern "C" int ctr_video_graphics_indexed_color_draw_v1(void* context, uint64_t pipeline, uint64_t color,
                                                        const CtrGraphicsViewportStateV1* viewport,
                                                        const CtrGraphicsDynamicStateV1* dynamic,
                                                        const CtrGraphicsPushDataV1* push,
                                                        const CtrGraphicsIndexedDrawV1* indexed) {
    if (!context || !indexed || indexed->version != 1 || indexed->size != sizeof(*indexed) || !indexed->index_count ||
        !indexed->index_token || indexed->vertex_buffer_count > 32)
        return 60;
    if ((indexed->index_bytes != 2 && indexed->index_bytes != 4) || indexed->instance_count != 1 ||
        indexed->first_instance)
        return 88;
    try {
        auto* session = static_cast<PresentationSession*>(context);
        const int domain = session->PreflightIndexedColorPipeline(pipeline);
        if (domain) return domain;
        CtrGraphicsDrawV1 draw{};
        draw.version = 1;
        draw.size = sizeof(draw);
        draw.vertex_count = indexed->index_count;
        draw.instance_count = indexed->instance_count;
        draw.first_instance = indexed->first_instance;
        draw.vertex_buffer_count = indexed->vertex_buffer_count;
        draw.written_buffers = indexed->written_buffers;
        draw.written_images = indexed->written_images;
        draw.atomic_resources = indexed->atomic_resources;
        std::memcpy(draw.vertex_buffers, indexed->vertex_buffers, sizeof(draw.vertex_buffers));
        return session->DrawGraphics(pipeline, color, viewport, dynamic, push, &draw, false, nullptr,
                                     indexed->index_token, indexed->index_bytes, indexed->index_offset,
                                     indexed->first_index, indexed->vertex_offset);
    } catch (...) {
        return 90;
    }
}
extern "C" int ctr_video_graphics_indexed_depth_draw_v1(void* context, uint64_t pipeline,
                                                        const CtrGraphicsViewportStateV1* viewport,
                                                        const CtrGraphicsDynamicStateV1* dynamic,
                                                        const CtrGraphicsPushDataV1* push,
                                                        const CtrGraphicsIndexedDrawV1* indexed) {
    if (!context || !indexed || indexed->version != 1 || indexed->size != sizeof(*indexed) || !indexed->index_count ||
        !indexed->index_token || indexed->vertex_buffer_count > 32)
        return 60;
    if ((indexed->index_bytes != 2 && indexed->index_bytes != 4) || indexed->instance_count != 1 ||
        indexed->first_instance)
        return 88;
    try {
        auto* session = static_cast<PresentationSession*>(context);
        const int domain = session->PreflightIndexedDepthPipeline(pipeline);
        if (domain) return domain;
        CtrGraphicsDrawV1 draw{};
        draw.version = 1;
        draw.size = sizeof(draw);
        draw.vertex_count = indexed->index_count;
        draw.instance_count = indexed->instance_count;
        draw.first_instance = indexed->first_instance;
        draw.vertex_buffer_count = indexed->vertex_buffer_count;
        draw.written_buffers = indexed->written_buffers;
        draw.written_images = indexed->written_images;
        draw.atomic_resources = indexed->atomic_resources;
        std::memcpy(draw.vertex_buffers, indexed->vertex_buffers, sizeof(draw.vertex_buffers));
        return session->DrawMrtDepth(pipeline, viewport, dynamic, push, &draw, nullptr, true, indexed->index_token,
                                     indexed->index_bytes, indexed->index_offset, indexed->first_index,
                                     indexed->vertex_offset);
    } catch (...) {
        return 90;
    }
}
extern "C" int ctr_video_graphics_mrt_depth_draw_v1(void* context, uint64_t pipeline,
                                                    const CtrGraphicsViewportStateV1* viewport,
                                                    const CtrGraphicsDynamicStateV1* dynamic,
                                                    const CtrGraphicsPushDataV1* push, const CtrGraphicsDrawV1* draw) {
    if (!context) return 60;
    try {
        return static_cast<PresentationSession*>(context)->DrawMrtDepth(pipeline, viewport, dynamic, push, draw);
    } catch (...) {
        return 90;
    }
}

extern "C" int ctr_video_depth_sampled_alias_v1(void* context, uint64_t depth, uint64_t* image) {
    if (!context) return 60;
    try {
        return static_cast<PresentationSession*>(context)->AliasSampledDepth(depth, image);
    } catch (...) {
        return 90;
    }
}
extern "C" int ctr_video_depth_attachment_restore_v1(void* context, uint64_t depth) {
    if (!context) return 60;
    return static_cast<PresentationSession*>(context)->RestoreDepthAttachment(depth);
}
extern "C" int ctr_video_graphics_mrt_draw_v1(void* context, uint64_t pipeline,
                                              const CtrGraphicsViewportStateV1* viewport,
                                              const CtrGraphicsDynamicStateV1* dynamic,
                                              const CtrGraphicsPushDataV1* push, const CtrGraphicsDrawV1* draw) {
    if (!context) return 60;
    try {
        return static_cast<PresentationSession*>(context)->DrawMrtDepth(pipeline, viewport, dynamic, push, draw,
                                                                        nullptr, false);
    } catch (...) {
        return 90;
    }
}
