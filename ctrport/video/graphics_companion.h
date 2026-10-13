// SPDX-License-Identifier: GPL-2.0-only
#pragma once
#include <stdint.h>
#include "shader_inputs.h"
#include "../shader_owned_ranges.h"
#ifdef __cplusplus
extern "C" {
#endif
// Optional synchronous thread-local diagnostic observer; reset with (NULL,NULL).
// Metadata only: source_kind0 direct userdata,1 captured SRT leaf,2 unknown.
typedef struct CtrShaderProvenanceV1 {uint32_t pc,flat_index,source_opcode,root_sgpr,source_dword,source_kind;} CtrShaderProvenanceV1;
typedef void (*CtrShaderProvenanceObserverV1)(void*,const CtrShaderProvenanceV1*);
int ctr_graphics_provenance_observer_v1(CtrShaderProvenanceObserverV1 observer,void* context);
// During observer callback only; metadata for1..8 consecutive flattened DWORDs.
// Unknown mappings retained explicitly. Output unchanged on rejection.
int ctr_graphics_provenance_span_v1(uint32_t flat_index,uint32_t count,CtrShaderProvenanceV1* output,uint32_t capacity);
// Effective public ImageResource::GetSharp classification used before BindImages.
typedef struct CtrGraphicsEffectiveImageClassV1 {uint32_t version,size,raw_type_valid,effective_type_valid,null_reasons,fallback_reason;} CtrGraphicsEffectiveImageClassV1;
int ctr_graphics_effective_image_classify_v1(const uint32_t* descriptor,uint32_t words,uint32_t is_depth,uint32_t is_r128,CtrGraphicsEffectiveImageClassV1* output);
typedef struct CtrGraphicsViewportV1 {float x,y,width,height,minDepth,maxDepth;} CtrGraphicsViewportV1;
typedef struct CtrGraphicsScissorV1 {struct {int32_t x,y;} offset;struct {uint32_t width,height;} extent;} CtrGraphicsScissorV1;
typedef struct CtrGraphicsViewportStateV1 {
 uint32_t version,size,count;CtrGraphicsViewportV1 viewports[16];CtrGraphicsScissorV1 scissors[16];
} CtrGraphicsViewportStateV1;
// Complete public viewport/scissor register policy. Capabilities from selected device only.
int ctr_graphics_viewport_state_v1(const uint32_t* flat,uint32_t words,
 uint32_t maxWidth,uint32_t maxHeight,uint32_t unrestrictedDepth,CtrGraphicsViewportStateV1* output);
typedef struct CtrGraphicsStencilFaceV1 {
 uint32_t fail_op,pass_op,depth_fail_op,compare_op,reference,write_mask,compare_mask;
} CtrGraphicsStencilFaceV1;
typedef struct CtrGraphicsDynamicStateV1 {
 uint32_t version,size,depth_test,depth_write,depth_compare,depth_bounds_test,depth_bias,stencil_test;
 float depth_min,depth_max,bias_constant,bias_clamp,bias_slope;
 CtrGraphicsStencilFaceV1 front,back;
 uint32_t primitive_restart,rasterizer_discard,cull_mode,front_face;
 float line_width,blend_constants[4];
} CtrGraphicsDynamicStateV1;
// Enum values remain public AMD values; disabled conditional values are not GPU state defaults.
int ctr_graphics_dynamic_state_v1(const uint32_t* flat,uint32_t words,uint32_t indexed,
 uint32_t listRestart,uint32_t patchRestart,CtrGraphicsDynamicStateV1* output);
// Exact captured bank + same compiler-pass MRT + actual enabled mixed-sample capabilities.
// Values are public AMD enums, never Vulkan enum casts. Metadata only, no GPU admission.
typedef struct CtrGraphicsKeyColorV1 {
    uint32_t data_format,number_format,number_conversion,export_format,swizzle[4];
    uint32_t write_mask,samples;
    uint32_t blend_enable,color_src_factor,color_func,color_dst_factor;
    uint32_t alpha_src_factor,alpha_func,alpha_dst_factor,separate_alpha_blend,disable_rop3;
} CtrGraphicsKeyColorV1;
typedef struct CtrGraphicsKeyV1 {
    uint32_t version,size,mrt_mask,num_color_attachments,patch_control_points;
    uint32_t cb_shader_mask,logic_op,num_samples,depth_samples,z_format,stencil_format;
    uint32_t depth_clamp_enable,depth_clip_enable,primitive_type,polygon_mode,clip_space,provoking_vtx_last;
    CtrGraphicsKeyColorV1 colors[8];
} CtrGraphicsKeyV1;
int ctr_graphics_key_decode_v1(const uint32_t* flat,uint32_t words,uint32_t mrt_mask,
    uint32_t mixed_any_samples,uint32_t mixed_depth_samples,CtrGraphicsKeyV1* output);

// Public complete SurfaceFormats table; output is Vulkan VkFormat numeric value.
// Actual selected-device VERTEX_BUFFER/color/image usage query is still required.
int ctr_graphics_surface_format_v1(uint32_t data_format,uint32_t number_format,uint32_t* output);
// Host-only synchronous diagnostic ABI. Every profile field must come from
// actual enabled-device capabilities/public driver policy; no default profile.
typedef struct CtrGraphicsProfileV1 {
    uint32_t version, size;
    uint64_t max_ubo_size;
    uint32_t max_viewport_width,max_viewport_height,max_shared_memory_size;
    uint32_t supported_spirv,subgroup_size,depth_clip_control;
    uint32_t support_int8;
    uint32_t support_int16;
    uint32_t support_int64;
    uint32_t support_float16;
    uint32_t support_float64;
    uint32_t supports_denorm_behavior_independence;
    uint32_t supports_rounding_mode_independence;
    uint32_t support_fp16_denorm_preserve;
    uint32_t support_fp16_denorm_flush;
    uint32_t support_fp16_round_to_zero;
    uint32_t support_fp32_denorm_preserve;
    uint32_t support_fp32_denorm_flush;
    uint32_t support_fp32_round_to_zero;
    uint32_t support_fp64_denorm_preserve;
    uint32_t support_fp64_denorm_flush;
    uint32_t support_fp64_round_to_zero;
    uint32_t support_fp16_signed_zero_inf_nan_preserve;
    uint32_t support_fp32_signed_zero_inf_nan_preserve;
    uint32_t support_fp64_signed_zero_inf_nan_preserve;
    uint32_t supports_image_load_store_lod;
    uint32_t supports_native_cube_calc;
    uint32_t supports_trinary_minmax;
    uint32_t supports_buffer_fp32_atomic_min_max;
    uint32_t supports_image_fp32_atomic_min_max;
    uint32_t supports_buffer_int64_atomics;
    uint32_t supports_shared_int64_atomics;
    uint32_t supports_workgroup_explicit_memory_layout;
    uint32_t supports_amd_shader_explicit_vertex_parameter;
    uint32_t supports_fragment_shader_barycentric;
    uint32_t has_broken_spirv_clamp;
    uint32_t lower_left_origin_mode;
    uint32_t needs_manual_interpolation;
    uint32_t needs_lds_barriers;
    uint32_t needs_buffer_offsets;
    uint32_t needs_unorm_fixup;
    uint32_t needs_clip_distance_emulation;
    uint32_t supports_shader_stencil_export;
} CtrGraphicsProfileV1;
// Compiler-discovered binding identities only: never raw descriptors or addresses.
// kind: 0 buffer,1 image,2 sampler,3 fmask. sharp_index is a DWORD index
// in compiler flattened userdata, NOT an assumed guest pointer or SGPR pair.
// flags: bit0 written,1 inline,2 formatted,3 depth,4 atomic,5 array,6 r128.
typedef struct CtrGraphicsResourceIdentityV1 {
    uint32_t kind,sharp_index,flags,buffer_type,associated_image;
} CtrGraphicsResourceIdentityV1;
typedef struct CtrGraphicsResourcesV1 {
    uint32_t version,size,buffers,images,samplers,fmasks;
    uint32_t readconst_types,userdata_mask,flattened_userdata_words;
    uint32_t has_fetch_shader,fetch_shader_sgpr_base,mrt_mask;
    uint32_t loaded_attribute_components,stored_attribute_components;
    uint32_t binding_count;
    CtrGraphicsResourceIdentityV1 bindings[128];
} CtrGraphicsResourcesV1;
// All pointers in this view are HOST-only and valid solely during the sink call.
// The sink must finish or deep-copy before returning. The caller keeps this DLL
// generation loaded through return; no opaque compiler object crosses the ABI.
// Source userdata is copied register data, not authorized pointed-to resources.
typedef struct CtrCompiledGraphicsStageV1 {
    uint32_t version,size,vertex,spirv_words;
    uint64_t code_address;
    const uint32_t* spirv;
    const CtrGraphicsResourcesV1* resources;
    const uint32_t* userdata;
    uint32_t userdata_words;
} CtrCompiledGraphicsStageV1;
typedef int (*CtrCompiledGraphicsSinkV1)(void* context,const CtrCompiledGraphicsStageV1* stage);
typedef struct CtrCompiledGraphicsStageV2 {
    uint32_t version,size;
    CtrCompiledGraphicsStageV1 stage;
    const uint32_t* flattened_userdata;
    uint32_t flattened_words;
} CtrCompiledGraphicsStageV2;
typedef int (*CtrCompiledGraphicsSinkV2)(void*,const CtrCompiledGraphicsStageV2*);
// Whole ordinary PS then VS compilation with shared public binding counters.
// Pair delivery is atomic: no sink call unless both stages compile. Pointers are
// host-only, synchronous and DLL-generation-bound; sink must finish/deep-copy.
typedef struct CtrGraphicsDescriptorBindingV1 {
    uint32_t kind,sharp_index,binding,count,buffer_slot,storage;
} CtrGraphicsDescriptorBindingV1;
typedef struct CtrGraphicsBindingContractV1 {
    uint32_t version,size;
    uint32_t unified_before,buffer_before,userdata_before;
    uint32_t unified_after,buffer_after,userdata_after;
    uint32_t userdata_count,packed_userdata[16],descriptor_count;
    CtrGraphicsDescriptorBindingV1 descriptors[120];
} CtrGraphicsBindingContractV1;
typedef struct CtrGraphicsPushDataV1 {uint32_t version,size;uint8_t bytes[128];} CtrGraphicsPushDataV1;
// Whole MakeUserData + same-pass PS-first/VS PushUd + real buffer-cache byte offsets.
// With individual owner buffers beginning at offset0, provide explicit zero offsets.
int ctr_graphics_push_data_v1(const uint32_t* flat,uint32_t words,
 const CtrGraphicsBindingContractV1* ps,const CtrGraphicsBindingContractV1* vs,
 const uint32_t* bufferOffsets,uint32_t offsetCount,CtrGraphicsPushDataV1* output);
// Creates actual set layout, pipeline layout128B, pool and one UNWRITTEN set.
// No shader pipeline/resource binding/draw; owner-thread session-local token.
// Vertex-only compiler binding contract starts at genuine shared origin zero.
int ctr_video_graphics_vertex_layout_create_v1(void* session,const CtrGraphicsBindingContractV1* vertex,uint64_t* token);
int ctr_video_graphics_layout_create_v1(void* session,const CtrGraphicsBindingContractV1* ps,const CtrGraphicsBindingContractV1* vs,uint64_t* token);
int ctr_video_graphics_layout_release_v1(void* session,uint64_t token);
typedef struct CtrCompiledGraphicsPairV1 {
    uint32_t version,size;
    const CtrCompiledGraphicsStageV2* fragment;
    const CtrCompiledGraphicsStageV2* vertex;
    const CtrGraphicsBindingContractV1* fragment_bindings;
    const CtrGraphicsBindingContractV1* vertex_bindings;
} CtrCompiledGraphicsPairV1;
typedef int (*CtrCompiledGraphicsPairSinkV1)(void*,const CtrCompiledGraphicsPairV1*);
int ctr_graphics_compile_pair_v1(const CtrShaderInputsV1*,const CtrShaderOwnedRangesV1*,
    uint64_t ps_address,uint64_t ps_bytes,uint64_t vs_address,uint64_t vs_bytes,
    const uint32_t* flat,uint32_t words,const CtrGraphicsProfileV1*,
    CtrCompiledGraphicsPairSinkV1,void*,char*,uint32_t);
typedef struct CtrGraphicsFetchAttributeV1 {
 uint32_t semantic,dest_vgpr,num_elements,sgpr_base,dword_offset,instance_data,inst_offset,data_format,num_format;
} CtrGraphicsFetchAttributeV1;
typedef struct CtrGraphicsPipelineStageV1 {
 uint32_t version,size,mrt_mask,persp_sample,linear_sample,has_fetch,attribute_count,step_rate0,step_rate1;
 int32_t vertex_offset_sgpr,instance_offset_sgpr;
 CtrGraphicsFetchAttributeV1 attributes[32];
} CtrGraphicsPipelineStageV1;
typedef struct CtrGraphicsDrawParametersV1 {
 uint32_t version,size,vertex_offset,instance_offset,instance_count;
} CtrGraphicsDrawParametersV1;
// Whole public GetDrawOffsets and VgtNumInstances::NumInstances; same-pass VS raw userdata.
int ctr_graphics_draw_parameters_v1(const uint32_t* flat,uint32_t words,
 const CtrGraphicsPipelineStageV1* vertex,const uint32_t* userdata,uint32_t userdataCount,
 CtrGraphicsDrawParametersV1* output);
// Whole public GetVertexInputs metadata, with immutable owner-bounded sharp capture.
// One binding per fetch semantic; AMD formats only, Vulkan format capability pending.
typedef struct CtrGraphicsVertexInputV1 {
 uint32_t location,binding,offset,stride,input_rate,divisor,data_format,number_format;
 uint32_t descriptor[4],buffer_size;
 uint64_t guest_address;
} CtrGraphicsVertexInputV1;
typedef struct CtrGraphicsVertexInputsV1 {
 uint32_t version,size,count;
 CtrGraphicsVertexInputV1 inputs[32];
} CtrGraphicsVertexInputsV1;
int ctr_graphics_vertex_inputs_v1(const CtrShaderInputsV1*,const CtrShaderOwnedRangesV1*,
 const uint32_t* userdata,uint32_t words,const CtrGraphicsPipelineStageV1*,CtrGraphicsVertexInputsV1*);
typedef struct CtrCompiledGraphicsPairV2 {
 uint32_t version,size;
 const CtrCompiledGraphicsPairV1* pair;
 const CtrGraphicsPipelineStageV1* fragment_pipeline;
 const CtrGraphicsPipelineStageV1* vertex_pipeline;
} CtrCompiledGraphicsPairV2;
typedef int (*CtrCompiledGraphicsPairSinkV2)(void*,const CtrCompiledGraphicsPairV2*);
int ctr_graphics_compile_pair_v2(const CtrShaderInputsV1*,const CtrShaderOwnedRangesV1*,
 uint64_t,uint64_t,uint64_t,uint64_t,const uint32_t*,uint32_t,const CtrGraphicsProfileV1*,
 CtrCompiledGraphicsPairSinkV2,void*,char*,uint32_t);
// Genuine original PS-address-zero path. No fragment shader or fabricated fragment record.
typedef struct CtrCompiledGraphicsVertexOnlyV1 {
 uint32_t version,size;
 const CtrCompiledGraphicsStageV2* vertex;
 const CtrGraphicsBindingContractV1* vertex_bindings;
 const CtrGraphicsPipelineStageV1* vertex_pipeline;
} CtrCompiledGraphicsVertexOnlyV1;
typedef int (*CtrCompiledGraphicsVertexOnlySinkV1)(void*,const CtrCompiledGraphicsVertexOnlyV1*);
int ctr_graphics_compile_vertex_only_v1(const CtrShaderInputsV1*,const CtrShaderOwnedRangesV1*,
 uint64_t vs_address,uint64_t vs_bytes,const uint32_t* registers,uint32_t words,const CtrGraphicsProfileV1*,
 CtrCompiledGraphicsVertexOnlySinkV1,void*,char*,uint32_t);

// Explicit leaf-SRT capability and ephemeral full flattened descriptor input.
// Nested pointer graphs remain missing88. No GPU resource admission.
int ctr_graphics_compile_stage_v3(const CtrShaderInputsV1*,const CtrShaderOwnedRangesV1*,
    uint64_t,uint64_t,const uint32_t*,uint32_t,uint32_t,const CtrGraphicsProfileV1*,
    CtrCompiledGraphicsSinkV2,void*,char*,uint32_t);
// vertex is 1 for VS, 0 for PS; ordinary VS/PS routing only (others missing88).
// flat is full D000 HOST bank, seeded from actual retained init then replayed.
// Companion policy explicitly disables dumps and direct guest memory access.
// Shader bytes stay ephemeral. On failure output_words is unchanged; diagnostic
// is a truncated NUL-terminated missing/error explanation. No GPU work occurs.
int ctr_graphics_compile_diagnostic_v1(const CtrShaderInputsV1* inputs,
    uint64_t code_address,uint64_t observed_bytes,const uint32_t* flat,
    uint32_t flat_words,uint32_t vertex,const CtrGraphicsProfileV1* profile,
    uint32_t* output_words,char* diagnostic,uint32_t diagnostic_capacity);
// As v1, plus atomic resource metadata output on successful compilation.
int ctr_graphics_compile_diagnostic_v2(const CtrShaderInputsV1* inputs,
    uint64_t code_address,uint64_t observed_bytes,const uint32_t* flat,
    uint32_t flat_words,uint32_t vertex,const CtrGraphicsProfileV1* profile,
    uint32_t* output_words,char* diagnostic,uint32_t diagnostic_capacity,
    CtrGraphicsResourcesV1* resources,uint32_t resources_size);
// Synchronous delivery only; sink errors propagate unchanged. Sink is responsible
// for preflight and ownership of any resource it retains; this performs no GPU
// admission or submission. No sink invocation on capture/compiler failure.
int ctr_graphics_compile_stage_v1(const CtrShaderInputsV1* inputs,
    uint64_t code_address,uint64_t observed_bytes,const uint32_t* flat,
    uint32_t flat_words,uint32_t vertex,const CtrGraphicsProfileV1* profile,
    CtrCompiledGraphicsSinkV1 sink,void* sink_context,
    char* diagnostic,uint32_t diagnostic_capacity);
// Ephemeral decoded extent for a compiler-identified direct userdata buffer.
// This supplies a requirement, never ownership/coherence or binding admission.
// Inline/special/extended userdata resources remain missing88.
typedef struct CtrGraphicsBufferRequirementV1 {
    uint32_t version,size,sharp_index,is_written;
    uint64_t guest_address,bytes;
    uint32_t stride,num_records;
} CtrGraphicsBufferRequirementV1;
int ctr_graphics_buffer_requirement_v1(const CtrGraphicsResourceIdentityV1* resource,
    const uint32_t* userdata,uint32_t userdata_words,CtrGraphicsBufferRequirementV1* output);
int ctr_video_capture_graphics_profile_v1(void* session,CtrGraphicsProfileV1* output);
typedef struct CtrGraphicsClipPlanesV1 {
    uint32_t version,size,enabled_mask,bytes;
    uint32_t coefficients[24]; // exact retained register float bits, no guest pointer
} CtrGraphicsClipPlanesV1;
// Compiler inline ClipPlanes resource only; zero bytes means this permutation
// never reads it. GPU null-descriptor/layout policy remains separate admission.
int ctr_graphics_clip_planes_v1(const CtrGraphicsResourceIdentityV1* resource,
    const uint32_t* flat,uint32_t flat_words,CtrGraphicsClipPlanesV1* output);
typedef struct CtrGraphicsColorRequirementV1 {
    uint32_t version,size,slot,pitch,height,layers,samples,tile_mode,data_format,number_format;
    uint64_t guest_address,bytes;
} CtrGraphicsColorRequirementV1;
// Selected-device COLOR_ATTACHMENT image+view only. Extent pitch/height from demand;
// contents undefined, no guest tiling/coherence/LOAD or draw admission.
int ctr_video_color_attachment_create_v1(void* session,const CtrGraphicsColorRequirementV1* requirement,
    uint32_t blending,uint64_t* token);
// Actual selected-device feature plus KHR command availability; 0 or missing88.
// Selected core feature bits; query-required returns0 only for genuinely enabled features.
enum CtrGraphicsCoreFeatureV1 {
 CTR_GRAPHICS_CORE_DEPTHCLAMP=(1u<<0),
 CTR_GRAPHICS_CORE_SAMPLERATESHADING=(1u<<1),
 CTR_GRAPHICS_CORE_LOGICOP=(1u<<2),
 CTR_GRAPHICS_CORE_DUALSRCBLEND=(1u<<3),
 CTR_GRAPHICS_CORE_INDEPENDENTBLEND=(1u<<4),
 CTR_GRAPHICS_CORE_FILLMODENONSOLID=(1u<<5),
 CTR_GRAPHICS_CORE_DEPTHBOUNDS=(1u<<6),
 CTR_GRAPHICS_CORE_SHADERCLIPDISTANCE=(1u<<7),
 CTR_GRAPHICS_CORE_SHADERCULLDISTANCE=(1u<<8),
 CTR_GRAPHICS_CORE_GEOMETRYSHADER=(1u<<9),
 CTR_GRAPHICS_CORE_TESSELLATIONSHADER=(1u<<10),
 CTR_GRAPHICS_CORE_VERTEXPIPELINESTORESANDATOMICS=(1u<<11),
 CTR_GRAPHICS_CORE_FRAGMENTSTORESANDATOMICS=(1u<<12)
};
// Actual selected-device vertex-buffer format and vertex-input limit admission only.
// Formats must be decoded by the public surface mapping. Divisors other than1 remain88.
int ctr_video_graphics_vertex_inputs_preflight_v1(void* session,const CtrGraphicsVertexInputsV1* inputs,const uint32_t* vk_formats,uint32_t count);
int ctr_video_graphics_core_authority_v1(void* session,uint32_t required_mask);
// Required mask bit0 EXT_extended_dynamic_state, bit1 state2; actual features+commands.
int ctr_video_graphics_dynamic_authority_v1(void* session,uint32_t required_mask);
int ctr_video_dynamic_rendering_authority_v1(void* session);
// Synchronous fenced conversion for exact same guest backing/format/shape, no partial aliases.
int ctr_video_color_to_sampled_v1(void* session,uint64_t color_token,uint64_t image_token);
// Explicit linear RGBA8 snapshot, row pitch in bytes; no tiled payload admission.
int ctr_video_color_attachment_upload_linear_v1(void* session,uint64_t token,const void* source,uint64_t bytes,uint64_t pitch);
int ctr_video_color_attachment_readback_linear_v1(void* session,uint64_t token,void* destination,uint64_t bytes,uint64_t pitch);
int ctr_video_color_attachment_release_v1(void* session,uint64_t token);
// Tiled RGBA8 allocation requirement from retained public register layout.
// Other formats/linear/compressed/metadata/Neo policies remain missing88.
// This is an allocation demand, not image creation/layout/coherence admission.
int ctr_graphics_color_requirement_v1(const uint32_t* flat,uint32_t flat_words,
    uint32_t slot,CtrGraphicsColorRequirementV1* output);
// v2 admits nested fetch code/descriptors through explicit owner range leases.
// Owner callbacks and DLL remain live synchronously; no writer/GPU coherence.
int ctr_graphics_compile_stage_v2(const CtrShaderInputsV1* inputs,
    const CtrShaderOwnedRangesV1* ranges,uint64_t code_address,uint64_t observed_bytes,
    const uint32_t* flat,uint32_t flat_words,uint32_t vertex,
    const CtrGraphicsProfileV1* profile,CtrCompiledGraphicsSinkV1 sink,void* sink_context,
    char* diagnostic,uint32_t diagnostic_capacity);
// Same exact guest Buffer requirement; compiler-flattened host words up to 65536.
int ctr_graphics_buffer_requirement_v2(const CtrGraphicsResourceIdentityV1* resource,
    const uint32_t* flattened,uint32_t words,CtrGraphicsBufferRequirementV1* output);

// Copy the whole compiler Flatbuf into caller-owned host storage. No GPU binding.
int ctr_graphics_flatbuf_copy_v1(const CtrGraphicsResourceIdentityV1* resource,
    const uint32_t* flattened,uint32_t words,uint32_t* destination,
    uint32_t capacity_words,uint32_t* copied_bytes);
// Exact image(8 DWORD) or sampler(4 DWORD) from compiler-flattened host storage.
// Descriptor bits only; image allocation size and sampler Vulkan policy are pending.
int ctr_graphics_descriptor_copy_v1(const CtrGraphicsResourceIdentityV1* resource,
    const uint32_t* flattened,uint32_t words,uint32_t* destination,
    uint32_t capacity_words,uint32_t* copied_words);
typedef struct CtrGraphicsSamplerParametersV1 {
    uint32_t version,size,clamp_x,clamp_y,clamp_z,mag_filter,min_filter,mip_filter;
    uint32_t compare_func,border_color,unnormalized_in_shader,anisotropic;
    float lod_bias,min_lod,max_lod,max_anisotropy;
} CtrGraphicsSamplerParametersV1;
// Decoded public AMD semantics only; device limits/features and Vulkan mapping pending.
int ctr_graphics_sampler_parameters_v1(const uint32_t* descriptor,uint32_t words,
    CtrGraphicsSamplerParametersV1* output);
// Session-owner-thread only; opaque monotonic session-local token, never a Vulkan handle.
int ctr_video_sampler_create_v1(void* session,const CtrGraphicsSamplerParametersV1* parameters,
    uint64_t* token);
int ctr_video_sampler_release_v1(void* session,uint64_t token);
// Immutable initialized GPU buffer from caller-owned host bytes, no guest reads.
// usage:1 storage,2 index16,3 index32,4 vertex. Exact nonzero extent <=32MiB; storage
// also checked against actual selected-device maxStorageBufferRange.
// Copy returns INITIAL host-coherent bytes, NOT a GPU execution/readback oracle.
int ctr_video_buffer_create_v1(void* session,const void* source,uint64_t bytes,uint32_t usage,uint64_t* token);
int ctr_video_buffer_copy_initial_v1(void* session,uint64_t token,void* output,uint64_t bytes);
int ctr_video_buffer_release_v1(void* session,uint64_t token);
// Undefined-content depth backing only; no guest-memory/HTILE/load admission.
typedef struct CtrDepthAttachmentParametersV1 {
    uint32_t version,size,width,height,samples,z_format,stencil_format;
} CtrDepthAttachmentParametersV1;
int ctr_video_depth_attachment_create_v1(void* session,const CtrDepthAttachmentParametersV1* parameters,uint64_t* token);
// Distinct same-session token retaining the actual GPU image, memory and views.
// Release with depth_attachment_release; session must outlive every lease.
int ctr_video_depth_attachment_retain_v1(void* session,uint64_t token,uint64_t* lease);
int ctr_video_depth_attachment_release_v1(void* session,uint64_t token);
// Linear host D16 snapshot only. Does not publish guest bytes or clear dirty owners.
int ctr_video_depth_d16_read_linear_v1(void* session,uint64_t token,void* destination,uint64_t bytes,uint64_t pitch);
// Full logical-aspect clear with explicit IEEE float bits and stencil0..255;
// synchronous real GPU fence, persistent layout. No guest HTILE/coherence.
int ctr_video_depth_attachment_clear_v1(void* session,uint64_t token,uint32_t depth_float_bits,uint32_t stencil);


// Metadata classification only; reasons bit0 zero address, bit1 invalid remapped format.
// A successful classification does not admit a null Vulkan descriptor or resource.
// All buffer bindings of a paired layout, in PS-first/VS descriptor order.
// null_clip_planes=1 requires token0 and genuine nullDescriptor authority.
// All sampler bindings in paired PS-first/VS order, one session-local token each.
// All sampled-image bindings; uploaded single-view count1 only, storage/arrays88.
int ctr_video_graphics_layout_images_v1(void* session,uint64_t layout_token,
    const uint32_t* bindings,const uint64_t* image_tokens,uint32_t count);
// Explicit semantic-null sampled bindings only: flag1 requires token0 and enabled
// robustness2.nullDescriptor. Flag0 retains strict live owner lookup. Storage88.
int ctr_video_graphics_layout_images_null_v1(void* session,uint64_t layout_token,
    const uint32_t* bindings,const uint64_t* image_tokens,const uint32_t* null_sampled,uint32_t count);
int ctr_video_graphics_layout_samplers_v1(void* session,uint64_t layout_token,
    const uint32_t* bindings,const uint64_t* sampler_tokens,uint32_t count);
typedef struct CtrGraphicsBufferWriteV1 {
    uint32_t binding,buffer_slot,storage,null_clip_planes;
    uint64_t buffer_token;
} CtrGraphicsBufferWriteV1;
int ctr_video_graphics_layout_buffers_v1(void* session,uint64_t layout_token,
    const CtrGraphicsBufferWriteV1* writes,uint32_t count);
typedef struct CtrGraphicsImageClassV1 {
    uint32_t version,size,null_reasons,type_valid;
} CtrGraphicsImageClassV1;
int ctr_graphics_image_classify_v1(const uint32_t* descriptor,uint32_t words,
    CtrGraphicsImageClassV1* output);
typedef struct CtrGraphicsImageParametersV1 {
    uint32_t version,size,type,width,height,depth,pitch,layers,levels,samples;
    uint32_t data_format,number_format,tile_mode,pow2pad,base_level,base_array,last_array;
    uint32_t compression,alt_tile,swizzle[4];
    uint64_t guest_address;
} CtrGraphicsImageParametersV1;
// Allocation only on selected session owner thread. Tokens are paired with session;
// creation includes a 2D RGBA8 view with public component selectors; contents undefined.
// Explicit linear upload below is synchronous; no shader descriptor binding.
int ctr_video_sampled_image_create_v1(void* session,const CtrGraphicsImageParametersV1* parameters,
    uint64_t* token);
// Explicit LINEAR RGBA8 host rows, never tiled guest bytes. Source remains live for call.
// Synchronous upload retains staging/image through fence, then transitions shader-read layout.
// No shader binding/view or guest alias coherence authority. Tile conversion is caller prerequisite.
int ctr_video_sampled_image_upload_linear_v1(void* session,uint64_t token,const void* source,
    uint64_t source_bytes,uint64_t row_pitch);
// Synchronous linear RGBA8 readback after upload; rejects undefined content91.
// Failed capacity validation preserves destination; row padding remains untouched.
int ctr_video_sampled_image_readback_linear_v1(void* session,uint64_t token,void* destination,
    uint64_t destination_bytes,uint64_t row_pitch);
int ctr_video_sampled_image_release_v1(void* session,uint64_t token);
// Metadata only; this decoder does not admit resource creation or convert tiling.
int ctr_graphics_image_parameters_v1(const uint32_t* descriptor,uint32_t words,
    CtrGraphicsImageParametersV1* output);
typedef struct CtrGraphicsImageExtentV1 {
    uint32_t version,size,pitch,padded_height;
    uint64_t guest_address,bytes;
} CtrGraphicsImageExtentV1;
// Bounded original3242 layout: one D16 tile0 layer, no bank swizzle/alternate tiling.
// Metadata only; HTILE and guest ownership/publication remain caller responsibilities.
typedef struct CtrDepthD16LayoutV1 {
    uint32_t version,size,width,height,pitch,samples,z_format,stencil_format;
    uint32_t tile_mode,slice_tile_max,slice_start,slice_max,alt_tile,bank_swizzle;
    uint64_t guest_address;
} CtrDepthD16LayoutV1;
// Same decoded field layout, separately validated D32/stencil/MS2 tile1 domain.
typedef CtrDepthD16LayoutV1 CtrDepthMS2LayoutV1;
// Depth span only; stencil sample mapping and HTILE completion are not supplied.
int ctr_video_depth_ms2_extent_v1(const CtrDepthMS2LayoutV1* layout,CtrGraphicsImageExtentV1* output);
// Conversion only: caller supplies fenced interleaved D32 samples from this exact
// retained owner and immutable prior depth backing. Caller verifies generation.
// Preserves prior padding; writes output only on success. No stencil conversion,
// guest publication, guard removal, or residency retirement is performed.
int ctr_video_depth_ms2_retile_samples_v1(void* session,uint64_t token,
 const CtrDepthMS2LayoutV1* layout,const void* prior,uint64_t bytes,
 const void* samples,uint64_t sample_bytes,void* output);

// Exact backing intervals only, not CPU page padding or stencil sample mapping.
typedef struct CtrDepthBackingSpansV1 {
    uint32_t version,size;
    uint64_t depth_address,depth_bytes,stencil_address,stencil_bytes;
} CtrDepthBackingSpansV1;
int ctr_video_depth_ms2_spans_v1(const CtrDepthMS2LayoutV1* layout,uint64_t stencil_address,CtrDepthBackingSpansV1* output);
// Separate exact5851 D32S8 sample1 tile0 domain; logical960x540/pitch1024.
// Backing identity only: depth2621440B/stencil552960B, padding excluded.
typedef CtrDepthD16LayoutV1 CtrDepthSingleLayoutV1;
int ctr_video_depth_single_spans_v1(const CtrDepthSingleLayoutV1*,uint64_t stencil_address,CtrDepthBackingSpansV1*);
int ctr_video_depth_single_residency_register_v1(void*,uint64_t token,const CtrDepthSingleLayoutV1*,uint64_t stencil_address,uint64_t* generation);
// Register completed GPU backing as an immutable same-session generation. Registration
// owns the actual image. Clear/draw reject91 until unregister; lookup creates a lease.
int ctr_video_depth_residency_register_v1(void* session,uint64_t token,const CtrDepthMS2LayoutV1* layout,uint64_t stencil_address,uint64_t* generation);
int ctr_video_depth_residency_acquire_v1(void* session,const CtrDepthBackingSpansV1* spans,uint64_t generation,uint64_t* lease);
int ctr_video_depth_residency_remove_v1(void* session,uint64_t generation);


int ctr_video_depth_d16_extent_v1(const CtrDepthD16LayoutV1* layout,CtrGraphicsImageExtentV1* output);
// Full host snapshot; output is changed only after download and public retile complete.
int ctr_video_depth_d16_read_tiled_v1(void* session,uint64_t token,const CtrDepthD16LayoutV1* layout,
    const void* prior,uint64_t bytes,void* output);
int ctr_video_color_attachment_upload_tiled_v1(void* session,uint64_t token,const void* source,uint64_t bytes,const CtrGraphicsImageExtentV1* extent);

// Whole public macro-tile sizing for single-level, single-layer 2D RGBA8 tile14.
int ctr_graphics_image_extent_v1(const CtrGraphicsImageParametersV1* parameters,
    CtrGraphicsImageExtentV1* output);
// Expanded reached single-level/layer formats3/4/5/10, tiles1/8/13/14, samples1/2.
// Also BC3 UNORM type9/tile13/sample1, one/two mips, optional pow2 padding.
// BC3 extent pitch/height are padded base-level pixels; bytes include all mips.
int ctr_graphics_image_extent_v2(const CtrGraphicsImageParametersV1* parameters,
    CtrGraphicsImageExtentV1* output);
// Owned tiled RG8(two-byte), RGBA8 or R32float(four-byte) tile14 payload and whole-public extent; synchronous detile/upload.
// Exact single mip/layer/sample/non-alt domain; source remains live through call.
// BC3 UNORM type9/tile13 also accepts the exact extent_v2 concatenated one/two-mip
// block payload. Transfers preserve16-byte compressed blocks; no RGBA decoding.
int ctr_video_sampled_image_upload_tiled_v1(void* session,uint64_t token,const void* source,
    uint64_t source_bytes,const CtrGraphicsImageExtentV1* extent);
// Selected-session bounded paired pipeline: original key/fetch + exact session-owned color/layout.
// sampleShading is same-pass PS persp_sample OR linear_sample. No draw/command recording.
int ctr_video_graphics_pipeline_create_v1(void* context,const CtrGraphicsKeyV1* key,
 const CtrGraphicsVertexInputsV1* vertices,const uint32_t* formats,uint32_t formatCount,
 uint64_t layout,uint64_t color,const uint32_t* vs,uint32_t vsWords,const uint32_t* ps,uint32_t psWords,
 uint32_t sampleShading,uint64_t* token);
int ctr_video_graphics_pipeline_release_v1(void* context,uint64_t token);
typedef struct CtrGraphicsDrawV1 {
 uint32_t version,size,vertex_count,instance_count,first_vertex,first_instance;
 uint32_t vertex_buffer_count,written_buffers,written_images,atomic_resources;
 uint64_t vertex_buffers[32];
} CtrGraphicsDrawV1;
// Synchronous bounded TriangleList/color1/noDepth/nonindexed draw. Zero shader storage writes;
// exact pipeline vertex order, immutable token owners; unsupported images/samplers explicit88.
int ctr_video_graphics_draw_v1(void* context,uint64_t pipeline,uint64_t color,
 const CtrGraphicsViewportStateV1*,const CtrGraphicsDynamicStateV1*,const CtrGraphicsPushDataV1*,const CtrGraphicsDrawV1*);
// Selected-device bounded first-draw dynamic capability/finite/bounds preflight. No mutation.
int ctr_video_graphics_dynamic_preflight_v1(void* context,const CtrGraphicsViewportStateV1*,const CtrGraphicsDynamicStateV1*);
// Whole public auxiliary tessellation emitter, vertex-only/no fragment varyings.
// kind0 RectListTCS, kind1 PassthroughTES. Sink words live only during callback.
typedef int (*CtrGraphicsAuxiliarySinkV1)(void*,uint32_t kind,const uint32_t*,uint32_t words);
int ctr_graphics_auxiliary_vertex_only_v1(uint32_t kind,CtrGraphicsAuxiliarySinkV1,void*);
// Bounded original680 shape: RectList, no fragment/color/vertex inputs, depth/stencil z3/s1 MSAA2.
// TCS/TES must be owned copies from whole public auxiliary emitter. Allocation only, no draw.
int ctr_video_graphics_rect_depth_pipeline_create_v1(void*,const CtrGraphicsKeyV1*,uint64_t layout,uint64_t depth,
 const uint32_t* vs,uint32_t vsWords,const uint32_t* tcs,uint32_t tcsWords,const uint32_t* tes,uint32_t tesWords,uint64_t* token);
// Synchronous bounded RectList VS-only depth/MSAA2 draw; buffers-only descriptors, no storage writes.
int ctr_video_graphics_rect_depth_draw_v1(void*,uint64_t pipeline,uint64_t depth,
 const CtrGraphicsViewportStateV1*,const CtrGraphicsDynamicStateV1*,const CtrGraphicsPushDataV1*,const CtrGraphicsDrawV1*);
// Owner-thread conservative queue-wide Vulkan memory dependency and real fence.
// No guest snapshots, image layout transitions or alias/writeback synchronization implied.
int ctr_video_queue_memory_visibility_v1(void* session);
// Compute-only descriptor/push layout and real VkComputePipeline allocation; no dispatch.
int ctr_video_compute_layout_create_v1(void*,const CtrGraphicsBindingContractV1*,uint64_t*);
int ctr_video_compute_pipeline_create_v1(void*,uint64_t,const uint32_t*,uint32_t,uint64_t*);
int ctr_video_compute_pipeline_release_v1(void*,uint64_t);
// Synchronous compute dispatch with fully written SSBO/sampled-image/sampler descriptors.
// Actual shader-write->host barrier/fence; initialized R32float/RGBA8 storage images supported, guest writeback separate.
int ctr_video_compute_dispatch_v1(void*,uint64_t,const CtrGraphicsPushDataV1*,const uint32_t groups[3]);
// R32float (format4/number7) or RGBA8 UNORM (format10/number0) storage owner, one mip/layer/sample. Existing image upload/readback/release and layout_images accept this token.
// Initialize via linear upload before GENERAL descriptor binding; no tiled guest coherence implied.
int ctr_video_storage_image_create_v1(void*,const CtrGraphicsImageParametersV1*,uint64_t*);
// Four-byte tile14 snapshot conversion; prior complete tiled bytes preserve padding.
// Output committed only after actual image download + public GPU detile/retile fences. No guest memory write.
// BC3: prior/output are extent.bytes concatenated tiled mips. Read back every actual
// image block at each mip; retain prior bytes for padded blocks outside image dimensions.
// Output changes only after every GPU readback and public retile succeeds. Legacy linear
// readback rejects BC3 with88; pixel pitch*4 and RGBA scans are invalid for compressed data.
int ctr_video_image_readback_tiled_v1(void*,uint64_t,const void*,uint64_t,const CtrGraphicsImageExtentV1*,void*);
// Whole public compute translator/emitter and compiler-derived descriptors. All pointers ephemeral;
// sink must deep-copy before return/DLL unload. Metadata compilation alone never admits dispatch.
// stage.vertex is a legacy graphics field and ignored here; SPIRV execution model is GLCompute.
typedef struct CtrCompiledComputeV1 {
 uint32_t version,size;const CtrCompiledGraphicsStageV2* stage;
 const CtrGraphicsBindingContractV1* bindings;const uint32_t* compute_registers;uint32_t compute_words;
} CtrCompiledComputeV1;
typedef int (*CtrCompiledComputeSinkV1)(void*,const CtrCompiledComputeV1*);
int ctr_compute_compile_v1(const CtrShaderInputsV1*,const CtrShaderOwnedRangesV1*,uint64_t,uint64_t,
 const uint32_t*,uint32_t,const uint32_t*,uint32_t,const CtrGraphicsProfileV1*,CtrCompiledComputeSinkV1,void*,char*,uint32_t);
#ifdef __cplusplus
}
#endif

