#include "bc1_layout.hpp"
#include "video_core/texture_cache/tile.h"
// SPDX-License-Identifier: GPL-2.0-only
#include "graphics_companion.h"
#include "graphics_compile.hpp"
#include "vertex_runtime_info.hpp"
#include "fragment_runtime_info.hpp"
#include "graphics_key_decode.hpp"
#include "viewport_state_decode.hpp"
#include "dynamic_state_decode.hpp"
#include "graphics_push_data.hpp"
#include "surface_format_decode.hpp"
#include <algorithm>
#include <mutex>
#include <memory>
#include "owned_fetch_context.hpp"
#include "shader_provenance_observer.hpp"
#include "effective_image_class.hpp"
#include "shader_settings.h"
#include <cstring>
#include <chrono>
#include <cstdio>
#include <cstdlib>
namespace {
struct PairStageTiming {
    double originalCaptureMs = 0.0;
    double compilerWallMs = 0.0;
    double stageSinkMs = 0.0;
    std::string originalSha256;
    CtrPm4::PairCaptureTiming nested;
};
}
static int Explain(int status, const char* text, char* out, uint32_t capacity) {
    if (out && capacity) {
        const auto n = std::min<size_t>(std::strlen(text), capacity - 1);
        std::memcpy(out, text, n);
        out[n] = 0;
    }
    return status;
}
static int CompileDiagnostic(const CtrShaderInputsV1* inputs, uint64_t address, uint64_t bytes, const uint32_t* flat,
                             uint32_t words, uint32_t vertex, const CtrGraphicsProfileV1* supplied,
                             uint32_t* output_words, char* diagnostic, uint32_t capacity,
                             CtrGraphicsResourcesV1* resources, CtrCompiledGraphicsSinkV1 sink = nullptr,
                             void* sink_context = nullptr, const CtrShaderOwnedRangesV1* ranges = nullptr,
                             CtrCompiledGraphicsSinkV2 sink2 = nullptr,
                             Shader::Backend::Bindings* sharedBindings = nullptr,
                             CtrPm4::GraphicsBindingMetadata* bindingContract = nullptr,
                             CtrGraphicsPipelineStageV1* pipelineMetadata = nullptr, const uint32_t* compute = nullptr,
                             PairStageTiming* pairTiming = nullptr) {
    if (!inputs || !flat || words != 0xD000 || vertex > 1 || !supplied || !output_words || !diagnostic || !capacity ||
        supplied->version != 1 || supplied->size != sizeof(*supplied))
        return Explain(60, "Invalid host graphics diagnostic ABI", diagnostic, capacity);
    if (!supplied->max_ubo_size || !supplied->max_viewport_width || !supplied->max_viewport_height ||
        !supplied->max_shared_memory_size || !supplied->subgroup_size || supplied->depth_clip_control > 1 ||
        supplied->supported_spirv < 0x10000 || supplied->supported_spirv > 0x10600 || (supplied->supported_spirv & 255))
        return Explain(60, "Incomplete or invalid enabled-device profile", diagnostic, capacity);
    if (supplied->support_int8 > 1) return Explain(60, "Invalid profile boolean support_int8", diagnostic, capacity);
    if (supplied->support_int16 > 1) return Explain(60, "Invalid profile boolean support_int16", diagnostic, capacity);
    if (supplied->support_int64 > 1) return Explain(60, "Invalid profile boolean support_int64", diagnostic, capacity);
    if (supplied->support_float16 > 1)
        return Explain(60, "Invalid profile boolean support_float16", diagnostic, capacity);
    if (supplied->support_float64 > 1)
        return Explain(60, "Invalid profile boolean support_float64", diagnostic, capacity);
    if (supplied->supports_denorm_behavior_independence > 1)
        return Explain(60, "Invalid profile boolean supports_denorm_behavior_independence", diagnostic, capacity);
    if (supplied->supports_rounding_mode_independence > 1)
        return Explain(60, "Invalid profile boolean supports_rounding_mode_independence", diagnostic, capacity);
    if (supplied->support_fp16_denorm_preserve > 1)
        return Explain(60, "Invalid profile boolean support_fp16_denorm_preserve", diagnostic, capacity);
    if (supplied->support_fp16_denorm_flush > 1)
        return Explain(60, "Invalid profile boolean support_fp16_denorm_flush", diagnostic, capacity);
    if (supplied->support_fp16_round_to_zero > 1)
        return Explain(60, "Invalid profile boolean support_fp16_round_to_zero", diagnostic, capacity);
    if (supplied->support_fp32_denorm_preserve > 1)
        return Explain(60, "Invalid profile boolean support_fp32_denorm_preserve", diagnostic, capacity);
    if (supplied->support_fp32_denorm_flush > 1)
        return Explain(60, "Invalid profile boolean support_fp32_denorm_flush", diagnostic, capacity);
    if (supplied->support_fp32_round_to_zero > 1)
        return Explain(60, "Invalid profile boolean support_fp32_round_to_zero", diagnostic, capacity);
    if (supplied->support_fp64_denorm_preserve > 1)
        return Explain(60, "Invalid profile boolean support_fp64_denorm_preserve", diagnostic, capacity);
    if (supplied->support_fp64_denorm_flush > 1)
        return Explain(60, "Invalid profile boolean support_fp64_denorm_flush", diagnostic, capacity);
    if (supplied->support_fp64_round_to_zero > 1)
        return Explain(60, "Invalid profile boolean support_fp64_round_to_zero", diagnostic, capacity);
    if (supplied->support_fp16_signed_zero_inf_nan_preserve > 1)
        return Explain(60, "Invalid profile boolean support_fp16_signed_zero_inf_nan_preserve", diagnostic, capacity);
    if (supplied->support_fp32_signed_zero_inf_nan_preserve > 1)
        return Explain(60, "Invalid profile boolean support_fp32_signed_zero_inf_nan_preserve", diagnostic, capacity);
    if (supplied->support_fp64_signed_zero_inf_nan_preserve > 1)
        return Explain(60, "Invalid profile boolean support_fp64_signed_zero_inf_nan_preserve", diagnostic, capacity);
    if (supplied->supports_image_load_store_lod > 1)
        return Explain(60, "Invalid profile boolean supports_image_load_store_lod", diagnostic, capacity);
    if (supplied->supports_native_cube_calc > 1)
        return Explain(60, "Invalid profile boolean supports_native_cube_calc", diagnostic, capacity);
    if (supplied->supports_trinary_minmax > 1)
        return Explain(60, "Invalid profile boolean supports_trinary_minmax", diagnostic, capacity);
    if (supplied->supports_buffer_fp32_atomic_min_max > 1)
        return Explain(60, "Invalid profile boolean supports_buffer_fp32_atomic_min_max", diagnostic, capacity);
    if (supplied->supports_image_fp32_atomic_min_max > 1)
        return Explain(60, "Invalid profile boolean supports_image_fp32_atomic_min_max", diagnostic, capacity);
    if (supplied->supports_buffer_int64_atomics > 1)
        return Explain(60, "Invalid profile boolean supports_buffer_int64_atomics", diagnostic, capacity);
    if (supplied->supports_shared_int64_atomics > 1)
        return Explain(60, "Invalid profile boolean supports_shared_int64_atomics", diagnostic, capacity);
    if (supplied->supports_workgroup_explicit_memory_layout > 1)
        return Explain(60, "Invalid profile boolean supports_workgroup_explicit_memory_layout", diagnostic, capacity);
    if (supplied->supports_amd_shader_explicit_vertex_parameter > 1)
        return Explain(60, "Invalid profile boolean supports_amd_shader_explicit_vertex_parameter", diagnostic,
                       capacity);
    if (supplied->supports_fragment_shader_barycentric > 1)
        return Explain(60, "Invalid profile boolean supports_fragment_shader_barycentric", diagnostic, capacity);
    if (supplied->has_broken_spirv_clamp > 1)
        return Explain(60, "Invalid profile boolean has_broken_spirv_clamp", diagnostic, capacity);
    if (supplied->lower_left_origin_mode > 1)
        return Explain(60, "Invalid profile boolean lower_left_origin_mode", diagnostic, capacity);
    if (supplied->needs_manual_interpolation > 1)
        return Explain(60, "Invalid profile boolean needs_manual_interpolation", diagnostic, capacity);
    if (supplied->needs_lds_barriers > 1)
        return Explain(60, "Invalid profile boolean needs_lds_barriers", diagnostic, capacity);
    if (supplied->needs_buffer_offsets > 1)
        return Explain(60, "Invalid profile boolean needs_buffer_offsets", diagnostic, capacity);
    if (supplied->needs_unorm_fixup > 1)
        return Explain(60, "Invalid profile boolean needs_unorm_fixup", diagnostic, capacity);
    if (supplied->needs_clip_distance_emulation > 1)
        return Explain(60, "Invalid profile boolean needs_clip_distance_emulation", diagnostic, capacity);
    if (supplied->supports_shader_stencil_export > 1)
        return Explain(60, "Invalid profile boolean supports_shader_stencil_export", diagnostic, capacity);
    try {
        static std::once_flag settings;
        std::call_once(settings, [] { CtrVideo::InitializeShaderSettings(CtrVideo::ShaderSettings(false, false)); });
        CtrPm4::Registers captured;
        std::memcpy(captured.words.data(), flat, sizeof(captured.words));
        CtrPm4::GraphicsShaderState state;
        int status = 0;
        Shader::RuntimeInfo computeRuntime{};
        if (compute) {
            AmdGpu::ComputeProgram cs{};
            static_assert(sizeof(cs) == 80 * sizeof(uint32_t));
            std::memcpy(&cs, compute, sizeof(cs));
            state.codeAddress = cs.Address<uint64_t>();
            state.userRegisters = static_cast<uint32_t>(cs.settings.num_user_regs);
            state.allocatedVgprs = static_cast<uint32_t>(cs.settings.num_vgprs) * 4;
            state.userData = cs.user_data;
            if (!state.codeAddress || state.userRegisters > 16)
                return Explain(88, "Missing/unsupported compute program", diagnostic, capacity);
            computeRuntime.stage = Shader::Stage::Compute;
            computeRuntime.cs_info.workgroup_size = {cs.num_thread_x.full, cs.num_thread_y.full, cs.num_thread_z.full};
            computeRuntime.cs_info.tgid_enable = {cs.IsTgidEnabled(0), cs.IsTgidEnabled(1), cs.IsTgidEnabled(2)};
            computeRuntime.cs_info.shared_memory_size = cs.SharedMemSize();
            for (auto size : computeRuntime.cs_info.workgroup_size)
                if (!size) return Explain(88, "Zero compute workgroup dimension", diagnostic, capacity);
        } else
            status = CtrPm4::CaptureGraphicsShaderState(captured, vertex != 0, state);
        if (status) return Explain(status, "Missing/invalid graphics program state", diagnostic, capacity);
        if (state.codeAddress != address)
            return Explain(60, "Graphics address differs from retained state", diagnostic, capacity);
        Shader::RuntimeInfo runtime{};
        Shader::Profile profile{};
        profile.max_ubo_size = supplied->max_ubo_size;
        profile.max_viewport_width = supplied->max_viewport_width;
        profile.max_viewport_height = supplied->max_viewport_height;
        profile.max_shared_memory_size = supplied->max_shared_memory_size;
        profile.supported_spirv = supplied->supported_spirv;
        profile.subgroup_size = supplied->subgroup_size;
        profile.support_int8 = supplied->support_int8 != 0;
        profile.support_int16 = supplied->support_int16 != 0;
        profile.support_int64 = supplied->support_int64 != 0;
        profile.support_float16 = supplied->support_float16 != 0;
        profile.support_float64 = supplied->support_float64 != 0;
        profile.supports_denorm_behavior_independence = supplied->supports_denorm_behavior_independence != 0;
        profile.supports_rounding_mode_independence = supplied->supports_rounding_mode_independence != 0;
        profile.support_fp16_denorm_preserve = supplied->support_fp16_denorm_preserve != 0;
        profile.support_fp16_denorm_flush = supplied->support_fp16_denorm_flush != 0;
        profile.support_fp16_round_to_zero = supplied->support_fp16_round_to_zero != 0;
        profile.support_fp32_denorm_preserve = supplied->support_fp32_denorm_preserve != 0;
        profile.support_fp32_denorm_flush = supplied->support_fp32_denorm_flush != 0;
        profile.support_fp32_round_to_zero = supplied->support_fp32_round_to_zero != 0;
        profile.support_fp64_denorm_preserve = supplied->support_fp64_denorm_preserve != 0;
        profile.support_fp64_denorm_flush = supplied->support_fp64_denorm_flush != 0;
        profile.support_fp64_round_to_zero = supplied->support_fp64_round_to_zero != 0;
        profile.support_fp16_signed_zero_inf_nan_preserve = supplied->support_fp16_signed_zero_inf_nan_preserve != 0;
        profile.support_fp32_signed_zero_inf_nan_preserve = supplied->support_fp32_signed_zero_inf_nan_preserve != 0;
        profile.support_fp64_signed_zero_inf_nan_preserve = supplied->support_fp64_signed_zero_inf_nan_preserve != 0;
        profile.supports_image_load_store_lod = supplied->supports_image_load_store_lod != 0;
        profile.supports_native_cube_calc = supplied->supports_native_cube_calc != 0;
        profile.supports_trinary_minmax = supplied->supports_trinary_minmax != 0;
        profile.supports_buffer_fp32_atomic_min_max = supplied->supports_buffer_fp32_atomic_min_max != 0;
        profile.supports_image_fp32_atomic_min_max = supplied->supports_image_fp32_atomic_min_max != 0;
        profile.supports_buffer_int64_atomics = supplied->supports_buffer_int64_atomics != 0;
        profile.supports_shared_int64_atomics = supplied->supports_shared_int64_atomics != 0;
        profile.supports_workgroup_explicit_memory_layout = supplied->supports_workgroup_explicit_memory_layout != 0;
        profile.supports_amd_shader_explicit_vertex_parameter =
            supplied->supports_amd_shader_explicit_vertex_parameter != 0;
        profile.supports_fragment_shader_barycentric = supplied->supports_fragment_shader_barycentric != 0;
        profile.has_broken_spirv_clamp = supplied->has_broken_spirv_clamp != 0;
        profile.lower_left_origin_mode = supplied->lower_left_origin_mode != 0;
        profile.needs_manual_interpolation = supplied->needs_manual_interpolation != 0;
        profile.needs_lds_barriers = supplied->needs_lds_barriers != 0;
        profile.needs_buffer_offsets = supplied->needs_buffer_offsets != 0;
        profile.needs_unorm_fixup = supplied->needs_unorm_fixup != 0;
        profile.needs_clip_distance_emulation = supplied->needs_clip_distance_emulation != 0;
        profile.supports_shader_stencil_export = supplied->supports_shader_stencil_export != 0;

        if (compute) {
            runtime = computeRuntime;
            status = 0;
        } else
            status = vertex ? CtrPm4::BuildVertexRuntimeInfo(captured, supplied->depth_clip_control != 0, runtime)
                            : CtrPm4::BuildFragmentRuntimeInfo(captured, profile, runtime);
        if (status)
            return Explain(status, "Unsupported graphics routing or invalid fragment input count", diagnostic,
                           capacity);
        CtrPm4::OwnedShaderBinary binary;
        const auto captureStarted = std::chrono::steady_clock::now();
        status = CtrPm4::CaptureShaderBinaryInputs(inputs, address, bytes, binary);
        if (pairTiming)
            pairTiming->originalCaptureMs +=
                std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - captureStarted).count();
        if (status) return Explain(status, "Owned shader input capture/metadata/release failed", diagnostic, capacity);
        if (pairTiming)
            pairTiming->originalSha256 = CtrPm4::ExactInputSha256(binary.words.data(), binary.words.size() * 4);
        std::vector<uint32_t> spirv, flattened;
        std::string reason;
        CtrGraphicsResourcesV1 metadata{};
        CtrPm4::OwnedFetchContext fetchContext{inputs, ranges, state.userRegisters};
        fetchContext.allowSrtLeaf = sink2 != nullptr;
        std::unique_ptr<CtrPm4::ScopedOwnedFetchContext> fetchScope;
        if (ranges) fetchScope = std::make_unique<CtrPm4::ScopedOwnedFetchContext>(fetchContext);
        std::unique_ptr<CtrPm4::ScopedPairCaptureTiming> pairCaptureScope;
        if (pairTiming) pairCaptureScope = std::make_unique<CtrPm4::ScopedPairCaptureTiming>(pairTiming->nested);
        const auto compilerStarted = std::chrono::steady_clock::now();
        status = CtrPm4::CompileGraphicsDiagnostic(binary, state, vertex != 0, runtime, profile, spirv, reason,
                                                   &metadata, &flattened, sharedBindings, bindingContract,
                                                   pipelineMetadata, compute != nullptr);
        if (pairTiming)
            pairTiming->compilerWallMs +=
                std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - compilerStarted).count();
        if (status) return Explain(status, reason.c_str(), diagnostic, capacity);
        if (sink2 && (flattened.size() < 16 || flattened.size() > 65536 ||
                      flattened.size() != metadata.flattened_userdata_words ||
                      !std::equal(state.userData.begin(), state.userData.end(), flattened.begin())))
            return Explain(88, "Invalid flattened userdata transport extent or prefix", diagnostic, capacity);
        if (sink || sink2) {
            const CtrCompiledGraphicsStageV1 stage{1,
                                                   sizeof(CtrCompiledGraphicsStageV1),
                                                   vertex,
                                                   static_cast<uint32_t>(spirv.size()),
                                                   address,
                                                   spirv.data(),
                                                   &metadata,
                                                   state.userData.data(),
                                                   state.userRegisters};
            const CtrCompiledGraphicsStageV2 extended{2, sizeof(CtrCompiledGraphicsStageV2), stage, flattened.data(),
                                                      static_cast<uint32_t>(flattened.size())};
            const auto sinkStarted = std::chrono::steady_clock::now();
            const int accepted = sink2 ? sink2(sink_context, &extended) : sink(sink_context, &stage);
            if (pairTiming)
                pairTiming->stageSinkMs +=
                    std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - sinkStarted).count();
            if (accepted) return Explain(accepted, "Compiled stage sink rejected delivery", diagnostic, capacity);
        }
        if (resources) *resources = metadata;
        *output_words = static_cast<uint32_t>(spirv.size());
        diagnostic[0] = 0;
        return 0;
    } catch (const std::exception& error) {
        return Explain(112, error.what(), diagnostic, capacity);
    } catch (...) {
        return Explain(112, "Unknown compiler diagnostic exception", diagnostic, capacity);
    }
}

