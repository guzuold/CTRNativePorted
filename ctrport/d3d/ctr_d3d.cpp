// SPDX-License-Identifier: GPL-2.0-or-later
// ctr_d3d.dll: the engine-state D3D11 backend behind the same C interface as ctr_gpu.dll
// (ctrport/gpu/ctr_gpu.h), selected with CTR_GPU_BACKEND=d3d11. ctrport binds the guest imports
// of libSceGnmDriver, libSceVideoOut and the libkernel event queue functions it provides.
#define CTR_GPU_BUILD
#include <windows.h>

#include <cstdarg>
#include <cstdlib>
#include <mutex>
#include <string>
#include <vector>

#include "../gpu/ctr_gpu.h"
#include "../settings/display_controls/gpu_policy.hpp"
#include "../settings/renderer_controls/live.hpp"
#include "engine.h"
#include "gfx.h"
#include "guest.h"
#include "videoout.h"

Eboot g_eboot;

namespace {
struct Export {
    std::string nid, library, name;
    void* function;
};
std::vector<Export>& Exports() {
    static std::vector<Export> e;
    return e;
}
FILE* g_log;
std::mutex g_log_mutex;
std::filesystem::path g_user_root = "runtime/user/gpu_d3d";
const eng::Version* g_version;

void LogV(const char* fmt, va_list ap) {
    char buf[4096];
    vsnprintf(buf, sizeof(buf), fmt, ap);
    std::lock_guard lk(g_log_mutex);
    fprintf(stderr, "ctr_d3d: %s\n", buf);
    if (g_log) {
        fprintf(g_log, "[%5lu] %s\n", GetCurrentThreadId(), buf);
        fflush(g_log);
    }
}
}  // namespace

void RegisterExport(const char* nid, const char* library, const char* name, void* function) {
    Exports().push_back({nid, library, name, function});
}

void LogInit(const char* path) { g_log = fopen(path, "w"); }
void Log(const char* fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    LogV(fmt, ap);
    va_end(ap);
}
void LogFlush() {
    std::lock_guard lk(g_log_mutex);
    fflush(stderr);
    if (g_log) fflush(g_log);
}
void Fatal(const char* fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    LogV(fmt, ap);
    va_end(ap);
    LogFlush();
    ExitProcess(4);
}
void Unsupported(const char* library, const char* name, const char* nid, u64 caller, const char* why) {
    fprintf(stderr, "NID=%s library=%s caller=0x%llx unsupported: %s (%s, ctr_d3d)\n", nid, library,
            (unsigned long long)caller, name, why);
    Fatal("%s unsupported: %s (caller %llx)", name, why, (unsigned long long)caller);
}

int ConfigInt(const char* name, int def) {
    char key[96], v[32] = {};
    snprintf(key, sizeof(key), "CTR_D3D_%s", name);
    return GetEnvironmentVariableA(key, v, sizeof(v)) ? atoi(v) : def;
}
std::filesystem::path UserRoot() { return g_user_root; }
const eng::Version* eng::CurrentVersion() { return g_version; }

// ctrport's settings startup writes the process environment after the C runtime copied it
// (SetEnvironmentVariable); getenv, used by the shared settings policies, reads the copy. The
// variables this backend consumes are copied into it, removed ones included.
void SyncSettingsEnvironment() {
    for (const char* key : {"CTR_GPU_PRESENT_MODE", "CTR_SETTINGS_ANISOTROPY", "CTR_SETTINGS_FSR1_SHARPEN",
                            "CTR_SETTINGS_HDR", "CTR_SETTINGS_GAMMA", "CTR_SETTINGS_HDR_PEAK", "CTR_SETTINGS_HDR_EXPOSURE",
                            "CTR_SETTINGS_PRESENTATION_LIMIT", "CTR_SETTINGS_WINDOW_MODE", "CTR_SETTINGS_DISPLAY_REFRESH",
                            "CTR_UPSCALER", "CTR_SCENE_HEIGHT", "CTR_GPU_AO", "CTR_GPU_HERO_SHADOWS", "CTR_GPU_WIDTH", "CTR_GPU_HEIGHT", "CTR_GPU_FULLSCREEN"}) {
        char value[256];
        const DWORD n = GetEnvironmentVariableA(key, value, sizeof(value));
        _putenv_s(key, n && n < sizeof(value) ? value : "");
    }
}