#ifdef __cplusplus
extern "C" {
#endif
// Two-target allocation only; retains exact color owners after token release. No MRT draw yet.
int ctr_video_graphics_mrt_pipeline_create_v1(void* context,const CtrGraphicsKeyV1* key,
 const CtrGraphicsVertexInputsV1* vertices,const uint32_t* formats,uint32_t formatCount,
 uint64_t layout,const uint64_t* colors,uint32_t colorCount,const uint32_t* vs,uint32_t vsWords,
 const uint32_t* ps,uint32_t psWords,uint32_t sampleShading,uint64_t* token);

#ifdef __cplusplus
}
#endif

#ifdef __cplusplus
extern "C" {
#endif
// Atomic selected-device snapshot: bit0 enumeration completed, bit1 AMD supported,
// bit2 AMD extension enabled on the successfully created device. Zero means unknown.
// This grants no combined MRT/depth pipeline or draw authority.
int ctr_video_mixed_depth_authority_v1(void* session,uint32_t* output_mask);
#ifdef __cplusplus
}
#endif

#ifdef __cplusplus
extern "C" {
#endif
// Pinned public shader backing copy: direction0 single->retained MS2,1 MS2->single sample0.
// Full padded color extent, synchronous fence, source COLOR restored. No guest writeback.
int ctr_video_color_backing_copy_v1(void* session,uint64_t color_token,uint32_t direction);
#ifdef __cplusplus
}
#endif

