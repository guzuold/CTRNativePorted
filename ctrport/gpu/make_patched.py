# SPDX-License-Identifier: GPL-2.0-or-later
"""Derive the few shadPS4 v0.18.0 sources that ctr_gpu compiles with changes.

Usage: python -B ctrport/gpu/make_patched.py <shadPS4 src dir> <output dir>

Each change is an exact text replacement that must match once; a mismatch (other
shadPS4 revision) stops the build instead of producing a silently different file.

vk_presenter.cpp: shadPS4 v0.18.0 draws the game image inside an ImGui window. The
runtime has no ImGui, so Present() blits the frame to the swapchain image (the method
earlier shadPS4 versions used) and the screenshot/notification code is dropped.
"""
import sys
from pathlib import Path

src, out = Path(sys.argv[1]), Path(sys.argv[2])


def replace_once(text, old, new, what):
    count = text.count(old)
    if count != 1:
        raise SystemExit(f'make_patched: {what}: expected 1 match, found {count}')
    return text.replace(old, new)


def cut_between(text, start, end, what, keep_end=True):
    a = text.find(start)
    b = text.find(end, a + 1) if a >= 0 else -1
    if a < 0 or b < 0 or text.count(start) != 1:
        raise SystemExit(f'make_patched: {what}: markers not found exactly once')
    return text[:a] + (text[b:] if keep_end else text[b + len(end):])


p = src / 'video_core/renderer_vulkan/vk_presenter.cpp'
t = p.read_text(encoding='utf-8')
for line in ('#include "core/devtools/layer.h"\n', '#include "imgui/friends_layer.h"\n',
             '#include "imgui/invitation_prompt_layer.h"\n', '#include "imgui/notifications_layer.h"\n',
             '#include "imgui/renderer/imgui_core.h"\n', '#include "imgui/renderer/imgui_impl_vulkan.h"\n',
             '#include "imgui/shadnet_notifications_layer.h"\n', '#include <imgui.h>\n', '#include <png.h>\n'):
    t = replace_once(t, line, '', 'include ' + line.strip())
t = cut_between(t, 'enum class ScreenshotKind : u8 {', 'Presenter::Presenter(', 'screenshot helpers')
t = replace_once(t, '''    ImGui::Layer::AddLayer(Common::Singleton<Core::Devtools::Layer>::Instance());
    ImGui::Friends::Register();
    ImGui::ShadNetNotify::Register();
    ImGui::InvitationPrompt::Register();
''', '', 'constructor layers')
t = replace_once(t, '''    ImGui::InvitationPrompt::Unregister();
    ImGui::ShadNetNotify::Unregister();
    ImGui::Friends::Unregister();
    ImGui::Layer::RemoveLayer(Common::Singleton<Core::Devtools::Layer>::Instance());
''', '', 'destructor layers')
t = replace_once(t, '''    if (frame->imgui_texture) {
        ImGui::Vulkan::RemoveTexture(frame->imgui_texture);
    }
''', '', 'frame texture removal')
t = replace_once(t, '''    frame->imgui_texture = ImGui::Vulkan::AddTexture(view, vk::ImageLayout::eShaderReadOnlyOptimal);
''', '', 'frame texture registration')
t = cut_between(t, '    const u32 capture_game_only_count = VideoCore::ConsumeGameOnlyScreenshotRequests();',
                '    // Continue with host-side passes', 'game-only screenshot')
t = cut_between(t, '    std::shared_ptr<std::vector<ScreenshotReadback>> deferred_screenshots{};\n    if (!pending_screenshots.empty()) {\n        deferred_screenshots =\n            std::make_shared<std::vector<ScreenshotReadback>>(std::move(pending_screenshots));\n        draw_scheduler',
                '    // Flush frame creation commands.\n    frame->ready_semaphore = draw_scheduler', 'game-only screenshot flush')