extern "C" __declspec(dllexport) int ctr_graphics_compile_diagnostic_v1(
    const CtrShaderInputsV1* inputs, uint64_t address, uint64_t bytes, const uint32_t* flat, uint32_t words,
    uint32_t vertex, const CtrGraphicsProfileV1* profile, uint32_t* output_words, char* diagnostic, uint32_t capacity) {
    return CompileDiagnostic(inputs, address, bytes, flat, words, vertex, profile, output_words, diagnostic, capacity,
                             nullptr);
}
extern "C" __declspec(dllexport) int
ctr_graphics_compile_diagnostic_v2(const CtrShaderInputsV1* inputs, uint64_t address, uint64_t bytes,
                                   const uint32_t* flat, uint32_t words, uint32_t vertex,
                                   const CtrGraphicsProfileV1* profile, uint32_t* output_words, char* diagnostic,
                                   uint32_t capacity, CtrGraphicsResourcesV1* resources, uint32_t resources_size) {
    if (!resources || resources_size != sizeof(*resources))
        return Explain(60, "Invalid resource metadata capacity", diagnostic, capacity);
    return CompileDiagnostic(inputs, address, bytes, flat, words, vertex, profile, output_words, diagnostic, capacity,
                             resources);
}

extern "C" __declspec(dllexport) int ctr_graphics_compile_stage_v1(const CtrShaderInputsV1* inputs, uint64_t address,
                                                                   uint64_t bytes, const uint32_t* flat, uint32_t words,
                                                                   uint32_t vertex, const CtrGraphicsProfileV1* profile,
                                                                   CtrCompiledGraphicsSinkV1 sink, void* sink_context,
                                                                   char* diagnostic, uint32_t capacity) {
    if (!sink) return Explain(60, "Missing synchronous compiled stage sink", diagnostic, capacity);
    uint32_t compiledWords = 0;
    return CompileDiagnostic(inputs, address, bytes, flat, words, vertex, profile, &compiledWords, diagnostic, capacity,
                             nullptr, sink, sink_context);
}