#ifdef __cplusplus
extern "C" {
#endif
// Genuine public fallback key colors1-or2/depth2: retains initialized MS color
// backings and original depth generation; creates pipeline only, no draw.
int ctr_video_graphics_mrt_depth_pipeline_create_v1(void* session,const CtrGraphicsKeyV1* key,
 const CtrGraphicsVertexInputsV1* vertices,const uint32_t* formats,uint32_t format_count,
 uint64_t layout,const uint64_t* colors,uint32_t color_count,uint64_t depth,
 const uint32_t* vs,uint32_t vs_words,const uint32_t* ps,uint32_t ps_words,
 uint32_t sample_shading,uint64_t* output);
#ifdef __cplusplus
}
#endif

#ifdef __cplusplus
extern "C" {
#endif
// Synchronous two MS-color plus retained depth/stencil draw; no guest writeback.
// Typed immutable index token, byte offset and signed base are preserved verbatim.
// Bounded indexed color1/MS2 + retained D32S8/MS2; one instance, firstInstance0.
typedef struct CtrGraphicsIndexedDrawV1 {
 uint32_t version,size,index_count,instance_count,first_index,first_instance;
 int32_t vertex_offset;uint32_t index_bytes;
 uint64_t index_token,index_offset;
 uint32_t vertex_buffer_count,written_buffers,written_images,atomic_resources;
 uint64_t vertex_buffers[32];
} CtrGraphicsIndexedDrawV1;
// Typed single-color indexed draw, no depth attachment or index rewriting.
int ctr_video_graphics_indexed_color_draw_v1(void* session,uint64_t pipeline,uint64_t color,
 const CtrGraphicsViewportStateV1*,const CtrGraphicsDynamicStateV1*,const CtrGraphicsPushDataV1*,const CtrGraphicsIndexedDrawV1*);
int ctr_video_graphics_indexed_depth_draw_v1(void* session,uint64_t pipeline,
 const CtrGraphicsViewportStateV1*,const CtrGraphicsDynamicStateV1*,const CtrGraphicsPushDataV1*,const CtrGraphicsIndexedDrawV1*);
int ctr_video_graphics_mrt_depth_draw_v1(void* session,uint64_t pipeline,
 const CtrGraphicsViewportStateV1* viewport,const CtrGraphicsDynamicStateV1* dynamic,
 const CtrGraphicsPushDataV1* push,const CtrGraphicsDrawV1* draw);
#ifdef __cplusplus
}
#endif

