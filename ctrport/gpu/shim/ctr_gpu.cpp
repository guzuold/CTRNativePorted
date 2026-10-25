// SPDX-License-Identifier: GPL-2.0-or-later
// ctrport: glue between ctrport's loader and the shadPS4 v0.18.0 video core.
// Structure adapted from bbport gpu/shim/bbgpu.cpp (GPL-2.0); memory, time and fault
// services are implemented here on Win32 for ctrport's identity-mapped guest memory.
#include <windows.h>
#include <algorithm>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdio>
#include <cstring>
#include <mutex>
#include <shared_mutex>
#include <boost/icl/interval_set.hpp>
#include <string>
#include <thread>
#include <vector>
#include <SDL3/SDL.h>
#include <boost/asio/io_context.hpp>
#include "common/elf_info.h"
#include "common/logging/log.h"
#include "common/polyfill_thread.h"
#include "common/rdtsc.h"
#include "common/thread.h"
#include "core/debug_state.h"
#include "core/libraries/kernel/orbis_error.h"
#include "core/libraries/kernel/threads/pthread.h"
#include "core/libraries/libs.h"
#include "core/memory.h"
#include "core/signals.h"
#include "ctr_gpu.h"
#include "../../scene_resolution.hpp"
#include "sdl_window.h"
#include "common/path_util.h"
#include "core/emulator_settings.h"
#include "video_core/cache_storage.h"
#include "video_core/renderdoc.h"
#include "video_core/renderer_vulkan/vk_rasterizer.h"

Frontend::WindowSDL* g_window = nullptr;

namespace Libraries::GnmDriver {
void RegisterLib(Core::Loader::SymbolsResolver* sym);
}
namespace Libraries::VideoOut {
void RegisterLib(Core::Loader::SymbolsResolver* sym);
}
namespace Libraries::Kernel {
void RegisterEventQueue(Core::Loader::SymbolsResolver* sym);
}

namespace {
struct Symbol {
    std::string nid, library, module;
    u64 address;
};
std::vector<Symbol> g_symbols;
u32 g_sdk_version;
std::mutex g_mapping_mutex;
std::vector<std::pair<u64, u64>> g_pending_mappings; // reported before the rasterizer exists
LARGE_INTEGER g_qpc_frequency{}, g_qpc_start{};
// GPU-visible guest mappings reported by ctrport (ctr_gpu_map_memory). Validity and
// copies inside them need no VirtualQuery system call (that call per buffer per draw
// was the frame-rate bottleneck); addresses outside fall back to VirtualQuery.
std::shared_mutex g_ranges_mutex;
boost::icl::interval_set<VAddr> g_ranges;
bool InMappedRanges(VAddr address, u64 size) {
    std::shared_lock lock{g_ranges_mutex};
    return boost::icl::contains(g_ranges, boost::icl::interval<VAddr>::right_open(address, address + std::max<u64>(size, 1)));
}
} // namespace

namespace Core::Loader {
void SymbolsResolver::AddSymbol(const char* nid, const char* library, const char* module,
                                SymbolType, u64 address) {
    g_symbols.push_back({nid, library, module, address});
}
} // namespace Core::Loader