static int BufferRequirement(const CtrGraphicsResourceIdentityV1* resource, const uint32_t* userdata,
                             uint32_t userdata_words, CtrGraphicsBufferRequirementV1* output, uint32_t limit) {
    if (!resource || !userdata || !output || userdata_words > limit) return 60;
    if (resource->kind != 0 || resource->buffer_type != uint32_t(Shader::BufferType::Guest) || (resource->flags & 2) ||
        resource->sharp_index > userdata_words || userdata_words - resource->sharp_index < 4)
        return 88;
    AmdGpu::Buffer buffer{};
    std::memcpy(&buffer, userdata + resource->sharp_index, sizeof(buffer));
    if (!buffer.Valid() || buffer.base_address <= 1 || !buffer.num_records) return 88;
    const uint64_t extent = buffer.stride ? uint64_t(buffer.stride) * buffer.num_records : buffer.num_records;
    if (!extent || extent > UINT32_MAX || buffer.base_address > UINT64_MAX - extent) return 88;
    CtrGraphicsBufferRequirementV1 prepared{1,
                                            sizeof(CtrGraphicsBufferRequirementV1),
                                            resource->sharp_index,
                                            resource->flags & 1,
                                            buffer.base_address,
                                            buffer.GetSize(),
                                            buffer.GetStride(),
                                            buffer.num_records};
    *output = prepared;
    return 0;
}

extern "C" __declspec(dllexport) int ctr_graphics_buffer_requirement_v1(const CtrGraphicsResourceIdentityV1* resource,
                                                                        const uint32_t* userdata, uint32_t words,
                                                                        CtrGraphicsBufferRequirementV1* output) {
    return BufferRequirement(resource, userdata, words, output, 16);
}
extern "C" __declspec(dllexport) int ctr_graphics_buffer_requirement_v2(const CtrGraphicsResourceIdentityV1* resource,
                                                                        const uint32_t* flattened, uint32_t words,
                                                                        CtrGraphicsBufferRequirementV1* output) {
    return BufferRequirement(resource, flattened, words, output, 65536);
}

extern "C" __declspec(dllexport) int
ctr_graphics_compile_stage_v2(const CtrShaderInputsV1* inputs, const CtrShaderOwnedRangesV1* ranges, uint64_t address,
                              uint64_t bytes, const uint32_t* flat, uint32_t words, uint32_t vertex,
                              const CtrGraphicsProfileV1* profile, CtrCompiledGraphicsSinkV1 sink, void* sink_context,
                              char* diagnostic, uint32_t capacity) {
    if (!sink || !ranges || ranges->version != 1 || ranges->size != sizeof(*ranges) || !ranges->context ||
        !ranges->begin || !ranges->end)
        return Explain(60, "Invalid owned fetch stage API", diagnostic, capacity);
    uint32_t compiledWords = 0;
    return CompileDiagnostic(inputs, address, bytes, flat, words, vertex, profile, &compiledWords, diagnostic, capacity,
                             nullptr, sink, sink_context, ranges);
}

extern "C" __declspec(dllexport) int ctr_graphics_clip_planes_v1(const CtrGraphicsResourceIdentityV1* resource,
                                                                 const uint32_t* flat, uint32_t words,
                                                                 CtrGraphicsClipPlanesV1* output) {
    if (!resource || !flat || !output || words != 0xD000) return 60;
    if (resource->kind != 0 || resource->buffer_type != uint32_t(Shader::BufferType::ClipPlanes) ||
        !(resource->flags & 2) || (resource->flags & 1))
        return 88;
    AmdGpu::Regs regs{};
    static_assert(sizeof(regs) == 0xD000 * sizeof(uint32_t));
    memcpy(&regs, flat, sizeof(regs));
    CtrGraphicsClipPlanesV1 prepared{};
    prepared.version = 1;
    prepared.size = sizeof(prepared);
    prepared.enabled_mask = regs.clipper_control.user_clip_plane_enable;
    if (prepared.enabled_mask) {
        prepared.bytes = 96;
        for (uint32_t i = 0; i < 6; ++i) {
            const auto& plane = regs.clip_user_data[i];
            prepared.coefficients[i * 4] = plane.data_x;
            prepared.coefficients[i * 4 + 1] = plane.data_y;
            prepared.coefficients[i * 4 + 2] = plane.data_z;
            prepared.coefficients[i * 4 + 3] = plane.data_w;
        }
    }
    *output = prepared;
    return 0;
}

