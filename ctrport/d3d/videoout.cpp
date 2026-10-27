// SPDX-License-Identifier: MIT
// Adapted from FrankHUN88453/CTR-NitroFueled-PC 96b4aa5474b1d42b0d9f383349dc091ea9a2d385
// port/src/hle/videoout.cpp (MIT, ctrport/d3d/LICENSE.FrankHUN88453): window created at backend
// start, event ids and data as the console's (flip 0x6, vblank 0x7), flip rate honoured unless
// CTR_D3D_FpsLimit is set, the CTR-specific underscore entry points CUSA14876 imports, settings
// from the environment; mouse/glyph/settings-menu integration of the original removed.
#include "videoout.h"

#include <windows.h>
#include <intrin.h>

#include <atomic>
#include <condition_variable>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <mutex>
#include <thread>
#include <vector>

#include "../settings/display_controls/gpu_policy.hpp"
#include "../settings/pacing_controls/gpu_policy.hpp"
#include "equeue.h"
#include "gfx.h"

namespace {

constexpr int kHandle = 1;
constexpr int kErrInvalidHandle = (int)0x8029000Bu, kErrInvalidAddress = (int)0x80290004u,
              kErrInvalidValue = (int)0x80290001u;
HWND g_hwnd;
std::atomic<u64> g_flips{0}, g_vblanks{0};
std::atomic<s64> g_flip_arg{-1};
std::atomic<int> g_current_buffer{-1};
std::atomic<int> g_flip_rate{0};  // 0: 60 Hz, 1: 30 Hz, 2: 20 Hz
std::atomic<u64> g_flip_tsc{0};
std::mutex g_mutex;
std::condition_variable g_vblank_cv;
std::vector<Equeue*> g_vblank_queues, g_flip_queues;
u64 g_start_tick = GetTickCount64();
u64 g_buffers[16];
bool g_open = false;

void SetFullscreen(HWND h, bool on) {
    static RECT windowed{};
    static bool fullscreen = false;
    if (on == fullscreen) return;
    fullscreen = on;
    if (on) {
        GetWindowRect(h, &windowed);
        MONITORINFO mi{sizeof(mi)};
        GetMonitorInfoA(MonitorFromWindow(h, MONITOR_DEFAULTTONEAREST), &mi);
        SetWindowLongA(h, GWL_STYLE, WS_POPUP | WS_VISIBLE);
        SetWindowPos(h, HWND_TOP, mi.rcMonitor.left, mi.rcMonitor.top, mi.rcMonitor.right - mi.rcMonitor.left,
                     mi.rcMonitor.bottom - mi.rcMonitor.top, SWP_FRAMECHANGED);
    } else {
        SetWindowLongA(h, GWL_STYLE, WS_OVERLAPPEDWINDOW | WS_VISIBLE);
        SetWindowPos(h, nullptr, windowed.left, windowed.top, windowed.right - windowed.left,
                     windowed.bottom - windowed.top, SWP_FRAMECHANGED | SWP_NOZORDER);
    }
}
bool g_fullscreen = false;
DisplayConfig g_display;

LRESULT CALLBACK WndProc(HWND h, UINT msg, WPARAM w, LPARAM l) {
    switch (msg) {
    case WM_SYSKEYDOWN:
        if (w == VK_RETURN && (l & (1 << 29)) && g_display.mode != DisplayConfig::Exclusive) {  // Alt+Enter
            SetFullscreen(h, g_fullscreen = !g_fullscreen);
            return 0;
        }
        break;
    case WM_KEYDOWN:
        if (w == VK_F11 && g_display.mode != DisplayConfig::Exclusive) {
            SetFullscreen(h, g_fullscreen = !g_fullscreen);
            return 0;
        }
        if (w == VK_F12) {  // record 3 seconds of frames (flicker reports)
            GfxRecordFrames(180);
            return 0;
        }
        break;
    case WM_CLOSE:
        LOG("window closed - exiting");
        LogFlush();
        std::fflush(nullptr);
        // Not ExitProcess: unloading the DLLs while guest threads still run can fault and turn
        // the exit code into an error (the launcher then reports a crash).
        TerminateProcess(GetCurrentProcess(), 0);
        return 0;
    case WM_PAINT:
        if (GfxActive()) {
            ValidateRect(h, nullptr);
            return 0;
        }
        break;
    }
    return DefWindowProcA(h, msg, w, l);
}

void VblankThread() {
    HANDLE timer = CreateWaitableTimerExW(nullptr, nullptr, CREATE_WAITABLE_TIMER_HIGH_RESOLUTION, TIMER_ALL_ACCESS);
    LARGE_INTEGER freq, start, now;
    QueryPerformanceFrequency(&freq);
    QueryPerformanceCounter(&start);
    for (u64 n = 1;; n++) {
        // 59.94 Hz, scheduled from the start so the rate does not drift
        const s64 due_ticks = start.QuadPart + (s64)(n * freq.QuadPart * 1001 / 60000);
        QueryPerformanceCounter(&now);
        const s64 wait = due_ticks - now.QuadPart;
        if (wait > 0) {
            LARGE_INTEGER due;
            due.QuadPart = -(wait * 10000000 / freq.QuadPart);
            SetWaitableTimer(timer, &due, 0, nullptr, nullptr, FALSE);
            WaitForSingleObject(timer, INFINITE);
        }
        const u64 count = ++g_vblanks;
        std::lock_guard lk(g_mutex);
        g_vblank_cv.notify_all();
        for (Equeue* q : g_vblank_queues) EqueueTriggerDisplay(q, 0x7, 0x7 | (count << 16));
    }
}

struct FlipStatus {
    u64 count;
    u64 process_time;
    u64 tsc;
    s64 flip_arg;
    u64 submit_tsc;
    u64 reserved0;
    s32 gc_queue_num;
    s32 flip_pending_num;
    s32 current_buffer;
    u32 reserved1;
};
struct ResolutionStatus {
    s32 full_width, full_height, pane_width, pane_height;
    u64 refresh_rate;
    float screen_size_in_inch;
    u16 flags, reserved0;
    u32 reserved1[3];
};

SYSV int sceVideoOutOpen(int user, int bus, int index, const void*) {
    LOG("videoout: open(user %d, bus %d, index %d)", user, bus, index);
    if (bus != 0) return kErrInvalidValue;
    static std::once_flag once;
    std::call_once(once, [] { std::thread(VblankThread).detach(); });
    g_open = true;
    return kHandle;
}
SYSV int sceVideoOutClose(int handle) { return handle == kHandle ? 0 : kErrInvalidHandle; }
SYSV int sceVideoOutRegisterBuffers(int handle, int start, void* const* addrs, int num, const void*) {
    if (handle != kHandle) return kErrInvalidHandle;
    if (!addrs || start < 0 || num < 1 || start + num > 16) return kErrInvalidValue;
    LOG("videoout: register buffers (start %d, num %d, first %p)", start, num, addrs[0]);
    for (int i = 0; i < num; i++) g_buffers[start + i] = (u64)addrs[i];
    return start;
}
SYSV int sceVideoOutSubmitFlip(int handle, int index, int mode, s64 arg) {
    if (handle != kHandle) return kErrInvalidHandle;
    if (index < -1 || index >= 16) return kErrInvalidValue;
    VideoOutFlip(handle, index, arg);
    return 0;
}
SYSV int sceVideoOutGetFlipStatus(int handle, FlipStatus* s) {
    if (handle != kHandle) return kErrInvalidHandle;
    if (!s) return kErrInvalidAddress;
    memset(s, 0, sizeof(*s));
    s->count = g_flips;
    s->flip_arg = g_flip_arg;
    s->current_buffer = g_current_buffer;
    s->process_time = (GetTickCount64() - g_start_tick) * 1000;
    s->tsc = s->submit_tsc = g_flip_tsc;
    return 0;
}
SYSV int sceVideoOutGetResolutionStatus(int handle, ResolutionStatus* s) {
    if (handle != kHandle) return kErrInvalidHandle;
    if (!s) return kErrInvalidAddress;
    memset(s, 0, sizeof(*s));
    // the display the game sizes its output for: 4K with Neo mode or a scene height (as ctrport's
    // GPU library reports it), else 1080p
    static const bool uhd = getenv("CTR_SCENE_HEIGHT") || (getenv("CTR_NEO_MODE") && getenv("CTR_NEO_MODE")[0] == '1');
    s->full_width = s->pane_width = uhd ? 3840 : 1920;
    s->full_height = s->pane_height = uhd ? 2160 : 1080;
    s->refresh_rate = 3;  // 59.94 Hz
    s->screen_size_in_inch = 50;
    return 0;
}
SYSV int sceVideoOutWaitVblank(int handle) {
    if (handle != kHandle) return kErrInvalidHandle;
    std::unique_lock lk(g_mutex);
    const u64 n = g_vblanks;
    g_vblank_cv.wait(lk, [&] { return g_vblanks != n; });
    return 0;
}
SYSV int sceVideoOutAddVblankEvent(u64 eqh, int handle, void* udata) {
    if (handle != kHandle) return kErrInvalidHandle;
    Equeue* eq = EqueueFromHandle(eqh);
    if (!eq) return SceErr(err::EBADF_);
    EqueueAdd(eq, 0x7, kFilterVideoOut, kEvAdd | kEvClear, 0, udata);
    std::lock_guard lk(g_mutex);
    g_vblank_queues.push_back(eq);
    return 0;
}
SYSV int sceVideoOutSetFlipRate(int handle, int rate) {
    if (handle != kHandle) return kErrInvalidHandle;
    if (rate < 0 || rate > 2) return kErrInvalidValue;
    LOG("videoout: flip rate %d (%d Hz)", rate, 60 / (rate + 1));
    g_flip_rate = rate;
    return 0;
}
SYSV int sceVideoOutSetBufferAttribute(void* attr, u32 fmt, u32 tiling, u32 aspect, u32 w, u32 h, u32 pitch) {
    if (!attr) return kErrInvalidAddress;
    LOG("videoout: buffer attribute format 0x%x tiling %u %ux%u pitch %u", fmt, tiling, w, h, pitch);
    memset(attr, 0, 0x40);
    u32* a = (u32*)attr;
    a[0] = fmt;
    a[1] = tiling;
    a[2] = aspect;
    a[3] = w;
    a[4] = h;
    a[5] = pitch;
    return 0;
}
// HDR10 output is offered to the game when the PC setting asks for native HDR and the display
// showing the window takes it (as the GPU library: the selection permits negotiation, the
// display decides). CUSA14876 declares HDR support in its param.sfo.
bool HdrOffered() {
    static const bool offered = [] {
        bool requested = false;
        try {
            requested = ctrport::settings::display::native_hdr();
        } catch (const std::exception& e) {
            Fatal("HDR setting: %s", e.what());
        }
        const bool display = requested && GfxHdrDisplay();
        LOG("CTR_SETTINGS_HDR requested=%d display_pq10=%d available=%d", int(requested), int(display), int(display));
        return display;
    }();
    return offered;
}
std::atomic<bool> g_hdr{false};
constexpr u64 kCapabilityBt2020Pq = 0x80;
SYSV int sceVideoOutGetDeviceCapabilityInfo_(int handle, u64* info, u64 size) {
    if (handle != kHandle) return kErrInvalidHandle;
    if (!info) return kErrInvalidAddress;
    memset(info, 0, size && size < 64 ? size : 8);
    if (HdrOffered()) info[0] |= kCapabilityBt2020Pq;
    return 0;
}
struct Mode {
    u32 size;
    u8 encoding, range, colorimetry, depth;
    u64 refresh_rate;
    u64 resolution;
    u8 reserved[8];
};
SYSV void sceVideoOutModeSetAny_(Mode* mode, u32 size) {
    if (!mode || size < 8) return;
    memset(mode, 0xFF, size);
    mode->size = size;
}
// "Any" colorimetry selects SDR output; BT.2020 PQ (12) selects HDR10 when it is offered, and is
// refused like a display without HDR otherwise.
SYSV int sceVideoOutConfigureOutputMode_(int handle, u32 reserved, const Mode* mode, const void*, u32, u32) {
    if (handle != kHandle) return kErrInvalidHandle;
    if (reserved || !mode) return kErrInvalidValue;
    bool hdr;
    if (mode->colorimetry == 0xFF) hdr = false;
    else if (mode->colorimetry == 12 && HdrOffered()) hdr = true;
    else return kErrInvalidValue;
    if (g_hdr.exchange(hdr) != hdr) LOG("videoout: output mode %s", hdr ? "HDR10 (BT.2020 PQ)" : "SDR");
    return 0;
}
struct ColorSettings {
    float gamma;
    u32 reserved[3];
};
std::atomic<float> g_gamma{1.0f};
SYSV int sceVideoOutColorSettingsSetGamma_(ColorSettings* s, float gamma, u32 size) {
    if (!s) return kErrInvalidAddress;
    if (gamma < 0.1f || gamma > 2.0f) return kErrInvalidValue;
    s->gamma = gamma;
    return 0;
}
// The game's brightness setting, applied by the presenter (PC settings display_gamma overrides it).
SYSV int sceVideoOutAdjustColor_(int handle, const ColorSettings* s, u32 size) {
    if (handle != kHandle) return kErrInvalidHandle;
    if (!s) return kErrInvalidAddress;
    if (g_gamma.exchange(s->gamma) != s->gamma) LOG("videoout: game gamma %.3f", s->gamma);
    return 0;
}

EXPORT("Up36PTk687E", "libSceVideoOut", sceVideoOutOpen);
EXPORT("uquVH4-Du78", "libSceVideoOut", sceVideoOutClose);
EXPORT("w3BY+tAEiQY", "libSceVideoOut", sceVideoOutRegisterBuffers);
EXPORT("U46NwOiJpys", "libSceVideoOut", sceVideoOutSubmitFlip);
EXPORT("SbU3dwp80lQ", "libSceVideoOut", sceVideoOutGetFlipStatus);
EXPORT("6kPnj51T62Y", "libSceVideoOut", sceVideoOutGetResolutionStatus);
EXPORT("j6RaAUlaLv0", "libSceVideoOut", sceVideoOutWaitVblank);
EXPORT("Xru92wHJRmg", "libSceVideoOut", sceVideoOutAddVblankEvent);
EXPORT("CBiu4mCE1DA", "libSceVideoOut", sceVideoOutSetFlipRate);
EXPORT("i6-sR91Wt-4", "libSceVideoOut", sceVideoOutSetBufferAttribute);
EXPORT("kGVLc3htQE8", "libSceVideoOut", sceVideoOutGetDeviceCapabilityInfo_);
EXPORT("pjkDsgxli6c", "libSceVideoOut", sceVideoOutModeSetAny_);
EXPORT("N1bEoJ4SRw4", "libSceVideoOut", sceVideoOutConfigureOutputMode_);
EXPORT("DYhhWbJSeRg", "libSceVideoOut", sceVideoOutColorSettingsSetGamma_);
EXPORT("pv9CI5VC+R0", "libSceVideoOut", sceVideoOutAdjustColor_);

// Flip pacing: the flip rate the game asked for, as the console's vblank-timed flips (debugging:
// CTR_D3D_FpsLimit, 0 = unlimited). A frame rate limit other than "Console" unlocks the game:
// no console pacing here, the PC settings limit (or nothing) is applied at present.
void PaceFrame() {
    static const int forced = ConfigInt("FpsLimit", -1);
    if (forced < 0 && !ctrport::settings::pacing::console_pacing()) return;
    const int limit = forced >= 0 ? forced : 60 / (g_flip_rate + 1);
    if (limit <= 0) return;
    static HANDLE timer = CreateWaitableTimerExW(nullptr, nullptr, CREATE_WAITABLE_TIMER_HIGH_RESOLUTION, TIMER_ALL_ACCESS);
    static const s64 freq = [] {
        LARGE_INTEGER f;
        QueryPerformanceFrequency(&f);
        return f.QuadPart;
    }();
    static s64 next = 0;
    LARGE_INTEGER now;
    QueryPerformanceCounter(&now);
    const s64 period = freq * 1001 / (limit * 1000);  // 59.94 / 29.97 Hz like the display
    if (next == 0 || now.QuadPart - next > period) next = now.QuadPart;  // fell behind: resync
    next += period;
    s64 wait = next - now.QuadPart;
    if (wait <= 0) return;
    s64 hns = wait * 10000000 / freq;
    if (hns > 15000) {  // sleep, then spin the last ~1 ms
        LARGE_INTEGER due;
        due.QuadPart = -(hns - 10000);
        SetWaitableTimer(timer, &due, 0, nullptr, nullptr, FALSE);
        WaitForSingleObject(timer, INFINITE);
    }
    do {
        YieldProcessor();
        QueryPerformanceCounter(&now);
    } while (now.QuadPart < next);
}

}  // namespace