present_start = '    ImGuiID dockId = ImGui::Core::NewFrame(is_reusing_frame);\n'
present_end = '    SubmitInfo info{};\n    info.AddWait(swapchain.GetImageAcquiredSemaphore());'
blit = '''    // ctrport: blit the frame to the swapchain image (no ImGui overlay in the runtime).
    const vk::Image swapchain_image = swapchain.Image();
    auto& scheduler = present_scheduler;
    const auto cmdbuf = scheduler.CommandBuffer();
    if (!is_reusing_frame) CapturePresentedFrame(instance, scheduler, frame->image,
        {frame->width,frame->height},swapchain.GetSurfaceFormat().format);
    const vk::Extent2D extent = swapchain.GetExtent();
    SetExpectedGameSize(static_cast<s32>(extent.width), static_cast<s32>(extent.height));
    const vk::ImageSubresourceRange color_range{
        .aspectMask = vk::ImageAspectFlagBits::eColor,
        .baseMipLevel = 0,
        .levelCount = 1,
        .baseArrayLayer = 0,
        .layerCount = VK_REMAINING_ARRAY_LAYERS,
    };
    const std::array pre_barriers{
        vk::ImageMemoryBarrier{
            .srcAccessMask = vk::AccessFlagBits::eNone,
            .dstAccessMask = vk::AccessFlagBits::eTransferWrite,
            .oldLayout = vk::ImageLayout::eUndefined,
            .newLayout = vk::ImageLayout::eTransferDstOptimal,
            .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
            .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
            .image = swapchain_image,
            .subresourceRange = color_range,
        },
        vk::ImageMemoryBarrier{
            .srcAccessMask = vk::AccessFlagBits::eColorAttachmentWrite,
            .dstAccessMask = vk::AccessFlagBits::eTransferRead,
            .oldLayout = vk::ImageLayout::eGeneral,
            .newLayout = vk::ImageLayout::eTransferSrcOptimal,
            .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
            .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
            .image = frame->image,
            .subresourceRange = color_range,
        },
    };
    cmdbuf.pipelineBarrier(vk::PipelineStageFlagBits::eAllCommands,
                           vk::PipelineStageFlagBits::eTransfer, vk::DependencyFlagBits::eByRegion,
                           {}, {}, pre_barriers);
    const vk::ClearColorValue black{std::array<float, 4>{0.0f, 0.0f, 0.0f, 1.0f}};
    cmdbuf.clearColorImage(swapchain_image, vk::ImageLayout::eTransferDstOptimal, black,
                           color_range);
    const vk::MemoryBarrier clear_to_blit{
        .srcAccessMask = vk::AccessFlagBits::eTransferWrite,
        .dstAccessMask = vk::AccessFlagBits::eTransferWrite,
    };
    cmdbuf.pipelineBarrier(vk::PipelineStageFlagBits::eTransfer,
                           vk::PipelineStageFlagBits::eTransfer, vk::DependencyFlagBits::eByRegion,
                           clear_to_blit, {}, {});
    cmdbuf.blitImage(frame->image, vk::ImageLayout::eTransferSrcOptimal, swapchain_image,
                     vk::ImageLayout::eTransferDstOptimal,
                     MakeImageBlitFit(frame->width, frame->height, extent.width, extent.height),
                     vk::Filter::eLinear);
    const std::array post_barriers{
        vk::ImageMemoryBarrier{
            .srcAccessMask = vk::AccessFlagBits::eTransferWrite,
            .dstAccessMask = vk::AccessFlagBits::eNone,
            .oldLayout = vk::ImageLayout::eTransferDstOptimal,
            .newLayout = vk::ImageLayout::ePresentSrcKHR,
            .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
            .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
            .image = swapchain_image,
            .subresourceRange = color_range,
        },
        vk::ImageMemoryBarrier{
            .srcAccessMask = vk::AccessFlagBits::eTransferRead,
            .dstAccessMask = vk::AccessFlagBits::eShaderRead,
            .oldLayout = vk::ImageLayout::eTransferSrcOptimal,
            .newLayout = vk::ImageLayout::eShaderReadOnlyOptimal,
            .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
            .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
            .image = frame->image,
            .subresourceRange = color_range,
        },
    };
    cmdbuf.pipelineBarrier(vk::PipelineStageFlagBits::eTransfer,
                           vk::PipelineStageFlagBits::eAllCommands,
                           vk::DependencyFlagBits::eByRegion, {}, {}, post_barriers);

'''
a = t.find(present_start)
b = t.find(present_end)
if a < 0 or b < 0 or b < a or t.count(present_start) != 1 or t.count(present_end) != 1:
    raise SystemExit('make_patched: Present markers not found exactly once')