extern "C" __declspec(dllexport) int ctr_graphics_color_requirement_v1(const uint32_t* flat, uint32_t words,
                                                                       uint32_t slot,
                                                                       CtrGraphicsColorRequirementV1* output) {
    if (!flat || !output || words != 0xD000 || slot >= 8) return 60;
    AmdGpu::Regs regs{};
    memcpy(&regs, flat, sizeof(regs));
    const auto& buffer = regs.color_buffers[slot];
    if (!buffer || (buffer.info.format != 10 && buffer.info.format != 3 && buffer.info.format != 4) ||
        !buffer.IsTiled() || buffer.info.fast_clear || buffer.info.compression || buffer.info.dcc_enable ||
        buffer.info.alt_tile_mode || buffer.info.endian != AmdGpu::ColorBuffer::EndianSwap::None ||
        buffer.view.slice_start > buffer.view.slice_max)
        return 88;
    // Whole public tiled ImageInfo branch. Check public u32 slice arithmetic
    // before invoking it, so overflowing layouts cannot be admitted by wrap.
    const uint64_t elementBytes = buffer.info.format == 3 ? 2u : 4u;
    const uint64_t slice = elementBytes * (buffer.slice.tile_max + 1) * 64 * buffer.NumSamples();
    if (!slice || slice > UINT32_MAX) return 88;
    const uint64_t bytes = slice * buffer.NumSlices();
    if (buffer.Address() > UINT64_MAX - bytes || !buffer.Height()) return 88;
    CtrGraphicsColorRequirementV1 prepared{1,
                                           sizeof(CtrGraphicsColorRequirementV1),
                                           slot,
                                           buffer.Pitch(),
                                           buffer.Height(),
                                           buffer.NumSlices(),
                                           buffer.NumSamples(),
                                           uint32_t(buffer.GetTileMode()),
                                           uint32_t(buffer.GetDataFmt()),
                                           uint32_t(buffer.GetNumberFmt()),
                                           buffer.Address(),
                                           uint64_t(buffer.GetColorSliceSize()) * buffer.NumSlices()};
    *output = prepared;
    return 0;
}

extern "C" __declspec(dllexport) int
ctr_graphics_compile_stage_v3(const CtrShaderInputsV1* inputs, const CtrShaderOwnedRangesV1* ranges, uint64_t address,
                              uint64_t bytes, const uint32_t* flat, uint32_t words, uint32_t vertex,
                              const CtrGraphicsProfileV1* profile, CtrCompiledGraphicsSinkV2 sink, void* context,
                              char* diagnostic, uint32_t capacity) {
    if (!sink || !ranges || ranges->version != 1 || ranges->size != sizeof(*ranges) || !ranges->context ||
        !ranges->begin || !ranges->end)
        return Explain(60, "Invalid leaf SRT stage API", diagnostic, capacity);
    uint32_t count = 0;
    return CompileDiagnostic(inputs, address, bytes, flat, words, vertex, profile, &count, diagnostic, capacity,
                             nullptr, nullptr, context, ranges, sink);
}

extern "C" __declspec(dllexport) int ctr_graphics_flatbuf_copy_v1(const CtrGraphicsResourceIdentityV1* resource,
                                                                  const uint32_t* flattened, uint32_t words,
                                                                  uint32_t* destination, uint32_t capacity,
                                                                  uint32_t* copied_bytes) {
    if (!resource || !flattened || !destination || !copied_bytes || words < 16 || words > 65536 || capacity < words)
        return 60;
    if (resource->kind != 0 || resource->buffer_type != uint32_t(Shader::BufferType::Flatbuf) || (resource->flags & 1))
        return 88;
    // Whole public vk_rasterizer Flatbuf branch: all flattened DWORDs, no guest sharp.
    std::memmove(destination, flattened, size_t(words) * sizeof(uint32_t));
    *copied_bytes = words * sizeof(uint32_t);
    return 0;
}

extern "C" __declspec(dllexport) int ctr_graphics_descriptor_copy_v1(const CtrGraphicsResourceIdentityV1* resource,
                                                                     const uint32_t* flattened, uint32_t words,
                                                                     uint32_t* destination, uint32_t capacity,
                                                                     uint32_t* copied_words) {
    if (!resource || !flattened || !destination || !copied_words || words < 16 || words > 65536) return 60;
    const uint32_t count = resource->kind == 1 ? 8 : resource->kind == 2 ? 4 : 0;
    if (!count || (resource->flags & 2) || resource->sharp_index > words || words - resource->sharp_index < count)
        return 88;
    if (capacity < count) return 60;
    // Use the exact compiler-selected flattened index, never assume a root SGPR pair.
    std::memmove(destination, flattened + resource->sharp_index, count * sizeof(uint32_t));
    *copied_words = count;
    return 0;
}

extern "C" __declspec(dllexport) int ctr_graphics_sampler_parameters_v1(const uint32_t* descriptor, uint32_t words,
                                                                        CtrGraphicsSamplerParametersV1* output) {
    if (!descriptor || !output || words != 4) return 60;
    AmdGpu::Sampler sampler{};
    static_assert(sizeof(sampler) == 16);
    std::memcpy(&sampler, descriptor, sizeof(sampler));
    // Never enter the public MaxAniso unreachable branch on malformed descriptors.
    if (uint32_t(sampler.max_aniso.Value()) > 4 || uint32_t(sampler.mip_filter.Value()) > 2 ||
        uint32_t(sampler.filter_mode.Value()) != 0 || sampler.border_color_type == AmdGpu::BorderColor::Custom)
        return 88;
    CtrGraphicsSamplerParametersV1 prepared{
        1,
        sizeof(CtrGraphicsSamplerParametersV1),
        uint32_t(sampler.clamp_x.Value()),
        uint32_t(sampler.clamp_y.Value()),
        uint32_t(sampler.clamp_z.Value()),
        uint32_t(sampler.xy_mag_filter.Value()),
        uint32_t(sampler.xy_min_filter.Value()),
        uint32_t(sampler.mip_filter.Value()),
        uint32_t(sampler.depth_compare_func.Value()),
        uint32_t(sampler.border_color_type.Value()),
        uint32_t(sampler.force_unnormalized.Value()),
        uint32_t(AmdGpu::IsAnisoFilter(sampler.xy_mag_filter) || AmdGpu::IsAnisoFilter(sampler.xy_min_filter)),
        sampler.LodBias(),
        sampler.MinLod(),
        sampler.MaxLod(),
        sampler.MaxAniso()};
    *output = prepared;
    return 0;
}

extern "C" __declspec(dllexport) int ctr_graphics_image_classify_v1(const uint32_t* descriptor, uint32_t words,
                                                                    CtrGraphicsImageClassV1* output) {
    if (!descriptor || !output || words != 8) return 60;
    AmdGpu::Image image{};
    static_assert(sizeof(image) == 32);
    std::memcpy(&image, descriptor, sizeof(image));
    CtrGraphicsImageClassV1 value{1, sizeof(CtrGraphicsImageClassV1), 0, image.Valid() ? 1u : 0u};
    if (image.Address() == 0) value.null_reasons |= 1;
    if (image.GetDataFmt() == AmdGpu::DataFormat::FormatInvalid) value.null_reasons |= 2;
    *output = value;
    return 0;
}