// PC settings window mode (same domain and rules as the GPU library's SDL window policy).
DisplayConfig ResolveDisplay(int width, int height) {
    const auto setting = [](const char* key) { const char* v = getenv(key); return std::string(v && v[0] ? v : "original"); };
    const std::string mode = setting("CTR_SETTINGS_WINDOW_MODE"), refresh = setting("CTR_SETTINGS_DISPLAY_REFRESH");
    const char* fs = getenv("CTR_GPU_FULLSCREEN");
    const bool launcher_fullscreen = fs && fs[0] == '1';
    DisplayConfig d;
    d.width = width, d.height = height;
    if (mode == "original") d.mode = launcher_fullscreen ? DisplayConfig::Borderless : DisplayConfig::Windowed;
    else if (mode == "windowed") d.mode = DisplayConfig::Windowed;
    else if (mode == "borderless") d.mode = DisplayConfig::Borderless;
    else if (mode == "exclusive") d.mode = DisplayConfig::Exclusive;
    else Fatal("invalid window mode %s", mode.c_str());
    if (refresh != "original") {
        if (d.mode != DisplayConfig::Exclusive) Fatal("display refresh requires exclusive fullscreen");
        bool allowed = false;
        for (const char* hz : {"60", "90", "120", "144", "165", "240"}) allowed |= refresh == hz;
        if (!allowed) Fatal("invalid display refresh %s", refresh.c_str());
        d.refresh = std::strtof(refresh.c_str(), nullptr);
    }
    // the launcher passes no window size in fullscreen: the display's own size
    if (d.mode == DisplayConfig::Exclusive && launcher_fullscreen && !getenv("CTR_GPU_WIDTH"))
        d.width = GetSystemMetrics(SM_CXSCREEN), d.height = GetSystemMetrics(SM_CYSCREEN);
    LOG("videoout: window mode %s %dx%d refresh %s", mode == "original" ? (launcher_fullscreen ? "original (borderless)" : "original (windowed)") : mode.c_str(),
        d.width, d.height, refresh.c_str());
    return d;
}