t = t[:a] + blit + t[b:]
t = '#include "upscaling/frame_evidence.h"\n' + t
t = replace_once(t, '        DebugState.IncFlipFrameNum();', '        RecordGuestPresentation();\n        DebugState.IncFlipFrameNum();', 'guest presentation timestamps')

# Frame generation (CTR_FRAMEGEN, upscaling/ctr_framegen.h) fills only the gap between the
# game's real frame rate and its target (60 with CTR_60FPS_PATCH, else 30): at the target it
# stays idle. Below it, PrepareFrame records the interpolation after post-processing, and Present
# shows a generated frame before as many real frames as the deficit needs (one per frame at half
# the target), the real one half a frame later (FIFO paces the two presents by itself).
t = '#include <thread>\n#include "upscaling/ctr_framegen.h"\n' + t
t = replace_once(t, 'Presenter::Presenter(', '''namespace {
std::unique_ptr<CtrFrameGen> ctr_framegen;
std::array<bool, CtrFrameGen::MaxSlots> fg_ready{};
std::array<float, CtrFrameGen::MaxSlots> fg_interval{};
std::chrono::steady_clock::time_point fg_last_prepare{}, fg_stats_since{};
float fg_interval_ms = 33.3f;
float fg_credit = 0.0f;   // generated frames owed to reach the target rate
bool fg_recording = false;
u64 fg_flips = 0, fg_presented = 0;
float FrameGenTargetMs() {
    static const float value = [] {
        const char* v = std::getenv("CTR_60FPS_PATCH");
        return v && std::strcmp(v, "1") == 0 ? 1000.0f / 60.0f : 1000.0f / 30.0f;
    }();
    return value;
}
} // namespace

Presenter::Presenter(''', 'frame generation state')
t = replace_once(t, '    pp_pass.Create(device, swapchain.GetSurfaceFormat().format);\n',
                 '    pp_pass.Create(device, swapchain.GetSurfaceFormat().format);\n'
                 '    ctr_framegen = std::make_unique<CtrFrameGen>(instance, draw_scheduler);\n',
                 'create frame generation')
t = replace_once(t, 'Presenter::~Presenter() {\n', 'Presenter::~Presenter() {\n    ctr_framegen.reset();\n',
                 'destroy frame generation')