namespace Core {
class Emulator {
public:
    static void FillElfInfo(const CtrGpuConfig& config) {
        auto& info = Common::ElfInfo::Instance();
        info.initialized = true;
        info.game_serial = config.serial ? config.serial : "UNKNOWN";
        info.title = config.title ? config.title : "";
        info.sdk_ver = config.sdk_version;
        info.psf_attributes.raw = config.psf_attributes;
    }
};

static bool Committed(VAddr address, MEMORY_BASIC_INFORMATION& info) {
    return VirtualQuery(reinterpret_cast<void*>(address), &info, sizeof(info)) == sizeof(info) &&
           info.State == MEM_COMMIT;
}

void MemoryManager::SetRasterizer(Vulkan::Rasterizer* rasterizer_) {
    std::scoped_lock lock{g_mapping_mutex};
    rasterizer = rasterizer_;
    for (const auto& [address, size] : g_pending_mappings) {
        rasterizer->MapMemory(address, size);
    }
    g_pending_mappings.clear();
}
void MemoryManager::InvalidateMemory(VAddr address, u64 size) {
    if (rasterizer) {
        rasterizer->InvalidateMemory(address, size);
    }
}
bool MemoryManager::IsValidMapping(VAddr virtual_addr, u64 size) {
    if (InMappedRanges(virtual_addr, size)) {
        return true;
    }
    MEMORY_BASIC_INFORMATION info{};
    if (!Committed(virtual_addr, info)) {
        return false;
    }
    const VAddr end = reinterpret_cast<VAddr>(info.BaseAddress) + info.RegionSize;
    return size == 0 || virtual_addr + size <= end || ClampRangeSize(virtual_addr, size) == size;
}
u64 MemoryManager::ClampRangeSize(VAddr virtual_addr, u64 size) {
    if (InMappedRanges(virtual_addr, size)) {
        return size;
    }
    VAddr cursor = virtual_addr;
    const VAddr end = virtual_addr + size;
    while (cursor < end) {
        MEMORY_BASIC_INFORMATION info{};
        if (!Committed(cursor, info)) {
            break;
        }
        cursor = reinterpret_cast<VAddr>(info.BaseAddress) + info.RegionSize;
    }
    return std::min(cursor, end) - virtual_addr;
}
void MemoryManager::CopySparseMemory(VAddr source, u8* dest, u64 size) {
    if (InMappedRanges(source, size)) {
        std::memcpy(dest, reinterpret_cast<const void*>(source), size);
        return;
    }
    while (size) {
        MEMORY_BASIC_INFORMATION info{};
        const bool committed = Committed(source, info);
        const VAddr region_end = VirtualQuery(reinterpret_cast<void*>(source), &info, sizeof(info))
                                     ? reinterpret_cast<VAddr>(info.BaseAddress) + info.RegionSize
                                     : source + size;
        const u64 n = std::min<u64>(size, region_end - source);
        if (committed && !(info.Protect & (PAGE_NOACCESS | PAGE_GUARD))) {
            std::memcpy(dest, reinterpret_cast<const void*>(source), n);
        } else {
            std::memset(dest, 0, n);
        }
        source += n;
        dest += n;
        size -= n;
    }
}
bool MemoryManager::TryWriteBacking(void* address, const void* data, u64 size) {
    // Writes guest memory without tripping GPU page tracking: the kernel copy
    // temporarily lifts the write protection the page manager installed.
    SIZE_T written = 0;
    return WriteProcessMemory(GetCurrentProcess(), address, data, size, &written) &&
           written == size;
}
void AddressSpace::Protect(VAddr virtual_addr, u64 size, MemoryPermission perms) {
    DWORD protection = PAGE_NOACCESS;
    if (True(perms & MemoryPermission::Write)) {
        protection = PAGE_READWRITE;
    } else if (True(perms & MemoryPermission::Read)) {
        protection = PAGE_READONLY;
    }
    DWORD previous = 0;
    if (!VirtualProtect(reinterpret_cast<void*>(virtual_addr), size, protection, &previous)) {
        LOG_ERROR(Render, "VirtualProtect {:#x}+{:#x} -> {:#x} failed: {}", virtual_addr, size,
                  protection, GetLastError());
    }
}
boost::icl::interval_set<VAddr> AddressSpace::GetUsableRegions() {
    boost::icl::interval_set<VAddr> set;
    set += boost::icl::interval<VAddr>::right_open(0x10'0000'0000ULL, 0xfc'0000'0000ULL);
    return set;
}
} // namespace Core

