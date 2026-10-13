// SPDX-License-Identifier: GPL-2.0-or-later
// ctr_hle.dll: system library functions for ctrport written directly in the guest
// SysV ABI (clang-cl, __attribute__((sysv_abi))), so new services need no assembly
// bridge. ctrport binds every guest import that ctr_hle_resolve() provides.
// Each function logs every call as one JSON line on stdout.
#pragma once
#include <cstdint>
#include <cstdio>
#include <mutex>

#define CTR_SYSV __attribute__((sysv_abi))
#ifdef CTR_HLE_BUILD
#define CTR_HLE_API extern "C" __declspec(dllexport)
#else
#define CTR_HLE_API extern "C" __declspec(dllimport)
#endif

// Services ctrport offers to ctr_hle (Win64 ABI callbacks into ctrport.exe).
struct CtrHleHostV1 {
    std::uint32_t version; // 1
    const char* title_id;  // e.g. "CUSA14876"
    // Guest path prefix (e.g. "/savedata0") resolved by ctrport's file system to a host
    // directory inside runtime/user (UTF-8 path relative to the working directory).
    int (*mount)(const char* guest_point, const char* host_root);
    int (*unmount)(const char* guest_point);
};

CTR_HLE_API std::uintptr_t ctr_hle_resolve(const char* nid, const char* library);
CTR_HLE_API unsigned ctr_hle_count(void);
CTR_HLE_API int ctr_hle_set_host(const CtrHleHostV1* host);

namespace ctr_hle {
const CtrHleHostV1* host();
struct Registration {
    const char* nid;
    const char* library;
    const void* function;
    Registration* next;
    Registration(const char* nid_, const char* library_, const void* function_);
};
// One JSON line per call: {"scope":"hle_call","library":..,"function":..,"result":..,...}
inline void log_call(const char* library, const char* function, long long result, const char* extra = "") {
    static std::mutex mutex;
    std::scoped_lock lock{mutex};
    std::printf("{\"scope\":\"hle_call\",\"library\":\"%s\",\"function\":\"%s\",\"result\":%lld%s}\n",
                library, function, result, extra);
    std::fflush(stdout);
}
} // namespace ctr_hle

#define CTR_HLE_REGISTER(nid, library, function)                                                 \
    static ctr_hle::Registration registration_##function{nid, library,                          \
                                                         reinterpret_cast<const void*>(&function)}
