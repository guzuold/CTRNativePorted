// SPDX-License-Identifier: GPL-2.0-only
#pragma once
#include "graphics_companion_client.hpp"
namespace ctrport {
// v22 owns the whole public register decode. The runtime gates capture on a
// selected-device query proving AMD mixed depth disabled; enabled is pending.
inline int decode_owned_graphics_key_on_host(const CtrPm4::Registers& bank, uint32_t mrt, CtrGraphicsKeyV1& output) {
    wchar_t path[4096]{};
    const auto length = GetFullPathNameW(L"build/CTRPort-infra/graphics_companions/ctr_graphics_companion_v22.dll",
                                         4096, path, nullptr);
    if (!length || length >= 4096) return 88;
    HMODULE module = LoadLibraryExW(path, nullptr, LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR | LOAD_LIBRARY_SEARCH_DEFAULT_DIRS);
    if (!module) return 88;
    struct Loaded {
        HMODULE value;
        ~Loaded() { FreeLibrary(value); }
    } loaded{module};
    const auto call =
        reinterpret_cast<decltype(&ctr_graphics_key_decode_v1)>(GetProcAddress(module, "ctr_graphics_key_decode_v1"));
    if (!call) return 88;
    CtrGraphicsKeyV1 result{};
    const int status = call(bank.words.data(), static_cast<uint32_t>(bank.words.size()), mrt, 0, 0, &result);
    if (status) return status;
    if (result.version != 1 || result.size != sizeof(result) || result.mrt_mask != mrt ||
        result.num_color_attachments > 8)
        return 112;
    output = result;
    return 0;
}
template <class Function> inline int graphics_metadata_host_call(Function&& function) {
    struct Work {
        Function& function;
        int status = 112;
    };
    Work work{function, 112};
    auto entry = [](void* raw) -> DWORD {
        auto& w = *static_cast<Work*>(raw);
        try {
            w.status = w.function();
        } catch (...) {
            w.status = 112;
        }
        return 0;
    };
    HANDLE thread = CreateThread(nullptr, 8 * 1024 * 1024, entry, &work, 0, nullptr);
    if (!thread) return 112;
    const auto waited = WaitForSingleObject(thread, INFINITE);
    CloseHandle(thread);
    if (waited != WAIT_OBJECT_0) std::terminate();
    return work.status;
}
inline int decode_owned_graphics_key(const CtrPm4::Registers& bank, uint32_t mrt, CtrGraphicsKeyV1& output) {
    return graphics_metadata_host_call([&]() { return decode_owned_graphics_key_on_host(bank, mrt, output); });
}
inline std::string graphics_key_event(uint32_t word, bool fragment, int status, const CtrGraphicsKeyV1& key) {
    std::ostringstream line;
    line << "{\"scope\":\"original_GnmSubmitFlip_graphics_key\",\"word\":" << word
         << ",\"fragment_present\":" << (fragment ? "true" : "false") << ",\"status\":" << status
         << ",\"mrt_mask\":" << key.mrt_mask << ",\"color_count\":" << key.num_color_attachments
         << ",\"samples\":" << key.num_samples << ",\"depth_samples\":" << key.depth_samples
         << ",\"primitive_type\":" << key.primitive_type << ",\"z_format\":" << key.z_format
         << ",\"stencil_format\":" << key.stencil_format
         << ",\"mixed_any_enabled\":false,\"mixed_depth_enabled\":false,\"GPU_execution\":false,\"colors\":[";
    for (uint32_t slot = 0; slot < 8; ++slot) {
        const auto& c = key.colors[slot];
        if (slot) line << ',';
        line << "{\"slot\":" << slot << ",\"data_format\":" << c.data_format << ",\"number_format\":" << c.number_format
             << ",\"write_mask\":" << c.write_mask << ",\"samples\":" << c.samples << '}';
    }
    line << "]}\n";
    return line.str();
}
// Bound retained vertex snapshots per original submission. The second retail
// submission needs three 7,167,984-byte attributes after earlier draws.
inline uint64_t vertex_capture_budget(uint64_t retained) {
    constexpr uint64_t total = 256ull * 1024 * 1024, per_buffer = 16ull * 1024 * 1024;
    return retained >= total ? 0 : (std::min)(per_buffer, total - retained);
}
struct GraphicsVertexFetchOwner {
    CtrGraphicsVertexInputsV1 value{};
    int status = 88;
    std::array<uint32_t, 32> formats{};
    int format_status = 88;
    int physical_status = 88;
    std::vector<GraphicsBufferCapture> buffers;
};
inline GraphicsVertexFetchOwner capture_vertex_fetch_on_host(const CtrShaderInputsV1& inputs,
                                                             const CtrShaderOwnedRangesV1& ranges,
                                                             const GraphicsStageOwner& stage,
                                                             const CtrGraphicsPipelineStageV1& pipeline,
                                                             uint64_t& retained_bytes) {
    GraphicsVertexFetchOwner result;
    wchar_t path[4096]{};
    const auto length = GetFullPathNameW(L"build/CTRPort-infra/graphics_companions/ctr_graphics_companion_v28.dll",
                                         4096, path, nullptr);
    if (!length || length >= 4096) return result;
    HMODULE module = LoadLibraryExW(path, nullptr, LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR | LOAD_LIBRARY_SEARCH_DEFAULT_DIRS);
    if (!module) return result;
    struct Loaded {
        HMODULE value;
        ~Loaded() { FreeLibrary(value); }
    } loaded{module};
    const auto call = reinterpret_cast<decltype(&ctr_graphics_vertex_inputs_v1)>(
        GetProcAddress(module, "ctr_graphics_vertex_inputs_v1"));
    if (!call) return result;
    result.status = call(&inputs, &ranges, stage.userdata.data(), stage.userdata_words, &pipeline, &result.value);
    if (result.status) return result;
    if (result.value.version != 1 || result.value.size != sizeof(result.value) || result.value.count > 32) {
        result.status = 112;
        return result;
    }
    const auto format = reinterpret_cast<decltype(&ctr_graphics_surface_format_v1)>(
        GetProcAddress(module, "ctr_graphics_surface_format_v1"));
    result.format_status = format ? 0 : 88;
    for (uint32_t i = 0; i < result.value.count; ++i) {
        const auto& v = result.value.inputs[i];
        if (format) {
            const int mapped = format(v.data_format, v.number_format, &result.formats[i]);
            if (mapped) result.format_status = mapped;
        }

        CtrGraphicsBufferRequirementV1 demand{};
        demand.version = 1;
        demand.size = sizeof(demand);
        demand.guest_address = v.guest_address;
        demand.bytes = v.buffer_size;
        demand.stride = v.stride;
        const auto budget = vertex_capture_budget(retained_bytes);
        auto captured = capture_graphics_buffer(inputs, ranges, demand, budget);
        if (captured.requirement_status || captured.lease_status || captured.capture_status ||
            captured.release_status || captured.lease_end_status) {
            result.status = 88;
            return result;
        }
        retained_bytes += captured.snapshot.size();
        result.buffers.push_back(std::move(captured));
    }
    return result;
}
inline GraphicsVertexFetchOwner capture_vertex_fetch(const CtrShaderInputsV1& inputs,
                                                     const CtrShaderOwnedRangesV1& ranges,
                                                     const GraphicsStageOwner& stage,
                                                     const CtrGraphicsPipelineStageV1& pipeline,
                                                     uint64_t& retained_bytes) {
    GraphicsVertexFetchOwner result;
    const int status = graphics_metadata_host_call([&]() {
        result = capture_vertex_fetch_on_host(inputs, ranges, stage, pipeline, retained_bytes);
        return result.status;
    });
    result.status = status;
    return result;
}
inline std::string vertex_inputs_metadata(const GraphicsVertexFetchOwner& owner) {
    std::ostringstream out;
    out << "[";
    for (uint32_t i = 0; i < (std::min)(owner.value.count, uint32_t(32)); ++i) {
        if (i) out << ',';
        const auto& v = owner.value.inputs[i];
        out << "{\"location\":" << v.location << ",\"binding\":" << v.binding << ",\"offset\":" << v.offset
            << ",\"stride\":" << v.stride << ",\"input_rate\":" << v.input_rate << ",\"divisor\":" << v.divisor
            << ",\"data_format\":" << v.data_format << ",\"number_format\":" << v.number_format
            << ",\"vk_format\":" << owner.formats[i] << ",\"buffer_size\":" << v.buffer_size
            << ",\"guest_address\":" << v.guest_address << '}';
    }
    out << ']';
    return out.str();
}
inline std::string vertex_fetch_event(uint32_t word, const GraphicsVertexFetchOwner& owner) {
    uint64_t bytes = 0;
    for (const auto& b : owner.buffers) bytes += b.snapshot.size();
    std::ostringstream line;
    line << "{\"scope\":\"original_GnmSubmitFlip_vertex_fetch\",\"word\":" << word << ",\"status\":" << owner.status
         << ",\"attribute_count\":" << owner.value.count << ",\"format_status\":" << owner.format_status
         << ",\"inputs\":" << vertex_inputs_metadata(owner)
         << ",\"physical_vertex_format_authority\":false,\"captured_buffers\":" << owner.buffers.size()
         << ",\"captured_bytes\":" << bytes << ",\"GPU_execution\":false}\n";
    return line.str();
}

// v25 returns an owned POD; no pointers survive companion unload.
inline int decode_owned_viewports(const CtrPm4::Registers& bank, const CtrGraphicsProfileV1& profile,
                                  CtrGraphicsViewportStateV1& output) {
    if (profile.version != 1 || profile.size != sizeof(profile) || !profile.max_viewport_width ||
        !profile.max_viewport_height)
        return 60;
    return graphics_metadata_host_call([&]() {
        wchar_t path[4096]{};
        const auto n = GetFullPathNameW(L"build/CTRPort-infra/graphics_companions/ctr_graphics_companion_v25.dll", 4096,
                                        path, nullptr);
        if (!n || n >= 4096) return 88;
        HMODULE module =
            LoadLibraryExW(path, nullptr, LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR | LOAD_LIBRARY_SEARCH_DEFAULT_DIRS);
        if (!module) return 88;
        struct Loaded {
            HMODULE value;
            ~Loaded() { FreeLibrary(value); }
        } loaded{module};
        const auto call = reinterpret_cast<decltype(&ctr_graphics_viewport_state_v1)>(
            GetProcAddress(module, "ctr_graphics_viewport_state_v1"));
        if (!call) return 88;
        CtrGraphicsViewportStateV1 result{};
        const int status = call(bank.words.data(), static_cast<uint32_t>(bank.words.size()), profile.max_viewport_width,
                                profile.max_viewport_height, 0, &result);
        if (status) return status;
        if (result.version != 1 || result.size != sizeof(result) || !result.count || result.count > 16) return 112;
        output = result;
        return 0;
    });
}
inline int decode_owned_dynamic_state(const CtrPm4::Registers& bank, bool indexed, CtrGraphicsDynamicStateV1& output) {
    // Neither VK_EXT_primitive_topology_list_restart feature is enabled by our device policy.
    return graphics_metadata_host_call([&]() {
        wchar_t path[4096]{};
        const auto n = GetFullPathNameW(L"build/CTRPort-infra/graphics_companions/ctr_graphics_companion_v26.dll", 4096,
                                        path, nullptr);
        if (!n || n >= 4096) return 88;
        HMODULE module =
            LoadLibraryExW(path, nullptr, LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR | LOAD_LIBRARY_SEARCH_DEFAULT_DIRS);
        if (!module) return 88;
        struct Loaded {
            HMODULE value;
            ~Loaded() { FreeLibrary(value); }
        } loaded{module};
        const auto call = reinterpret_cast<decltype(&ctr_graphics_dynamic_state_v1)>(
            GetProcAddress(module, "ctr_graphics_dynamic_state_v1"));
        if (!call) return 88;
        CtrGraphicsDynamicStateV1 result{};
        const int status =
            call(bank.words.data(), static_cast<uint32_t>(bank.words.size()), indexed ? 1u : 0u, 0, 0, &result);
        if (status) return status;
        if (result.version != 1 || result.size != sizeof(result)) return 112;
        output = result;
        return 0;
    });
}
inline int decode_owned_push_data(const CtrPm4::Registers& bank, const CtrGraphicsBindingContractV1& ps,
                                  const CtrGraphicsBindingContractV1& vs, const uint32_t* offsets, uint32_t count,
                                  CtrGraphicsPushDataV1& output) {
    return graphics_metadata_host_call([&]() {
        wchar_t path[4096]{};
        const auto n = GetFullPathNameW(L"build/CTRPort-infra/graphics_companions/ctr_graphics_companion_v27.dll", 4096,
                                        path, nullptr);
        if (!n || n >= 4096) return 88;
        HMODULE module =
            LoadLibraryExW(path, nullptr, LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR | LOAD_LIBRARY_SEARCH_DEFAULT_DIRS);
        if (!module) return 88;
        struct Loaded {
            HMODULE value;
            ~Loaded() { FreeLibrary(value); }
        } loaded{module};
        const auto call =
            reinterpret_cast<decltype(&ctr_graphics_push_data_v1)>(GetProcAddress(module, "ctr_graphics_push_data_v1"));
        if (!call) return 88;
        CtrGraphicsPushDataV1 result{};
        const int status =
            call(bank.words.data(), static_cast<uint32_t>(bank.words.size()), &ps, &vs, offsets, count, &result);
        if (status) return status;
        if (result.version != 1 || result.size != sizeof(result)) return 112;
        for (unsigned i = 120; i < 128; ++i)
            if (result.bytes[i]) return 112;
        output = result;
        return 0;
    });
}

}

