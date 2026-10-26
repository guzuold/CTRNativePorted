// SPDX-License-Identifier: GPL-2.0-or-later
// Layout of the Alchemy PS4 render objects of CUSA14876 1.00 and 1.21: field offsets read from
// the retail field registration of igPS4GraphicsDevice / igPS4GraphicsDeviceContext /
// igPS4ShaderHeader and checked on live objects during draws ((development notes)), the
// same as FrankHUN88453's engine.h for CUSA13795 (MIT). Addresses are per eboot SHA256.
#pragma once
#include <cstring>

#include "common.h"

namespace eng {

struct Version {
    const char* eboot_sha256;
    u64 device_global;          // igGraphicsDevice* (image offset)
    u64 clear_function;         // igPS4GraphicsDeviceContext::clear(ctx, mode, float4*, float depth, stencil)
    u64 copy_texture_function;  // igPS4GraphicsDeviceContext::copyTexture(ctx, src, dst, params)
    u32 clear_prologue[4];      // first 16 code bytes, checked before hooking
    u32 copy_prologue[4];
};
// Device globals: located by the engine-state probe (metaobject vtables). clear / copyTexture: the
// callees of igCommandStreamDecoder's clear and copy commands and of igWorkflowGpuPicking::getPixel.
inline constexpr Version kVersions[] = {
    {"ca9c94411375ebec52de70fbc62f605ac8e2cea95595c627029969b5b9d9b5b1", 0x26e7920, 0x1306ba0, 0x1307f20,
     {0xe5894855, 0x56415741, 0x54415541, 0xec834853}, {0xe5894855, 0x56415741, 0x54415541, 0xe4834853}},
    {"39d716b70a299b2546bd39996c8592f9375d3f18f9bb8bf11f9139e9af1eb3f5", 0x2aebce0, 0x13c9e40, 0x13cb1c0,
     {0xe5894855, 0x56415741, 0x54415541, 0xec834853}, {0xe5894855, 0x56415741, 0x54415541, 0xe4834853}},
};
const Version* CurrentVersion();

// igPS4GraphicsDevice
constexpr u64 DEV_COLOR_BB_TEX0 = 0x508, DEV_COLOR_BB_TEX1 = 0x510, DEV_BB_W = 0x540,
              DEV_BB_H = 0x544, DEV_MISSING_TEXTURE = 0x580, DEV_CONTEXT = 0x5b0;

// igPS4GraphicsDeviceContext
constexpr u64 CTX_VIEWPORT = 0x0c, CTX_SCISSOR_ENABLED = 0x29, CTX_SCISSOR = 0x2c, CTX_DEVICE = 0x40,
              CTX_ALPHA_TO_MASK = 0x100, CTX_BLEND_CONTROL = 0x104, CTX_CLIP_CONTROL = 0x108,
              CTX_PRIM_SETUP = 0x10c, CTX_DEPTH_STENCIL_CONTROL = 0x110,
              CTX_STENCIL_CONTROL = 0x114, CTX_STENCIL_OP_CONTROL = 0x118,
              CTX_COLOR_TARGETS = 0x130, CTX_RASTER_STATE = 0x190, CTX_DEPTH_TARGET = 0x170, CTX_RT_MASK = 0x198,
              CTX_PS_SAMPLERS = 0x1a0, CTX_PS_TEXTURES = 0x220, CTX_VS_SAMPLERS = 0x2a0,
              CTX_VS_TEXTURES = 0x320, CTX_VERTEX_BUFFERS = 0x4a0, CTX_VERTEX_FORMATS = 0x520,
              CTX_INDEX_BUFFER = 0x5a0, CTX_VS = 0x5a8, CTX_GS = 0x5b0, CTX_PS = 0x5b8,
              CTX_PRIM_TYPE = 0x5c0, CTX_INDEX_SIZE = 0x5c4, CTX_STENCIL_REF = 0x5c8,
              CTX_INDEX_OFFSET = 0x5cc, CTX_VB_OFFSETS = 0x5d0, CTX_VB_STRIDES = 0x610,
              CTX_INSTANCED_VBS = 0x650;

// shader resource (pool object): igPS4ShaderHeader* and the Gnm shader struct
constexpr u64 SH_HEADER = 0x18, SH_GNM = 0x30;
// igPS4ShaderHeader vectors: count at +0, data pointer at +0x10
constexpr u64 HDR_INPUT_ELEMENTS = 0x30, HDR_CB_MEMORY = 0x48, HDR_CB_SIZE = 0x60,
              HDR_CB_INDEX = 0x78;

template <class T>
inline T R(u64 addr) {
    T v;
    memcpy(&v, (const void*)addr, sizeof(T));
    return v;
}
inline u64 Q(u64 a) { return a ? R<u64>(a) : 0; }

inline u64 ShaderCode(u64 shader, u32* size) {
    u64 gnm = Q(shader + SH_GNM);
    u32 common = R<u32>(gnm);
    *size = common & 0x7FFFFF;
    u64 lo = R<u32>(gnm + 8), hi = R<u32>(gnm + 12);
    return (lo << 8) | (hi << 40);
}

struct ViewportI {
    s32 x, y, w, h;
    float zmin, zmax;
};

}  // namespace eng