t = replace_once(t, '    pp_pass.Render(cmdbuf, image_view, image_size, *frame, pp_settings);\n', '''    pp_pass.Render(cmdbuf, image_view, image_size, *frame, pp_settings);

    if (frame->id < CtrFrameGen::MaxSlots) {
        fg_ready[frame->id] = false;
        if (ctr_framegen && ctr_framegen->Enabled()) {
            const auto now = std::chrono::steady_clock::now();
            float frame_ms = 100.0f;
            if (fg_last_prepare.time_since_epoch().count() != 0) {
                frame_ms = std::chrono::duration<float, std::milli>(now - fg_last_prepare).count();
            }
            fg_last_prepare = now;
            if (frame_ms < 100.0f) {
                fg_interval_ms += (std::clamp(frame_ms, 2.0f, 50.0f) - fg_interval_ms) * 0.2f;
            }
            fg_interval[frame->id] = fg_interval_ms;
            // Short of the target: generate (with a little hysteresis so it does not flap).
            const float target = FrameGenTargetMs();
            const bool needed = fg_interval_ms > target * (fg_recording ? 1.02f : 1.06f);
            if (needed) {
            CtrFrameGen::Inputs in{};
            in.slot = frame->id;
            in.color = frame->image;
            in.color_format = swapchain.GetSurfaceFormat().format;
            in.width = frame->width;
            in.height = frame->height;
            in.frame_ms = frame_ms;
            // A load, a pause or a hitch: interpolating across it would only smear two
            // unrelated frames, so the history starts over.
            in.reset = frame_ms >= 100.0f || !fg_recording;
            fg_ready[frame->id] = ctr_framegen->Record(in);
            }
            fg_recording = needed;
        }
    }
''', 'record frame generation')
t = replace_once(t, '''    if (!swapchain.AcquireNextImage()) {
        swapchain.Recreate(window.GetWidth(), window.GetHeight());
''', '''    const bool generated_ready = !is_reusing_frame && frame->id < CtrFrameGen::MaxSlots &&
                                 std::exchange(fg_ready[frame->id], false);
    if (!is_reusing_frame && ctr_framegen && ctr_framegen->Enabled()) {
        ++fg_flips;
        // The share of this frame's time the target expects another frame in, at most one.
        const float extra = std::clamp(fg_interval[frame->id] / FrameGenTargetMs() - 1.0f, 0.0f, 1.0f);
        fg_credit = generated_ready ? std::min(fg_credit + extra, 1.5f) : 0.0f;
        const bool owed = fg_credit >= 0.999f;
        const vk::Image generated = generated_ready && owed ? ctr_framegen->OutputImage(frame->id) : vk::Image{};
        if (generated && window.GetWidth() != 0 && window.GetHeight() != 0 &&
            swapchain.AcquireNextImage()) {
            const vk::Image target = swapchain.Image();
            const vk::Extent2D target_extent = swapchain.GetExtent();
            const auto fg_cmdbuf = present_scheduler.CommandBuffer();
            const vk::ImageSubresourceRange range{vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1};
            const std::array fg_pre{
                vk::ImageMemoryBarrier{
                    .dstAccessMask = vk::AccessFlagBits::eTransferWrite,
                    .oldLayout = vk::ImageLayout::eUndefined,
                    .newLayout = vk::ImageLayout::eTransferDstOptimal,
                    .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                    .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                    .image = target,
                    .subresourceRange = range,
                },
                vk::ImageMemoryBarrier{
                    .srcAccessMask = vk::AccessFlagBits::eShaderWrite,
                    .dstAccessMask = vk::AccessFlagBits::eTransferRead,
                    .oldLayout = vk::ImageLayout::eGeneral,
                    .newLayout = vk::ImageLayout::eTransferSrcOptimal,
                    .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                    .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                    .image = generated,
                    .subresourceRange = range,
                },
            };
            fg_cmdbuf.pipelineBarrier(vk::PipelineStageFlagBits::eAllCommands,
                                      vk::PipelineStageFlagBits::eTransfer,
                                      vk::DependencyFlagBits::eByRegion, {}, {}, fg_pre);
            const vk::ClearColorValue fg_black{std::array<float, 4>{0.0f, 0.0f, 0.0f, 1.0f}};
            fg_cmdbuf.clearColorImage(target, vk::ImageLayout::eTransferDstOptimal, fg_black, range);
            const vk::MemoryBarrier fg_clear{
                .srcAccessMask = vk::AccessFlagBits::eTransferWrite,
                .dstAccessMask = vk::AccessFlagBits::eTransferWrite,
            };
            fg_cmdbuf.pipelineBarrier(vk::PipelineStageFlagBits::eTransfer,
                                      vk::PipelineStageFlagBits::eTransfer,
                                      vk::DependencyFlagBits::eByRegion, fg_clear, {}, {});
            fg_cmdbuf.blitImage(generated, vk::ImageLayout::eTransferSrcOptimal, target,
                                vk::ImageLayout::eTransferDstOptimal,
                                MakeImageBlitFit(frame->width, frame->height, target_extent.width,
                                                 target_extent.height),
                                vk::Filter::eLinear);
            const std::array fg_post{
                vk::ImageMemoryBarrier{
                    .srcAccessMask = vk::AccessFlagBits::eTransferWrite,
                    .oldLayout = vk::ImageLayout::eTransferDstOptimal,
                    .newLayout = vk::ImageLayout::ePresentSrcKHR,
                    .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                    .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                    .image = target,
                    .subresourceRange = range,
                },
                vk::ImageMemoryBarrier{
                    .srcAccessMask = vk::AccessFlagBits::eTransferRead,
                    .dstAccessMask = vk::AccessFlagBits::eShaderWrite,
                    .oldLayout = vk::ImageLayout::eTransferSrcOptimal,
                    .newLayout = vk::ImageLayout::eGeneral,
                    .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                    .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                    .image = generated,
                    .subresourceRange = range,
                },
            };
            fg_cmdbuf.pipelineBarrier(vk::PipelineStageFlagBits::eTransfer,
                                      vk::PipelineStageFlagBits::eAllCommands,
                                      vk::DependencyFlagBits::eByRegion, {}, {}, fg_post);
            SubmitInfo fg_info{};
            fg_info.AddWait(swapchain.GetImageAcquiredSemaphore());
            fg_info.AddWait(frame->ready_semaphore, frame->ready_tick);
            fg_info.AddSignal(swapchain.GetPresentReadySemaphore());
            present_scheduler.Flush(fg_info);
            {
                std::scoped_lock submit_lock{Scheduler::submit_mutex};
                if (!swapchain.Present()) {
                    swapchain.Recreate(window.GetWidth(), window.GetHeight());
                }
            }
            ++fg_presented;
            fg_credit -= 1.0f;
            if (EmulatorSettings.GetPresentMode() != "Fifo") {
                std::this_thread::sleep_for(std::chrono::duration<float, std::milli>(
                    std::clamp(fg_interval[frame->id] * 0.5f, 0.0f, 25.0f)));
            }
        }
        const auto now = std::chrono::steady_clock::now();
        if (fg_stats_since.time_since_epoch().count() == 0) {
            fg_stats_since = now;
        } else if (now - fg_stats_since >= std::chrono::seconds(10)) {
            const auto& stats = ctr_framegen->GetStats();
            const float seconds = std::chrono::duration<float>(now - fg_stats_since).count();
            std::fprintf(stderr, "CTR_FRAMEGEN %.1f game FPS -> %.1f shown (target %.0f); %llu generated, %llu history resets (%s)\\n",
                         float(fg_flips) / seconds, float(fg_flips + fg_presented) / seconds,
                         1000.0f / FrameGenTargetMs(),
                         static_cast<unsigned long long>(stats.generated),
                         static_cast<unsigned long long>(stats.resets),
                         EmulatorSettings.GetPresentMode().c_str());
            std::fflush(stderr);
            fg_stats_since = now;
            fg_flips = fg_presented = 0;
        }
    }

    if (!swapchain.AcquireNextImage()) {
        swapchain.Recreate(window.GetWidth(), window.GetHeight());
''', 'present generated frame first')
for marker in ("ImGui::", "Screenshot", "png_"):
    if marker in t:  # report position
        print(t[max(0, t.find(marker) - 300):t.find(marker) + 100])
        raise SystemExit(f'make_patched: {marker} reference left in vk_presenter.cpp')
