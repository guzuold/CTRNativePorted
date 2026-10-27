// SPDX-License-Identifier: GPL-2.0-or-later
// ctr_d3d: engine-state D3D11 backend for ctrport (see VENDOR.md). Shared types, the guest
// SysV calling convention and the backend log (runtime/user/logs/ctr_d3d.log).
#pragma once
#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <string>

#define SYSV __attribute__((sysv_abi))

using u8 = uint8_t;
using u16 = uint16_t;
using u32 = uint32_t;
using u64 = uint64_t;
using s16 = int16_t;
using s32 = int32_t;
using s64 = int64_t;

void LogInit(const char* path);
void Log(const char* fmt, ...) __attribute__((format(printf, 1, 2)));
void LogFlush();
[[noreturn]] void Fatal(const char* fmt, ...) __attribute__((format(printf, 1, 2)));
#define LOG(...) Log(__VA_ARGS__)

// Integer setting from the environment (CTR_D3D_<name>), else `def`.
int ConfigInt(const char* name, int def);
// Settings do not change while the game runs; kept for the renderer's change checks.
inline unsigned ConfigGeneration() {
    return 0;
}
// Writable backend data: runtime/user/gpu_d3d (translated shaders, dumps, captures).
std::filesystem::path UserRoot();

// SCE error codes: 0x80020000 | errno (FreeBSD numbering)
namespace err {
constexpr int ENOENT_ = 2, ESRCH_ = 3, EBADF_ = 9, ENOMEM_ = 12, EFAULT_ = 14, EBUSY_ = 16, EEXIST_ = 17, EINVAL_ = 22,
              EAGAIN_ = 35, ETIMEDOUT_ = 60, ENOSYS_ = 78;
}
constexpr int SceErr(int e) {
    return (int)(0x80020000u | (u32)e);
}

// The eboot the backend reads engine state from (ctr_d3d_set_eboot).
struct Eboot {
    u8* base = nullptr;
    u64 size = 0;
    std::string sha256;
};
extern Eboot g_eboot;

// Exports registered by NID; filled by the REGISTER tables of each source file.
void RegisterExport(const char* nid, const char* library, const char* name, void* function);
#define D3D_CAT2(a, b) a##b
#define D3D_CAT(a, b) D3D_CAT2(a, b)
#define EXPORT(nid, lib, fn)                                                                                           \
    static const int D3D_CAT(export_, __COUNTER__) = (RegisterExport(nid, lib, #fn, (void*)&fn), 0)

// Reports a guest call the backend cannot serve and stops the process (no silent success).
[[noreturn]] void Unsupported(const char* library, const char* name, const char* nid, u64 caller, const char* why);
#define CALLER() ((u64)__builtin_return_address(0))