#ifdef __cplusplus
extern "C" {
#endif
// Retains actual depth owner and fences both-aspect read-only transition; no guest upload.
int ctr_video_depth_sampled_alias_v1(void* session,uint64_t depth,uint64_t* image);
int ctr_video_depth_attachment_restore_v1(void* session,uint64_t depth);
#ifdef __cplusplus
}
#endif
#ifdef __cplusplus
extern "C" {
#endif
int ctr_video_graphics_mrt_draw_v1(void* session,uint64_t pipeline,
 const CtrGraphicsViewportStateV1* viewport,const CtrGraphicsDynamicStateV1* dynamic,
 const CtrGraphicsPushDataV1* push,const CtrGraphicsDrawV1* draw);
#ifdef __cplusplus
}
#endif

#ifdef __cplusplus
extern "C" {
#endif
// Genuine attachment-free rasterization; original key is validated without mutation.
int ctr_video_graphics_no_attachment_pipeline_create_v1(void* context,const CtrGraphicsKeyV1* key,const CtrGraphicsVertexInputsV1* inputs,const uint32_t* formats,uint32_t count,uint64_t layout,const uint32_t* vs,uint32_t vsWords,const uint32_t* ps,uint32_t psWords,uint64_t* token);
int ctr_video_graphics_no_attachment_draw_v1(void* context,uint64_t pipeline,const CtrGraphicsViewportStateV1* viewport,const CtrGraphicsDynamicStateV1* dynamic,const CtrGraphicsPushDataV1* push,const CtrGraphicsDrawV1* draw);

#ifdef __cplusplus
}
#endif

#ifdef __cplusplus
extern "C" {
#endif
// Vertex-only attachment-free RectList: original VS plus public TCS/TES, no fragment substitute.
int ctr_video_graphics_rect_no_attachment_pipeline_create_v1(void* context,const CtrGraphicsKeyV1* key,uint64_t layout,const uint32_t* vs,uint32_t vsWords,const uint32_t* tcs,uint32_t tcsWords,const uint32_t* tes,uint32_t tesWords,uint64_t* token);
#ifdef __cplusplus
}
#endif
