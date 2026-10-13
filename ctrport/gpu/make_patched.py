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