// Kernel services the vendored libraries call directly.
namespace Libraries::Kernel {
u64 PS4_SYSV_ABI sceKernelGetTscFrequency() {
    return Common::EstimateRDTSCFrequency();
}
u64 PS4_SYSV_ABI sceKernelReadTsc() {
    return Common::FencedRDTSC();
}
u64 PS4_SYSV_ABI sceKernelGetProcessTimeCounter() {
    LARGE_INTEGER now{};
    QueryPerformanceCounter(&now);
    return u64(now.QuadPart - g_qpc_start.QuadPart);
}
u64 PS4_SYSV_ABI sceKernelGetProcessTimeCounterFrequency() {
    return u64(g_qpc_frequency.QuadPart);
}
u64 PS4_SYSV_ABI sceKernelGetProcessTime() {
    const u64 ticks = sceKernelGetProcessTimeCounter();
    const u64 frequency = sceKernelGetProcessTimeCounterFrequency();
    return ticks / frequency * 1'000'000 + (ticks % frequency) * 1'000'000 / frequency;
}
s32 PS4_SYSV_ABI sceKernelIsNeoMode() {
    // Same selection as ctrport's process_neo_mode: explicit scene height or CTR_NEO_MODE=1,
    // valid for CTR because its param.sfo declares Neo support (ATTRIBUTE bit 23).
    static const s32 neo = [] {
        if (ctrport::scene_height_selected()) return 1;
        char val[4]{};
        return (GetEnvironmentVariableA("CTR_NEO_MODE", val, sizeof(val)) == 1 && val[0] == '1') ? 1 : 0;
    }();
    return neo;
}
s32 PS4_SYSV_ABI sceKernelGetCompiledSdkVersion(s32* ver) {
    if (!ver) {
        return ORBIS_KERNEL_ERROR_EINVAL;
    }
    *ver = s32(g_sdk_version);
    return ORBIS_OK;
}
s32 PS4_SYSV_ABI sceKernelUsleep(u32 microseconds) {
    std::this_thread::sleep_for(std::chrono::microseconds(microseconds));
    return ORBIS_OK;
}
// Host threads of the library have no guest Pthread; common/thread.cpp checks for null.
thread_local Pthread* g_curthread = nullptr;

int* PS4_SYSV_ABI __Error() {
    thread_local int error = 0;
    return &error;
}

// Kernel service thread: runs boost::asio timers for equeue timer events.
boost::asio::io_context io_context;
static std::mutex m_asio_req;
static std::condition_variable_any cv_asio_req;
static std::atomic<u32> asio_requests;
static std::jthread service_thread;

void KernelSignalRequest() {
    std::unique_lock lock{m_asio_req};
    ++asio_requests;
    cv_asio_req.notify_one();
}

static void KernelServiceThread(std::stop_token stoken) {
    Common::SetCurrentThreadName("ctr_gpu:kernel_service");
    while (!stoken.stop_requested()) {
        {
            std::unique_lock lock{m_asio_req};
            cv_asio_req.wait(lock, stoken, [] { return asio_requests != 0; });
        }
        if (stoken.stop_requested()) {
            break;
        }
        io_context.run();
        io_context.restart();
        asio_requests = 0;
    }
}
} // namespace Libraries::Kernel

// RenderDoc capture and screenshot hooks: not used by the runtime.
namespace VideoCore {
void LoadRenderDoc() {}
void StartCapture() {}
void EndCapture() {}
void TriggerCapture() {}
void SetOutputDir(const std::filesystem::path&, const std::string&) {}
bool IsRenderDocLoaded() {
    return false;
}
void RequestScreenshot(ScreenshotRequest) {}
u32 ConsumeGameOnlyScreenshotRequests() {
    return 0;
}
u32 ConsumeWithOverlaysScreenshotRequests() {
    return 0;
}
ScreenshotRequests ConsumeScreenshotRequests() {
    return {};
}
} // namespace VideoCore

namespace {
// SDL video must be driven from one thread: the window lives on its own host thread.
std::mutex g_window_mutex;
std::condition_variable g_window_cv;
bool g_window_ready;
bool g_window_failed;

LONG CALLBACK GpuFaultHandler(EXCEPTION_POINTERS* exception) {
    if (exception->ExceptionRecord->ExceptionCode != EXCEPTION_ACCESS_VIOLATION ||
        exception->ExceptionRecord->NumberParameters < 2) {
        return EXCEPTION_CONTINUE_SEARCH;
    }
    void* address = reinterpret_cast<void*>(exception->ExceptionRecord->ExceptionInformation[1]);
    return Core::Signals::Instance()->DispatchAccessViolation(exception, address)
               ? EXCEPTION_CONTINUE_EXECUTION
               : EXCEPTION_CONTINUE_SEARCH;
}
} // namespace