void VideoOutStartWindow(const char* title, int width, int height) {
    g_display = ResolveDisplay(width, height);
    std::mutex m;
    std::condition_variable cv;
    bool ready = false;
    std::string name = title ? title : "CTRPort";
    std::thread([&, name, width, height] {
        WNDCLASSA wc{};
        wc.lpfnWndProc = WndProc;
        wc.hInstance = GetModuleHandleA(nullptr);
        wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
        wc.hIcon = LoadIconA(wc.hInstance, MAKEINTRESOURCEA(1));
        wc.hbrBackground = (HBRUSH)GetStockObject(BLACK_BRUSH);
        wc.lpszClassName = "CTRPortD3D11";
        RegisterClassA(&wc);
        RECT r{0, 0, g_display.width, g_display.height};
        AdjustWindowRect(&r, WS_OVERLAPPEDWINDOW, FALSE);
        HWND h = CreateWindowA(wc.lpszClassName, name.c_str(), WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT,
                               r.right - r.left, r.bottom - r.top, nullptr, nullptr, wc.hInstance, nullptr);
        ShowWindow(h, SW_SHOW);
        SetForegroundWindow(h);  // exclusive fullscreen holds only while the window is in front
        // borderless covers the monitor; exclusive starts there and the swap chain takes the output
        if (g_display.mode != DisplayConfig::Windowed) SetFullscreen(h, g_fullscreen = true);
        {
            std::lock_guard lk(m);
            g_hwnd = h;
            ready = true;
        }
        cv.notify_all();
        MSG msg;
        while (GetMessageA(&msg, nullptr, 0, 0) > 0) {
            TranslateMessage(&msg);
            DispatchMessageA(&msg);
        }
    }).detach();
    std::unique_lock lk(m);
    cv.wait(lk, [&] { return ready; });
}

const DisplayConfig& VideoOutDisplay() { return g_display; }
float VideoOutGamma() { return g_gamma; }
bool VideoOutHdr() { return g_hdr; }
bool VideoOutIsBuffer(u64 addr) {
    for (u64 b : g_buffers)
        if (b && b == addr) return true;
    return false;
}

bool VideoOutHasFocus() { return g_hwnd && GetForegroundWindow() == g_hwnd; }
HWND__* VideoOutHwndRaw() { return g_hwnd; }
u64 VideoOutFlipCount() { return g_flips; }

void VideoOutFlip(int, int index, s64 arg) {
    u64 addr = index >= 0 && index < 16 ? g_buffers[index] : 0;
    if (addr) GfxPresent(addr);
    PaceFrame();
    u64 n = ++g_flips;
    g_flip_arg = arg;
    g_current_buffer = index;
    g_flip_tsc = __rdtsc();
    if (n == 1 || (n & (n - 1)) == 0) LOG("videoout: flip #%llu (buffer %d)", (unsigned long long)n, index);
    std::lock_guard lk(g_mutex);
    for (Equeue* q : g_flip_queues) EqueueTriggerDisplay(q, 0x6, 0x6 | ((u64)arg << 16));
}