extern "C" __declspec(dllexport) int ctr_graphics_image_parameters_v1(const uint32_t* descriptor, uint32_t words,
                                                                      CtrGraphicsImageParametersV1* output) {
    if (!descriptor || !output || words != 8) return 60;
    AmdGpu::Image image{};
    static_assert(sizeof(image) == 32);
    std::memcpy(&image, descriptor, sizeof(image));
    if (!image.Valid()) return 88;
    CtrGraphicsImageParametersV1 prepared{};
    prepared.version = 1;
    prepared.size = sizeof(prepared);
    prepared.type = uint32_t(image.type);
    prepared.width = uint32_t(image.width) + 1;
    prepared.height = uint32_t(image.height) + 1;
    prepared.depth = image.GetType() == AmdGpu::ImageType::Color3D ? uint32_t(image.depth) + 1 : 1;
    prepared.pitch = image.Pitch();
    prepared.layers = image.NumLayers();
    prepared.levels = image.NumLevels();
    prepared.samples = image.NumSamples();
    prepared.data_format = uint32_t(image.data_format);
    prepared.number_format = uint32_t(image.num_format);
    prepared.tile_mode = uint32_t(image.tiling_index);
    prepared.pow2pad = uint32_t(image.pow2pad);
    prepared.base_level = uint32_t(image.base_level);
    prepared.base_array = uint32_t(image.base_array);
    prepared.last_array = uint32_t(image.last_array);
    prepared.compression = uint32_t(image.compression_en);
    prepared.alt_tile = uint32_t(image.alt_tile_mode);
    prepared.swizzle[0] = uint32_t(image.dst_sel_x);
    prepared.swizzle[1] = uint32_t(image.dst_sel_y);
    prepared.swizzle[2] = uint32_t(image.dst_sel_z);
    prepared.swizzle[3] = uint32_t(image.dst_sel_w);
    prepared.guest_address = image.Address();
    *output = prepared;
    return 0;
}

extern "C" __declspec(dllexport) int ctr_graphics_image_extent_v1(const CtrGraphicsImageParametersV1* p,
                                                                  CtrGraphicsImageExtentV1* output) {
    if (!p || !output || p->version != 1 || p->size != sizeof(*p) || !p->width || !p->height || p->width > 16384 ||
        p->height > 16384 || !p->pitch || p->pitch > 16384)
        return 60;
    if (p->type != 9 || p->depth != 1 || p->layers != 1 || p->levels != 1 || p->samples != 1 || p->data_format != 10 ||
        p->tile_mode != 14 || p->pow2pad || p->base_level || p->base_array || p->last_array || p->compression ||
        p->alt_tile || p->pitch < p->width || !p->guest_address)
        return 88;
    const auto [pitch, height, bytes] =
        VideoCore::ImageSizeMacroTiled(p->pitch, p->height, 1, 32, 1, static_cast<AmdGpu::TileMode>(14), 0, false);
    if (!bytes || p->guest_address > UINT64_MAX - bytes) return 88;
    CtrGraphicsImageExtentV1 prepared{1, sizeof(CtrGraphicsImageExtentV1), pitch, height, p->guest_address, bytes};
    *output = prepared;
    return 0;
}

extern "C" __declspec(dllexport) int ctr_graphics_image_extent_v2(const CtrGraphicsImageParametersV1* p,
                                                                  CtrGraphicsImageExtentV1* output) {
    if (!p || !output || p->version != 1 || p->size != sizeof(*p) || !p->width || !p->height || !p->pitch ||
        p->width > 4096 ||
        (p->height > 4096 && !(p->data_format == 2 && p->number_format == 0 && p->type == 9 && p->width == 2048 &&
                               p->height == 8192 && p->pitch == 2048 && p->tile_mode == 0)) ||
        p->pitch > 4096)
        return 60;
    if (p->data_format == 35) {
        CtrPm4::Bc1Layout layout{};
        const int rc = CtrPm4::Bc1LayoutFor(*p, layout);
        if (rc) return rc;
        *output = {1,
                   sizeof(CtrGraphicsImageExtentV1),
                   layout.mips[0].pitch * 4,
                   layout.mips[0].height * 4,
                   p->guest_address,
                   layout.bytes};
        return 0;
    }
    // Whole public ImageInfo::UpdateSize block-compressed/micro-tile branch.
    // Public mip layout pitch/height are converted back to pixels after sizing in blocks.
    if (p->data_format == uint32_t(AmdGpu::DataFormat::FormatBc3)) {
        if (p->type != 9 || p->number_format != 0 || p->tile_mode != 13 || p->depth != 1 || p->layers != 1 ||
            !p->levels || p->levels > 2 || (p->levels == 2 && p->width < 2 && p->height < 2) || p->samples != 1 ||
            p->pow2pad > 1 || p->base_level || p->base_array || p->last_array || p->compression || p->alt_tile ||
            p->pitch < p->width || !p->guest_address)
            return 88;
        uint32_t basePitch = 0, baseHeight = 0;
        uint64_t total = 0;
        for (uint32_t mip = 0; mip < p->levels; ++mip) {
            uint32_t w = std::max(((p->pitch >> mip) + 3) / 4, 1u);
            uint32_t h = std::max(((p->height >> mip) + 3) / 4, 1u);
            if (p->pow2pad) {
                w = std::bit_ceil(w);
                h = std::bit_ceil(h);
            }
            uint32_t pitch = 0, height = 0;
            size_t bytes = 0;
            std::tie(pitch, height, bytes) = VideoCore::ImageSizeMicroTiled(w, h, 1, 128, 1);
            if (!mip) {
                basePitch = std::max(pitch * 4, 32u);
                baseHeight = std::max(height * 4, 32u);
            }
            total += bytes;
        }
        if (p->guest_address > UINT64_MAX - total) return 88;
        *output = {1, sizeof(CtrGraphicsImageExtentV1), basePitch, baseHeight, p->guest_address, total};
        return 0;
    }
    if ((p->type != 8 && p->type != 9 && p->type != 14) || p->depth != 1 || p->layers != 1 || p->levels != 1 ||
        (p->samples != 1 && p->samples != 2) || ((p->type != 14) && p->samples != 1) || p->pow2pad || p->base_level ||
        p->base_array || p->last_array || p->compression || p->alt_tile || p->pitch < p->width || !p->guest_address ||
        (p->type == 8 && p->height != 1))
        return 88;
    uint32_t bits = 0;
    switch (p->data_format) {
    case uint32_t(AmdGpu::DataFormat::Format8):
        if (p->type != 9 || p->tile_mode != 8 || p->number_format != 0 || p->samples != 1 ||
            (p->width != 1024 && p->width != 2048) || p->height != p->width || p->pitch != p->width)
            return 88;
        bits = 8;
        break;
    case uint32_t(AmdGpu::DataFormat::Format16):
        if (p->type != 9 || p->number_format != 0 || p->width != 2048 || p->height != 8192 || p->pitch != 2048 ||
            p->tile_mode != 0 || p->samples != 1)
            return 88;
        bits = 16;
        break;
    case uint32_t(AmdGpu::DataFormat::Format8_8): bits = 16; break;
    case uint32_t(AmdGpu::DataFormat::Format32):
    case uint32_t(AmdGpu::DataFormat::Format16_16):
    case uint32_t(AmdGpu::DataFormat::Format8_8_8_8): bits = 32; break;
    default: return 88;
    }
    uint32_t pitch = 0, height = 0;
    size_t bytes = 0;
    // Whole public UpdateSize branches for reached nonblock, nonvolume, single-mip images.
    switch (p->tile_mode) {
    case 8:
        std::tie(pitch, height, bytes) = VideoCore::ImageSizeLinearAligned(p->pitch, p->height, bits, p->samples);
        break;
    case 13:
        std::tie(pitch, height, bytes) = VideoCore::ImageSizeMicroTiled(p->pitch, p->height, 1, bits, p->samples);
        break;
    case 0:
    case 1:
    case 10:
    case 14:
        std::tie(pitch, height, bytes) = VideoCore::ImageSizeMacroTiled(
            p->pitch, p->height, 1, bits, p->samples, static_cast<AmdGpu::TileMode>(p->tile_mode), 0, false);
        break;
    default: return 88;
    }
    if (!bytes || p->guest_address > UINT64_MAX - bytes) return 88;
    CtrGraphicsImageExtentV1 prepared{1, sizeof(CtrGraphicsImageExtentV1), pitch, height, p->guest_address, bytes};
    *output = prepared;
    return 0;
}

