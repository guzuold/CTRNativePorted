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
    // DLSS and FSR 3 are temporal: CtrDlss feeds both from the same inputs.
    if (CtrUpscaling::Mode() == 2 || CtrUpscaling::Mode() == 3) {
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

// Adapted from SacredFire's CaptureHudless (frame generation), revision recorded in SOURCES.json.
void Rasterizer::CaptureHudless(VideoCore::ImageId display) {
    if (!display) {
        return;
    }
    auto& image = texture_cache.GetImage(display);
    if (image.info.num_samples > 1 || image.info.props.is_depth) {
        return;
    }
    const u32 w = image.info.size.width, h = image.info.size.height;
    const vk::Format format = image.info.pixel_format;
    scheduler.EndRendering();
    if (!ctr_hudless_image || ctr_hudless.width != w || ctr_hudless.height != h ||
        ctr_hudless.format != format) {
        scheduler.Finish(); // the presenter may still read the old copy (a size change: rare)
        ctr_hudless_image = VideoCore::UniqueImage(instance.GetDevice(), instance.GetAllocator());
        ctr_hudless_image.Create(vk::ImageCreateInfo{
            .flags = vk::ImageCreateFlagBits::eMutableFormat,
            .imageType = vk::ImageType::e2D,
            .format = format,
            .extent = {w, h, 1},
            .mipLevels = 1,
            .arrayLayers = 1,
            .samples = vk::SampleCountFlagBits::e1,
            .tiling = vk::ImageTiling::eOptimal,
            .usage = vk::ImageUsageFlagBits::eTransferDst | vk::ImageUsageFlagBits::eSampled,
            .initialLayout = vk::ImageLayout::eUndefined,
        });
        ctr_hudless = {.image = vk::Image(ctr_hudless_image), .format = format, .width = w,
                       .height = h};
    }
    const auto cmdbuf = scheduler.CommandBuffer();
    image.Transit(vk::ImageLayout::eTransferSrcOptimal, vk::AccessFlagBits2::eTransferRead, {},
                  cmdbuf);
    constexpr vk::ImageSubresourceRange range{vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1};
    const vk::ImageMemoryBarrier2 to_dst{
        .srcStageMask = vk::PipelineStageFlagBits2::eAllCommands,
        .srcAccessMask = vk::AccessFlagBits2::eMemoryRead,
        .dstStageMask = vk::PipelineStageFlagBits2::eTransfer,
        .dstAccessMask = vk::AccessFlagBits2::eTransferWrite,
        .oldLayout = vk::ImageLayout::eUndefined,
        .newLayout = vk::ImageLayout::eTransferDstOptimal,
        .image = ctr_hudless.image,
        .subresourceRange = range,
    };
    cmdbuf.pipelineBarrier2({.imageMemoryBarrierCount = 1, .pImageMemoryBarriers = &to_dst});
    const vk::ImageCopy region{
        .srcSubresource = {vk::ImageAspectFlagBits::eColor, 0, 0, 1},
        .dstSubresource = {vk::ImageAspectFlagBits::eColor, 0, 0, 1},
        .extent = {w, h, 1},
    };
    cmdbuf.copyImage(image.GetImage(), vk::ImageLayout::eTransferSrcOptimal, ctr_hudless.image,
                     vk::ImageLayout::eTransferDstOptimal, region);
    const vk::ImageMemoryBarrier2 to_read{
        .srcStageMask = vk::PipelineStageFlagBits2::eTransfer,
        .srcAccessMask = vk::AccessFlagBits2::eTransferWrite,
        .dstStageMask = vk::PipelineStageFlagBits2::eAllCommands,
        .dstAccessMask = vk::AccessFlagBits2::eShaderRead,
        .oldLayout = vk::ImageLayout::eTransferDstOptimal,
        .newLayout = vk::ImageLayout::eShaderReadOnlyOptimal,
        .image = ctr_hudless.image,
        .subresourceRange = range,
    };
    cmdbuf.pipelineBarrier2({.imageMemoryBarrierCount = 1, .pImageMemoryBarriers = &to_read});
    ctr_hudless.address = ctr_hudless_address;
    ++ctr_hudless.serial;
}