extern "C" CTR_GPU_API int ctr_gpu_init(const CtrGpuConfig* config) {
    if (!config || config->version != 1) return -1;
    SyncSettingsEnvironment();
    if (config->user_dir) g_user_root = std::filesystem::path(config->user_dir).parent_path() / "gpu_d3d";
    std::error_code ec;
    std::filesystem::create_directories(g_user_root, ec);
    std::filesystem::create_directories("runtime/user/logs", ec);
    LogInit("runtime/user/logs/ctr_d3d.log");
    LOG("engine-state D3D11 backend, %zu exports, data in %s", Exports().size(), g_user_root.string().c_str());
    // Invalid PC settings values are refused here, not ignored. The temporal scene upscalers (FSR 3,
    // DLSS) are reported by the renderer: the game's own Neo scaling then shows the scene.
    try {  // HDR selection and calibration values, checked as the GPU library checks them
        (void)ctrport::settings::display::native_hdr();
        (void)ctrport::settings::display::calibration();
    } catch (const std::exception& e) {
        Fatal("display settings: %s", e.what());
    }
    VideoOutStartWindow(config->title, config->width, config->height);
    return 0;
}

// PC settings ABIs (ctrport/settings/renderer_startup.hpp): the controls this backend applies.
// Renderer: presentation mode, anisotropy, FSR 1 sharpness (live). Display: gamma, native HDR10 with peak / exposure. Pacing:
// presentation limit, window mode, exclusive refresh. AO and hero shadows: the "off" versions of
// the recognised shaders that ctr_shaders translates next to the originals (renderer.cpp).
extern "C" __declspec(dllexport) unsigned ctr_gpu_settings_abi() { return 1; }
extern "C" __declspec(dllexport) unsigned ctr_gpu_display_abi() { return 1; }
extern "C" __declspec(dllexport) unsigned ctr_gpu_pacing_abi() { return 1; }
extern "C" __declspec(dllexport) unsigned ctr_gpu_live_settings_abi() { return 1; }
extern "C" __declspec(dllexport) unsigned ctr_gpu_ao_abi() { return 1; }
extern "C" __declspec(dllexport) unsigned ctr_gpu_hero_abi() { return 1; }
// Settings only this renderer provides: shadow map detail (CTR_D3D_ShadowQuality).
extern "C" __declspec(dllexport) unsigned ctr_gpu_d3d_abi() { return 1; }
extern "C" __declspec(dllexport) unsigned ctr_gpu_live_settings_set(unsigned key, unsigned value) {
    return ctrport::settings::live_gpu::set(key, value);
}

// The eboot whose engine state is read: its load address and SHA256 select the recorded
// addresses (engine.h); the clear and copy hooks are installed here. 0 on success.
extern "C" __declspec(dllexport) int ctr_d3d_set_eboot(void* base, uint64_t size, const char* sha256) {
    g_eboot.base = static_cast<u8*>(base);
    g_eboot.size = size;
    g_eboot.sha256 = sha256 ? sha256 : "";
    for (const auto& v : eng::kVersions)
        if (g_eboot.sha256 == v.eboot_sha256) g_version = &v;
    if (!g_version) {
        LOG("eboot %s has no recorded engine addresses", g_eboot.sha256.c_str());
        return -1;
    }
    GuestAddRange((u64)base, size);
    if (!GfxInstallHooks()) return -2;
    if (!GfxShadersAvailable())
        LOG("no translated shaders in %s (tools/gpu_d3d/build_shaders.py): nothing will be drawn",
            g_user_root.string().c_str());
    LOG("eboot %s at %p: device global +%llx", g_eboot.sha256.c_str(), base, (unsigned long long)g_version->device_global);
    return 0;
}

extern "C" CTR_GPU_API int ctr_gpu_handle_fault(void*) { return 0; }

extern "C" CTR_GPU_API uintptr_t ctr_gpu_resolve(const char* nid, const char* library) {
    for (const auto& e : Exports())
        if (e.nid == nid && (!library || e.library == library)) return (uintptr_t)e.function;
    return 0;
}

extern "C" CTR_GPU_API void ctr_gpu_map_memory(uint64_t address, uint64_t size) { GuestAddRange(address, size); }
extern "C" CTR_GPU_API void ctr_gpu_unmap_memory(uint64_t address, uint64_t size) { GuestRemoveRange(address, size); }
// CPU writes outside the guest's own code (file reads) are seen by write watch or content hash.
extern "C" CTR_GPU_API void ctr_gpu_invalidate_memory(uint64_t, uint64_t) {}

extern "C" CTR_GPU_API unsigned ctr_gpu_symbol_count(void) { return (unsigned)Exports().size(); }
extern "C" CTR_GPU_API uint64_t ctr_gpu_flip_count(void) { return VideoOutFlipCount(); }
extern "C" CTR_GPU_API uint64_t ctr_gpu_submit_count(void) { return GnmSubmitCount(); }
// No host overlay takes the game's input.
extern "C" CTR_GPU_API int ctr_gpu_input_captured(void) { return 0; }
// PlayStation controllers are read through SDL only by ctr_gpu.dll; XInput pads work directly.
extern "C" CTR_GPU_API int ctr_gpu_gamepad_state(CtrGpuGamepad*) { return 0; }
extern "C" CTR_GPU_API void ctr_gpu_gamepad_rumble(uint16_t, uint16_t) {}
