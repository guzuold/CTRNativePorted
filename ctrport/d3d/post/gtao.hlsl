// SPDX-License-Identifier: GPL-2.0-or-later
// Ambient occlusion of the Direct3D 11 renderer: Intel XeGTAO (gtao/XeGTAO.hlsli, MIT) on the game's
// own half-resolution linear depth, written into the AO channel of the game's AO target right
// after the game's AO pass (ctrport/d3d/post.cpp); the game's bilateral blur and lighting follow.
// Compiled once per pass with one of GTAO_PREFILTER, GTAO_MAIN, GTAO_DENOISE, GTAO_APPLY.
// The game's depth is 32-bit viewspace distance in world units, up to the far plane (150000), so
// the 32-bit path is used and normals are made from depth in the main pass.
#define XE_GTAO_USE_HALF_FLOAT_PRECISION 0
#define XE_GTAO_FP32_DEPTHS 1
#define XE_GTAO_LINEAR_DEPTH_INPUT 1
#define XE_GTAO_GENERATE_NORMALS_INPLACE 1
// from Intel's sample framework (vaShared.hlsl), which XeGTAO.hlsli expects
#define VA_SATURATE saturate
#define VA_PI 3.1415926535897932384626433832795
#include "XeGTAO.h"

cbuffer GTAOConstantBuffer : register(b0) {
    GTAOConstants g_GTAOConsts;
    float4 gtao_apply; // x: depth pixels per AO target pixel (1 or 2), y: sky distance
};
#include "XeGTAO.hlsli"

SamplerState g_samplerPointClamp : register(s0);

#if GTAO_PREFILTER
Texture2D<float> g_srcRawDepth : register(t0);
RWTexture2D<lpfloat> g_outWorkingDepthMIP0 : register(u0);
RWTexture2D<lpfloat> g_outWorkingDepthMIP1 : register(u1);
RWTexture2D<lpfloat> g_outWorkingDepthMIP2 : register(u2);
RWTexture2D<lpfloat> g_outWorkingDepthMIP3 : register(u3);
RWTexture2D<lpfloat> g_outWorkingDepthMIP4 : register(u4);
[numthreads(8, 8, 1)] void main(uint2 dispatchThreadID : SV_DispatchThreadID, uint2 groupThreadID : SV_GroupThreadID) {
    XeGTAO_PrefilterDepths16x16(dispatchThreadID, groupThreadID, g_GTAOConsts, g_srcRawDepth, g_samplerPointClamp,
                                g_outWorkingDepthMIP0, g_outWorkingDepthMIP1, g_outWorkingDepthMIP2,
                                g_outWorkingDepthMIP3, g_outWorkingDepthMIP4);
}
#endif

#if GTAO_MAIN
Texture2D<lpfloat> g_srcWorkingDepth : register(t0);
RWTexture2D<uint> g_outWorkingAOTerm : register(u0);
RWTexture2D<unorm float> g_outWorkingEdges : register(u1);
// spatial noise as in Intel's sample (vaGTAO.hlsl): Hilbert curve index through an R2 sequence
lpfloat2 SpatialNoise(uint2 pixCoord) {
    const uint index = HilbertIndex(pixCoord.x % XE_HILBERT_WIDTH, pixCoord.y % XE_HILBERT_WIDTH);
    return lpfloat2(frac(0.5 + index * float2(0.75487766624669276005, 0.5698402909980532659114)));
}
// Intel's quality levels, as literals so the step loop unrolls: high 3 slices x 3 steps, ultra 9 x 3
#if GTAO_ULTRA
#define GTAO_SLICES 9
#else
#define GTAO_SLICES 3
#endif
#define GTAO_STEPS 3
[numthreads(XE_GTAO_NUMTHREADS_X, XE_GTAO_NUMTHREADS_Y, 1)] void main(uint2 pixCoord : SV_DispatchThreadID) {
    if (any(pixCoord >= (uint2)g_GTAOConsts.ViewportSize)) return;
    XeGTAO_MainPass(pixCoord, GTAO_SLICES, GTAO_STEPS, SpatialNoise(pixCoord), (lpfloat3)0,
                    g_GTAOConsts, g_srcWorkingDepth, g_samplerPointClamp, g_outWorkingAOTerm, g_outWorkingEdges);
}
#endif

#if GTAO_DENOISE
Texture2D<uint> g_srcWorkingAOTerm : register(t0);
Texture2D<lpfloat> g_srcWorkingEdges : register(t1);
RWTexture2D<uint> g_outFinalAOTerm : register(u0);
[numthreads(XE_GTAO_NUMTHREADS_X, XE_GTAO_NUMTHREADS_Y, 1)] void main(uint2 dispatchThreadID : SV_DispatchThreadID) {
    // each thread denoises two horizontally adjacent pixels
    const uint2 pixCoordBase = dispatchThreadID * uint2(2, 1);
    XeGTAO_Denoise(pixCoordBase, g_GTAOConsts, g_srcWorkingAOTerm, g_srcWorkingEdges, g_samplerPointClamp,
                   g_outFinalAOTerm, true);
}
#endif

#if GTAO_APPLY
// drawn over the game's AO target with only its AO channel writable; XeGTAO ran on the
// full-resolution depth, so each target pixel averages its ratio x ratio block
Texture2D<uint> g_finalAO : register(t0);
Texture2D<float> g_sceneDepth : register(t1); // the game's linear depth (negative viewspace z)
float4 vsmain(uint id : SV_VertexID) : SV_Position {
    const float2 uv = float2((id << 1) & 2, id & 2);
    return float4(uv * float2(2, -2) + float2(-1, 1), 0, 1);
}
float4 main(float4 pos : SV_Position) : SV_Target0 {
    const uint ratio = (uint)gtao_apply.x;
    const int2 base = int2(pos.xy) * ratio;
    float sum = 0;
    for (uint y = 0; y < ratio; y++)
        for (uint x = 0; x < ratio; x++) {
            const int3 p = int3(base + int2(x, y), 0);
            // the sky and the farthest scenery stay unoccluded
            sum += -g_sceneDepth.Load(p) > gtao_apply.y ? 1.0 : saturate(g_finalAO.Load(p).x / 255.0);
        }
    return (sum / (ratio * ratio)).xxxx;
}
// debugging aids (CTR_D3D_AODebug=2, 3): the depth XeGTAO works on, the game's depth
#define g_debugDepth g_sceneDepth
float4 depthps(float4 pos : SV_Position) : SV_Target0 {
    return saturate(g_debugDepth.Load(int3(pos.xy * gtao_apply.x, 0)) / 20000.0).xxxx;
}
float4 rawps(float4 pos : SV_Position) : SV_Target0 {
    const float d = g_debugDepth.Load(int3(pos.xy * gtao_apply.x, 0));
    // red: |d| x 100 (normalized depth), green: |d| / 20000 (world units), blue: 1 when negative
    return float4(saturate(abs(d) * 100.0), saturate(abs(d) / 20000.0), d < 0 ? 1.0 : 0.0, 1);
}
#endif