target = out / 'video_core/renderer_vulkan/vk_presenter.cpp'
target.parent.mkdir(parents=True, exist_ok=True)
header = ('// ctrport: generated by ctrport/gpu/make_patched.py from shadPS4 v0.18.0\n'
          '// src/video_core/renderer_vulkan/vk_presenter.cpp; do not edit.\n')
target.write_text(header + t, encoding='utf-8')
print('patched', target)


# Diagnostic only: observe actual Vulkan attachments selected for depth-tested
# draws. No desired/configured dimensions are used as evidence.
p = src / 'video_core/renderer_vulkan/vk_rasterizer.cpp'
t = p.read_text(encoding='utf-8')
t = '#include <cstdio>\n#include <cstdlib>\n#include <set>\n#include <tuple>\n' + t
marker = '''    if (state.num_layers == std::numeric_limits<u16>::max()) {
        state.num_layers = 1;
    }
'''
t = replace_once(t, marker, marker + r'''
    static const bool trace_scene = std::getenv("CTR_SCENE_TRACE") != nullptr;
    if (trace_scene && state.depth_stencil_attachment.has_depth &&
        regs.depth_control.depth_enable && cb_descs[0].first && db_desc.first) {
        const auto& color = texture_cache.GetImage(cb_descs[0].first).info.size;
        const auto& depth = texture_cache.GetImage(db_desc.first).info.size;
        auto dims = std::make_tuple(color.width, color.height, depth.width, depth.height,
                                          state.width, state.height);
        static std::set<decltype(dims)> seen;
        if (seen.size() < 256 && seen.insert(dims).second) {
            std::fprintf(stderr, "CTR_SCENE_TARGET color=%ux%u depth=%ux%u render=%ux%u\n",
                         color.width, color.height, depth.width, depth.height,
                         static_cast<unsigned>(state.width), static_cast<unsigned>(state.height));
            std::fflush(stderr);
        }
    }
''', 'scene attachment observation')
target = out / 'video_core/renderer_vulkan/vk_rasterizer.cpp'
target.write_text('// Generated from shadPS4 v0.18.0 by make_patched.py; do not edit.\n' + t, encoding='utf-8')
print('patched', target)

