// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include <chrono>
#include <fstream>
#include "upscaling/readback.h"
namespace Vulkan {
// Counts newly presented guest frames, excluding reused host refreshes. The CSV
// records monotonic timestamps; benchmark intervals are selected after race entry.
inline void RecordGuestPresentation() {
    static const char* path = std::getenv("CTR_FRAME_TIMES");
    if (!path) return;
    static std::ofstream stream(path);
    static u64 count = 0;
    const auto ns = std::chrono::duration_cast<std::chrono::nanoseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count();
    stream << ++count << ',' << ns << '\n';
    if (count % 60 == 0) stream.flush();
}
inline void CapturePresentedFrame(const Instance& instance, Scheduler& scheduler,
                                 vk::Image image, vk::Extent2D size, vk::Format format) {
    static const char* trigger = std::getenv("CTR_PRESENT_CAPTURE_TRIGGER");
    if (!trigger) return;
    static unsigned polls = 0;
    if (++polls % 12 || std::remove(trigger)) return;
    std::fprintf(stderr,"CTR_PRESENT_CAPTURE format=%s size=%ux%u\n",vk::to_string(format).c_str(),size.width,size.height);
    if (format != vk::Format::eB8G8R8A8Unorm && format != vk::Format::eB8G8R8A8Srgb &&
        format != vk::Format::eR8G8B8A8Unorm && format != vk::Format::eR8G8B8A8Srgb) {
        std::fprintf(stderr,"CTR_PRESENT_CAPTURE unsupported format\n");return;
    }
    UpscaleReadback(instance,scheduler,image,vk::ImageLayout::eGeneral,size,4,"present_rgba8");
}
}
