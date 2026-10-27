// SPDX-License-Identifier: GPL-2.0-only
#include "shader_recompiler/recompiler.h"
#include "shader_recompiler/frontend/control_flow_graph.h"
#include "shader_recompiler/profile.h"
#include "shader_recompiler/ir/passes/ir_passes.h"
#include "shader_recompiler/ir/ir_emitter.h"
#include "shader_recompiler/backend/spirv/emit_spirv.h"
#include "shader_settings.h"
#include "compute_runtime_info.hpp"
#include "compute_limits.hpp"
#include "graphics_compile.hpp"
#include "shader_recompiler/frontend/fetch_shader.h"
#include "vertex_runtime_info.hpp"
#include "fragment_runtime_info.hpp"
#include "graphics_key_decode.hpp"
#define VK_NO_PROTOTYPES
#include <vulkan/vulkan.h>
#include "color_format_preflight.hpp"
#include "sampler_preflight.hpp"
#include "sampled_image_preflight.hpp"
#include <windows.h>
#include <functional>
#include <array>
struct Cleanup {
    std::vector<std::function<void()>> actions;
    ~Cleanup() {
        for (auto i = actions.rbegin(); i != actions.rend(); ++i) (*i)();
        std::printf("cleanup_callbacks_completed=%zu\n", actions.size());
    }
};
#include <stdexcept>
#include "owned_nested_capture_control.hpp"
#include "bounded_fetch_parser.hpp"
#include "owned_fetch_context.hpp"
#include "owned_srt_leaf.hpp"
#include "shader_provenance_observer.hpp"
#include "effective_image_class.hpp"
int main() {
    if (!TestOwnedNestedCapture()) return 90;
    {
        std::array<uint32_t, 8> descriptor{};
        CtrGraphicsEffectiveImageClassV1 result{};
        if (CtrPm4::ClassifyEffectiveImage(descriptor.data(), 8, 0, 0, &result) || result.raw_type_valid ||
            !result.effective_type_valid || !(result.null_reasons & 1) || result.fallback_reason != 1)
            return 250;
        auto image = AmdGpu::Image::Null(false);
        image.base_address = 1;
        std::memcpy(descriptor.data(), &image, sizeof(image));
        if (CtrPm4::ClassifyEffectiveImage(descriptor.data(), 8, 0, 0, &result) || !result.raw_type_valid ||
            result.null_reasons || result.fallback_reason)
            return 250;
        image.data_format = uint32_t(AmdGpu::DataFormat::Format8_8_8_8);
        std::memcpy(descriptor.data(), &image, sizeof(image));
        if (CtrPm4::ClassifyEffectiveImage(descriptor.data(), 8, 1, 0, &result) || !result.raw_type_valid ||
            !(result.null_reasons & 1) || result.fallback_reason != 2)
            return 250;
        const auto saved = result;
        if (CtrPm4::ClassifyEffectiveImage(descriptor.data(), 7, 0, 0, &result) != 60 ||
            std::memcmp(&saved, &result, sizeof(result)) ||
            CtrPm4::ClassifyEffectiveImage(descriptor.data(), 8, 2, 0, &result) != 60 ||
            std::memcmp(&saved, &result, sizeof(result)))
            return 250;
        std::puts(
            "effective image GetSharp public invalid-type and depth-format fallback; valid unchanged; malformed atomic; synthetic only");
    }
    {
        // Authored preflattened IR query exercises the actual public TrackSharp pass.
        CtrPm4::OwnedFetchContext context{nullptr, nullptr, 16};
        for (uint32_t i = 0; i < 8; ++i) context.srtProvenance.push_back({16 + i, 8, 128 + i});
        CtrPm4::ScopedOwnedFetchContext scope(context);
        Shader::Info info{};
        info.flattened_ud_buf.resize(24);
        const auto image = AmdGpu::Image::Null(false);
        std::memcpy(info.flattened_ud_buf.data() + 16, &image, sizeof(image));
        Shader::Pools pools;
        Shader::IR::Block block(pools.inst_pool);
        Shader::Gcn::Block cfg{};
        block.cfg_block = &cfg;
        Shader::IR::IREmitter ir(block);
        const auto lo = ir.GetUserData(static_cast<Shader::IR::ScalarReg>(8));
        const auto hi = ir.GetUserData(static_cast<Shader::IR::ScalarReg>(9));
        const auto source = ir.ReadConst(ir.CompositeConstruct(lo, hi), ir.Imm32(u32{128}));
        source.Inst()->SetFlags<u32>(16);
        Shader::IR::TextureInstInfo texture{};
        texture.pc.Assign(u32{0x120});
        (void)ir.ImageQueryDimension(source, ir.Imm32(u32{0}), ir.Imm1(false), texture);
        Shader::IR::Program program(info);
        program.blocks.push_back(&block);
        struct Trace {
            CtrPm4::ShaderProvenanceRecord first{};
            CtrPm4::ShaderProvenanceRecord span[8]{};
            int status = 99;
        } trace;
        CtrPm4::provenanceObserverContext = &trace;
        CtrPm4::provenanceObserver = [](void* p, const CtrPm4::ShaderProvenanceRecord* r) {
            auto& t = *static_cast<Trace*>(p);
            t.first = *r;
            t.status = CtrPm4::CopyProvenanceSpan(r->flat_index, 8, t.span, 8);
        };
        Shader::Profile profile{};
        Shader::Optimization::ResourceTrackingPass(program, profile);
        CtrPm4::provenanceObserver = nullptr;
        CtrPm4::provenanceObserverContext = nullptr;
        const auto& observed = trace.first;
        if (trace.status) return 249;
        for (uint32_t i = 0; i < 8; ++i)
            if (trace.span[i].flat_index != 16 + i || trace.span[i].root_sgpr != 8 ||
                trace.span[i].source_dword != 128 + i || trace.span[i].source_kind != 1)
                return 249;
        if (CtrPm4::CopyProvenanceSpan(16, 8, trace.span, 8) != 91) return 249;
        if (observed.pc != 0x120 || observed.flat_index != 16 || observed.root_sgpr != 8 ||
            observed.source_dword != 128 || observed.source_kind != 1 ||
            observed.source_opcode != static_cast<u32>(Shader::IR::Opcode::ReadConst) || info.images.size() != 1 ||
            info.images[0].sharp_idx != 16)
            return 248;
        std::puts(
            "actual authored ResourceTracking image query selects ReadConst flat16 root8 source128 pc288; observer metadata only, not retail provenance");
    }
    {
        CtrPm4::Registers flat;
        AmdGpu::Regs regs{};
        regs.primitive_type = AmdGpu::PrimitiveType::RectList;
        regs.depth_render_override.disable_viewport_clamp = 1;
        regs.clipper_control.clip_space = AmdGpu::ClipSpace::MinusWToW;
        std::memcpy(flat.words.data(), regs.reg_array.data(), sizeof(regs));
        CtrPm4::DecodedGraphicsKey key{};
        for (unsigned mask = 0; mask < 256; ++mask) {
            if (CtrPm4::DecodeGraphicsKey(flat, mask, false, false, key) || key.mrt_mask != mask ||
                key.num_color_attachments != std::bit_width(mask) || key.depth_clamp_enable || key.num_samples != 1 ||
                key.depth_samples != 1 || key.prim_type != AmdGpu::PrimitiveType::RectList ||
                key.clip_space != AmdGpu::ClipSpace::MinusWToW)
                return 174;
        }
        regs.color_control.mode = AmdGpu::ColorControl::OperationMode::Normal;
        regs.color_buffers[0].base_address = 1;
        regs.color_buffers[0].info.format = 10;
        regs.blend_control[0].enable = 1;
        for (unsigned mask = 0; mask < 16; ++mask) {
            regs.color_target_mask.SetMask(0, mask);
            std::memcpy(flat.words.data(), regs.reg_array.data(), sizeof(regs));
            if (CtrPm4::DecodeGraphicsKey(flat, 1, false, false, key) ||
                key.write_masks[0] != regs.color_buffers[0].Swizzle().ApplyMask(mask) ||
                bool(key.blend_controls[0].enable) != bool(mask))
                return 174;
        }
        regs.color_buffers[0].info.blend_bypass = 1;
        std::memcpy(flat.words.data(), regs.reg_array.data(), sizeof(regs));
        if (CtrPm4::DecodeGraphicsKey(flat, 1, false, false, key) || key.blend_controls[0].enable) return 174;
        if (CtrPm4::DecodeGraphicsKey(flat, 0, false, false, key) || key.num_color_attachments != 0) return 174;
        key.mrt_mask = 255;
        if (CtrPm4::DecodeGraphicsKey(flat, 256, false, false, key) != 60 || key.mrt_mask != 255) return 174;
        flat.words[offsetof(AmdGpu::Regs, stage_enable) / 4] = AmdGpu::ShaderStageEnable::EsGs;
        if (CtrPm4::DecodeGraphicsKey(flat, 0, false, false, key) != 88 || key.mrt_mask != 255) return 174;
        std::puts(
            "graphics_key_decode=256MRT_nullcolors_raster_depth_invalidmask_atomic_unsupportedstage synthetic=true");
    }
    {
        const std::array<u32, 2> terminated{0xbe802000, 0xffffffff};
        auto parsed = Shader::Gcn::ParseCapturedFetchShader(terminated);
        if (parsed.size != 4 || !parsed.attributes.empty()) return 91;
        unsigned rejected = 0;
        for (auto words : {std::span<const u32>{}, std::span<const u32>{terminated.data() + 1, 1}}) {
            try {
                (void)Shader::Gcn::ParseCapturedFetchShader(words);
            } catch (const std::exception&) {
                ++rejected;
            }
        }
        const std::array<u32, 1> nop{0xbf800000};
        try {
            (void)Shader::Gcn::ParseCapturedFetchShader(nop);
        } catch (const std::exception&) {
            ++rejected;
        }
        if (rejected != 3) return 92;
        std::printf("bounded_fetch_parser=terminator4bytes_trailing_not_decoded_empty_invalid_unterminated_rejected\n");
    }
    {
        struct Owner {
            std::array<u32, 4> words{0xbe802000, 0, 0, 0};
            unsigned captures = 0;
        } owner;
        CtrShaderOwnedRangesV1 ranges{1, sizeof(CtrShaderOwnedRangesV1), &owner,
                                      [](void*, uint64_t address, uint64_t cap, CtrShaderRangeLeaseV1* out) -> int {
                                          if (address != 0x1000) return 88;
                                          *out = {address, std::min<uint64_t>(cap, 16), 1};
                                          return 0;
                                      },
                                      [](void*, const CtrShaderRangeLeaseV1*) -> int { return 0; }};
        CtrShaderInputsV1 inputs{1, sizeof(CtrShaderInputsV1), &owner,
                                 [](void* p, uint64_t address, uint64_t bytes, CtrShaderSnapshot* out) -> int {
                                     auto& o = *static_cast<Owner*>(p);
                                     ++o.captures;
                                     *out = {o.words.data(), bytes, address, 2};
                                     return 0;
                                 },
                                 [](void*, const CtrShaderSnapshot*) -> int { return 0; }};
        CtrPm4::OwnedFetchContext context{&inputs, &ranges, 2};
        Shader::Info fetch{};
        std::array<u32, 2> ud{0x1000, 0};
        fetch.user_data = ud;
        fetch.has_fetch_shader = true;
        {
            const std::array<CtrPm4::SrtLeafRoot, 1> roots{{{0, {0, 3}}}};
            std::vector<u32> flattened;
            if (CtrPm4::CaptureSrtLeafRoots(context, ud, roots, flattened) || flattened.size() != 18 ||
                flattened[0] != 0x1000 || flattened[16] != 0xbe802000 || flattened[17] != 0)
                return 112;
            const auto saved = flattened;
            for (auto offsets : {std::vector<u32>{3, 0}, std::vector<u32>{0, 0}, std::vector<u32>{UINT32_MAX}}) {
                const std::array<CtrPm4::SrtLeafRoot, 1> bad{{{0, offsets}}};
                if (CtrPm4::CaptureSrtLeafRoots(context, ud, bad, flattened) != 88 || flattened != saved) return 113;
            }
            std::printf(
                "owned_srt_leaf=sorted_sparse_compaction_after16UD_exactcopy_3atomic_rejections_not_integrated\n");
            owner.captures = 0;
        }
        {
            CtrPm4::ScopedOwnedFetchContext scope(context);
            Shader::Info leafInfo{};
            leafInfo.user_data = ud;
            Shader::Pools leafPools;
            Shader::IR::Block leafBlock(leafPools.inst_pool);
            Shader::IR::IREmitter leafIR(leafBlock);
            const auto leafLo = leafIR.GetUserData(static_cast<Shader::IR::ScalarReg>(0));
            const auto leafHi = leafIR.GetUserData(static_cast<Shader::IR::ScalarReg>(1));
            (void)leafIR.ReadConst(leafIR.CompositeConstruct(leafLo, leafHi), leafIR.Imm32(u32{0}));
            Shader::IR::Program leafProgram(leafInfo);
            leafProgram.post_order_blocks.push_back(&leafBlock);
            context.allowSrtLeaf = true;
            Shader::Optimization::FlattenExtendedUserdataPass(leafProgram);
            if (leafInfo.flattened_ud_buf.size() != 17 || leafInfo.flattened_ud_buf[16] != 0xbe802000 ||
                leafInfo.srt_info.walker_func || leafInfo.srt_info.flattened_bufsize_dw != 17 ||
                context.srtProvenance.size() != 1 || context.srtProvenance[0].flat_index != 16 ||
                context.srtProvenance[0].root_sgpr != 0 || context.srtProvenance[0].source_dword != 0)
                return 114;
            CtrPm4::ShaderProvenanceRecord observed{};
            CtrPm4::provenanceObserverContext = &observed;
            CtrPm4::provenanceObserver = [](void* p, const CtrPm4::ShaderProvenanceRecord* r) {
                *static_cast<CtrPm4::ShaderProvenanceRecord*>(p) = *r;
            };
            CtrPm4::ObserveImageSharp(48, 16, 123, false);
            if (observed.pc != 48 || observed.flat_index != 16 || observed.root_sgpr != 0 ||
                observed.source_dword != 0 || observed.source_kind != 1)
                return 247;
            CtrPm4::ObserveImageSharp(52, 8, 124, true);
            if (observed.root_sgpr != 8 || observed.source_kind != 0) return 247;
            CtrPm4::ObserveImageSharp(56, 999, 125, false);
            if (observed.root_sgpr != UINT32_MAX || observed.source_kind != 2) return 247;
            CtrPm4::provenanceObserver = nullptr;
            CtrPm4::provenanceObserverContext = nullptr;
            context.allowSrtLeaf = false;
            owner.captures = 0;
            std::printf("owned_srt_IR_leaf=flags_flattened17_no_walker_explicit_control_only\n");
            auto parsed = Shader::Gcn::ParseFetchShader(fetch);
            if (!parsed || parsed->size != 4 || owner.captures != 1) return 93;
            owner.words[0] = 0;
            if (Shader::Gcn::ParseFetchShader(fetch)->size != 4 || owner.captures != 1) return 94;
            bool nested = false;
            try {
                CtrPm4::ScopedOwnedFetchContext second(context);
            } catch (const std::exception&) {
                nested = true;
            }
            if (!nested) return 95;
            AmdGpu::Buffer expected{};
            expected.base_address = 0x2000;
            expected.stride = 16;
            expected.num_records = 4;
            memcpy(owner.words.data(), &expected, 16);
            Shader::Gcn::VertexAttribute attribute{};
            attribute.inst_offset = 7;
            auto buffer = CtrPm4::CaptureFetchAttribute(fetch, attribute);
            if (buffer.base_address != 0x2007 || buffer.stride != 16 || buffer.num_records != 4) return 98;
            attribute.data_format = 1;
            attribute.num_format = 2;
            buffer = CtrPm4::CaptureFetchAttribute(fetch, attribute);
            if (uint32_t(buffer.data_format) != 1 || uint32_t(buffer.num_format) != 2) return 99;
            expected.base_address = 0xffffffffffULL;
            memcpy(owner.words.data(), &expected, 16);
            bool overflow = false;
            try {
                (void)CtrPm4::CaptureFetchAttribute(fetch, attribute);
            } catch (const std::exception&) {
                overflow = true;
            }
            if (!overflow) return 100;
            std::printf("owned_fetch_attribute=exact_fields_offset_typed_override_overflow_rejected\n");
            ud[0] = 0x2000;
            bool changed = false;
            try {
                (void)Shader::Gcn::ParseFetchShader(fetch);
            } catch (const std::exception&) {
                changed = true;
            }
            if (!changed) return 96;
        }
        if (CtrPm4::activeFetchContext) return 97;
        std::printf(
            "owned_fetch_context=captured_parse_cache_immutable_nested_rejected_identity_rejected_scope_cleared\n");
    }
    Shader::Info info{};
    Shader::RuntimeInfo runtime{};
    Shader::Profile profile{};
    Shader::Pools pools;
    bool empty = false;
    try {
        (void)Shader::TranslateProgram({}, pools, info, runtime, profile);
    } catch (const std::invalid_argument& e) {
        empty = std::string(e.what()) == "Empty shader program";
    }
    if (!empty) return 1;
    bool serialized = false;
    try {
        (void)Shader::RegisterWalkerCode(nullptr, 0);
    } catch (const std::runtime_error& e) {
        serialized = std::string(e.what()).find("Serialized SRT walker execution missing") != std::string::npos;
    }
    if (!serialized) return 2;
    std::array<u32, 16> userdata{};
    for (u32 i = 0; i < 16; ++i) userdata[i] = 0x13570000U + i;
    info.user_data = userdata;
    Shader::IR::Program program(info);
    Shader::Optimization::FlattenExtendedUserdataPass(program);
    if (info.srt_info.walker_func || info.flattened_ud_buf.size() != 16) return 3;
    for (u32 i = 0; i < 16; ++i)
        if (info.flattened_ud_buf[i] != userdata[i]) return 4;
    Shader::IR::Block block(pools.inst_pool);
    Shader::IR::IREmitter ir(block);
    const auto lo = ir.GetUserData(static_cast<Shader::IR::ScalarReg>(0));
    const auto hi = ir.GetUserData(static_cast<Shader::IR::ScalarReg>(1));
    const auto pointer = ir.CompositeConstruct(lo, hi);
    (void)ir.ReadConst(pointer, ir.Imm32(u32{0}));
    Shader::IR::Program rooted(info);
    rooted.post_order_blocks.push_back(&block);
    const auto before = info.flattened_ud_buf;
    bool missing = false;
    try {
        Shader::Optimization::FlattenExtendedUserdataPass(rooted);
    } catch (const std::runtime_error& e) {
        missing =
            std::string(e.what()) == "SRT descriptor capture traversal missing: shader has extended userdata roots";
        const auto* shape = dynamic_cast<const CtrPm4::SrtMissingDiagnostic*>(&e);
        if (!shape || shape->roots.size() != 1 || shape->roots[0].sgpr != 0 ||
            shape->roots[0].offsets != std::vector<uint32_t>{0} || shape->pointer_nodes != 1)
            return 111;
        std::printf("srt_missing_shape=1root_sgpr0_offset0_pointer1_no_guest_dereference\n");
    }
    if (!missing || info.srt_info.walker_func || info.flattened_ud_buf != before) return 5;
    // Authored synthetic compute program: standard prefix followed by ENDPGM.
    // No retail register state or resource descriptor is inferred by this fixture.
    CtrVideo::InitializeShaderSettings(CtrVideo::ShaderSettings(false, false));
    Shader::Info compute_info{};
    compute_info.stage = Shader::Stage::Compute;
    compute_info.l_stage = Shader::LogicalStage::Compute;
    compute_info.user_data = userdata;
    CtrPm4::ComputeShaderState captured;
    captured.userRegisters = 16;
    captured.allocatedVgprs = 12;
    captured.sharedBytes = 512;
    captured.workgroup = {1, 1, 1};
    captured.partial = {17, 18, 19};
    captured.tgid = {true, false, true};
    auto compute_runtime = CtrPm4::BuildComputeRuntimeInfo(captured);
    if (compute_runtime.num_user_data != 16 || compute_runtime.num_allocated_vgprs != 12 ||
        compute_runtime.cs_info.shared_memory_size != 512 ||
        compute_runtime.cs_info.workgroup_size != captured.workgroup ||
        compute_runtime.cs_info.tgid_enable != captured.tgid)
        return 6;
    std::puts(
        "compute captured metadata to actual compiler RuntimeInfo passed partial_not_substituted=true synthetic_only=true");
    Shader::Pools compute_pools;
    const std::array<u32, 3> code{0xBEEB03FFU, 0U, 0xBF810000U};
    auto translated = Shader::TranslateProgram(code, compute_pools, compute_info, compute_runtime, profile);
    Shader::Backend::Bindings bindings{};
    auto spirv = Shader::Backend::SPIRV::EmitSPIRV(profile, compute_runtime, translated, bindings);
    if (translated.blocks.empty() || spirv.size() < 5 || spirv[0] != 0x07230203U) return 6;
    // Walk every instruction boundary and require a compute entry point and return.
    bool compute_entry = false, returned = false;
    for (size_t pos = 5; pos < spirv.size();) {
        const u32 count = spirv[pos] >> 16;
        const u32 opcode = spirv[pos] & 0xffff;
        if (!count || count > spirv.size() - pos) return 7;
        if (opcode == 15 && count >= 4 && spirv[pos + 1] == 5) compute_entry = true;
        if (opcode == 253 && count == 1) returned = true;
        pos += count;
    }
    if (!compute_entry || !returned) return 8;
    // Authored minimal vertex/fragment fixture; zero stage context explicitly
    // synthetic, never a substitute for captured retail context.
    {
        CtrPm4::Registers flat;
        AmdGpu::Regs regs{};
        regs.vgt_instance_step_rate_0 = 7;
        regs.vgt_instance_step_rate_1 = 13;
        regs.clipper_control.user_clip_plane_enable = 5;
        regs.clipper_control.clip_space = AmdGpu::ClipSpace::MinusWToW;
        regs.primitive_type = AmdGpu::PrimitiveType::RectList;
        regs.vs_output_control.vs_out_misc_enable = 1;
        regs.vs_output_control.use_vtx_point_size = 1;
        regs.vs_output_control.vs_out_ccdist0_enable = 1;
        regs.vs_output_control.vs_out_ccdist1_enable = 1;
        std::memcpy(flat.words.data(), regs.reg_array.data(), sizeof(regs));
        Shader::RuntimeInfo vertexInfo{};
        if (CtrPm4::BuildVertexRuntimeInfo(flat, false, vertexInfo) || vertexInfo.vs_info.step_rate_0 != 7 ||
            vertexInfo.vs_info.step_rate_1 != 13 || vertexInfo.vs_info.user_clip_plane_mask != 5 ||
            vertexInfo.vs_info.num_outputs != 3 || vertexInfo.vs_info.outputs[0][0] != Shader::Output::PointSize ||
            !vertexInfo.vs_info.emulate_depth_negative_one_to_one || !vertexInfo.vs_info.tess_emulated_primitive ||
            !vertexInfo.vs_info.clip_disable)
            return 29;
        if (CtrPm4::BuildVertexRuntimeInfo(flat, true, vertexInfo) ||
            vertexInfo.vs_info.emulate_depth_negative_one_to_one)
            return 29;
        for (unsigned mask = 0; mask < 256; ++mask) {
            regs.vs_output_control.clip_distance_enable = mask;
            regs.vs_output_control.cull_distance_enable = (~mask) & 255;
            std::memcpy(flat.words.data(), regs.reg_array.data(), sizeof(regs));
            if (CtrPm4::BuildVertexRuntimeInfo(flat, false, vertexInfo)) return 29;
            for (unsigned i = 0; i < 8; ++i) {
                const auto expected = static_cast<Shader::Output>(
                    unsigned((mask & (1u << i)) ? Shader::Output::ClipDist0 : Shader::Output::CullDist0) + i);
                if (vertexInfo.vs_info.outputs[1 + i / 4][i % 4] != expected) return 29;
            }
        }
        flat.words[offsetof(AmdGpu::Regs, stage_enable) / 4] = AmdGpu::ShaderStageEnable::EsGs;
        vertexInfo.num_user_data = 15;
        if (CtrPm4::BuildVertexRuntimeInfo(flat, false, vertexInfo) != 88 || vertexInfo.num_user_data != 15) return 29;
        std::puts(
            "vertex runtime context: fullflat copy, three output maps, rates, clip/rect, actual capability switch, unsupported routing atomic88 passed synthetic=true");
    }
    {
        CtrPm4::Registers flat;
        AmdGpu::Regs regs{};
        regs.num_interp = 32;
        regs.ps_inputs[31].input_offset = 17;
        regs.ps_inputs[31].use_default = 1;
        regs.ps_inputs[31].flat_shade = 1;
        regs.ps_inputs[31].default_value = 3;
        regs.depth_shader_control.z_export_enable = 1;
        regs.depth_shader_control.stencil_test_val_export_enable = 1;
        regs.depth_shader_control.mask_export_enable = 1;
        regs.depth_shader_control.coverage_to_mask_enable = 1;
        regs.blend_control[0].enable = 1;
        regs.blend_control[0].separate_alpha_blend = 1;
        regs.blend_control[0].alpha_src_factor = AmdGpu::BlendControl::BlendFactor::Src1Alpha;
        regs.clipper_control.user_clip_plane_enable = 1;
        regs.color_control.mode = AmdGpu::ColorControl::OperationMode::Normal;
        regs.color_buffers[0].base_address = 1;
        regs.color_buffers[0].info.format = 10;
        regs.color_target_mask.SetMask(0, 15);
        std::memcpy(flat.words.data(), regs.reg_array.data(), sizeof(regs));
        Shader::Profile psProfile{};
        psProfile.needs_clip_distance_emulation = true;
        Shader::RuntimeInfo ps{};
        if (CtrPm4::BuildFragmentRuntimeInfo(flat, psProfile, ps) || ps.fs_info.num_inputs != 32 ||
            ps.fs_info.inputs[31].param_index != 17 || !ps.fs_info.inputs[31].is_flat ||
            ps.fs_info.inputs[31].default_value != 3 || ps.fs_info.mrtz_mask != 15 ||
            !ps.fs_info.dual_source_blending || !ps.fs_info.clip_distance_emulation ||
            ps.fs_info.color_buffers[0].data_format != AmdGpu::DataFormat::Format8_8_8_8)
            return 30;
        regs.color_target_mask.SetMask(0, 0);
        regs.blend_control[0].enable = 0;
        std::memcpy(flat.words.data(), regs.reg_array.data(), sizeof(regs));
        if (CtrPm4::BuildFragmentRuntimeInfo(flat, psProfile, ps) || ps.fs_info.dual_source_blending ||
            ps.fs_info.color_buffers[0].data_format != AmdGpu::DataFormat::FormatInvalid)
            return 30;
        regs.num_interp = 33;
        std::memcpy(flat.words.data(), regs.reg_array.data(), sizeof(regs));
        ps.num_user_data = 15;
        if (CtrPm4::BuildFragmentRuntimeInfo(flat, psProfile, ps) != 88 || ps.num_user_data != 15) return 30;
        std::puts(
            "fragment fullcontext input32/mrtz15/dualalpha/clip/color-format-and-mask passed invalid33atomic88 synthetic=true noattachmentadmission=true");
    }
    {
        Shader::Info resources{};
        resources.readconst_types = Shader::Info::ReadConstType::Dynamic;
        resources.ud_mask.mask = 0x1234;
        resources.has_fetch_shader = true;
        resources.fetch_shader_sgpr_base = 6;
        resources.flattened_ud_buf.resize(20);
        resources.mrt_mask = 5;
        resources.loads.flags[0] = 15;
        resources.stores.flags[0] = 3;
        for (unsigned i = 0; i < Shader::NUM_BUFFERS; ++i) {
            Shader::BufferResource b{};
            b.sharp_idx = 100 + i;
            b.buffer_type = Shader::BufferType::Guest;
            b.is_written = true;
            b.is_formatted = true;
            resources.buffers.push_back(b);
        }
        for (unsigned i = 0; i < Shader::NUM_IMAGES; ++i) {
            Shader::ImageResource v{};
            v.sharp_idx = 200 + i;
            v.is_written = true;
            v.is_depth = true;
            v.is_atomic = true;
            v.is_array = true;
            v.is_r128 = true;
            resources.images.push_back(v);
        }
        for (unsigned i = 0; i < Shader::NUM_SAMPLERS; ++i) {
            Shader::SamplerResource v{};
            v.sharp_idx = 300 + i;
            v.is_inline_sampler = 1;
            v.associated_image = i;
            resources.samplers.push_back(v);
        }
        for (unsigned i = 0; i < Shader::NUM_FMASKS; ++i) {
            Shader::FMaskResource v{};
            v.sharp_idx = 400 + i;
            resources.fmasks.push_back(v);
        }
        const auto meta = CtrPm4::CaptureCompilerResourceMetadata(resources);
        if (meta.binding_count != 128 || meta.buffers != 40 || meta.images != 64 || meta.samplers != 16 ||
            meta.fmasks != 8 || meta.readconst_types != 2 || meta.userdata_mask != 0x1234 ||
            meta.flattened_userdata_words != 20 || !meta.has_fetch_shader || meta.fetch_shader_sgpr_base != 6 ||
            meta.mrt_mask != 5 || meta.loaded_attribute_components != 4 || meta.stored_attribute_components != 2 ||
            meta.bindings[0].flags != 5 || meta.bindings[40].flags != 121 || meta.bindings[104].flags != 2 ||
            meta.bindings[119].associated_image != 15 || meta.bindings[127].sharp_index != 407)
            return 31;
        for (unsigned i = 0; i < 128; ++i) {
            const auto& b = meta.bindings[i];
            const unsigned kind = i < 40 ? 0 : i < 104 ? 1 : i < 120 ? 2 : 3;
            const unsigned index = i < 40 ? 100 + i : i < 104 ? 200 + i - 40 : i < 120 ? 300 + i - 104 : 400 + i - 120;
            if (b.kind != kind || b.sharp_index != index) return 31;
        }
        std::puts(
            "compiler resource metadata all128identities/readconst/userdata/fetch/attributes passed no raw descriptors or assumed pointers=true synthetic=true");
    }
    {
        Shader::Info fetch{};
        std::array<u32, 16> malicious{};
        malicious[0] = 1;
        fetch.user_data = malicious;
        if (Shader::Gcn::ParseFetchShader(fetch)) return 32;
        fetch.has_fetch_shader = true;
        fetch.fetch_shader_sgpr_base = 0;
        bool missing = false;
        try {
            (void)Shader::Gcn::ParseFetchShader(fetch);
        } catch (const std::runtime_error& e) {
            missing = std::string(e.what()).find("Owned bounded fetch shader snapshot missing") != std::string::npos;
        }
        if (!missing) return 32;
        std::puts(
            "nested fetch authority failclosed passed nofetch nullopt, fetch sentinel guestpointer1 rejected before rawread/UINT32_MAX parser, attribute GetSharp unreachable; bounded owned implementation missing=true");
    }
    CtrPm4::OwnedShaderBinary graphicsBinary;
    graphicsBinary.guestAddress = 0x1000;
    graphicsBinary.words.assign(code.begin(), code.end());
    graphicsBinary.metadata.codeBytes = 12;
    CtrPm4::GraphicsShaderState graphicsState;
    graphicsState.codeAddress = 0x1000;
    graphicsState.allocatedVgprs = 4;
    for (bool vertex : {true, false}) {
        Shader::RuntimeInfo graphicsRuntime{};
        graphicsRuntime.Initialize(vertex ? Shader::Stage::Vertex : Shader::Stage::Fragment);
        std::vector<uint32_t> graphicsSpirv;
        std::string diagnostic;
        if (CtrPm4::CompileGraphicsDiagnostic(graphicsBinary, graphicsState, vertex, graphicsRuntime, profile,
                                              graphicsSpirv, diagnostic) ||
            graphicsSpirv.size() < 5)
            return 28;
        const auto preserved = graphicsSpirv;
        graphicsRuntime.Initialize(Shader::Stage::Compute);
        if (CtrPm4::CompileGraphicsDiagnostic(graphicsBinary, graphicsState, vertex, graphicsRuntime, profile,
                                              graphicsSpirv, diagnostic) != 60 ||
            graphicsSpirv != preserved || diagnostic.empty())
            return 28;
    }
    {
        // A nonzero seed must survive independent stages and failures atomically.
        Shader::Backend::Bindings shared{7, 3, 5};
        for (bool vertex : {false, true}) {
            Shader::RuntimeInfo runtime{};
            runtime.Initialize(vertex ? Shader::Stage::Vertex : Shader::Stage::Fragment);
            std::vector<uint32_t> result;
            std::string reason;
            CtrPm4::GraphicsBindingMetadata contract;
            if (CtrPm4::CompileGraphicsDiagnostic(graphicsBinary, graphicsState, vertex, runtime, profile, result,
                                                  reason, nullptr, nullptr, &shared, &contract) ||
                contract.before.unified != 7 || contract.before.buffer != 3 || contract.before.user_data != 5 ||
                contract.after != shared)
                return 33;
            std::printf("shared binding stage=%u before=%u/%u/%u after=%u/%u/%u descriptors=%zu\n", unsigned(vertex),
                        contract.before.unified, contract.before.buffer, contract.before.user_data, shared.unified,
                        shared.buffer, shared.user_data, contract.descriptors.size());
            if (vertex && (shared.unified != 8 || shared.buffer != 4 || contract.descriptors.size() != 1 ||
                           contract.descriptors[0].binding != 7 || contract.descriptors[0].bufferSlot != 3))
                return 33;
            const auto preserved = result;
            const auto saved = shared;
            runtime.Initialize(Shader::Stage::Compute);
            if (CtrPm4::CompileGraphicsDiagnostic(graphicsBinary, graphicsState, vertex, runtime, profile, result,
                                                  reason, nullptr, nullptr, &shared, &contract) != 60 ||
                shared != saved || result != preserved)
                return 33;
        }
        {
            Shader::RuntimeInfo runtime{};
            runtime.Initialize(Shader::Stage::Vertex);
            auto invalid = shared;
            invalid.user_data = 17;
            const auto saved = invalid;
            std::vector<uint32_t> result{0x1234};
            std::string reason;
            if (CtrPm4::CompileGraphicsDiagnostic(graphicsBinary, graphicsState, true, runtime, profile, result, reason,
                                                  nullptr, nullptr, &invalid) != 88 ||
                invalid != saved || result != std::vector<uint32_t>{0x1234})
                return 33;
        }
        Shader::Info info{};
        std::array<u32, 16> words{};
        words[1] = 0x12345678;
        words[9] = 0x89abcdef;
        info.user_data = words;
        info.ud_mask.mask = (1u << 1) | (1u << 9);
        Shader::BufferResource buffer{};
        buffer.inline_cbuf.base_address = 1;
        buffer.inline_cbuf.num_records = 64;
        buffer.buffer_type = Shader::BufferType::Guest;
        info.buffers.push_back(buffer);
        info.samplers.push_back({});
        info.fmasks.push_back({});
        const auto exact = CtrPm4::CaptureGraphicsBindingMetadata(info, {7, 3, 5}, {9, 4, 7});
        if (exact.descriptors.size() != 2 || exact.descriptors[0].binding != 7 ||
            exact.descriptors[0].bufferSlot != 3 || !exact.descriptors[0].storage ||
            exact.descriptors[1].binding != 8 || exact.userdataCount != 2 || exact.packedUserdata[0] != words[1] ||
            exact.packedUserdata[1] != words[9])
            return 33;
        std::puts(
            "shared graphics binding nonzero seeds VS/PS, failedstage atomic, storage-readonly buffer, compact userdata and fmask exclusion passed; synthetic=true GPUbinding=false");
    }
    std::puts(
        "synchronous graphics compiler diagnostics passed authored_VS_PS=true actual_translate_SPIRV=true stage_mismatch_atomic60=true synthetic_context_only=true no_GPU_draw=true");
    Cleanup cleanup;
    const auto loader = LoadLibraryW(L"vulkan-1.dll");
    if (!loader) return 9;
    cleanup.actions.push_back([=] { FreeLibrary(loader); });
    const auto get = reinterpret_cast<PFN_vkGetInstanceProcAddr>(GetProcAddress(loader, "vkGetInstanceProcAddr"));
    if (!get) return 10;
    const auto create_instance = reinterpret_cast<PFN_vkCreateInstance>(get(nullptr, "vkCreateInstance"));
    VkInstanceCreateInfo ici{VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO};
    VkInstance instance{};
    if (!create_instance || create_instance(&ici, nullptr, &instance) != VK_SUCCESS) return 11;
    const auto enumerate =
        reinterpret_cast<PFN_vkEnumeratePhysicalDevices>(get(instance, "vkEnumeratePhysicalDevices"));
    const auto families = reinterpret_cast<PFN_vkGetPhysicalDeviceQueueFamilyProperties>(
        get(instance, "vkGetPhysicalDeviceQueueFamilyProperties"));
    const auto create_device = reinterpret_cast<PFN_vkCreateDevice>(get(instance, "vkCreateDevice"));
    const auto destroy_instance = reinterpret_cast<PFN_vkDestroyInstance>(get(instance, "vkDestroyInstance"));
    cleanup.actions.push_back([=] { destroy_instance(instance, nullptr); });
    u32 device_count = 0;
    if (enumerate(instance, &device_count, nullptr) != VK_SUCCESS || !device_count) return 12;
    std::vector<VkPhysicalDevice> devices(device_count);
    if (enumerate(instance, &device_count, devices.data()) != VK_SUCCESS) return 13;
    VkPhysicalDevice physical{};
    u32 family = 0;
    for (auto candidate : devices) {
        u32 count = 0;
        families(candidate, &count, nullptr);
        std::vector<VkQueueFamilyProperties> properties(count);
        families(candidate, &count, properties.data());
        for (u32 i = 0; i < count; ++i) {
            if (properties[i].queueCount && (properties[i].queueFlags & VK_QUEUE_COMPUTE_BIT)) {
                physical = candidate;
                family = i;
                break;
            }
        }
        if (physical) break;
    }
    if (!physical) return 14;
    {
        const auto query = reinterpret_cast<PFN_vkGetPhysicalDeviceFormatProperties>(
            get(instance, "vkGetPhysicalDeviceFormatProperties"));
        CtrGraphicsColorRequirementV1 requirement{};
        requirement.version = 1;
        requirement.size = sizeof(requirement);
        requirement.data_format = 10;
        VkFormat format = VK_FORMAT_UNDEFINED;
        for (uint32_t numeric : {0u, 1u, 4u, 5u, 9u}) {
            requirement.number_format = numeric;
            if (CtrPm4::PreflightColorFormat(requirement, physical, query, false, format)) return 101;
        }
        requirement.number_format = 9;
        if (CtrPm4::PreflightColorFormat(requirement, physical, query, true, format) ||
            format != VK_FORMAT_R8G8B8A8_SRGB)
            return 102;
        const auto noFeatures = +[](VkPhysicalDevice, VkFormat, VkFormatProperties* p) { *p = {}; };
        format = VK_FORMAT_UNDEFINED;
        if (CtrPm4::PreflightColorFormat(requirement, physical, noFeatures, false, format) != 88 ||
            format != VK_FORMAT_UNDEFINED)
            return 103;
        requirement.number_format = 7;
        if (CtrPm4::PreflightColorFormat(requirement, physical, query, false, format) != 88 ||
            format != VK_FORMAT_UNDEFINED)
            return 104;
        if (CtrPm4::PreflightColorFormat(requirement, physical, nullptr, false, format) != 88) return 105;
        std::printf("color_format_preflight=5_actual_formats_srgb_blend_missing_features_numeric_query_atomic88\n");
        requirement.number_format = 0;
        requirement.pitch = 128;
        requirement.height = 64;
        requirement.layers = 1;
        requirement.samples = 1;
        const auto imageQuery = reinterpret_cast<PFN_vkGetPhysicalDeviceImageFormatProperties>(
            get(instance, "vkGetPhysicalDeviceImageFormatProperties"));
        VkImageCreateInfo image{VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};
        if (CtrPm4::PreflightColorImage(requirement, physical, query, imageQuery, false, image) ||
            image.extent.width != 128 || image.extent.height != 64 || image.format != VK_FORMAT_R8G8B8A8_UNORM)
            return 106;
        const auto savedImage = image;
        requirement.pitch = UINT32_MAX;
        if (CtrPm4::PreflightColorImage(requirement, physical, query, imageQuery, false, image) != 88 ||
            memcmp(&image, &savedImage, sizeof(image)))
            return 107;
        requirement.pitch = 128;
        requirement.samples = 3;
        if (CtrPm4::PreflightColorImage(requirement, physical, query, imageQuery, false, image) != 60 ||
            memcmp(&image, &savedImage, sizeof(image)))
            return 108;
        requirement.samples = 1;
        if (CtrPm4::PreflightColorImage(requirement, physical, query, nullptr, false, image) != 88 ||
            memcmp(&image, &savedImage, sizeof(image)))
            return 109;
        requirement.layers = UINT32_MAX;
        if (CtrPm4::PreflightColorImage(requirement, physical, query, imageQuery, false, image) != 88) return 110;
        std::printf(
            "color_image_preflight=actual128x64_createinfo_extent_layers_samples_query_atomic_rejections_no_image_created\n");
    }
    const auto query_properties =
        reinterpret_cast<PFN_vkGetPhysicalDeviceProperties>(get(instance, "vkGetPhysicalDeviceProperties"));
    VkPhysicalDeviceProperties properties{};
    query_properties(physical, &properties);
    CtrPm4::DispatchDirect dimensions;
    dimensions.x = 1;
    dimensions.y = 1;
    dimensions.z = 1;
    {
        CtrGraphicsSamplerParametersV1 p{};
        p.version = 1;
        p.size = sizeof(p);
        p.clamp_x = 2;
        p.clamp_y = 2;
        p.max_lod = 15.99609375f;
        p.max_anisotropy = 4;
        VkSamplerCreateInfo ci{};
        if (CtrPm4::PreflightSampler(p, properties.limits, false, ci) ||
            ci.addressModeU != VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE || ci.anisotropyEnable ||
            ci.magFilter != VK_FILTER_NEAREST || ci.maxLod != p.max_lod)
            return 111;
        const auto saved = ci;
        p.clamp_x = 4;
        if (CtrPm4::PreflightSampler(p, properties.limits, false, ci) != 88 || memcmp(&ci, &saved, sizeof(ci)))
            return 112;
        p.clamp_x = 2;
        p.mag_filter = 3;
        p.anisotropic = 1;
        if (CtrPm4::PreflightSampler(p, properties.limits, false, ci) != 88) return 113;
        auto limits = properties.limits;
        limits.maxSamplerAnisotropy = 2;
        if (CtrPm4::PreflightSampler(p, limits, true, ci) || ci.maxAnisotropy != 2 || ci.magFilter != VK_FILTER_LINEAR)
            return 114;
        p.min_lod = 20;
        if (CtrPm4::PreflightSampler(p, limits, true, ci) != 60) return 115;
        std::puts(
            "sampler createinfo physical limits: reached nearest clamp2 mapped; aniso enabled/clamped2, missing feature/halfborder88, invertedLOD60 atomic");
    }
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
        auto fq = reinterpret_cast<PFN_vkGetPhysicalDeviceFormatProperties>(
            get(instance, "vkGetPhysicalDeviceFormatProperties"));
        auto iq = reinterpret_cast<PFN_vkGetPhysicalDeviceImageFormatProperties>(
            get(instance, "vkGetPhysicalDeviceImageFormatProperties"));
        VkImageCreateInfo ci{};
        const uint32_t selectors[] = {6, 5, 4, 7};
        VkComponentMapping components{};
        if (CtrPm4::SampledRgba8Components(selectors, components) || components.r != VK_COMPONENT_SWIZZLE_B ||
            components.g != VK_COMPONENT_SWIZZLE_G || components.b != VK_COMPONENT_SWIZZLE_R ||
            components.a != VK_COMPONENT_SWIZZLE_A)
            return 120;
        const auto savedComponents = components;
        const uint32_t reserved[] = {0, 1, 2, 4};
        if (CtrPm4::SampledRgba8Components(reserved, components) != 88 ||
            memcmp(&components, &savedComponents, sizeof(components)))
            return 121;
        if (CtrPm4::PreflightSampledImage(p, physical, fq, iq, ci) || ci.extent.width != 1920 ||
            ci.usage !=
                (VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT))
            return 116;
        const auto saved = ci;
        p.samples = 2;
        if (CtrPm4::PreflightSampledImage(p, physical, fq, iq, ci) != 88 || memcmp(&ci, &saved, sizeof(ci))) return 117;
        p.samples = 1;
        p.width = UINT32_MAX;
        if (CtrPm4::PreflightSampledImage(p, physical, fq, iq, ci) != 88 || memcmp(&ci, &saved, sizeof(ci))) return 118;
        p.width = 1920;
        if (CtrPm4::PreflightSampledImage(p, physical, nullptr, iq, ci) != 88) return 119;
        std::puts(
            "sampled RGBA8 image selected physical format/usage/extent limits passed1920x1080; msaa/oversize/query missing88 atomic; no image created");
    }
    if (CtrPm4::AdmitComputeLimits(captured, dimensions, properties.limits)) return 26;
    auto boundedLimits = properties.limits;
    for (size_t axis = 0; axis < 3; ++axis) {
        auto bad = captured;
        bad.workgroup[axis] = 0;
        if (CtrPm4::AdmitComputeLimits(bad, dimensions, boundedLimits) != 88) return 26;
        boundedLimits.maxComputeWorkGroupSize[axis] = 0;
        if (CtrPm4::AdmitComputeLimits(captured, dimensions, boundedLimits) != 88) return 26;
        boundedLimits = properties.limits;
        boundedLimits.maxComputeWorkGroupCount[axis] = 0;
        if (CtrPm4::AdmitComputeLimits(captured, dimensions, boundedLimits) != 88) return 26;
        boundedLimits = properties.limits;
    }
    boundedLimits.maxComputeWorkGroupInvocations = 0;
    if (CtrPm4::AdmitComputeLimits(captured, dimensions, boundedLimits) != 88) return 26;
    boundedLimits = properties.limits;
    boundedLimits.maxComputeSharedMemorySize = 511;
    if (CtrPm4::AdmitComputeLimits(captured, dimensions, boundedLimits) != 88) return 26;
    boundedLimits = properties.limits;
    auto huge = captured;
    huge.workgroup = {UINT32_MAX, UINT32_MAX, UINT32_MAX};
    for (size_t axis = 0; axis < 3; ++axis) boundedLimits.maxComputeWorkGroupSize[axis] = UINT32_MAX;
    boundedLimits.maxComputeWorkGroupInvocations = UINT32_MAX;
    if (CtrPm4::AdmitComputeLimits(huge, dimensions, boundedLimits) != 88) return 26;
    dimensions.x = dimensions.y = dimensions.z = 0;
    if (CtrPm4::AdmitComputeLimits(captured, dimensions, properties.limits)) return 26;
    std::puts(
        "compute selected-device limit admission passed real_positive=true zero_groups_allowed=true zero_local_and_axes_rejected=true invocations_LDS_bounded=true overflow_safe=true no_shaderfeature_admission=true");

    // Conservative Vulkan1.0 profile: no optional features or extensions are enabled
    // on this control device. Limits come from the selected physical device.
    Shader::Profile device_profile{};
    device_profile.supported_spirv = 0x00010000;
    device_profile.max_ubo_size = properties.limits.maxUniformBufferRange;
    device_profile.max_viewport_width = properties.limits.maxViewportDimensions[0];
    device_profile.max_viewport_height = properties.limits.maxViewportDimensions[1];
    device_profile.max_shared_memory_size = properties.limits.maxComputeSharedMemorySize;
    if (!device_profile.max_ubo_size || !device_profile.max_shared_memory_size ||
        properties.limits.maxComputeWorkGroupInvocations < 1)
        return 26;
    Shader::Info device_info{};
    device_info.stage = Shader::Stage::Compute;
    device_info.l_stage = Shader::LogicalStage::Compute;
    device_info.user_data = userdata;
    Shader::Pools device_pools;
    auto device_program = Shader::TranslateProgram(code, device_pools, device_info, compute_runtime, device_profile);
    Shader::Backend::Bindings device_bindings{};
    spirv = Shader::Backend::SPIRV::EmitSPIRV(device_profile, compute_runtime, device_program, device_bindings);
    if (spirv.size() < 5 || spirv[0] != 0x07230203U || spirv[1] != 0x00010000U) return 27;
    std::printf("device=%s vendor=%u device_id=%u api=%u ubo=%u shared=%u viewport=%ux%u spirv_words=%zu\n",
                properties.deviceName, properties.vendorID, properties.deviceID, properties.apiVersion,
                properties.limits.maxUniformBufferRange, properties.limits.maxComputeSharedMemorySize,
                properties.limits.maxViewportDimensions[0], properties.limits.maxViewportDimensions[1], spirv.size());
    // Authored GCN: v_mov_b32 v0,literal; buffer_store_dword v0,s[0:3],0 offset4.
    std::array<u32, 16> store_userdata{};
    const auto sharp = AmdGpu::Buffer::Placeholder(64);
    static_assert(sizeof(sharp) == 4 * sizeof(u32));
    if (!sharp.Valid() || sharp.num_records != 64) return 28;
    std::memcpy(store_userdata.data(), &sharp, sizeof(sharp));
    const std::array<u32, 7> store_code{0xBEEB03FFU, 0U,          0x7E0002FFU, 0x13579BDFU,
                                        0xE0700004U, 0x80000000U, 0xBF810000U};
    {
        CtrPm4::OwnedShaderBinary binary;
        binary.guestAddress = 0x1000;
        binary.words.assign(store_code.begin(), store_code.end());
        binary.metadata.codeBytes = 28;
        CtrPm4::GraphicsShaderState state;
        state.codeAddress = 0x1000;
        state.userRegisters = 4;
        state.allocatedVgprs = 4;
        state.userData = store_userdata;
        Shader::Backend::Bindings pairBindings{};
        for (bool vertex : {false, true}) {
            const auto before = pairBindings;
            Shader::RuntimeInfo runtime{};
            runtime.Initialize(vertex ? Shader::Stage::Vertex : Shader::Stage::Fragment);
            std::vector<uint32_t> compiled;
            std::string reason;
            CtrPm4::GraphicsBindingMetadata metadata;
            if (CtrPm4::CompileGraphicsDiagnostic(binary, state, vertex, runtime, device_profile, compiled, reason,
                                                  nullptr, nullptr, &pairBindings, &metadata) ||
                metadata.descriptors.empty() || metadata.before != before || metadata.after != pairBindings ||
                pairBindings.unified <= before.unified || pairBindings.buffer <= before.buffer)
                return 34;
            if (vertex && (!before.unified || !before.buffer)) return 34;
            unsigned decorations = 0;
            for (size_t pos = 5; pos < compiled.size();) {
                const auto count = compiled[pos] >> 16;
                if (!count || count > compiled.size() - pos) return 34;
                if ((compiled[pos] & 65535) == 71 && count == 4 && compiled[pos + 2] == 33) {
                    const auto value = compiled[pos + 3];
                    if (value < before.unified || value >= pairBindings.unified) return 34;
                    ++decorations;
                }
                pos += count;
            }
            if (!decorations) return 34;
        }
        std::puts(
            "actual authored PS-buffer then VS-buffer SPIRV binding decorations use disjoint shared ranges; no decoration rewrite, no GPUdraw, synthetic=true");
    }
    Shader::Info store_info{};
    store_info.stage = Shader::Stage::Compute;
    store_info.l_stage = Shader::LogicalStage::Compute;
    store_info.user_data = store_userdata;
    auto store_runtime = compute_runtime;
    store_runtime.num_user_data = 4;
    Shader::Pools store_pools;
    auto store_program = Shader::TranslateProgram(store_code, store_pools, store_info, store_runtime, device_profile);
    Shader::Backend::Bindings store_bindings{};
    const auto store_spirv =
        Shader::Backend::SPIRV::EmitSPIRV(device_profile, store_runtime, store_program, store_bindings);
    if (store_info.buffers.size() != 1 || !store_info.buffers[0].is_written || store_bindings.unified != 1 ||
        store_bindings.buffer != 1)
        return 29;
    bool storage_write = false;
    for (size_t pos = 5; pos < store_spirv.size();) {
        const auto count = store_spirv[pos] >> 16;
        if (!count || count > store_spirv.size() - pos) return 30;
        if ((store_spirv[pos] & 0xffff) == 62) storage_write = true;
        pos += count;
    }
    if (!storage_write) return 31;
    std::printf("store_gcn_dwords=%zu buffers=%zu bindings=%u store_spirv_words=%zu\n", store_code.size(),
                store_info.buffers.size(), store_bindings.unified, store_spirv.size());
    const float priority = 1.0f;
    VkDeviceQueueCreateInfo qci{VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO};
    qci.queueFamilyIndex = family;
    qci.queueCount = 1;
    qci.pQueuePriorities = &priority;
    VkDeviceCreateInfo dci{VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO};
    dci.queueCreateInfoCount = 1;
    dci.pQueueCreateInfos = &qci;
    VkDevice device{};
    if (create_device(physical, &dci, nullptr, &device) != VK_SUCCESS) return 15;
    const auto get_device = reinterpret_cast<PFN_vkGetDeviceProcAddr>(get(instance, "vkGetDeviceProcAddr"));
    const auto create_module = reinterpret_cast<PFN_vkCreateShaderModule>(get_device(device, "vkCreateShaderModule"));
    const auto destroy_module =
        reinterpret_cast<PFN_vkDestroyShaderModule>(get_device(device, "vkDestroyShaderModule"));
    const auto destroy_device = reinterpret_cast<PFN_vkDestroyDevice>(get_device(device, "vkDestroyDevice"));
    cleanup.actions.push_back([=] { destroy_device(device, nullptr); });
    VkShaderModuleCreateInfo sci{VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO};
    sci.codeSize = store_spirv.size() * sizeof(u32);
    sci.pCode = store_spirv.data();
    VkShaderModule module{};
    const auto module_result = create_module(device, &sci, nullptr, &module);
    if (module) cleanup.actions.push_back([=] { destroy_module(device, module, nullptr); });
    if (module_result != VK_SUCCESS) return 16;