# Scene upscaling: keep pinned upstream immutable, including its headers.
overlay = Path(__file__).parent / 'upscaling'
def emit(relative, contents):
    path = out / relative
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text('// Generated by ctrport/gpu/make_patched.py\n' + contents, encoding='utf-8')

t = '#include "upscaling/readback.h"\n#include "upscaling/settings.h"\n#include "upscaling/ctr_dlss.h"\n#include <cmath>\n' + t
marker = '    memory->SetRasterizer(this);'
t = replace_once(t, marker, marker + '\n    ctr_dlss = std::make_unique<CtrDlss>(instance, scheduler, texture_cache);', 'create scene DLSS')
marker = '    // Prefetch render targets to handle overlaps with bound textures (e.g. mipgen)'
t = replace_once(t, marker, '''    const auto& regs_before = liverpool->regs;
    const bool display = regs_before.color_buffers[0] &&
        CtrUpscaling::IsDisplay(regs_before.color_buffers[0].Address());
    if (display && !last_display) {
        upscale_scene = scene_seen;
        scene_seen = false;
        upscale_pending = CtrUpscaling::Mode() != 0 && bool(cb_descs[0].first);
        upscale_source = cb_descs[0].first;
        upscale_source_uid = upscale_source ? texture_cache.GetImage(upscale_source).image_uid : 0;
        upscale_done = false;
        ctr_dlss->OnFrameEnd();
    } else if (!display) { upscale_pending = false; }
    last_display = display;
''' + marker, 'detect registered display pass')
marker = '''        db_desc.first = {};
    }
}'''
t = replace_once(t, marker, '''        db_desc.first = {};
    }
    const bool scene_gbuffer = std::popcount(key.mrt_mask) == 4 && db_desc.first && cb_descs[3].first;
    scene_seen |= scene_gbuffer;
    ctr_gbuffer_draw = scene_gbuffer && ctr_dlss->Enabled();
    if (ctr_gbuffer_draw) {
        const auto& v = regs.viewports[0];
        const auto& c = regs.viewport_control;
        ctr_dlss->OnGBufferPass(db_desc.first, cb_descs[3].first,
            {c.xscale_enable ? v.xscale : 1.f, c.yscale_enable ? v.yscale : 1.f,
             c.xoffset_enable ? v.xoffset : 0.f, c.yoffset_enable ? v.yoffset : 0.f,
             c.zscale_enable ? v.zscale : 1.f, c.zoffset_enable ? v.zoffset : 0.f});
    }
}''', 'remember CTR depth and velocity')
marker = '        const auto vsharp = desc.GetSharp(stage);'
t = replace_once(t, marker, marker + '''
        if (ctr_gbuffer_draw && (stage.stage == Shader::Stage::Vertex || stage.stage == Shader::Stage::Export) &&
            !desc.IsSpecial() && ctr_dlss->WantsCameraConstants() && vsharp.GetSize() >= 192 && vsharp.GetSize() <= 4096 &&
            memory->ClampRangeSize(vsharp.base_address, vsharp.GetSize()) == vsharp.GetSize()) {
            ctr_dlss->OnCameraConstants(reinterpret_cast<const float*>(vsharp.base_address),u32(vsharp.GetSize()));
        }
        if (ctr_gbuffer_draw && std::getenv("CTR_SCENE_TRACE")) {
            static std::set<std::tuple<u32,u32,u32>> buffers;
            const auto entry = std::make_tuple(u32(stage.stage),u32(vsharp.GetSize()),u32(desc.buffer_type));
            if (buffers.size() < 80 && buffers.insert(entry).second) {
                std::fprintf(stderr,"CTR_CAMERA_BUFFER stage=%u size=%u type=%u\\n",u32(stage.stage),u32(vsharp.GetSize()),u32(desc.buffer_type));
                if (!desc.IsSpecial() && vsharp.GetSize() >= 320 && vsharp.GetSize() < 4096 &&
                    memory->ClampRangeSize(vsharp.base_address,vsharp.GetSize()) == vsharp.GetSize()) {
                    if (const char* dir = std::getenv("CTR_UPSCALE_CAPTURE_DIR")) {
                        auto path = std::filesystem::path(dir) / ("camera_" + std::to_string(u32(stage.stage)) + "_" + std::to_string(vsharp.GetSize()) + ".bin");
                        if (FILE* f = std::fopen(path.string().c_str(),"wb")) {
                            std::fwrite(reinterpret_cast<const void*>(vsharp.base_address),1,vsharp.GetSize(),f); std::fclose(f);
                        }
                    }
                }
            }
        }
''', 'read retail camera constants')
marker = '''            image_infos.emplace_back(VK_NULL_HANDLE, *image_view.image_view,
                                     image.backing->state.layout);'''