namespace {
struct OwnedPairStage {
    std::vector<uint32_t> spirv, flattened, userdata;
    CtrGraphicsResourcesV1 resources{};
    CtrCompiledGraphicsStageV2 view{};
    CtrPm4::GraphicsBindingMetadata bindings;
    CtrGraphicsBindingContractV1 contract{};
    CtrGraphicsPipelineStageV1 pipeline{};
    static int Copy(void* context, const CtrCompiledGraphicsStageV2* source) {
        auto& out = *static_cast<OwnedPairStage*>(context);
        out.spirv.assign(source->stage.spirv, source->stage.spirv + source->stage.spirv_words);
        out.flattened.assign(source->flattened_userdata, source->flattened_userdata + source->flattened_words);
        out.userdata.assign(source->stage.userdata, source->stage.userdata + source->stage.userdata_words);
        out.resources = *source->stage.resources;
        out.view = *source;
        return 0;
    }
    void Finalize() {
        view.stage.spirv = spirv.data();
        view.stage.resources = &resources;
        view.stage.userdata = userdata.data();
        view.flattened_userdata = flattened.data();
        contract.version = 1;
        contract.size = sizeof(contract);
        contract.unified_before = bindings.before.unified;
        contract.buffer_before = bindings.before.buffer;
        contract.userdata_before = bindings.before.user_data;
        contract.unified_after = bindings.after.unified;
        contract.buffer_after = bindings.after.buffer;
        contract.userdata_after = bindings.after.user_data;
        contract.userdata_count = bindings.userdataCount;
        std::copy(bindings.packedUserdata.begin(), bindings.packedUserdata.end(), contract.packed_userdata);
        if (bindings.descriptors.size() > 120)
            throw std::runtime_error("Pair descriptor metadata exceeds ABI capacity");
        contract.descriptor_count = static_cast<uint32_t>(bindings.descriptors.size());
        for (size_t i = 0; i < bindings.descriptors.size(); ++i) {
            const auto& b = bindings.descriptors[i];
            contract.descriptors[i] = {b.kind, b.sharpIndex, b.binding, b.count, b.bufferSlot, uint32_t(b.storage)};
        }
    }
};
}
static int CompilePair(const CtrShaderInputsV1* inputs, const CtrShaderOwnedRangesV1* ranges, uint64_t psAddress,
                       uint64_t psBytes, uint64_t vsAddress, uint64_t vsBytes, const uint32_t* flat, uint32_t words,
                       const CtrGraphicsProfileV1* profile, CtrCompiledGraphicsPairSinkV1 sink, void* context,
                       char* diagnostic, uint32_t capacity, CtrCompiledGraphicsPairSinkV2 sink2 = nullptr) {
    if ((!sink && !sink2) || !psAddress || !vsAddress || !psBytes || !vsBytes)
        return Explain(60, "Invalid ordinary graphics pair input/sink", diagnostic, capacity);
    try {
        char timingValue[2]{};
        size_t timingLength = 0;
        const bool timing = getenv_s(&timingLength, timingValue, sizeof(timingValue), "CTR_TILING_TIMING") == 0 &&
                            timingLength == 2 && timingValue[0] == '1';
        const auto pairStarted = std::chrono::steady_clock::now();
        PairStageTiming fragmentTiming, vertexTiming;
        OwnedPairStage fragment, vertex;
        Shader::Backend::Bindings shared{};
        uint32_t outputWords = 0;
        int status =
            CompileDiagnostic(inputs, psAddress, psBytes, flat, words, 0, profile, &outputWords, diagnostic, capacity,
                              nullptr, nullptr, &fragment, ranges, OwnedPairStage::Copy, &shared, &fragment.bindings,
                              sink2 ? &fragment.pipeline : nullptr, nullptr, timing ? &fragmentTiming : nullptr);
        if (status) return status;
        status =
            CompileDiagnostic(inputs, vsAddress, vsBytes, flat, words, 1, profile, &outputWords, diagnostic, capacity,
                              nullptr, nullptr, &vertex, ranges, OwnedPairStage::Copy, &shared, &vertex.bindings,
                              sink2 ? &vertex.pipeline : nullptr, nullptr, timing ? &vertexTiming : nullptr);
        if (status) return status;
        fragment.Finalize();
        vertex.Finalize();
        const CtrCompiledGraphicsPairV1 pair{
            1, sizeof(CtrCompiledGraphicsPairV1), &fragment.view, &vertex.view, &fragment.contract, &vertex.contract};
        const CtrCompiledGraphicsPairV2 extended{2, sizeof(CtrCompiledGraphicsPairV2), &pair, &fragment.pipeline,
                                                 &vertex.pipeline};
        const auto pairSinkStarted = std::chrono::steady_clock::now();
        status = sink2 ? sink2(context, &extended) : sink(context, &pair);
        const double pairSinkMs =
            std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - pairSinkStarted).count();
        if (timing) {
            const double compilerWall = fragmentTiming.compilerWallMs + vertexTiming.compilerWallMs;
            const double nested = fragmentTiming.nested.nestedMs + vertexTiming.nested.nestedMs;
            std::fprintf(
                stderr,
                "CTR_GRAPHICS_PAIR_PHASE ps=%016llx vs=%016llx ps_bytes=%llu vs_bytes=%llu ps_sha256=%s vs_sha256=%s status=%d original_capture_ms=%.6f nested_fetch_capture_ms=%.6f nested_fetch_calls=%u recompiler_ms=%.6f stage_copy_sink_ms=%.6f pair_sink_ms=%.6f total_ms=%.6f\n",
                static_cast<unsigned long long>(psAddress), static_cast<unsigned long long>(vsAddress),
                static_cast<unsigned long long>(psBytes), static_cast<unsigned long long>(vsBytes),
                fragmentTiming.originalSha256.c_str(), vertexTiming.originalSha256.c_str(), status,
                fragmentTiming.originalCaptureMs + vertexTiming.originalCaptureMs, nested,
                fragmentTiming.nested.nestedCalls + vertexTiming.nested.nestedCalls, compilerWall - nested,
                fragmentTiming.stageSinkMs + vertexTiming.stageSinkMs, pairSinkMs,
                std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - pairStarted).count());
            const auto emitNested = [&](const char* stage, const PairStageTiming& measured) {
                for (size_t i = 0; i < measured.nested.nestedInputs.size(); ++i) {
                    const auto& input = measured.nested.nestedInputs[i];
                    std::fprintf(
                        stderr,
                        "CTR_GRAPHICS_PAIR_NESTED ps=%016llx vs=%016llx stage=%s index=%llu address=%016llx bytes=%llu sha256=%s\n",
                        static_cast<unsigned long long>(psAddress), static_cast<unsigned long long>(vsAddress), stage,
                        static_cast<unsigned long long>(i), static_cast<unsigned long long>(input.address),
                        static_cast<unsigned long long>(input.bytes), input.sha256.c_str());
                }
            };
            emitNested("ps", fragmentTiming);
            emitNested("vs", vertexTiming);
        }
        if (status) return Explain(status, "Graphics pair sink rejected delivery", diagnostic, capacity);
        diagnostic[0] = 0;
        return 0;
    } catch (const std::exception& e) {
        return Explain(112, e.what(), diagnostic, capacity);
    } catch (...) {
        return Explain(112, "Unknown graphics pair exception", diagnostic, capacity);
    }
}

extern "C" __declspec(dllexport) int
ctr_graphics_compile_pair_v1(const CtrShaderInputsV1* i, const CtrShaderOwnedRangesV1* r, uint64_t pa, uint64_t pb,
                             uint64_t va, uint64_t vb, const uint32_t* f, uint32_t n, const CtrGraphicsProfileV1* p,
                             CtrCompiledGraphicsPairSinkV1 s, void* c, char* d, uint32_t z) {
    return CompilePair(i, r, pa, pb, va, vb, f, n, p, s, c, d, z);
}
extern "C" __declspec(dllexport) int
ctr_graphics_compile_pair_v2(const CtrShaderInputsV1* i, const CtrShaderOwnedRangesV1* r, uint64_t pa, uint64_t pb,
                             uint64_t va, uint64_t vb, const uint32_t* f, uint32_t n, const CtrGraphicsProfileV1* p,
                             CtrCompiledGraphicsPairSinkV2 s, void* c, char* d, uint32_t z) {
    return CompilePair(i, r, pa, pb, va, vb, f, n, p, nullptr, c, d, z, s);
}