#define DEVICE_PROC(name) const auto name = reinterpret_cast<PFN_##name>(get_device(device, #name))
    DEVICE_PROC(vkCreatePipelineLayout);
    DEVICE_PROC(vkCreateComputePipelines);
    DEVICE_PROC(vkDestroyPipeline);
    DEVICE_PROC(vkDestroyPipelineLayout);
    DEVICE_PROC(vkCreateCommandPool);
    DEVICE_PROC(vkAllocateCommandBuffers);
    DEVICE_PROC(vkBeginCommandBuffer);
    DEVICE_PROC(vkEndCommandBuffer);
    DEVICE_PROC(vkCmdBindPipeline);
    DEVICE_PROC(vkCmdDispatch);
    DEVICE_PROC(vkGetDeviceQueue);
    DEVICE_PROC(vkQueueSubmit);
    DEVICE_PROC(vkCreateFence);
    DEVICE_PROC(vkWaitForFences);
    DEVICE_PROC(vkDestroyFence);
    DEVICE_PROC(vkDestroyCommandPool);
#undef DEVICE_PROC
#define DPROC(name) const auto name = reinterpret_cast<PFN_##name>(get_device(device, #name))
    DPROC(vkCreateBuffer);
    DPROC(vkGetBufferMemoryRequirements);
    DPROC(vkAllocateMemory);
    DPROC(vkBindBufferMemory);
    DPROC(vkMapMemory);
    DPROC(vkUnmapMemory);
    DPROC(vkFreeMemory);
    DPROC(vkDestroyBuffer);
    DPROC(vkCreateDescriptorSetLayout);
    DPROC(vkCreateDescriptorPool);
    DPROC(vkAllocateDescriptorSets);
    DPROC(vkUpdateDescriptorSets);
    DPROC(vkCmdBindDescriptorSets);
    DPROC(vkCmdPipelineBarrier);
    DPROC(vkDestroyDescriptorPool);
    DPROC(vkDestroyDescriptorSetLayout);