t = replace_once(t, marker, '''            auto view = *image_view.image_view;
            auto layout = image.backing->state.layout;
            if (upscale_pending && image_id == upscale_source && !is_storage &&
                !image.info.props.is_depth) {
                if (auto result = UpscaleScene(image_id, view, image_view.info.format,
                                               image_view.info.mapping)) {
                    view = result;
                    layout = upscale_layout;
                }
            }
            image_infos.emplace_back(VK_NULL_HANDLE, view, layout);''', 'replace scene sampler before HUD')
marker = '    dynamic_state.SetViewports(viewports);'
t = replace_once(t, marker, '''    const auto jitter = ctr_dlss->DrawJitter(db_desc.first,
        regs.num_indices <= 6 && regs.num_instances.NumInstances() <= 1);
    for (auto& viewport : viewports) { viewport.x += jitter[0]; viewport.y += jitter[1]; }
''' + marker, 'subpixel scene jitter')
t = replace_once(t, '} // namespace Vulkan', (overlay/'rasterizer_methods.inc').read_text()+'\n} // namespace Vulkan', 'scene upscaler methods')
emit('video_core/renderer_vulkan/vk_rasterizer.cpp', t)
h = (src/'video_core/renderer_vulkan/vk_rasterizer.h').read_text()
h = '#include "upscaling/ctr_dlss.h"\n#include "video_core/renderer_vulkan/host_passes/fsr_pass.h"\n'+h
h = replace_once(h, '    bool attachment_feedback_loop{};', '    bool attachment_feedback_loop{};\n'+(overlay/'rasterizer_members.inc').read_text(), 'upscaling members')
emit('video_core/renderer_vulkan/vk_rasterizer.h', h)
h = (src/'video_core/texture_cache/texture_cache.h').read_text()
h = replace_once(h, '    [[nodiscard]] Image& GetImage(ImageId id) {', '''    Image* TryGetImage(ImageId id, u64 uid) {
        if (!id || !slot_images.is_allocated(id)) return nullptr;
        auto& image = slot_images[id];
        return image.image_uid == uid ? &image : nullptr;
    }
    [[nodiscard]] Image& GetImage(ImageId id) {''', 'validate retained image id')
emit('video_core/texture_cache/texture_cache.h', h)
h = (src/'video_core/renderer_vulkan/host_passes/fsr_pass.h').read_text()
h = replace_once(h, 'private:', '''    vk::Image LastOutputImage() const {
        return available_imgs[(cur_image + available_imgs.size() - 1) % available_imgs.size()].output_image;
    }
private:''', 'FSR evidence output handle')
emit('video_core/renderer_vulkan/host_passes/fsr_pass.h', h)
for name in ['vk_instance.cpp', 'vk_platform.cpp']:
    p = 'video_core/renderer_vulkan/'+name
    s = (src/p).read_text()
    s = replace_once(s, 'namespace Vulkan {', '#include "upscaling/vk_dlss_ngx.h"\n\nnamespace Vulkan {', 'DLSS include after platform macros')
    if name == 'vk_instance.cpp':
        s = s.replace('static_vector<const char*, 32> enabled_extensions', 'static_vector<const char*, 64> enabled_extensions')
        marker = '    const auto vk11_features = feature_chain.get<vk::PhysicalDeviceVulkan11Features>();'
        s = replace_once(s, marker, '''    if (auto* dlss = DlssNgx::Get()) {
        const std::vector<std::string> already(enabled_extensions.begin(), enabled_extensions.end());
        for (const char* name : dlss->DeviceExtensions(*instance, physical_device, already)) {
            enabled_extensions.push_back(name);
        }
    }
''' + marker, 'DLSS device extensions')
        marker = '    VULKAN_HPP_DEFAULT_DISPATCHER.init(*device);'
        s = replace_once(s, marker, marker+'\n    if (auto* dlss = DlssNgx::Get()) dlss->Initialize(*instance, physical_device, *device);', 'initialize NGX')
        # FSR 3 (FSR-Vulkan) stores to images through views declared without a format.
        s = replace_once(s, '                .shaderStorageImageMultisample = features.shaderStorageImageMultisample,',
                         '                .shaderStorageImageMultisample = features.shaderStorageImageMultisample,\n'
                         '                .shaderStorageImageWriteWithoutFormat = features.shaderStorageImageWriteWithoutFormat,',
                         'FSR 3 storage without format')
        s = replace_once(s, 'Instance::~Instance() {', 'Instance::~Instance() {\n    if (auto* dlss = DlssNgx::Get()) dlss->Shutdown();', 'NGX shutdown')
    else:
        marker = '    const auto extensions = GetLayerExtensions(GetInstanceExtensions(window_type, true), layers);'
        s = replace_once(s, marker, marker.replace('const auto', 'auto')+'\n    if (auto* dlss = DlssNgx::Get()) dlss->AppendInstanceExtensions(extensions);', 'NGX instance extensions')
    emit(p,s)
p = 'core/libraries/videoout/driver.cpp'
s = '#include "upscaling/settings.h"\n'+(src/p).read_text()
s = replace_once(s, '        presenter->RegisterVideoOutSurface(group, address);', '        CtrUpscaling::display_buffers.at(startIndex + i).store(address);\n        presenter->RegisterVideoOutSurface(group, address);', 'register display addresses')
s = replace_once(s, '''        if (buffer.group_index != attributeIndex) {
            continue;
        }
        buffer.group_index = -1;''', '''        if (buffer.group_index != attributeIndex) {
            continue;
        }
        CtrUpscaling::RemoveDisplay(buffer.address_left);
        buffer.group_index = -1;''', 'unregister display addresses')
emit(p,s)