extern "C" __declspec(dllexport) int
ctr_graphics_compile_vertex_only_v1(const CtrShaderInputsV1* inputs, const CtrShaderOwnedRangesV1* ranges,
                                    uint64_t address, uint64_t bytes, const uint32_t* flat, uint32_t words,
                                    const CtrGraphicsProfileV1* profile, CtrCompiledGraphicsVertexOnlySinkV1 sink,
                                    void* context, char* diagnostic, uint32_t capacity) {
    if (!sink || !flat || words != 0xD000 || !address || !bytes)
        return Explain(60, "Invalid vertex-only input/sink", diagnostic, capacity);
    // Public Regs::ShaderProgram::Address: do not confuse invalid PS state with absence.
    if (flat[0x2c08] || (flat[0x2c09] & 255))
        return Explain(88, "Vertex-only path requires original zero PS program address", diagnostic, capacity);
    try {
        OwnedPairStage vertex;
        Shader::Backend::Bindings shared{};
        uint32_t outputWords = 0;
        const int compiled = CompileDiagnostic(inputs, address, bytes, flat, words, 1, profile, &outputWords,
                                               diagnostic, capacity, nullptr, nullptr, &vertex, ranges,
                                               OwnedPairStage::Copy, &shared, &vertex.bindings, &vertex.pipeline);
        if (compiled) return compiled;
        vertex.Finalize();
        const CtrCompiledGraphicsVertexOnlyV1 delivery{1, sizeof(delivery), &vertex.view, &vertex.contract,
                                                       &vertex.pipeline};
        const int accepted = sink(context, &delivery);
        if (accepted) return Explain(accepted, "Vertex-only sink rejected delivery", diagnostic, capacity);
        diagnostic[0] = 0;
        return 0;
    } catch (const std::exception& e) {
        return Explain(112, e.what(), diagnostic, capacity);
    } catch (...) {
        return Explain(112, "Unknown vertex-only exception", diagnostic, capacity);
    }
}
extern "C" __declspec(dllexport) int ctr_graphics_key_decode_v1(const uint32_t* flat, uint32_t words, uint32_t mrt_mask,
                                                                uint32_t mixed_any_samples,
                                                                uint32_t mixed_depth_samples,
                                                                CtrGraphicsKeyV1* output) {
    if (!flat || words != 0xD000 || !output || mixed_any_samples > 1 || mixed_depth_samples > 1) return 60;
    CtrPm4::Registers captured{};
    std::memcpy(captured.words.data(), flat, sizeof(captured.words));
    CtrPm4::DecodedGraphicsKey key{};
    const int status =
        CtrPm4::DecodeGraphicsKey(captured, mrt_mask, mixed_any_samples != 0, mixed_depth_samples != 0, key);
    if (status) return status;
    CtrGraphicsKeyV1 result{};
    result.version = 1;
    result.size = sizeof(result);
    result.mrt_mask = key.mrt_mask;
    result.num_color_attachments = key.num_color_attachments;
    result.patch_control_points = key.patch_control_points;
    result.cb_shader_mask = key.cb_shader_mask.raw;
    result.logic_op = uint32_t(key.logic_op);
    result.num_samples = key.num_samples;
    result.depth_samples = key.depth_samples;
    result.z_format = uint32_t(key.z_format);
    result.stencil_format = uint32_t(key.stencil_format);
    result.depth_clamp_enable = key.depth_clamp_enable;
    result.depth_clip_enable = key.depth_clip_enable;
    result.primitive_type = uint32_t(key.prim_type);
    result.polygon_mode = uint32_t(key.polygon_mode);
    result.clip_space = uint32_t(key.clip_space);
    result.provoking_vtx_last = uint32_t(key.provoking_vtx_last);
    for (unsigned i = 0; i < 8; ++i) {
        auto& target = result.colors[i];
        const auto& color = key.color_buffers[i];
        const auto& blend = key.blend_controls[i];
        target.data_format = uint32_t(color.data_format);
        target.number_format = uint32_t(color.num_format);
        target.number_conversion = uint32_t(color.num_conversion);
        target.export_format = uint32_t(color.export_format);
        for (unsigned c = 0; c < 4; ++c) target.swizzle[c] = uint32_t(color.swizzle.array[c]);
        target.write_mask = key.write_masks[i];
        target.samples = key.color_samples[i];
        target.blend_enable = blend.enable;
        target.color_src_factor = uint32_t(blend.color_src_factor);
        target.color_func = uint32_t(blend.color_func);
        target.color_dst_factor = uint32_t(blend.color_dst_factor);
        target.alpha_src_factor = uint32_t(blend.alpha_src_factor);
        target.alpha_func = uint32_t(blend.alpha_func);
        target.alpha_dst_factor = uint32_t(blend.alpha_dst_factor);
        target.separate_alpha_blend = blend.separate_alpha_blend;
        target.disable_rop3 = blend.disable_rop3;
    }
    *output = result;
    return 0;
}
extern "C" __declspec(dllexport) int ctr_graphics_vertex_inputs_v1(const CtrShaderInputsV1* inputs,
                                                                   const CtrShaderOwnedRangesV1* ranges,
                                                                   const uint32_t* userdata, uint32_t words,
                                                                   const CtrGraphicsPipelineStageV1* stage,
                                                                   CtrGraphicsVertexInputsV1* output) {
    if (!stage || stage->version != 1 || stage->size != sizeof(*stage) || !output || words > 16 ||
        (words && !userdata) || stage->has_fetch > 1 || stage->attribute_count > 32 ||
        (!stage->has_fetch && stage->attribute_count))
        return 60;
    try {
        CtrGraphicsVertexInputsV1 result{};
        result.version = 1;
        result.size = sizeof(result);
        result.count = stage->attribute_count;
        Shader::Info info{};
        info.user_data = std::span<const uint32_t>(userdata, words);
        CtrPm4::OwnedFetchContext context{inputs, ranges, words};
        CtrPm4::ScopedOwnedFetchContext scope(context);
        uint32_t semantics = 0;
        for (uint32_t i = 0; i < result.count; ++i) {
            const auto& a = stage->attributes[i];
            if (a.semantic >= 32 || (semantics & (1u << a.semantic)) || a.dest_vgpr > 255 || a.num_elements > 4 ||
                !a.num_elements || a.sgpr_base > 255 || a.dword_offset > 255 || a.instance_data > 3 ||
                a.inst_offset > 255 || a.data_format > 15 || a.num_format > 7)
                return 60;
            semantics |= 1u << a.semantic;
            Shader::Gcn::VertexAttribute attr{uint8_t(a.semantic),    uint8_t(a.dest_vgpr),    uint8_t(a.num_elements),
                                              uint8_t(a.sgpr_base),   uint8_t(a.dword_offset), uint8_t(a.instance_data),
                                              uint8_t(a.inst_offset), uint8_t(a.data_format),  uint8_t(a.num_format)};
            const auto buffer = CtrPm4::CaptureFetchAttribute(info, attr);
            auto& target = result.inputs[i];
            target.location = target.binding = a.semantic;
            target.stride = buffer.GetStride();
            target.input_rate = a.instance_data == 0 ? 0 : 1;
            target.divisor = a.instance_data == 1 ? stage->step_rate0 : (a.instance_data == 2 ? stage->step_rate1 : 1);
            target.data_format = uint32_t(buffer.GetDataFmt());
            target.number_format = uint32_t(buffer.GetNumberFmt());
            target.guest_address = buffer.base_address;
            target.buffer_size = buffer.GetSize();
            std::memcpy(target.descriptor, &buffer, sizeof(buffer));
        }
        *output = result;
        return 0;
    } catch (...) {
        return 88;
    }
}
extern "C" __declspec(dllexport) int ctr_graphics_surface_format_v1(uint32_t data_format, uint32_t number_format,
                                                                    uint32_t* output) {
    if (!output) return 60;
    return CtrPm4::DecodeSurfaceFormat(data_format, number_format, *output);
}