extern "C" CTR_GPU_API int ctr_gpu_init(const CtrGpuConfig* config) {
    if (!config || config->version != 1) {
        return -1;
    }
    QueryPerformanceFrequency(&g_qpc_frequency);
    QueryPerformanceCounter(&g_qpc_start);
    g_sdk_version = config->sdk_version;
    Core::Emulator::FillElfInfo(*config);
    if (!AddVectoredExceptionHandler(1, GpuFaultHandler)) {
        return -2;
    }
    if (config->user_dir) {
        std::filesystem::path user_path = config->user_dir;
        std::error_code ec;
        std::filesystem::create_directories(user_path / "cache", ec);
        Common::FS::SetUserPath(Common::FS::PathType::UserDir, user_path);
        Common::FS::SetUserPath(Common::FS::PathType::CacheDir, user_path / "cache");
    }
    std::atexit([] {
        Storage::DataBase::Instance().Close();
    });
    const std::string title = config->title ? config->title : "CTRPort";
    const s32 width = config->width, height = config->height;
    std::thread([title, width, height] {
        Common::SetCurrentThreadName("ctr_gpu:window");
        Frontend::WindowSDL* window=nullptr;
        try { window=new Frontend::WindowSDL(width,height,title.c_str()); }
        catch (const std::exception& error) {
            std::fprintf(stderr, "CTR_SETTINGS_WINDOW rejected: %s\n",error.what());
            { std::scoped_lock lock{g_window_mutex}; g_window_failed=true; g_window_ready=true; }
            g_window_cv.notify_all();
            return;
        }
        {
            std::scoped_lock lock{g_window_mutex};
            g_window = window;
            g_window_ready = true;
        }
        g_window_cv.notify_all();
        while (window->PollEvents()) {
            SDL_Delay(2);
        }
        std::fputs("ctr_gpu: window closed by user\n", stderr);
        std::fflush(stderr);
        Storage::DataBase::Instance().Close();
        std::_Exit(0);
    }).detach();
    {
        std::unique_lock lock{g_window_mutex};
        g_window_cv.wait(lock, [] { return g_window_ready; });
        if (g_window_failed) return -3;
    }
    Libraries::Kernel::service_thread = std::jthread{Libraries::Kernel::KernelServiceThread};
    // Frame-rate log: flips, submissions and draws per second every two seconds.
    std::thread([] {
        Common::SetCurrentThreadName("ctr_gpu:stats");
        FILE* file = std::fopen("runtime/user/logs/gpu_rate.log", "w");
        if (!file) {
            return;
        }
        u64 flips = 0, frames = 0, draws = 0;
        for (u64 seconds = 2;; seconds += 2) {
            std::this_thread::sleep_for(std::chrono::seconds(2));
            const u64 f = DebugState.flips.load(), g = DebugState.gnm_frames.load(),
                      d = DebugState.draws.load();
            std::fprintf(file, "t=%llus flips/s=%.1f submits/s=%.1f draws/s=%.0f\n",
                         static_cast<unsigned long long>(seconds), (f - flips) / 2.0,
                         (g - frames) / 2.0, (d - draws) / 2.0);
            std::fflush(file);
            flips = f;
            frames = g;
            draws = d;
        }
    }).detach();
    Core::Loader::SymbolsResolver resolver;
    Libraries::Kernel::RegisterEventQueue(&resolver);
    // GnmDriver creates the presenter that the VideoOut present thread uses.
    Libraries::GnmDriver::RegisterLib(&resolver);
    Libraries::VideoOut::RegisterLib(&resolver);
    return 0;
}

extern "C" CTR_GPU_API int ctr_gpu_handle_fault(void* exception_pointers) {
    auto* exception = static_cast<EXCEPTION_POINTERS*>(exception_pointers);
    return exception && GpuFaultHandler(exception) == EXCEPTION_CONTINUE_EXECUTION ? 1 : 0;
}

extern "C" CTR_GPU_API uintptr_t ctr_gpu_resolve(const char* nid, const char* library) {
    for (const auto& symbol : g_symbols) {
        if (symbol.nid == nid && (!library || symbol.library == library)) {
            return uintptr_t(symbol.address);
        }
    }
    return 0;
}

extern "C" CTR_GPU_API void ctr_gpu_map_memory(uint64_t address, uint64_t size) {
    std::scoped_lock lock{g_mapping_mutex};
    auto* memory = Core::Memory::Instance();
    if (!memory->IsValidGpuMapping(address, size)) {
        return;
    }
    {
        std::unique_lock ranges{g_ranges_mutex};
        g_ranges += boost::icl::interval<VAddr>::right_open(address, address + size);
    }
    if (auto* rasterizer = memory->GetRasterizer()) {
        rasterizer->MapMemory(address, size);
    } else {
        g_pending_mappings.emplace_back(address, size);
    }
}

extern "C" CTR_GPU_API void ctr_gpu_unmap_memory(uint64_t address, uint64_t size) {
    std::scoped_lock lock{g_mapping_mutex};
    {
        std::unique_lock ranges{g_ranges_mutex};
        g_ranges -= boost::icl::interval<VAddr>::right_open(address, address + size);
    }
    if (auto* rasterizer = Core::Memory::Instance()->GetRasterizer()) {
        rasterizer->UnmapMemory(address, size);
    }
    std::erase_if(g_pending_mappings, [&](const auto& m) { return m.first == address; });
}

extern "C" CTR_GPU_API void ctr_gpu_invalidate_memory(uint64_t address, uint64_t size) {
    Core::Memory::Instance()->InvalidateMemory(address, size);
}

extern "C" CTR_GPU_API unsigned ctr_gpu_symbol_count(void) {
    return unsigned(g_symbols.size());
}
extern "C" CTR_GPU_API uint64_t ctr_gpu_flip_count(void) {
    return DebugState.flips.load(std::memory_order_relaxed);
}
extern "C" CTR_GPU_API uint64_t ctr_gpu_submit_count(void) {
    return DebugState.gnm_frames.load(std::memory_order_relaxed);
}
