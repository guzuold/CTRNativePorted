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
    // Frame generation's HUD-less frame: the display buffer as the first draw of the display
    // pass (the scene stretch) left it, copied before the first HUD draw.
    struct CtrHudless {
        vk::Image image{};
        vk::Format format{};
        u32 width = 0, height = 0;
        uintptr_t address = 0;
        u64 serial = 0;
    };
    CtrHudless ctr_hudless{};
    VideoCore::UniqueImage ctr_hudless_image;
    bool ctr_hudless_due = false;
    uintptr_t ctr_hudless_address = 0;
    void CaptureHudless(VideoCore::ImageId display);
public:
    [[nodiscard]] const CtrHudless& GetCtrHudless() const { return ctr_hudless; }
    [[nodiscard]] const CtrDlss* GetCtrDlss() const { return ctr_dlss.get(); }
private:
