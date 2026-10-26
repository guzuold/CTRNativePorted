// SPDX-License-Identifier: MIT
// AMD FidelityFX Super Resolution 1 (EASU upscale, RCAS sharpen) as D3D11 compute shaders, after
// the Vulkan compute shader shadPS4 v0.18.0 uses (src/video_core/host_shaders/fsr.comp, AMD sample
// code, MIT): 32-bit float path for Shader Model 5, constants set up on the GPU from the sizes.
// Compiled by ctrport/d3d/renderer.cpp with SAMPLE_EASU or SAMPLE_RCAS defined.
#define A_GPU 1
#define A_HLSL 1
#include "ffx_a.h"

cbuffer FsrSizes : register(b0) {
    float4 fsr_sizes;      // input width, height, output width, height (pixels)
    float fsr_sharpness;   // RCAS attenuation in stops (0 = sharpest)
    float3 fsr_pad;
};
Texture2D<float4> InputTexture : register(t0);
RWTexture2D<float4> OutputTexture : register(u0);
SamplerState InputSampler : register(s0);

#if SAMPLE_EASU
#define FSR_EASU_F 1
AF4 FsrEasuRF(AF2 p) { return InputTexture.GatherRed(InputSampler, p); }
AF4 FsrEasuGF(AF2 p) { return InputTexture.GatherGreen(InputSampler, p); }
AF4 FsrEasuBF(AF2 p) { return InputTexture.GatherBlue(InputSampler, p); }
#endif
#if SAMPLE_RCAS
#define FSR_RCAS_F 1
AF4 FsrRcasLoadF(ASU2 p) { return InputTexture.Load(int3(p, 0)); }
void FsrRcasInputF(inout AF1 r, inout AF1 g, inout AF1 b) {}
#endif

#include "ffx_fsr1.h"

void CurrFilter(AU2 pos) {
#if SAMPLE_EASU
    AU4 con0, con1, con2, con3;
    FsrEasuCon(con0, con1, con2, con3, fsr_sizes.x, fsr_sizes.y, fsr_sizes.x, fsr_sizes.y, fsr_sizes.z, fsr_sizes.w);
    AF3 c;
    FsrEasuF(c, pos, con0, con1, con2, con3);
    OutputTexture[pos] = AF4(c, 1);
#endif
#if SAMPLE_RCAS
    AU4 con;
    FsrRcasCon(con, fsr_sharpness);
    AF3 c;
    FsrRcasF(c.r, c.g, c.b, pos, con);
    OutputTexture[pos] = AF4(c, 1);
#endif
}

[numthreads(64, 1, 1)]
void main(uint3 lid : SV_GroupThreadID, uint3 gid : SV_GroupID) {
    // remap the local xy of the workgroup for a more PS-like swizzle pattern
    AU2 gxy = ARmp8x8(lid.x) + AU2(gid.x << 4u, gid.y << 4u);
    CurrFilter(gxy);
    gxy.x += 8u;
    CurrFilter(gxy);
    gxy.y += 8u;
    CurrFilter(gxy);
    gxy.x -= 8u;
    CurrFilter(gxy);
}