namespace ctrport {
inline int decode_owned_draw_parameters(const CtrPm4::Registers& bank, const CtrGraphicsPipelineStageV1& vertex,
                                        const GraphicsStageOwner& stage, CtrGraphicsDrawParametersV1& output) {
    return graphics_metadata_host_call([&]() {
        wchar_t path[4096]{};
        const auto n = GetFullPathNameW(L"build/CTRPort-infra/graphics_companions/ctr_graphics_companion_v28.dll", 4096,
                                        path, nullptr);
        if (!n || n >= 4096) return 88;
        HMODULE module =
            LoadLibraryExW(path, nullptr, LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR | LOAD_LIBRARY_SEARCH_DEFAULT_DIRS);
        if (!module) return 88;
        struct Loaded {
            HMODULE value;
            ~Loaded() { FreeLibrary(value); }
        } loaded{module};
        const auto call = reinterpret_cast<decltype(&ctr_graphics_draw_parameters_v1)>(
            GetProcAddress(module, "ctr_graphics_draw_parameters_v1"));
        if (!call) return 88;
        if (stage.flattened_userdata.size() < 16 || stage.userdata_words > 16 ||
            !std::equal(stage.userdata.begin(), stage.userdata.begin() + stage.userdata_words,
                        stage.flattened_userdata.begin()))
            return 112;
        CtrGraphicsDrawParametersV1 result{};
        const int status = call(bank.words.data(), static_cast<uint32_t>(bank.words.size()), &vertex,
                                stage.flattened_userdata.data(), 16, &result);
        if (status) return status;
        if (result.version != 1 || result.size != sizeof(result) || !result.instance_count) return 112;
        output = result;
        return 0;
    });
}
}

namespace ctrport {}
