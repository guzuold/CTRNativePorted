// SPDX-License-Identifier: GPL-2.0-or-later
// ctrport: shadPS4 settings used by the video core, read once from CTR_GPU_* environment
// variables. Defaults are shadPS4 v0.18.0's defaults (core/emulator_settings.h).
// Adapted from bbport gpu/shim/core/emulator_settings.h (GPL-2.0).
#pragma once
#include <algorithm>
#include <cstdlib>
#include <string>
#include "common/types.h"

enum GpuReadbacksMode : int { Disabled, Relaxed, Precise };

class EmulatorSettingsImpl {
public:
    static EmulatorSettingsImpl* GetInstance() {
        static EmulatorSettingsImpl s;
        return &s;
    }
    static bool Flag(const char* name, bool fallback) {
        const char* v = std::getenv(name);
        return v ? (v[0] == '1' || v[0] == 'y' || v[0] == 't') : fallback;
    }
    static long Number(const char* name, long fallback) {
        const char* v = std::getenv(name);
        return v ? std::strtol(v, nullptr, 10) : fallback;
    }
    s32 GetGpuId() {
        static const auto value = s32(Number("CTR_GPU_ID", -1));
        return value;
    }
    u32 GetInternalScreenWidth() {
        static const auto value = u32(Number("CTR_GPU_INTERNAL_WIDTH", 1280));
        return value;
    }
    u32 GetInternalScreenHeight() {
        static const auto value = u32(Number("CTR_GPU_INTERNAL_HEIGHT", 720));
        return value;
    }
    std::string GetPresentMode() {
        const char* v = std::getenv("CTR_GPU_PRESENT_MODE");
        return v ? v : "Mailbox";
    }
    int GetRcasAttenuation() {
        static const auto value = int(Number("CTR_GPU_RCAS_ATTENUATION", 250));
        return value;
    }
    u32 GetReadbacksMode() {
        static const auto value = u32(Number("CTR_GPU_READBACKS", GpuReadbacksMode::Disabled));
        return value;
    }
    u32 GetVblankFrequency() {
        static const auto value = u32(std::max<long>(30, Number("CTR_GPU_VBLANK_HZ", 60)));
        return value;
    }
    bool IsCopyGpuBuffers() {
        static const auto value = Flag("CTR_GPU_COPY_BUFFERS", false);
        return value;
    }
    bool IsDirectMemoryAccessEnabled() {
        static const auto value = Flag("CTR_GPU_DIRECT_MEMORY_ACCESS", false);
        return value;
    }
    bool IsDumpShaders() {
        static const auto value = Flag("CTR_GPU_DUMP_SHADERS", false);
        return value;
    }
    bool IsFsrEnabled() {
        static const auto value = Flag("CTR_GPU_FSR", false);
        return value;
    }
    bool IsHdrAllowed() {
        return false;
    }
    bool IsNullGPU() {
        static const auto value = Flag("CTR_GPU_NULL", false);
        return value;
    }
    bool IsPatchShaders() {
        return false;
    }
    bool IsPipelineCacheArchived() {
        return false;
    }
    // Persistent pipeline cache, opt-in (experimental, off by default in shadPS4 too).
    // Reloading it in a later run corrupted v01.21 geometry (Crash's fur on the title
    // screen became screen-wide streaks); runtime/user/probes 20261013_000804 vs 001718.
    bool IsPipelineCacheEnabled() {
        static const auto value = Flag("CTR_GPU_PIPELINE_CACHE", false);
        return value;
    }
    bool IsRcasEnabled() {
        static const auto value = Flag("CTR_GPU_RCAS", true);
        return value;
    }
    bool IsReadbackLinearImagesEnabled() {
        static const auto value = Flag("CTR_GPU_READBACK_LINEAR", false);
        return value;
    }
    bool IsRenderdocEnabled() {
        return false;
    }
    bool IsShaderCollect() {
        return false;
    }
    bool IsVkCrashDiagnosticEnabled() {
        return false;
    }
    bool IsVkGuestMarkersEnabled() {
        static const auto value = Flag("CTR_GPU_VK_MARKERS", false);
        return value;
    }
    bool IsVkHostMarkersEnabled() {
        static const auto value = Flag("CTR_GPU_VK_MARKERS", false);
        return value;
    }
    bool IsVkValidationCoreEnabled() {
        return true;
    }
    bool IsVkValidationEnabled() {
        static const auto value = Flag("CTR_GPU_VK_VALIDATION", false);
        return value;
    }
    bool IsVkValidationGpuEnabled() {
        return false;
    }
    bool IsVkValidationSyncEnabled() {
        static const auto value = Flag("CTR_GPU_VK_VALIDATION_SYNC", false);
        return value;
    }
};
#define EmulatorSettings (*EmulatorSettingsImpl::GetInstance())