#undef DPROC
    VkBufferCreateInfo bci{VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
    bci.size = 64;
    bci.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
    bci.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    VkBuffer buffer{};
    if (vkCreateBuffer(device, &bci, nullptr, &buffer) != VK_SUCCESS) return 32;
    cleanup.actions.push_back([=] { vkDestroyBuffer(device, buffer, nullptr); });
    VkMemoryRequirements requirements{};
    vkGetBufferMemoryRequirements(device, buffer, &requirements);
    const auto memory_properties =
        reinterpret_cast<PFN_vkGetPhysicalDeviceMemoryProperties>(get(instance, "vkGetPhysicalDeviceMemoryProperties"));
    VkPhysicalDeviceMemoryProperties memory{};
    memory_properties(physical, &memory);
    u32 type = memory.memoryTypeCount;
    for (u32 i = 0; i < memory.memoryTypeCount; ++i)
        if ((requirements.memoryTypeBits & (1u << i)) &&
            (memory.memoryTypes[i].propertyFlags &
             (VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT)) ==
                (VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT)) {
            type = i;
            break;
        }
    if (type == memory.memoryTypeCount) return 33; // Noncoherent support explicitly missing.
    VkMemoryAllocateInfo mai{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
    mai.allocationSize = requirements.size;
    mai.memoryTypeIndex = type;
    VkDeviceMemory allocation{};
    if (vkAllocateMemory(device, &mai, nullptr, &allocation) != VK_SUCCESS) return 34;
    cleanup.actions.insert(cleanup.actions.end() - 1, [=] { vkFreeMemory(device, allocation, nullptr); });
    if (vkBindBufferMemory(device, buffer, allocation, 0) != VK_SUCCESS) return 35;
    void* mapped{};
    if (vkMapMemory(device, allocation, 0, requirements.size, 0, &mapped) != VK_SUCCESS) return 36;
    cleanup.actions.push_back([=] { vkUnmapMemory(device, allocation); });
    if (GetEnvironmentVariableW(L"CTR_SHADER_CONTROL_FAIL_AFTER_MAP", nullptr, 0)) return 41;
    auto* words = static_cast<u32*>(mapped);
    for (size_t i = 0; i < requirements.size / 4; ++i) words[i] = 0xA5000000U + static_cast<u32>(i);
    VkDescriptorSetLayoutBinding binding{};
    binding.binding = 0;
    binding.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    binding.descriptorCount = 1;
    binding.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
    VkDescriptorSetLayoutCreateInfo dlci{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO};
    dlci.bindingCount = 1;
    dlci.pBindings = &binding;
    VkDescriptorSetLayout descriptor_layout{};
    if (vkCreateDescriptorSetLayout(device, &dlci, nullptr, &descriptor_layout) != VK_SUCCESS) return 37;
    cleanup.actions.push_back([=] { vkDestroyDescriptorSetLayout(device, descriptor_layout, nullptr); });
    VkDescriptorPoolSize pool_size{VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1};
    VkDescriptorPoolCreateInfo dpci{VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO};
    dpci.maxSets = 1;
    dpci.poolSizeCount = 1;
    dpci.pPoolSizes = &pool_size;
    VkDescriptorPool descriptor_pool{};
    if (vkCreateDescriptorPool(device, &dpci, nullptr, &descriptor_pool) != VK_SUCCESS) return 38;
    cleanup.actions.push_back([=] { vkDestroyDescriptorPool(device, descriptor_pool, nullptr); });
    VkDescriptorSetAllocateInfo dsai{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO};
    dsai.descriptorPool = descriptor_pool;
    dsai.descriptorSetCount = 1;
    dsai.pSetLayouts = &descriptor_layout;
    VkDescriptorSet descriptor{};
    if (vkAllocateDescriptorSets(device, &dsai, &descriptor) != VK_SUCCESS) return 39;
    VkDescriptorBufferInfo dbi{buffer, 0, 64};
    VkWriteDescriptorSet write{VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
    write.dstSet = descriptor;
    write.descriptorCount = 1;
    write.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    write.pBufferInfo = &dbi;
    vkUpdateDescriptorSets(device, 1, &write, 0, nullptr);
    VkPipelineLayoutCreateInfo lci{VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
    lci.setLayoutCount = 1;
    lci.pSetLayouts = &descriptor_layout;
    VkPipelineLayout layout{};
    if (vkCreatePipelineLayout(device, &lci, nullptr, &layout) != VK_SUCCESS) return 17;
    cleanup.actions.push_back([=] { vkDestroyPipelineLayout(device, layout, nullptr); });
    VkComputePipelineCreateInfo pci{VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO};
    pci.stage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    pci.stage.stage = VK_SHADER_STAGE_COMPUTE_BIT;
    pci.stage.module = module;
    pci.stage.pName = "main";
    pci.layout = layout;
    VkPipeline pipeline{};
    if (vkCreateComputePipelines(device, VK_NULL_HANDLE, 1, &pci, nullptr, &pipeline) != VK_SUCCESS) return 18;
    cleanup.actions.push_back([=] { vkDestroyPipeline(device, pipeline, nullptr); });
    VkCommandPoolCreateInfo pool_ci{VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO};
    pool_ci.queueFamilyIndex = family;
    VkCommandPool command_pool{};
    if (vkCreateCommandPool(device, &pool_ci, nullptr, &command_pool) != VK_SUCCESS) return 19;
    cleanup.actions.push_back([=] { vkDestroyCommandPool(device, command_pool, nullptr); });
    VkCommandBufferAllocateInfo ai{VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};
    ai.commandPool = command_pool;
    ai.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    ai.commandBufferCount = 1;
    VkCommandBuffer command{};
    if (vkAllocateCommandBuffers(device, &ai, &command) != VK_SUCCESS) return 20;
    VkCommandBufferBeginInfo bi{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
    bi.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    if (vkBeginCommandBuffer(command, &bi) != VK_SUCCESS) return 21;
    vkCmdBindPipeline(command, VK_PIPELINE_BIND_POINT_COMPUTE, pipeline);
    vkCmdBindDescriptorSets(command, VK_PIPELINE_BIND_POINT_COMPUTE, layout, 0, 1, &descriptor, 0, nullptr);
    vkCmdDispatch(command, 1, 1, 1);
    VkMemoryBarrier barrier{VK_STRUCTURE_TYPE_MEMORY_BARRIER};
    barrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
    barrier.dstAccessMask = VK_ACCESS_HOST_READ_BIT;
    vkCmdPipelineBarrier(command, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_HOST_BIT, 0, 1, &barrier, 0,
                         nullptr, 0, nullptr);
    if (vkEndCommandBuffer(command) != VK_SUCCESS) return 22;
    VkFenceCreateInfo fci{VK_STRUCTURE_TYPE_FENCE_CREATE_INFO};
    VkFence fence{};
    if (vkCreateFence(device, &fci, nullptr, &fence) != VK_SUCCESS) return 23;
    cleanup.actions.push_back([=] { vkDestroyFence(device, fence, nullptr); });
    VkQueue queue{};
    vkGetDeviceQueue(device, family, 0, &queue);
    VkSubmitInfo si{VK_STRUCTURE_TYPE_SUBMIT_INFO};
    si.commandBufferCount = 1;
    si.pCommandBuffers = &command;
    if (vkQueueSubmit(queue, 1, &si, fence) != VK_SUCCESS) return 24;
    // Bounded completion; failure exits this isolated control process without reusing resources.
    if (vkWaitForFences(device, 1, &fence, VK_TRUE, 5000000000ULL) != VK_SUCCESS) ExitProcess(25);
    bool exact = true;
    for (size_t i = 0; i < requirements.size / 4; ++i)
        if (words[i] != (i == 1 ? 0x13579BDFU : 0xA5000000U + static_cast<u32>(i))) exact = false;
    std::printf("store_readback_exact=%u allocation_bytes=%llu\n", exact ? 1 : 0,
                static_cast<unsigned long long>(requirements.size));
    if (!exact) return 40;
    return 0;
}
