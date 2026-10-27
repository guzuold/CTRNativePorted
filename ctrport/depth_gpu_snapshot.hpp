// SPDX-License-Identifier: GPL-2.0-only
#pragma once
#include "depth_residency.hpp"
namespace ctrport {
// Original infrastructure using the project's authored full-MS2 extraction shader
// (video/compute_depth_ms_full_control.comp); no guest memory publication.
inline int snapshot_retained_depth_ms2(void* context, uint64_t lease, uint64_t generation,
                                       const CtrDepthMS2LayoutV1& layout, const CtrDepthBackingSpansV1& spans,
                                       std::vector<unsigned char>& output) {
    CtrGraphicsImageExtentV1 extent{};
    const int extent_status = ctr_video_depth_ms2_extent_v1(&layout, &extent);
    if (extent_status) return extent_status;
    if (spans.depth_address != extent.guest_address || spans.depth_bytes != extent.bytes) return 88;
    std::shared_ptr<const std::vector<unsigned char>> prior;
    int rc = runtime_depth_residency.prior_backing(context, lease, generation, prior);
    if (rc) return rc;
    uint64_t checked = 0;
    std::shared_ptr<void> retained;
    rc = runtime_depth_residency.resolve(context, spans, generation, checked, retained);
    if (rc) return rc;
    if (checked != lease || layout.guest_address != spans.depth_address || prior->size() < spans.depth_bytes) return 88;
    uint64_t guard_image = 0, guard_generation = 0, guard_begin = 0, guard_bytes = 0;
    if (!runtime_gpu_resident_memory.identify(spans.depth_address, guard_image, guard_generation, guard_begin,
                                              guard_bytes) ||
        guard_begin != spans.depth_address || guard_image != lease || guard_generation != generation ||
        guard_bytes != prior->size())
        return 88;
    constexpr uint64_t sample_bytes = 1920ULL * 1080 * 2 * 4;
    std::ifstream file("build/CTRPort-data/compute_depth_ms_full_control.spv", std::ios::binary | std::ios::ate);
    if (!file) return 62;
    const auto length = file.tellg();
    if (length <= 0 || length > 65536 || length % 4) return 60;
    std::vector<uint32_t> code(static_cast<size_t>(length) / 4);
    file.seekg(0);
    if (!file.read(reinterpret_cast<char*>(code.data()), length)) return 62;
    const unsigned char expected[] = {0xbd, 0x0f, 0x87, 0x8b, 0xba, 0x9d, 0x27, 0xb2, 0x36, 0x02, 0x18,
                                      0x4f, 0x6c, 0x70, 0xb8, 0x15, 0xf6, 0x96, 0xe5, 0x29, 0x90, 0xd1,
                                      0x44, 0xe6, 0x6b, 0x1e, 0x00, 0x2a, 0xc6, 0x52, 0xfc, 0xea};
    unsigned char digest[32]{};
    BCRYPT_ALG_HANDLE algorithm = nullptr;
    if (BCryptOpenAlgorithmProvider(&algorithm, BCRYPT_SHA256_ALGORITHM, nullptr, 0) < 0) return 112;
    const auto hash = BCryptHash(algorithm, nullptr, 0, reinterpret_cast<PUCHAR>(code.data()),
                                 static_cast<ULONG>(length), digest, 32);
    BCryptCloseAlgorithmProvider(algorithm, 0);
    if (hash < 0 || std::memcmp(digest, expected, 32)) return 88;
    struct Resources {
        void* c;
        uint64_t buffer = 0, layout = 0, pipeline = 0, image = 0, depth = 0;
        ~Resources() {
            if (pipeline) ctr_video_compute_pipeline_release_v1(c, pipeline);
            if (layout) ctr_video_graphics_layout_release_v1(c, layout);
            if (image) ctr_video_sampled_image_release_v1(c, image);
            if (depth) ctr_video_depth_attachment_restore_v1(c, depth);
            if (buffer) ctr_video_buffer_release_v1(c, buffer);
        }
    } resources{context};
    std::vector<unsigned char> samples(sample_bytes), converted(spans.depth_bytes);
    CtrGraphicsBindingContractV1 binding{};
    binding.version = 1;
    binding.size = sizeof(binding);
    binding.unified_after = 2;
    binding.buffer_after = 1;
    binding.descriptor_count = 2;
    binding.descriptors[0] = {0, 0, 0, 1, 0, 1};
    binding.descriptors[1] = {1, 0, 1, 1, UINT32_MAX, 0};
    if ((rc = ctr_video_buffer_create_v1(context, samples.data(), samples.size(), 1, &resources.buffer))) return rc;
    if ((rc = ctr_video_compute_layout_create_v1(context, &binding, &resources.layout))) return rc;
    if ((rc = ctr_video_compute_pipeline_create_v1(context, resources.layout, code.data(),
                                                   static_cast<uint32_t>(code.size()), &resources.pipeline)))
        return rc;
    if ((rc = ctr_video_depth_sampled_alias_v1(context, lease, &resources.image))) return rc;
    resources.depth = lease;
    CtrGraphicsBufferWriteV1 write{0, 0, 1, 0, resources.buffer};
    uint32_t image_binding = 1;
    if ((rc = ctr_video_graphics_layout_buffers_v1(context, resources.layout, &write, 1))) return rc;
    if ((rc = ctr_video_graphics_layout_images_v1(context, resources.layout, &image_binding, &resources.image, 1)))
        return rc;
    CtrGraphicsPushDataV1 push{};
    push.version = 1;
    push.size = sizeof(push);
    const uint32_t groups[] = {480, 1080, 1};
    if ((rc = ctr_video_compute_dispatch_v1(context, resources.pipeline, &push, groups))) return rc;
    // This exact host-coherent SSBO follows the synchronous dispatch's real
    // shader-write/host-read barrier and fence. Never treat an unfenced initial copy as GPU data.
    if ((rc = ctr_video_buffer_copy_initial_v1(context, resources.buffer, samples.data(), samples.size()))) return rc;
    if ((rc = ctr_video_depth_ms2_retile_samples_v1(context, lease, &layout, prior->data(), spans.depth_bytes,
                                                    samples.data(), samples.size(), converted.data())))
        return rc;
    if ((rc = ctr_video_depth_attachment_restore_v1(context, lease))) return rc;
    resources.depth = 0;
    std::shared_ptr<const std::vector<unsigned char>> rechecked;
    if ((rc = runtime_depth_residency.prior_backing(context, lease, generation, rechecked))) return rc;
    if (rechecked != prior) return 88;
    output.swap(converted);
    return 0;
}
// Assemble a host-owned image snapshot from its readable CPU prefix and an
// already fenced/retiled depth prefix. This never reads the excluded guest
// suffix and grants no writeback, guard retirement or whole-depth alias rights.
inline int compose_depth_prefix_snapshot(const CtrGuestCopyApi& memory, uint64_t address, uint64_t bytes,
                                         uint64_t depth_address, const std::vector<unsigned char>& depth,
                                         std::vector<unsigned char>& output) {
    if (memory.version != 1 || memory.size != sizeof(memory) || !memory.read || !address || !bytes ||
        bytes > 32 * 1024 * 1024 || address > UINT64_MAX - bytes || !depth_address || depth.empty() ||
        depth_address > UINT64_MAX - depth.size())
        return 60;
    if (address >= depth_address || address + bytes <= depth_address || address + bytes > depth_address + depth.size())
        return 88;
    const uint64_t prefix = depth_address - address, overlap = bytes - prefix;
    try {
        std::vector<unsigned char> composed(static_cast<size_t>(bytes)), recheck(static_cast<size_t>(prefix));
        int rc = memory.read(memory.context, address, composed.data(), prefix);
        if (rc) return rc;
        std::copy_n(depth.data(), static_cast<size_t>(overlap), composed.data() + prefix);
        rc = memory.read(memory.context, address, recheck.data(), prefix);
        if (rc) return rc;
        if (!std::equal(recheck.begin(), recheck.end(), composed.begin())) return 88;
        output.swap(composed);
        return 0;
    } catch (...) {
        return 112;
    }
}

}
