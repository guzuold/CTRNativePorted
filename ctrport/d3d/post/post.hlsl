// SPDX-License-Identifier: GPL-2.0-or-later
// Scene post-processing of the Direct3D 11 renderer, as compute shaders run on the finished 3D
// scene before the game's display pass draws the HUD over it (ctrport/d3d/post.cpp).
// Compiled once per pass with one of PASS_EDGES, PASS_WEIGHTS, PASS_BLEND, PASS_COLOR, PASS_CAS:
// - SMAA 1x (smaa/SMAA.hlsl, Jimenez et al., MIT): luma edges, blending weights, neighbourhood
//   blending. The pixel shader entry points of SMAA.hlsl are called per thread with the texture
//   coordinates and offsets its vertex shaders would have produced.
// - colour: warmth, contrast, saturation and vibrance on the display-encoded scene.
// - CAS (cas/ffx_cas.h, AMD FidelityFX Contrast Adaptive Sharpening, MIT) without scaling.

cbuffer PostConstants : register(b0) {
    float4 post_rt;    // 1 / width, 1 / height, width, height
    float4 post_color; // saturation, contrast, vibrance, warmth (1, 1, 0, 0 = unchanged)
    float4 post_cas;   // x: CAS sharpness 0..1
};

SamplerState LinearSampler : register(s0);
SamplerState PointSampler : register(s1);

float2 TexCoord(uint2 p) {
    return (float2(p) + 0.5) * post_rt.xy;
}
bool Outside(uint2 p) {
    return p.x >= (uint)post_rt.z || p.y >= (uint)post_rt.w;
}

#if PASS_EDGES || PASS_WEIGHTS || PASS_BLEND
#define SMAA_CUSTOM_SL 1
#define SMAA_RT_METRICS post_rt
#define SMAATexture2D(tex) Texture2D tex
#define SMAATexturePass2D(tex) tex
#define SMAASampleLevelZero(tex, coord) tex.SampleLevel(LinearSampler, coord, 0)
#define SMAASampleLevelZeroPoint(tex, coord) tex.SampleLevel(PointSampler, coord, 0)
#define SMAASampleLevelZeroOffset(tex, coord, offset) tex.SampleLevel(LinearSampler, coord, 0, offset)
#define SMAASample(tex, coord) tex.SampleLevel(LinearSampler, coord, 0)
#define SMAASamplePoint(tex, coord) tex.SampleLevel(PointSampler, coord, 0)
#define SMAASampleOffset(tex, coord, offset) tex.SampleLevel(LinearSampler, coord, 0, offset)
#define SMAA_FLATTEN [flatten]
#define SMAA_BRANCH [branch]
#define SMAATexture2DMS2(tex) Texture2DMS<float4, 2> tex
#define SMAALoad(tex, pos, sample) tex.Load(pos, sample)
// the edge detection functions discard pixels without an edge: here they store no edge
#define discard return float2(0.0, 0.0)
#include "SMAA.hlsl"
#undef discard
#endif

#if PASS_EDGES
Texture2D ColorTex : register(t0);
RWTexture2D<float2> EdgesOut : register(u0);
[numthreads(8, 8, 1)] void main(uint3 id : SV_DispatchThreadID) {
    if (Outside(id.xy)) return;
    const float2 uv = TexCoord(id.xy);
    float4 offset[3];
    SMAAEdgeDetectionVS(uv, offset);
    EdgesOut[id.xy] = SMAALumaEdgeDetectionPS(uv, offset, ColorTex);
}
#endif

#if PASS_WEIGHTS
Texture2D EdgesTex : register(t0);
Texture2D AreaTex : register(t1);
Texture2D SearchTex : register(t2);
RWTexture2D<float4> BlendOut : register(u0);
[numthreads(8, 8, 1)] void main(uint3 id : SV_DispatchThreadID) {
    if (Outside(id.xy)) return;
    const float2 uv = TexCoord(id.xy);
    float2 pixcoord;
    float4 offset[3];
    SMAABlendingWeightCalculationVS(uv, pixcoord, offset);
    BlendOut[id.xy] = SMAABlendingWeightCalculationPS(uv, pixcoord, offset, EdgesTex, AreaTex, SearchTex, 0);
}
#endif

#if PASS_BLEND
Texture2D ColorTex : register(t0);
Texture2D BlendTex : register(t1);
RWTexture2D<float4> ColorOut : register(u0);
[numthreads(8, 8, 1)] void main(uint3 id : SV_DispatchThreadID) {
    if (Outside(id.xy)) return;
    const float2 uv = TexCoord(id.xy);
    float4 offset;
    SMAANeighborhoodBlendingVS(uv, offset);
    ColorOut[id.xy] = SMAANeighborhoodBlendingPS(uv, offset, ColorTex, BlendTex);
}
#endif

#if PASS_COLOR
Texture2D ColorTex : register(t0);
RWTexture2D<float4> ColorOut : register(u0);
[numthreads(8, 8, 1)] void main(uint3 id : SV_DispatchThreadID) {
    if (Outside(id.xy)) return;
    float4 c = ColorTex.Load(int3(id.xy, 0));
    float3 rgb = c.rgb;
    // warmth: a mild white balance shift towards amber (positive) or blue (negative)
    rgb *= float3(1.0 + 0.1 * post_color.w, 1.0, 1.0 - 0.1 * post_color.w);
    // contrast around mid grey
    rgb = (rgb - 0.5) * post_color.y + 0.5;
    const float luma = dot(rgb, float3(0.2126, 0.7152, 0.0722));
    // vibrance: more saturation where colours are still dull
    const float chroma = max(rgb.r, max(rgb.g, rgb.b)) - min(rgb.r, min(rgb.g, rgb.b));
    const float saturation = post_color.x * (1.0 + post_color.z * (1.0 - saturate(chroma)));
    rgb = lerp(luma.xxx, rgb, saturation);
    ColorOut[id.xy] = float4(max(rgb, 0.0), c.a);
}
#endif

#if PASS_CAS
#define A_GPU 1
#define A_HLSL 1
#include "ffx_a.h"
Texture2D ColorTex : register(t0);
RWTexture2D<float4> ColorOut : register(u0);
AF3 CasLoad(ASU2 p) {
    return ColorTex.Load(int3(p, 0)).rgb;
}
void CasInput(inout AF1 r, inout AF1 g, inout AF1 b) {}
#include "ffx_cas.h"
void CasPixel(AU2 p, AU4 const0, AU4 const1) {
    if (Outside(p)) return;
    AF3 c;
    CasFilter(c.r, c.g, c.b, p, const0, const1, AP1(true));
    ColorOut[p] = AF4(c, 1);
}
[numthreads(64, 1, 1)] void main(uint3 lid : SV_GroupThreadID, uint3 gid : SV_GroupID) {
    AU4 const0, const1;
    CasSetup(const0, const1, post_cas.x, post_rt.z, post_rt.w, post_rt.z, post_rt.w);
    // as AMD's sample: a 16x16 tile per group of 64 threads
    AU2 p = ARmp8x8(lid.x) + AU2(gid.x << 4u, gid.y << 4u);
    CasPixel(p, const0, const1);
    p.x += 8u;
    CasPixel(p, const0, const1);
    p.y += 8u;
    CasPixel(p, const0, const1);
    p.x -= 8u;
    CasPixel(p, const0, const1);
}
#endif