extern "C" __declspec(dllexport) int ctr_graphics_viewport_state_v1(const uint32_t* flat, uint32_t words,
                                                                    uint32_t maxWidth, uint32_t maxHeight,
                                                                    uint32_t unrestrictedDepth,
                                                                    CtrGraphicsViewportStateV1* output) {
    if (!flat || words != 0xD000 || !output || !maxWidth || !maxHeight || unrestrictedDepth > 1) return 60;
    try {
        CtrPm4::Registers captured{};
        std::memcpy(captured.words.data(), flat, sizeof(captured.words));
        return CtrPm4::DecodeViewportState(captured, maxWidth, maxHeight, unrestrictedDepth, *output);
    } catch (...) {
        return 90;
    }
}

extern "C" __declspec(dllexport) int ctr_graphics_dynamic_state_v1(const uint32_t* flat, uint32_t words,
                                                                   uint32_t indexed, uint32_t listRestart,
                                                                   uint32_t patchRestart,
                                                                   CtrGraphicsDynamicStateV1* output) {
    if (!flat || words != 0xD000 || !output || indexed > 1 || listRestart > 1 || patchRestart > 1) return 60;
    try {
        CtrPm4::Registers captured{};
        std::memcpy(captured.words.data(), flat, sizeof(captured.words));
        return CtrPm4::DecodeDynamicState(captured, indexed != 0, listRestart != 0, patchRestart != 0, *output);
    } catch (...) {
        return 90;
    }
}

extern "C" __declspec(dllexport) int ctr_graphics_push_data_v1(const uint32_t* flat, uint32_t words,
                                                               const CtrGraphicsBindingContractV1* ps,
                                                               const CtrGraphicsBindingContractV1* vs,
                                                               const uint32_t* offsets, uint32_t count,
                                                               CtrGraphicsPushDataV1* output) {
    if (!flat || words != 0xD000 || !ps || !vs || !output) return 60;
    try {
        CtrPm4::Registers captured{};
        std::memcpy(captured.words.data(), flat, sizeof(captured.words));
        return CtrPm4::DecodePushData(captured, *ps, *vs, offsets, count, *output);
    } catch (...) {
        return 90;
    }
}

// Whole public GetDrawOffsets (vk_rasterizer.cpp:150), pinned e3ce810f.
extern "C" __declspec(dllexport) int ctr_graphics_draw_parameters_v1(const uint32_t* flat, uint32_t words,
                                                                     const CtrGraphicsPipelineStageV1* vertex,
                                                                     const uint32_t* userdata, uint32_t count,
                                                                     CtrGraphicsDrawParametersV1* output) {
    if (!flat || words != 0xD000 || !vertex || vertex->version != 1 || vertex->size != sizeof(*vertex) ||
        vertex->has_fetch > 1 || !userdata || count != 16 || !output)
        return 60;
    if (vertex->has_fetch && (vertex->vertex_offset_sgpr < -1 || vertex->vertex_offset_sgpr >= 16 ||
                              vertex->instance_offset_sgpr < -1 || vertex->instance_offset_sgpr >= 16))
        return 60;
    try {
        AmdGpu::Regs regs{};
        std::memcpy(regs.reg_array.data(), flat, sizeof(regs.reg_array));
        uint32_t vertex_offset = regs.index_offset, instance_offset = 0;
        if (vertex->has_fetch) {
            if (vertex_offset == 0 && vertex->vertex_offset_sgpr != -1)
                vertex_offset = userdata[vertex->vertex_offset_sgpr];
            if (vertex->instance_offset_sgpr != -1) instance_offset = userdata[vertex->instance_offset_sgpr];
        }
        CtrGraphicsDrawParametersV1 result{1, sizeof(result), vertex_offset, instance_offset,
                                           regs.num_instances.NumInstances()};
        *output = result;
        return 0;
    } catch (...) {
        return 90;
    }
}

#include "shader_recompiler/backend/spirv/emit_spirv_quad_rect.h"
extern "C" __declspec(dllexport) int
ctr_graphics_auxiliary_vertex_only_v1(uint32_t kind, CtrGraphicsAuxiliarySinkV1 sink, void* context) {
    if (kind > 1 || !sink) return 60;
    try {
        // No fragment stage consumes varyings in this bounded entry point.
        Shader::FragmentRuntimeInfo fs{};
        const auto type = kind == 0 ? Shader::Backend::SPIRV::AuxShaderType::RectListTCS
                                    : Shader::Backend::SPIRV::AuxShaderType::PassthroughTES;
        const auto words = Shader::Backend::SPIRV::EmitAuxilaryTessShader(type, fs);
        return sink(context, kind, words.data(), static_cast<uint32_t>(words.size()));
    } catch (...) {
        return 90;
    }
}

extern "C" __declspec(dllexport) int
ctr_compute_compile_v1(const CtrShaderInputsV1* inputs, const CtrShaderOwnedRangesV1* ranges, uint64_t address,
                       uint64_t bytes, const uint32_t* flat, uint32_t words, const uint32_t* compute,
                       uint32_t computeWords, const CtrGraphicsProfileV1* profile, CtrCompiledComputeSinkV1 sink,
                       void* context, char* diagnostic, uint32_t capacity) {
    if (!compute || computeWords != 80 || !sink || !ranges || ranges->version != 1 || ranges->size != sizeof(*ranges) ||
        !ranges->context || !ranges->begin || !ranges->end)
        return Explain(60, "Invalid owned compute ABI", diagnostic, capacity);
    struct Bridge {
        CtrCompiledComputeSinkV1 sink;
        void* context;
        const uint32_t* compute;
        CtrPm4::GraphicsBindingMetadata bindings;
    };
    Bridge bridge{sink, context, compute, {}};
    const auto deliver = [](void* opaque, const CtrCompiledGraphicsStageV2* stage) -> int {
        auto& b = *static_cast<Bridge*>(opaque);
        CtrGraphicsBindingContractV1 contract{};
        contract.version = 1;
        contract.size = sizeof(contract);
        contract.unified_before = b.bindings.before.unified;
        contract.unified_after = b.bindings.after.unified;
        contract.buffer_before = b.bindings.before.buffer;
        contract.buffer_after = b.bindings.after.buffer;
        contract.userdata_before = b.bindings.before.user_data;
        contract.userdata_after = b.bindings.after.user_data;
        if (b.bindings.descriptors.size() > 120) return 88;
        contract.descriptor_count = static_cast<uint32_t>(b.bindings.descriptors.size());
        contract.userdata_count = b.bindings.userdataCount;
        std::copy(b.bindings.packedUserdata.begin(), b.bindings.packedUserdata.end(), contract.packed_userdata);
        for (size_t i = 0; i < b.bindings.descriptors.size(); ++i) {
            const auto& d = b.bindings.descriptors[i];
            contract.descriptors[i] = {d.kind, d.sharpIndex, d.binding, d.count, d.bufferSlot, uint32_t(d.storage)};
        }
        const CtrCompiledComputeV1 result{1, sizeof(CtrCompiledComputeV1), stage, &contract, b.compute, 80};
        return b.sink(b.context, &result);
    };
    uint32_t count = 0;
    return CompileDiagnostic(inputs, address, bytes, flat, words, 0, profile, &count, diagnostic, capacity, nullptr,
                             nullptr, &bridge, ranges, deliver, nullptr, &bridge.bindings, nullptr, compute);
}

extern "C" __declspec(dllexport) int ctr_graphics_provenance_observer_v1(CtrShaderProvenanceObserverV1 observer,
                                                                         void* context) {
    if (!observer && context) return 60;
    CtrPm4::provenanceObserver = observer;
    CtrPm4::provenanceObserverContext = context;
    return 0;
}

extern "C" __declspec(dllexport) int ctr_graphics_provenance_span_v1(uint32_t flat, uint32_t count,
                                                                     CtrShaderProvenanceV1* output, uint32_t capacity) {
    return CtrPm4::CopyProvenanceSpan(flat, count, output, capacity);
}

extern "C" __declspec(dllexport) int
ctr_graphics_effective_image_classify_v1(const uint32_t* descriptor, uint32_t words, uint32_t depth, uint32_t r128,
                                         CtrGraphicsEffectiveImageClassV1* output) {
    try {
        return CtrPm4::ClassifyEffectiveImage(descriptor, words, depth, r128, output);
    } catch (...) {
        return 90;
    }
}
