#include "display_controls/gpu_policy.hpp"
#include "ctr_inputs.h"
#include "readback.h"
// SPDX-License-Identifier: GPL-2.0-or-later
#include "ctr_dlss.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

#include <vk_mem_alloc.h>

#include "settings.h"
#include "vk_dlss_ngx.h"
#include "ffx_vk_portable.h"
#include "video_core/renderer_vulkan/vk_instance.h"
#include "video_core/renderer_vulkan/vk_platform.h"
#include "video_core/renderer_vulkan/vk_scheduler.h"
#include "video_core/renderer_vulkan/vk_shader_util.h"
#include "video_core/texture_cache/texture_cache.h"

namespace Vulkan {

namespace {

// Moving objects (CTR's velocity target, memory channel 2 marks them; their own motion in
// channels 1 and 0): CTR_DLSS_MV="sx,sy,bias,mode,cx,cy" adds pixels = v * s to the camera's
// motion, with e = channel - bias and v = e (mode 1) or sign(e) e^2 (mode 2); mode 0 (default)
// leaves them still on the screen, which measured best. CTR_DLSS_MV=nocamera drops the camera's
// motion (tests).
struct Decode {
    float sx = 0.0f, sy = 0.0f, bias = 127.0f / 255.0f;
    u32 mode = 0, cx = 1, cy = 0;
    bool camera = true;
};

const Decode& MotionDecode() {
    static const Decode decode = [] {
        Decode d;
        if (const char* env = std::getenv("CTR_DLSS_MV")) {
            if (std::strcmp(env, "nocamera") == 0) {
                d.camera = false;
            } else {
                std::sscanf(env, "%f,%f,%f,%u,%u,%u", &d.sx, &d.sy, &d.bias, &d.mode, &d.cx, &d.cy);
                d.cx &= 3;
                d.cy &= 3;
                d.mode = std::min(d.mode, 2u);
            }
        }
        std::printf("CTR DLSS: motion: camera %s; moving objects %s (channels %u/%u - %.4f) * %g/%g px\n",
                    d.camera ? "reprojected" : "off",
                    d.mode == 0 ? "still" : d.mode == 1 ? "linear" : "squared", d.cx, d.cy, d.bias,
                    d.sx, d.sy);
        return d;
    }();
    return decode;
}

int DebugMode() {
    static const int mode = [] {
        const char* env = std::getenv("CTR_DLSS_DEBUG");
        return env ? std::atoi(env) : 0;
    }();
    return mode;
}

/// CTR_DLSS_DUMP_TRIGGER=<file>: saves the next CTR_DLSS_DUMP_FRAMES (3) input frames
/// in CTR_DLSS_DUMP_DIR (default runtime/user/upscaling) and prints camera constants.
/// Returns the frame number to save or -1.
int DumpFrame() {
    static const char* trigger = std::getenv("CTR_DLSS_DUMP_TRIGGER");
    static u32 polls = 0;
    static int remaining = 0, index = 0;
    if (!trigger) {
        return -1;
    }
    if (remaining == 0) {
        if (++polls % 30 != 0 || std::remove(trigger) != 0) {
            return -1;
        }
        const char* frames = std::getenv("CTR_DLSS_DUMP_FRAMES");
        remaining = frames ? std::clamp(std::atoi(frames), 1, 16) : 3;
    }
    --remaining;
    return index++;
}

/// Column-major 4x4 matrices (the game's layout).
using Mat4 = std::array<double, 16>;
Mat4 Multiply(const Mat4& a, const Mat4& b) {
    Mat4 r{};
    for (int col = 0; col < 4; ++col) {
        for (int row = 0; row < 4; ++row) {
            double sum = 0.0;
            for (int k = 0; k < 4; ++k) {
                sum += a[k * 4 + row] * b[col * 4 + k];
            }
            r[col * 4 + row] = sum;
        }
    }
    return r;
}
bool Invert(const Mat4& m, Mat4& out) {
    // Gauss-Jordan on a row-major copy.
    double a[4][8];
    for (int row = 0; row < 4; ++row) {
        for (int col = 0; col < 4; ++col) {
            a[row][col] = m[col * 4 + row];
            a[row][col + 4] = row == col ? 1.0 : 0.0;
        }
    }
    for (int col = 0; col < 4; ++col) {
        int pivot = col;
        for (int row = col + 1; row < 4; ++row) {
            if (std::abs(a[row][col]) > std::abs(a[pivot][col])) pivot = row;
        }
        if (std::abs(a[pivot][col]) < 1e-12) return false;
        if (pivot != col) {
            for (int k = 0; k < 8; ++k) std::swap(a[pivot][k], a[col][k]);
        }
        const double inv = 1.0 / a[col][col];
        for (int k = 0; k < 8; ++k) a[col][k] *= inv;
        for (int row = 0; row < 4; ++row) {
            if (row == col) continue;
            const double f = a[row][col];
            for (int k = 0; k < 8; ++k) a[row][k] -= f * a[col][k];
        }
    }
    for (int row = 0; row < 4; ++row) {
        for (int col = 0; col < 4; ++col) {
            out[col * 4 + row] = a[row][col + 4];
        }
    }
    return true;
}
Mat4 Load(const float* f) {
    Mat4 m{};
    for (int i = 0; i < 16; ++i) m[i] = f[i];
    return m;
}

float Halton(u32 index, u32 base) {
    float f = 1.0f, r = 0.0f;
    while (index > 0) {
        f /= float(base);
        r += f * float(index % base);
        index /= base;
    }
    return r;
}

vk::Format Linear(vk::Format format) {
    switch (format) {
    case vk::Format::eR8G8B8A8Srgb:
        return vk::Format::eR8G8B8A8Unorm;
    case vk::Format::eB8G8R8A8Srgb:
        return vk::Format::eB8G8R8A8Unorm;
    default:
        return format;
    }
}

} // namespace

CtrDlss::CtrDlss(const Instance& instance_, Scheduler& scheduler_,
                 VideoCore::TextureCache& texture_cache_)
    : instance{instance_}, scheduler{scheduler_}, texture_cache{texture_cache_} {}

CtrDlss::~CtrDlss() {
    if (fsr3_context) ffxVkPortableUpscaleContextDestroy(fsr3_context); ctrport::settings::display::temporal_capabilities.fetch_and(~1u, std::memory_order_relaxed);
}

// The FSR 3 context for these sizes; made again when they change. A failure turns FSR 3 off
// for the session, logged, without substituting another upscaler.
bool CtrDlss::EnsureFsr3(u32 w, u32 h, u32 ow, u32 oh) {
    const std::array<u32, 4> size{w, h, ow, oh};
    if (fsr3_context && fsr3_size == size) return true;
    scheduler.Finish();
    if (fsr3_context) {
        ffxVkPortableUpscaleContextDestroy(fsr3_context); ctrport::settings::display::temporal_capabilities.fetch_and(~1u, std::memory_order_relaxed);
        fsr3_context = nullptr;
    }
    const auto physical = instance.GetPhysicalDevice();
    const auto chain = physical.getFeatures2<vk::PhysicalDeviceFeatures2, vk::PhysicalDeviceVulkan12Features,
                                             vk::PhysicalDeviceVulkan13Features>();
    const auto& core = chain.get<vk::PhysicalDeviceFeatures2>().features;
    const auto& vk12 = chain.get<vk::PhysicalDeviceVulkan12Features>();
    const auto& vk13 = chain.get<vk::PhysicalDeviceVulkan13Features>();
    FfxVkPortableDeviceInfo device_info{};
    device_info.structSize = sizeof(device_info);
    device_info.instance = instance.GetInstance();
    device_info.physicalDevice = physical;
    device_info.device = instance.GetDevice();
    device_info.getDeviceProcAddr = VULKAN_HPP_DEFAULT_DISPATCHER.vkGetDeviceProcAddr;
    device_info.queue = instance.GetGraphicsQueue();
    device_info.queueFamilyIndex = instance.GetGraphicsQueueFamilyIndex();
    device_info.shaderFloat16Enabled = vk12.shaderFloat16;
    device_info.subgroupSizeControlEnabled = vk13.subgroupSizeControl;
    device_info.synchronization2Enabled = vk13.synchronization2;
    device_info.shaderStorageImageWriteWithoutFormatEnabled = core.shaderStorageImageWriteWithoutFormat;
    FfxVkPortableUpscaleCreateInfo create_info{};
    create_info.structSize = sizeof(create_info);
    create_info.maxRenderSize = {w, h};
    create_info.maxOutputSize = {ow, oh};
    if (const u64 issues = ffxVkPortableValidateUpscaleCreateInfo(&create_info)) {
        std::printf("FSR 3: sizes %ux%u -> %ux%u not supported (issues 0x%llx); off for this session\n", w, h, ow, oh,
                    static_cast<unsigned long long>(issues));
        fsr3_failed = true; ctrport::settings::display::temporal_capabilities.fetch_and(~1u, std::memory_order_relaxed);
        return false;
    }
    if (ffxVkPortableUpscaleContextCreate(&device_info, &create_info, &fsr3_context) != FFX_VK_PORTABLE_OK) {
        std::printf("FSR 3: context creation failed for %ux%u -> %ux%u; off for this session\n", w, h, ow, oh);
        fsr3_context = nullptr;
        fsr3_failed = true; ctrport::settings::display::temporal_capabilities.fetch_and(~1u, std::memory_order_relaxed);
        return false;
    }
    ctrport::settings::display::temporal_capabilities.fetch_or(1, std::memory_order_relaxed);
    fsr3_size = size;
    reset = true;
    std::printf("FSR 3: %ux%u -> %ux%u\n", w, h, ow, oh);
    std::fflush(stdout);
    return true;
}
void CtrDlss::CameraPlanes(float& near_plane, float& far_plane, float& fov) const {
    // Planes from the retail projection (floats 48-63, column-major): [5] = 1/tan(fov/2),
    // [10] = A, [14] = B, near = B/(A-1), far = B/(A+1); CTR's own values otherwise.
    near_plane = 16.0f;
    far_plane = 150000.0f;
    fov = 1.2870f;
    if (camera_valid) {
        const float* p = &camera[48];
        const float a = p[10], b = p[14];
        if (p[5] > 0.0f && std::isfinite(a) && std::isfinite(b) && a != 1.0f && a != -1.0f) {
            fov = 2.0f * std::atan(1.0f / p[5]);
            near_plane = std::abs(b / (a - 1.0f));
            far_plane = std::abs(b / (a + 1.0f));
        }
    }
    if (!(near_plane > 0.0f) || !(far_plane > near_plane) || !(fov > 0.01f && fov < 3.1f)) {
        near_plane = 16.0f;
        far_plane = 150000.0f;
        fov = 1.2870f;
    }
}

void CtrDlss::NoteVectors(u32 w, u32 h) {
    auto& v = last_vectors;
    ++v.sequence;
    v.depth = vk::Image(depth_image);
    v.motion = vk::Image(motion_image);
    v.width = w;
    v.height = h;
    v.jitter[0] = frame_jitter[0];
    v.jitter[1] = frame_jitter[1];
    CameraPlanes(v.near_plane, v.far_plane, v.fov);
}

bool CtrDlss::Enabled() const {
    return !failed && ((CtrUpscaling::Mode() == 2 && !dlss_failed) || (CtrUpscaling::Mode() == 3 && !fsr3_failed));
}

void CtrDlss::OnGBufferPass(VideoCore::ImageId depth, VideoCore::ImageId velocity_,
                            const Viewport& viewport_) {
    scene_depth = depth;
    velocity = velocity_;
    // The texture cache may delete either before the display pass (loading screens reuse their
    // memory): the ids are only trusted with these uids.
    scene_depth_uid = texture_cache.GetImage(depth).image_uid;
    velocity_uid = texture_cache.GetImage(velocity_).image_uid;
    viewport = viewport_;
    scene_this_frame = true;
}

void CtrDlss::OnCameraConstants(const float* data, u32 bytes) {
    // Retail title-scene captures: WVP at 0, world at 16, projection at 32
    // (400/2864-byte shaders) or 48 (464/2928-byte shaders). Derive the camera
    // independently of the first object's transform. Do not treat world-view
    // or previous-object matrices as the camera (see UPSCALING.md).
    if (bytes < 192 || bytes > 4096 || camera_this_frame) return;
    int projection_offset = -1;
    for (int offset : {32,48}) {
        if (bytes < u32((offset+16)*4)) continue;
        const float* p = data+offset;
        if (std::isfinite(p[0]) && std::isfinite(p[5]) && p[0] > .01f && p[5] > .01f &&
            p[0] < 100 && p[5] < 100 && std::abs(p[11]+1) < .0001f &&
            std::abs(p[15]) < .0001f && p[10] < -.9f && p[14] < 0 &&
            std::abs(p[1])+std::abs(p[2])+std::abs(p[3])+std::abs(p[4])+
            std::abs(p[6])+std::abs(p[7])+std::abs(p[8])+std::abs(p[9])+
            std::abs(p[12])+std::abs(p[13]) < .001f) { projection_offset=offset; break; }
    }
    if (projection_offset < 0) return;
    Mat4 inv_projection{}, inv_world{};
    const auto projection = Load(data+projection_offset);
    if (!Invert(projection,inv_projection) || !Invert(Load(data+16),inv_world)) return;
    auto view = Multiply(Multiply(inv_projection,Load(data)),inv_world);
    for (double v : view) if (!std::isfinite(v)) return;
    if (std::abs(view[3])+std::abs(view[7])+std::abs(view[11]) > .001 ||
        std::abs(view[15]-1) > .001) return;
    view[3]=view[7]=view[11]=0; view[15]=1;
    if (!previous_view_valid) reset = true;
    for (int i=0;i<16;++i) {
        camera[32+i]=float(view[i]); camera[48+i]=float(projection[i]);
        camera[64+i]=float(previous_view_valid ? previous_view[i] : view[i]);
    }
    camera_this_frame = true;
    camera_valid = true;
}

void CtrDlss::OnFrameEnd() {
    if (camera_this_frame) { previous_view=Load(&camera[32]); previous_view_valid=true; }
    else { previous_view_valid=false; }
    frame_jitter = jitter;
    if (!Enabled()) {
        jitter = {};
        reset = true;
        return;
    }
    jitter_index = jitter_index % std::max(jitter_phases, 1u) + 1;
    jitter = {Halton(jitter_index, 2) - 0.5f, Halton(jitter_index, 3) - 0.5f};
    if (std::getenv("CTR_DLSS_NO_JITTER")) {
        jitter = {};
    }
}

std::array<float, 2> CtrDlss::DrawJitter(VideoCore::ImageId depth, bool full_screen) const {
    if (full_screen || !depth || depth != scene_depth || !Enabled()) {
        return {};
    }
    return jitter;
}

vk::ImageView CtrDlss::SourceView(const VideoCore::Image& image, vk::Format format,
                                  vk::ImageAspectFlags aspect, int slot) {
    auto& entry = source_views[slot];
    const vk::Image handle = image.GetImage();
    if (entry.view && entry.image == handle && entry.uid == image.image_uid &&
        entry.format == format) {
        return *entry.view;
    }
    if (entry.view) {
        scheduler.DeferOperation([view = std::move(entry.view)] {});
    }
    entry.image = handle;
    entry.uid = image.image_uid;
    entry.format = format;
    entry.view = Check(instance.GetDevice().createImageViewUnique({
        .image = handle,
        .viewType = vk::ImageViewType::e2D,
        .format = format,
        .subresourceRange = {aspect, 0, 1, 0, 1},
    }));
    return *entry.view;
}

// The display pass reads the game's frame through a view with its own format and channel
// swizzle (CTR's swaps red and blue); the upscaled frame is read the same way.
vk::ImageView CtrDlss::OutputView(vk::Format format, vk::ComponentMapping mapping) {
    for (auto& entry : output_views) {
        if (entry.view && entry.format == format && entry.mapping == mapping) {
            return *entry.view;
        }
    }
    auto& entry = output_views[output_view_next++ % output_views.size()];
    if (entry.view) {
        scheduler.DeferOperation([view = std::move(entry.view)] {});
    }
    const vk::ImageViewUsageCreateInfo usage{.usage = vk::ImageUsageFlagBits::eSampled};
    entry.format = format;
    entry.mapping = mapping;
    entry.view = Check(instance.GetDevice().createImageViewUnique({
        .pNext = &usage,
        .image = vk::Image(output_image),
        .viewType = vk::ImageViewType::e2D,
        .format = format,
        .components = mapping,
        .subresourceRange = {vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1},
    }));
    return *entry.view;
}

void CtrDlss::Skip(const char* reason) {
    reset = true;
    if (reason != last_skip) {
        last_skip = reason;
        std::printf("CTR DLSS: skipped (%s)\n", reason);
        std::fflush(stdout);
    }
}

void CtrDlss::CreatePipelines() {
    if (pipelines[0]) {
        return;
    }
    const auto device = instance.GetDevice();
    const std::array<vk::DescriptorSetLayoutBinding, 5> bindings{{
        {0, vk::DescriptorType::eSampledImage, 1, vk::ShaderStageFlagBits::eCompute},
        {1, vk::DescriptorType::eSampledImage, 1, vk::ShaderStageFlagBits::eCompute},
        {2, vk::DescriptorType::eStorageImage, 1, vk::ShaderStageFlagBits::eCompute},
        {3, vk::DescriptorType::eStorageImage, 1, vk::ShaderStageFlagBits::eCompute},
        {4, vk::DescriptorType::eStorageImage, 1, vk::ShaderStageFlagBits::eCompute},
    }};
    desc_layout = Check(device.createDescriptorSetLayoutUnique({
        .flags = vk::DescriptorSetLayoutCreateFlagBits::ePushDescriptorKHR,
        .bindingCount = u32(bindings.size()),
        .pBindings = bindings.data(),
    }));
    const vk::PushConstantRange push{vk::ShaderStageFlagBits::eCompute, 0, 128};
    pipeline_layout = Check(device.createPipelineLayoutUnique({
        .setLayoutCount = 1,
        .pSetLayouts = &*desc_layout,
        .pushConstantRangeCount = 1,
        .pPushConstantRanges = &push,
    }));
    const auto compute = [&](std::vector<std::string> defines) {
        const auto module = Compile(CTR_INPUTS, vk::ShaderStageFlagBits::eCompute, device, defines);
        auto result = Check(device.createComputePipelineUnique(
            {}, vk::ComputePipelineCreateInfo{
                    .stage = {.stage = vk::ShaderStageFlagBits::eCompute,
                              .module = module,
                              .pName = "main"},
                    .layout = *pipeline_layout,
                }));
        device.destroyShaderModule(module);
        return result;
    };
    pipelines[0] = compute({});
    pipelines[1] = compute({"DEPTH_MSAA"});
    pipelines[2] = compute({"VELOCITY_MSAA"});
    pipelines[3] = compute({"DEPTH_MSAA", "VELOCITY_MSAA"});
}

bool CtrDlss::EnsureResources(u32 w, u32 h, u32 ow, u32 oh, vk::Format format) {
    if (output_image && w == width && h == height && ow == out_width && oh == out_height &&
        format == output_format) {
        return true;
    }
    const auto device = instance.GetDevice();
    scheduler.Finish();
    width = w;
    height = h;
    out_width = ow;
    out_height = oh;
    output_format = format;
    reset = true;

    const auto allocator = instance.GetAllocator();
    const auto make = [&](VideoCore::UniqueImage& image, vk::UniqueImageView* view,
                          vk::Format format, u32 iw, u32 ih, vk::ImageUsageFlags usage,
                          const void* next = nullptr, vk::ImageCreateFlags flags = {}) {
        if (view) {
            view->reset();
        }
        image.Destroy();
        image = VideoCore::UniqueImage(device, allocator);
        image.Create(vk::ImageCreateInfo{
            .pNext = next,
            .flags = flags,
            .imageType = vk::ImageType::e2D,
            .format = format,
            .extent = {iw, ih, 1},
            .mipLevels = 1,
            .arrayLayers = 1,
            .samples = vk::SampleCountFlagBits::e1,
            .tiling = vk::ImageTiling::eOptimal,
            .usage = usage,
            .initialLayout = vk::ImageLayout::eUndefined,
        });
        if (view) {
            *view = Check(device.createImageViewUnique({
                .image = vk::Image(image),
                .viewType = vk::ImageViewType::e2D,
                .format = format,
                .subresourceRange = {vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1},
            }));
        }
    };
    const auto inputs = vk::ImageUsageFlagBits::eStorage | vk::ImageUsageFlagBits::eSampled |
                        vk::ImageUsageFlagBits::eTransferSrc;
    make(depth_image, &depth_view, vk::Format::eR32Sfloat, w, h, inputs);
    make(motion_image, &motion_view, vk::Format::eR16G16Sfloat, w, h, inputs);
    make(debug_image, &debug_view, vk::Format::eR8G8B8A8Unorm, w, h, inputs);
    // The output has the frame's memory layout (RGBA or BGRA) and is sampled through views like
    // the game's (sRGB or not, swizzled).
    static constexpr std::array<vk::Format, 4> formats{
        vk::Format::eR8G8B8A8Unorm, vk::Format::eR8G8B8A8Srgb, vk::Format::eB8G8R8A8Unorm,
        vk::Format::eB8G8R8A8Srgb};
    const vk::ImageFormatListCreateInfo list{.viewFormatCount = u32(formats.size()),
                                             .pViewFormats = formats.data()};
    for (auto& entry : output_views) {
        entry.view.reset();
    }
    make(output_image, nullptr, format, ow, oh,
         vk::ImageUsageFlagBits::eStorage | vk::ImageUsageFlagBits::eSampled |
             vk::ImageUsageFlagBits::eTransferSrc,
         &list, vk::ImageCreateFlagBits::eMutableFormat);
    {
        const vk::ImageViewUsageCreateInfo storage{.usage = vk::ImageUsageFlagBits::eStorage};
        output_storage_view = Check(device.createImageViewUnique({
            .pNext = &storage,
            .image = vk::Image(output_image),
            .viewType = vk::ImageViewType::e2D,
            .format = format,
            .subresourceRange = {vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1},
        }));
    }
    CreatePipelines();

    // Converted depth/motion and output start in General.
    const auto cmdbuf = scheduler.CommandBuffer();
    std::array<vk::ImageMemoryBarrier2, 4> barriers{};
    const std::array<vk::Image, 4> images{vk::Image(depth_image), vk::Image(motion_image),
                                          vk::Image(debug_image), vk::Image(output_image)};
    for (u32 i = 0; i < barriers.size(); ++i) {
        barriers[i] = vk::ImageMemoryBarrier2{
            .srcStageMask = vk::PipelineStageFlagBits2::eAllCommands,
            .dstStageMask = vk::PipelineStageFlagBits2::eAllCommands,
            .dstAccessMask = vk::AccessFlagBits2::eMemoryRead | vk::AccessFlagBits2::eMemoryWrite,
            .oldLayout = vk::ImageLayout::eUndefined,
            .newLayout = vk::ImageLayout::eGeneral,
            .image = images[i],
            .subresourceRange = {vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1},
        };
    }
    cmdbuf.pipelineBarrier2({.imageMemoryBarrierCount = u32(barriers.size()),
                             .pImageMemoryBarriers = barriers.data()});

    // FSR's recommended jitter sequence length: 8 times the area ratio.
    const float ratio = float(ow) / float(w);
    jitter_phases = std::clamp(u32(std::ceil(8.0f * ratio * ratio)), 8u, 72u);
    return true;
}

vk::ImageView CtrDlss::Upscale(VideoCore::ImageId source_id, vk::Format view_format,
                               vk::ComponentMapping mapping, vk::Extent2D out,
                               vk::ImageLayout& layout) {
    const bool scene = scene_this_frame;
    scene_this_frame = false;
    camera_valid = camera_this_frame;
    camera_this_frame = false;
    if (!Enabled()) {
        reset = true;
        return {};
    }
    if (!scene || !source_id || !scene_depth || !velocity) {
        Skip("no 3D scene this frame");
        return {};
    }
    if (!camera_valid) { Skip("no validated retail camera constants"); return {}; }
    VideoCore::Image* depth_ptr = texture_cache.TryGetImage(scene_depth, scene_depth_uid);
    VideoCore::Image* vel_ptr = texture_cache.TryGetImage(velocity, velocity_uid);
    constexpr auto registered = VideoCore::ImageFlagBits::Registered;
    if (!depth_ptr || !vel_ptr || False(depth_ptr->flags & registered) ||
        False(vel_ptr->flags & registered)) {
        Skip("the G-buffer was released");
        return {};
    }
    auto& source = texture_cache.GetImage(source_id);
    auto& depth = *depth_ptr;
    auto& vel = *vel_ptr;
    const u32 w = source.info.size.width, h = source.info.size.height;
    // The finished frame, the depth and the velocity share the render size; DLSS does not
    // downscale.
    if (depth.info.size.width != w || depth.info.size.height != h ||
        vel.info.size.width != w || vel.info.size.height != h || out.width < w ||
        out.height < h || w * 4 < out.width ||
        std::abs(double(w) / h - double(out.width) / out.height) > 0.02 ||
        !depth.info.props.is_depth) {
        static u32 logged = 0;
        if (logged++ < 4) {
            std::printf("CTR DLSS: not used: frame %ux%u, depth %ux%u, velocity %ux%u, output %ux%u\n",
                        w, h, depth.info.size.width, depth.info.size.height, vel.info.size.width,
                        vel.info.size.height, out.width, out.height);
        }
        Skip("sizes do not match");
        return {};
    }
    const u32 variant = (depth.info.num_samples > 1 ? 1u : 0u) | (vel.info.num_samples > 1 ? 2u : 0u);
    // Inside the display pass's draw: its rendering ends here (and resumes when it draws).
    scheduler.EndRendering();
    const vk::Format frame_format = Linear(source.info.pixel_format);
    if (frame_format != vk::Format::eR8G8B8A8Unorm && frame_format != vk::Format::eB8G8R8A8Unorm) {
        Skip("the frame is not 8-bit RGBA");
        return {};
    }
    if (!EnsureResources(w, h, out.width, out.height, frame_format)) {
        failed = true;
        std::printf("CTR DLSS: off for this session\n");
        return {};
    }

    const auto now = std::chrono::steady_clock::now();
    float frame_ms = 16.6f;
    if (last_frame.time_since_epoch().count() != 0) {
        frame_ms = std::clamp(
            std::chrono::duration<float, std::milli>(now - last_frame).count(), 1.0f, 100.0f);
        if (frame_ms >= 100.0f) {
            reset = true;  // a pause, a load, a menu
        }
    }
    last_frame = now;

    // DLSS when it is chosen and can run (an RTX GPU, the bridge and nvngx_dlss.dll next to the
    // game). Failures are logged without substituting another upscaler. A new DLSS feature
    // is made with the GPU idle, before this frame's
    // commands.
    bool use_dlss = false;
    if (CtrUpscaling::Mode() == 2 && !dlss_failed) {
        auto* dlss = DlssNgx::Get();
        if (dlss && dlss->IsAvailable()) {
            // The model: NVIDIA's transformer presets (the DLL's defaults, K/L/M by mode) or the
            // CNN one (preset E, deprecated by NVIDIA but still in DLSS 310). CTR_DLSS_PRESET
            // picks any preset number.
            static const char* preset_env = std::getenv("CTR_DLSS_PRESET");
            const u32 preset = preset_env ? u32(std::atoi(preset_env))
                               : 0u;
            const DlssNgx::FeatureDesc desc{w, h, out.width, out.height,
                                            DlssNgx::QualityForScale(float(out.width) / float(w)),
                                            false, preset, false};
            if (!dlss->HasFeature(desc)) {
                scheduler.Finish();
                if (dlss->CreateFeature(scheduler.CommandBuffer(), desc)) {
                    reset = true;
                    std::printf("DLSS: %ux%u -> %ux%u, quality %d, preset %u\n", w, h, out.width,
                                out.height, desc.quality, preset);
                } else {
                    dlss_failed = true;
                    std::printf("DLSS: the feature could not be created; upscaling disabled (error)\n");
                }
                std::fflush(stdout);
            }
            use_dlss = !dlss_failed;
        } else if (!dlss_warned) {
            dlss_warned = true;
            std::printf("DLSS: not available (%s); upscaling unavailable\n",
                        dlss ? dlss->Problem().c_str()
                             : "ctr_dlss.dll or nvngx_dlss.dll is not next to the game");
            std::fflush(stdout);
        }
    }
    const bool use_fsr3 = CtrUpscaling::Mode() == 3 && !fsr3_failed;
    if (!use_dlss && !use_fsr3) {
        if (CtrUpscaling::Mode() == 2) dlss_failed = true;
        return {};
    }
    if (use_dlss != last_dlss) {
        last_dlss = use_dlss;
        reset = true;
        std::printf("Temporal upscaling: %s\n", use_dlss ? "DLSS" : "FSR 3");
    }

    source.Transit(vk::ImageLayout::eShaderReadOnlyOptimal, vk::AccessFlagBits2::eShaderRead, {});
    depth.Transit(vk::ImageLayout::eShaderReadOnlyOptimal, vk::AccessFlagBits2::eShaderRead, {});
    vel.Transit(vk::ImageLayout::eShaderReadOnlyOptimal, vk::AccessFlagBits2::eShaderRead, {});
    const auto cmdbuf = scheduler.CommandBuffer();

    // 1. Inputs: sample 0 of the depth and the motion.
    const int dump_frame = DumpFrame();
    const bool dump = dump_frame >= 0;
    const int debug = DebugMode();
    const auto& decode = MotionDecode();
    const vk::ImageView depth_src =
        SourceView(depth, depth.info.pixel_format, vk::ImageAspectFlagBits::eDepth, 0);
    const vk::ImageView vel_src =
        SourceView(vel, Linear(vel.info.pixel_format), vk::ImageAspectFlagBits::eColor, 1);
    {
        const vk::DescriptorImageInfo infos[5] = {
            {{}, depth_src, vk::ImageLayout::eShaderReadOnlyOptimal},
            {{}, vel_src, vk::ImageLayout::eShaderReadOnlyOptimal},
            {{}, *depth_view, vk::ImageLayout::eGeneral},
            {{}, *motion_view, vk::ImageLayout::eGeneral},
            {{}, *debug_view, vk::ImageLayout::eGeneral},
        };
        std::array<vk::WriteDescriptorSet, 5> writes{};
        for (u32 i = 0; i < writes.size(); ++i) {
            writes[i] = {.dstBinding = i,
                         .descriptorCount = 1,
                         .descriptorType = i < 2 ? vk::DescriptorType::eSampledImage
                                                 : vk::DescriptorType::eStorageImage,
                         .pImageInfo = &infos[i]};
        }
        struct {
            float prev[3][4];
            float proj[4];
            float viewport[4];
            float depth_jitter[4];
            float object[4];
            u32 flags;
        } params{};
        // The camera: previous projection * previous view * inverse current view (floats 48, 64
        // and 32 of the G-buffer constants).
        bool camera_ok = false;
        if (camera_valid && decode.camera && viewport.xscale != 0.0f && viewport.yscale != 0.0f &&
            viewport.zscale != 0.0f) {
            const Mat4 view = Load(&camera[32]), proj = Load(&camera[48]),
                       prev_view = Load(&camera[64]);
            Mat4 inverse{};
            if (Invert(view, inverse)) {
                const Mat4 m = Multiply(Multiply(proj, prev_view), inverse);
                constexpr int rows[3] = {0, 1, 3};
                for (int r = 0; r < 3; ++r) {
                    for (int col = 0; col < 4; ++col) {
                        params.prev[r][col] = float(m[col * 4 + rows[r]]);
                    }
                }
                params.proj[0] = camera[48];
                params.proj[1] = camera[48 + 5];
                params.proj[2] = camera[48 + 10];
                params.proj[3] = camera[48 + 14];
                camera_ok = params.proj[0] != 0.0f && params.proj[1] != 0.0f;
            }
        }
        params.viewport[0] = viewport.xscale;
        params.viewport[1] = viewport.yscale;
        params.viewport[2] = viewport.xoffset;
        params.viewport[3] = viewport.yoffset;
        params.depth_jitter[0] = viewport.zscale;
        params.depth_jitter[1] = viewport.zoffset;
        params.depth_jitter[2] = frame_jitter[0];
        params.depth_jitter[3] = frame_jitter[1];
        params.object[0] = decode.sx;
        params.object[1] = decode.sy;
        params.object[2] = decode.bias;
        params.object[3] = float(decode.mode);
        params.flags = decode.cx | (decode.cy << 2) | (u32(dump ? 3 : std::clamp(debug, 0, 15)) << 4) |
                       (dump || debug > 0 ? 0x100u : 0u) | (camera_ok ? 0x200u : 0u);
        if (!camera_ok && !camera_warned) {
            camera_warned = true;
            std::printf("CTR DLSS: no camera this frame: world motion left out\n");
        }
        const vk::MemoryBarrier2 before{
            .srcStageMask = vk::PipelineStageFlagBits2::eAllCommands,
            .srcAccessMask = vk::AccessFlagBits2::eMemoryRead | vk::AccessFlagBits2::eMemoryWrite,
            .dstStageMask = vk::PipelineStageFlagBits2::eComputeShader,
            .dstAccessMask = vk::AccessFlagBits2::eShaderWrite,
        };
        cmdbuf.pipelineBarrier2({.memoryBarrierCount = 1, .pMemoryBarriers = &before});
        cmdbuf.bindPipeline(vk::PipelineBindPoint::eCompute, *pipelines[variant]);
        cmdbuf.pushDescriptorSetKHR(vk::PipelineBindPoint::eCompute, *pipeline_layout, 0, writes);
        cmdbuf.pushConstants(*pipeline_layout, vk::ShaderStageFlagBits::eCompute, 0,
                             sizeof(params), &params);
        cmdbuf.dispatch((w + 7) / 8, (h + 7) / 8, 1);
        const vk::MemoryBarrier2 after{
            .srcStageMask = vk::PipelineStageFlagBits2::eComputeShader,
            .srcAccessMask = vk::AccessFlagBits2::eShaderWrite,
            .dstStageMask = vk::PipelineStageFlagBits2::eAllCommands,
            .dstAccessMask = vk::AccessFlagBits2::eMemoryRead,
        };
        cmdbuf.pipelineBarrier2({.memoryBarrierCount = 1, .pMemoryBarriers = &after});
    }
    if (dump) {
        Dump(cmdbuf, dump_frame, source);
        source.Transit(vk::ImageLayout::eShaderReadOnlyOptimal, vk::AccessFlagBits2::eShaderRead, {});
        }
    if (debug > 0) {
        // The inputs instead of the upscaled frame (stretched by the display pass).
        layout = vk::ImageLayout::eGeneral;
        return *debug_view;
    }

    // 2. DLSS: its inputs in shader-read layout, the output in General.
    if (use_dlss) {
        const auto transition = [&](vk::Image image, vk::ImageLayout from, vk::ImageLayout to) {
            return vk::ImageMemoryBarrier2{
                .srcStageMask = vk::PipelineStageFlagBits2::eAllCommands,
                .srcAccessMask = vk::AccessFlagBits2::eMemoryWrite,
                .dstStageMask = vk::PipelineStageFlagBits2::eAllCommands,
                .dstAccessMask = vk::AccessFlagBits2::eMemoryRead | vk::AccessFlagBits2::eMemoryWrite,
                .oldLayout = from,
                .newLayout = to,
                .image = image,
                .subresourceRange = {vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1},
            };
        };
        const auto ro = vk::ImageLayout::eShaderReadOnlyOptimal, general = vk::ImageLayout::eGeneral;
        const std::array to_read{transition(vk::Image(depth_image), general, ro),
                                 transition(vk::Image(motion_image), general, ro)};
        cmdbuf.pipelineBarrier2({.imageMemoryBarrierCount = u32(to_read.size()),
                                 .pImageMemoryBarriers = to_read.data()});
        constexpr vk::ImageSubresourceRange color_range{vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1};
        const DlssNgx::Resource color{source.GetImage(),
                                      SourceView(source, source.info.pixel_format,
                                                 vk::ImageAspectFlagBits::eColor, 2),
                                      color_range, source.info.pixel_format, {w, h}};
        const DlssNgx::Resource depth_res{vk::Image(depth_image), *depth_view, color_range,
                                          vk::Format::eR32Sfloat, {w, h}};
        const DlssNgx::Resource motion_res{vk::Image(motion_image), *motion_view, color_range,
                                           vk::Format::eR16G16Sfloat, {w, h}};
        const DlssNgx::Resource output_res{vk::Image(output_image), *output_storage_view,
                                           color_range, output_format, {out.width, out.height}};
        const float sign = std::getenv("CTR_DLSS_JITTER_FLIP") ? -1.0f : 1.0f;
        const DlssNgx::EvalDesc eval{sign * frame_jitter[0], sign * frame_jitter[1], reset,
                                     frame_ms};
        const bool ok = DlssNgx::Get()->Evaluate(cmdbuf, color, depth_res, motion_res, output_res,
                                                eval);
        scheduler.GetDynamicState().Invalidate(); // NGX records its own Vulkan state
        const std::array back{transition(vk::Image(depth_image), ro, general),
                              transition(vk::Image(motion_image), ro, general),
                              transition(vk::Image(output_image), general, general)};
        cmdbuf.pipelineBarrier2({.imageMemoryBarrierCount = u32(back.size()),
                                 .pImageMemoryBarriers = back.data()});
        if (!ok) {
            dlss_failed = true;
            std::printf("DLSS: evaluation failed; upscaling disabled (error)\n");
            std::fflush(stdout);
            return {};
        }
        if (++frame_id <= 3 || frame_id % 120 == 0) {
            std::printf("CTR_UPSCALE_DISPATCH mode=dlss frame=%llu input=%ux%u output=%ux%u depth_samples=%u velocity_samples=%u camera=%d jitter=%g,%g\n", (unsigned long long)frame_id, w,h,out.width,out.height,depth.info.num_samples,vel.info.num_samples,camera_valid,frame_jitter[0],frame_jitter[1]); std::fflush(stdout);
        }
        if (frame_id == 30) {
            UpscaleReadback(instance, scheduler, source.GetImage(), source.backing->state.layout, {w,h}, 4, "dlss_input_rgba8");
            UpscaleReadback(instance, scheduler, vk::Image(output_image), vk::ImageLayout::eGeneral, out, 4, "dlss_output_rgba8");
            UpscaleReadback(instance, scheduler, vk::Image(depth_image), vk::ImageLayout::eGeneral, {w,h}, 4, "dlss_depth_r32f");
            UpscaleReadback(instance, scheduler, vk::Image(motion_image), vk::ImageLayout::eGeneral, {w,h}, 4, "dlss_motion_rg16f");
        }
        NoteVectors(w, h);
        reset = false;
        if (dump) {
            const auto name = "dlss_seq"+std::to_string(dump_frame)+"_output_rgba8";
            UpscaleReadback(instance,scheduler,vk::Image(output_image),vk::ImageLayout::eGeneral,out,4,name.c_str());
        }
        if (last_skip) {
            last_skip = nullptr;
            std::printf("DLSS: upscaling again\n");
        }
        layout = vk::ImageLayout::eGeneral;
        return OutputView(view_format, mapping);
    }

    // 3. FSR 3.1: the same inputs (General layout), the frame in shader-read layout.
    if (use_fsr3) {
        if (!EnsureFsr3(w, h, out.width, out.height)) return {};
        float near_plane, far_plane, fov;
        CameraPlanes(near_plane, far_plane, fov);
        const auto describe = [](vk::Image image, vk::Format format, u32 iw, u32 ih, vk::ImageUsageFlags usage,
                                 FfxVkPortableResourceState state) {
            FfxVkPortableImage d{};
            d.structSize = sizeof(d);
            d.image = image;
            d.format = static_cast<VkFormat>(format);
            d.extent = {iw, ih};
            d.mipCount = 1;
            d.arrayLayers = 1;
            d.usage = static_cast<VkImageUsageFlags>(usage);
            d.aspect = VK_IMAGE_ASPECT_COLOR_BIT;
            d.state = state;
            return d;
        };
        const auto internal = vk::ImageUsageFlagBits::eStorage | vk::ImageUsageFlagBits::eSampled |
                              vk::ImageUsageFlagBits::eTransferSrc;
        FfxVkPortableUpscaleDispatchInfo info{};
        info.structSize = sizeof(info);
        info.commandBuffer = cmdbuf;
        info.color = describe(source.GetImage(), source.info.pixel_format, w, h, source.usage_flags,
                              FFX_VK_PORTABLE_RESOURCE_STATE_COMPUTE_READ);
        info.depth = describe(vk::Image(depth_image), vk::Format::eR32Sfloat, w, h, internal,
                              FFX_VK_PORTABLE_RESOURCE_STATE_GENERIC_READ);
        info.motionVectors = describe(vk::Image(motion_image), vk::Format::eR16G16Sfloat, w, h, internal,
                                      FFX_VK_PORTABLE_RESOURCE_STATE_GENERIC_READ);
        info.output = describe(vk::Image(output_image), output_format, out.width, out.height, internal,
                               FFX_VK_PORTABLE_RESOURCE_STATE_UNORDERED_ACCESS);
        info.exposure.structSize = sizeof(info.exposure);
        info.reactiveMask.structSize = sizeof(info.reactiveMask);
        info.transparencyAndCompositionMask.structSize = sizeof(info.transparencyAndCompositionMask);
        info.jitterOffset = {frame_jitter[0], frame_jitter[1]};
        info.motionVectorScale = {1.0f, 1.0f};
        info.renderSize = {w, h};
        info.outputSize = {out.width, out.height};
        info.frameTimeMilliseconds = frame_ms;
        info.preExposure = 1.0f;
        info.cameraNear = near_plane;
        info.cameraFar = far_plane;
        info.cameraVerticalFovRadians = fov;
        info.viewSpaceToMeters = 0.01f;
        static const float sharpness = [] {
            const char* v = std::getenv("CTR_FSR3_SHARPNESS");
            return v ? std::clamp(float(std::atof(v)), 0.0f, 1.0f) : 0.3f;
        }();
        info.sharpness = sharpness;
        info.enableSharpening = sharpness > 0.0f ? VK_TRUE : VK_FALSE;
        info.reset = reset ? VK_TRUE : VK_FALSE;
        info.frameId = frame_id;
        FfxVkPortableUpscaleCreateInfo limits{};
        limits.structSize = sizeof(limits);
        limits.maxRenderSize = {fsr3_size[0], fsr3_size[1]};
        limits.maxOutputSize = {fsr3_size[2], fsr3_size[3]};
        if (const u64 issues = ffxVkPortableValidateUpscaleDispatchInfo(&limits, &info)) {
            std::printf("FSR 3: dispatch rejected (issues 0x%llx); off for this session\n", static_cast<unsigned long long>(issues));
            fsr3_failed = true; ctrport::settings::display::temporal_capabilities.fetch_and(~1u, std::memory_order_relaxed);
            return {};
        }
        if (ffxVkPortableUpscaleContextRecordDispatch(fsr3_context, &info) != FFX_VK_PORTABLE_OK) {
            std::printf("FSR 3: dispatch failed; off for this session\n");
            fsr3_failed = true; ctrport::settings::display::temporal_capabilities.fetch_and(~1u, std::memory_order_relaxed);
            return {};
        }
        scheduler.GetDynamicState().Invalidate(); // FSR records its own Vulkan state
        const vk::MemoryBarrier2 done{
            .srcStageMask = vk::PipelineStageFlagBits2::eAllCommands,
            .srcAccessMask = vk::AccessFlagBits2::eMemoryWrite,
            .dstStageMask = vk::PipelineStageFlagBits2::eAllCommands,
            .dstAccessMask = vk::AccessFlagBits2::eMemoryRead | vk::AccessFlagBits2::eMemoryWrite,
        };
        cmdbuf.pipelineBarrier2({.memoryBarrierCount = 1, .pMemoryBarriers = &done});
        if (++frame_id <= 3 || frame_id % 120 == 0) {
            std::printf("CTR_UPSCALE_DISPATCH mode=fsr3 frame=%llu input=%ux%u output=%ux%u near=%.2f far=%.0f fov=%.1f jitter=%g,%g\n",
                        (unsigned long long)frame_id, w, h, out.width, out.height, near_plane, far_plane, fov * 57.29578f,
                        frame_jitter[0], frame_jitter[1]);
            std::fflush(stdout);
        }
        NoteVectors(w, h);
        reset = false;
        if (last_skip) {
            last_skip = nullptr;
            std::printf("FSR 3: upscaling again\n");
        }
        layout = vk::ImageLayout::eGeneral;
        return OutputView(view_format, mapping);
    }

    return {};
}

void CtrDlss::Dump(vk::CommandBuffer cmdbuf, int frame, VideoCore::Image& source) {
    static const std::string dir = [] {
        const char* env = std::getenv("CTR_DLSS_DUMP_DIR");
        const std::string path{env && env[0] ? env : "runtime/user/upscaling"};
        std::filesystem::create_directories(path);
        return path;
    }();
    // The frame DLSS reads, for matching moving objects between frames.
    source.Transit(vk::ImageLayout::eTransferSrcOptimal, vk::AccessFlagBits2::eTransferRead, {});
    struct Item {
        vk::Image image;
        vk::ImageLayout layout;
        const char* name;
        std::string format;
    };
    const Item items[] = {
        {vk::Image(depth_image), vk::ImageLayout::eGeneral, "depth", "r32f"},
        {vk::Image(debug_image), vk::ImageLayout::eGeneral, "velocity", "rgba8"},
        {vk::Image(motion_image), vk::ImageLayout::eGeneral, "motion", "rg16f"},
        {source.GetImage(), vk::ImageLayout::eTransferSrcOptimal, "color",
         vk::to_string(source.info.pixel_format)},
    };
    const vk::MemoryBarrier2 before{
        .srcStageMask = vk::PipelineStageFlagBits2::eAllCommands,
        .srcAccessMask = vk::AccessFlagBits2::eMemoryWrite,
        .dstStageMask = vk::PipelineStageFlagBits2::eTransfer,
        .dstAccessMask = vk::AccessFlagBits2::eTransferRead,
    };
    cmdbuf.pipelineBarrier2({.memoryBarrierCount = 1, .pMemoryBarriers = &before});
    for (const auto& item : items) {
        const VkDeviceSize size = VkDeviceSize(width) * height * 4;
        const VkBufferCreateInfo buffer_ci{
            .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
            .size = size,
            .usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT,
        };
        const VmaAllocationCreateInfo alloc_ci{
            .flags = VMA_ALLOCATION_CREATE_MAPPED_BIT | VMA_ALLOCATION_CREATE_HOST_ACCESS_RANDOM_BIT,
            .usage = VMA_MEMORY_USAGE_AUTO_PREFER_HOST,
        };
        VkBuffer buffer{};
        VmaAllocation allocation{};
        VmaAllocationInfo alloc_info{};
        if (vmaCreateBuffer(instance.GetAllocator(), &buffer_ci, &alloc_ci, &buffer, &allocation,
                            &alloc_info) != VK_SUCCESS) {
            continue;
        }
        const vk::BufferImageCopy region{
            .imageSubresource = {vk::ImageAspectFlagBits::eColor, 0, 0, 1},
            .imageExtent = {width, height, 1},
        };
        cmdbuf.copyImageToBuffer(item.image, item.layout, buffer, region);
        char path[512];
        std::snprintf(path, sizeof(path), "%s/fsr3_f%03d_%s_%ux%u_%s.raw", dir.c_str(), frame,
                      item.name, width, height, item.format.c_str());
        scheduler.DeferPriorityOperation([allocator = instance.GetAllocator(), buffer, allocation,
                                          alloc_info, size, file = std::string{path}] {
            vmaInvalidateAllocation(allocator, allocation, 0, VK_WHOLE_SIZE);
            if (FILE* f = std::fopen(file.c_str(), "wb")) {
                std::fwrite(alloc_info.pMappedData, 1, size, f);
                std::fclose(f);
            }
            vmaDestroyBuffer(allocator, buffer, allocation);
        });
    }
    const vk::MemoryBarrier2 after{
        .srcStageMask = vk::PipelineStageFlagBits2::eTransfer,
        .dstStageMask = vk::PipelineStageFlagBits2::eAllCommands,
    };
    cmdbuf.pipelineBarrier2({.memoryBarrierCount = 1, .pMemoryBarriers = &after});
    std::printf("CTR DLSS dump %d (%s), jitter %.4f %.4f, viewport scale %g %g offset %g %g, depth "
                "%g + %g ndc, camera constants%s:\n",
                frame, dir.c_str(), frame_jitter[0], frame_jitter[1], viewport.xscale,
                viewport.yscale, viewport.xoffset, viewport.yoffset, viewport.zoffset,
                viewport.zscale, camera_valid ? "" : " (none)");
    for (u32 row = 0; camera_valid && row < camera.size() / 4; ++row) {
        std::printf("  [%3u] %12g %12g %12g %12g\n", row * 4, camera[row * 4], camera[row * 4 + 1],
                    camera[row * 4 + 2], camera[row * 4 + 3]);
    }
    std::fflush(stdout);
}

} // namespace Vulkan
