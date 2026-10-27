// SPDX-License-Identifier: GPL-2.0-only
#pragma once
// Original image reads require retained ownership and immutable Windows protection.
namespace ctrport {
struct MarkerCopyOwner {
    std::shared_ptr<DirectMemoryBacking> direct;
    std::shared_ptr<std::vector<MappedImage>> images;
    std::vector<ImageMetadata> metadata;
};
inline int marker_read(void* context, uint64_t address, void* output, uint64_t size) noexcept {
    auto& owner = *static_cast<MarkerCopyOwner*>(context);
    if (!direct_copy_read(owner.direct.get(), address, output, size)) return 0;
    if (!size || size > 65536 || address > UINT64_MAX - size || !owner.images) return 1;
    for (size_t i = 0; i < owner.images->size() && i < owner.metadata.size(); ++i) {
        const auto base = reinterpret_cast<uint64_t>((*owner.images)[i].get());
        if (address < base) continue;
        const auto offset = address - base;
        for (const auto& span : owner.metadata[i].spans) {
            if (offset < span.first || offset >= span.second || size > span.second - offset) continue;
            MEMORY_BASIC_INFORMATION region{};
            if (!VirtualQuery(reinterpret_cast<void*>(address), &region, sizeof(region))) return 1;
            const auto start = reinterpret_cast<uint64_t>(region.BaseAddress);
            if (region.State != MEM_COMMIT ||
                (region.Protect != PAGE_READONLY && region.Protect != PAGE_EXECUTE_READ) ||
                address - start > region.RegionSize || size > region.RegionSize - (address - start))
                return 1;
            SIZE_T copied = 0;
            return ReadProcessMemory(GetCurrentProcess(), reinterpret_cast<void*>(address), output,
                                     static_cast<SIZE_T>(size), &copied) &&
                           copied == size
                       ? 0
                       : 1;
        }
    }
    return 1;
}
inline int marker_write(void* context, uint64_t address, const void* input, uint64_t size) noexcept {
    return direct_copy_write(static_cast<MarkerCopyOwner*>(context)->direct.get(), address, input, size);
}
inline int runtime_gnm_push_marker(uint64_t command, uint32_t size, uint64_t marker) {
    MarkerCopyOwner owner{direct_memory_backing, startup_worker_images, startup_worker_metadata};
    if (!owner.direct) throw std::runtime_error("PushMarker direct mapping owner missing");
    auto transaction = owner.direct->copy_transaction();
    const CtrGuestCopyApi memory{sizeof(CtrGuestCopyApi), 1, &owner, marker_read, marker_write};
    int32_t result = 123;
    const int host = ctr_gnm_insert_push_marker(&memory, command, size, marker, &result);
    if (host) throw std::runtime_error("PushMarker checked emitter host_status=" + std::to_string(host));
    return result;
}
extern "C" int ctr_gnm_push_marker_bridge(uint64_t command, uint32_t size, uint64_t marker, uint64_t caller) {
    try {
        const int result = runtime_gnm_push_marker(command, size, marker);
        std::printf(
            "{\"scope\":\"original_GnmInsertPushMarker_return\",\"caller\":%llu,\"size_dwords\":%u,\"result\":%d,\"gpu_execution\":false}\n",
            static_cast<unsigned long long>(caller), size, result);
        std::fflush(stdout);
        return result;
    } catch (const std::exception& e) {
        std::fprintf(stderr, "NID=W1Etj-jlW7Y library=libSceGnmDriver caller=0x%llx marker=0x%llx unsupported: %s\n",
                     static_cast<unsigned long long>(caller), static_cast<unsigned long long>(marker), e.what());
        std::fflush(stderr);
        ExitProcess(4);
    }
}
extern "C" void ctr_sysv_gnm_push_marker();
inline int runtime_gnm_pop_marker(uint64_t command, uint32_t size) {
    auto owner = direct_memory_backing;
    if (!owner) throw std::runtime_error("PopMarker owner missing");
    auto transaction = owner->copy_transaction();
    const auto memory = make_direct_copy_api(*owner);
    int32_t result = 123;
    const int host = ctr_gnm_insert_pop_marker(&memory, command, size, &result);
    if (host) throw std::runtime_error("PopMarker checked emitter host_status=" + std::to_string(host));
    return result;
}
extern "C" int ctr_gnm_pop_marker_bridge(uint64_t command, uint32_t size, uint64_t caller) {
    try {
        const int result = runtime_gnm_pop_marker(command, size);
        std::printf(
            "{\"scope\":\"original_GnmInsertPopMarker_return\",\"caller\":%llu,\"size_dwords\":%u,\"result\":%d,\"gpu_execution\":false}\n",
            static_cast<unsigned long long>(caller), size, result);
        std::fflush(stdout);
        return result;
    } catch (const std::exception& e) {
        std::fprintf(stderr, "NID=7qZVNgEu+SY library=libSceGnmDriver caller=0x%llx unsupported: %s\n",
                     static_cast<unsigned long long>(caller), e.what());
        ExitProcess(4);
    }
}
extern "C" void ctr_sysv_gnm_pop_marker();
}
