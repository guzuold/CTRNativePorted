// SPDX-License-Identifier: GPL-2.0-only
// Address-bound values of each supported retail eboot, keyed by the SHA256 of the whole
// eboot.bin. Offsets are in the image as flat_loader.hpp maps it; every patch site is still
// checked against its original bytes before it is written.
// EU CUSA14876: v01.00 and v01.21, validated by original execution (AGENTS.md targets).
// US CUSA13795: v01.00 and v01.21, derived from the user's dump by byte comparison with the
// EU sites (functions shifted by 0x10..0x40 in places); not yet run in this runtime.
#pragma once
#include <cstdint>
#include <string>
namespace ctrport {
struct RetailProfile {
    const char* sha256;
    const char* region;     // "EU" or "US"
    const char* title_id;
    unsigned version;       // 100 or 121
    std::uint64_t proc;     // process parameter (size 88, SDK 0x6008001)
    std::uint64_t mem;      // memory parameter the process parameter points to
    std::uint64_t worker;   // startup worker thread entry
    std::uint64_t tsd;      // TSD key global the worker reads
    std::uint64_t fps60;    // GoldHEN 60 FPS store, 0 when no binding
    std::uint64_t scale4k;  // native 4K: scene scale load in the Neo branch
    std::uint64_t vram4k;   // native 4K: Gfx/@vramBSize immediate in the Neo branch
    std::uint64_t online_key; // Demonware auth3 traffic key, 0x126 bytes
};
inline constexpr RetailProfile retail_profiles[]={
    {"ca9c94411375ebec52de70fbc62f605ac8e2cea95595c627029969b5b9d9b5b1","EU","CUSA14876",100,
     0x2600000,0x2200090,0x1409af0,0x260cb50,0,0x12e74c2,0x1aacfcb,0x1ade440},
    {"39d716b70a299b2546bd39996c8592f9375d3f18f9bb8bf11f9139e9af1eb3f5","EU","CUSA14876",121,
     0x2a00000,0x2400090,0x14cfb10,0x2a0cb50,0x13a9d80,0x13aa652,0x1bbb918,0x1becf40},
    {"23ddf3909d87a42a4c74be395865b130e4128b892de59d71082d8620cd8cd858","US","CUSA13795",100,
     0x2600000,0x2200090,0x1409ae0,0x260cb50,0,0x12e74b2,0x1aacfbb,0x1ade440},
    {"e3b7a068b5e7be0f976102aa223a74425e7817c67e26e21416163291750c1b83","US","CUSA13795",121,
     0x2a00000,0x2400090,0x14cfb20,0x2a0cb50,0x13a9d90,0x13aa662,0x1bbb938,0x1becf80},
};
inline const RetailProfile* retail_profile(const std::string& sha256) {
    for(const auto& profile:retail_profiles) if(sha256==profile.sha256) return &profile;
    return nullptr;
}
}
