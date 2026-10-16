// SPDX-License-Identifier: GPL-2.0-or-later
// Adapted from SacredFire's DisplayFsr, revision recorded in SOURCES.json.
vk::ImageView Rasterizer::UpscaleScene(VideoCore::ImageId id, vk::ImageView view,
                                     vk::Format format, vk::ComponentMapping mapping) {
    if (upscale_done) return upscale_view;
    upscale_done = true;
    upscale_view = vk::ImageView{};
    upscale_layout = vk::ImageLayout::eShaderReadOnlyOptimal;
    if (!upscale_scene) return {};
    if (!cb_descs[0].first) return {};
    auto* source_ptr = texture_cache.TryGetImage(id, upscale_source_uid);
    if (!source_ptr) return {};
    auto& source = *source_ptr;
    const auto& display = texture_cache.GetImage(cb_descs[0].first);
    const vk::Extent2D in{source.info.size.width, source.info.size.height};
    const vk::Extent2D out{display.info.size.width, display.info.size.height};
    if (CtrUpscaling::Mode() == 2) {
        upscale_view = ctr_dlss->Upscale(id, format, mapping, out, upscale_layout);
        return upscale_view;
    }
    if (CtrUpscaling::Mode() != 1 || in.width >= out.width || in.height >= out.height ||
        in.width * 4 < out.width ||
        std::abs(double(in.width) / in.height - double(out.width) / out.height) > 0.02) return {};
    scheduler.EndRendering();
    if (!scene_fsr_created) {
        scene_fsr.Create(instance.GetDevice(), instance.GetAllocator(), 3);
        scene_fsr_created = true;
    }
    source.Transit(vk::ImageLayout::eShaderReadOnlyOptimal, vk::AccessFlagBits2::eShaderRead, {});
    upscale_view = scene_fsr.Render(scheduler.CommandBuffer(), view, in, out, {}, false);
    if (upscale_frames == 29) {
        UpscaleReadback(instance, scheduler, source.GetImage(), source.backing->state.layout, in, 4, "fsr1_input_rgba8");
        UpscaleReadback(instance, scheduler, scene_fsr.LastOutputImage(), upscale_layout, out, 8, "fsr1_output_rgba16f");
    }
    if (++upscale_frames <= 3 || upscale_frames % 120 == 0) {
        std::printf("CTR_UPSCALE_DISPATCH mode=fsr1 frame=%llu input=%ux%u output=%ux%u before_hud=1\n",
                    (unsigned long long)upscale_frames, in.width,in.height,out.width,out.height);
        std::fflush(stdout);
    }
    return upscale_view;
}
