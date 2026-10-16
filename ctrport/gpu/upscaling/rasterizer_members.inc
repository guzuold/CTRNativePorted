    std::unique_ptr<CtrDlss> ctr_dlss;
    HostPasses::FsrPass scene_fsr;
    bool scene_fsr_created = false, last_display = false, upscale_pending = false;
    bool upscale_done = false, ctr_gbuffer_draw = false;
    bool scene_seen = false, upscale_scene = false;
    VideoCore::ImageId upscale_source{};
    u64 upscale_source_uid = 0, upscale_frames = 0;
    vk::ImageView upscale_view{};
    vk::ImageLayout upscale_layout{};
    vk::ImageView UpscaleScene(VideoCore::ImageId id, vk::ImageView view,
                              vk::Format format, vk::ComponentMapping mapping);
