// SPDX-License-Identifier: GPL-2.0-or-later
// Scene post-processing of the Direct3D 11 renderer: see post.h and post/post.hlsl. SMAA's
// lookup textures come from smaa/AreaTex.h and smaa/SearchTex.h (MIT).
#include "post.h"

#include <d3dcompiler.h>
#include <wrl/client.h>

#include <algorithm>
#include <cstring>

#include <cassert>
#include <cmath>

#include "../settings/renderer_controls/gpu_policy.hpp"
#include "gtao/XeGTAO.h"
#include "smaa/AreaTex.h"
#include "smaa/SearchTex.h"

using Microsoft::WRL::ComPtr;

namespace {
// post/post.hlsl and the headers it includes, embedded by CMake
struct PostSource {
    const char* name;
    const char* text;
};
const PostSource kPostSources[] = {
#include "post_sources.inc"
};
struct PostInclude : ID3DInclude {
    HRESULT __stdcall Open(D3D_INCLUDE_TYPE, LPCSTR name, LPCVOID, LPCVOID* data, UINT* bytes) override {
        for (const auto& s : kPostSources)
            if (!strcmp(s.name, name)) {
                *data = s.text, *bytes = (UINT)strlen(s.text);
                return S_OK;
            }
        return E_FAIL;
    }
    HRESULT __stdcall Close(LPCVOID) override { return S_OK; }
};

struct Target {
    ComPtr<ID3D11Texture2D> tex;
    ComPtr<ID3D11ShaderResourceView> srv;
    ComPtr<ID3D11UnorderedAccessView> uav;
};

struct Post {
    ID3D11Device* dev = nullptr;
    ID3D11DeviceContext* ctx = nullptr;
    int aa = 0;             // 0 off, 1 SMAA high, 2 SMAA ultra
    float sharpen = -1.0f;  // CAS sharpness 0..1, negative = off
    float color[4] = {1, 1, 0, 0};
    bool color_on = false;
    ComPtr<ID3D11ComputeShader> edges, weights, blend, color_cs, cas;
    ComPtr<ID3D11PixelShader> fxaa_copy; // stands in for the game's FXAA with SMAA or anti-aliasing off
    u64 fxaa_replaced = 0;
    ComPtr<ID3D11Buffer> cb;
    ComPtr<ID3D11SamplerState> linear, point;
    ComPtr<ID3D11ShaderResourceView> area, search;
    Target edge_tex, blend_tex, out[2];
    u32 w = 0, h = 0;
    u64 frames = 0;
} g_post;

// XeGTAO state (PostAmbientOcclusion)
struct Gtao {
    int level = 0; // 0 off (the game's AO), 1 high, 2 ultra
    ComPtr<ID3D11ComputeShader> prefilter, main, denoise;
    ComPtr<ID3D11VertexShader> apply_vs;
    ComPtr<ID3D11PixelShader> apply_ps, depth_ps, raw_ps;
    ComPtr<ID3D11BlendState> red_only;
    ComPtr<ID3D11RasterizerState> raster;
    ComPtr<ID3D11DepthStencilState> no_depth;
    ComPtr<ID3D11Buffer> cb;
    ComPtr<ID3D11Texture2D> depth_tex; // viewspace depth, 5 mips
    ComPtr<ID3D11ShaderResourceView> depth_srv;
    ComPtr<ID3D11UnorderedAccessView> depth_uav[XE_GTAO_DEPTH_MIP_LEVELS];
    Target term, edges, final_term;
    u32 w = 0, h = 0;
    u64 frames = 0, skipped = 0;
} g_gtao;
struct GtaoCB {
    XeGTAO::GTAOConstants consts;
    float apply[4]; // x: depth pixels per AO target pixel, y: distance from which the sky starts
};

float Percent(const char* name, int def, int lo, int hi) {
    return std::clamp(ConfigInt(name, def), lo, hi) / 100.0f;
}

// CAS sharpness 0..1 from a Sharpness percent; 0% and Original sharpen nothing (-1)
float SharpenFromPercent(int percent) {
    return percent <= 0 ? -1.0f : std::min(percent, 100) / 100.0f;
}
// The Sharpness setting now: a choice made in the game menu (live) or the one at start
float Sharpen() {
    namespace live = ctrport::settings::live_gpu;
    const auto word = live::sharpness.word.load(std::memory_order_acquire);
    if (!(word >> 32)) return g_post.sharpen;
    static std::uint64_t seen = 0;
    static float value = -1.0f;
    if (word != seen) { // asked once per draw: decode only a new choice
        const std::string text = live::sharp(static_cast<unsigned>(word));
        value = text == "original" ? -1.0f : SharpenFromPercent(std::stoi(text));
        seen = word;
    }
    return value;
}

// One pass of an embedded source (`file`), selected by the macro `pass`
ComPtr<ID3DBlob> CompileCode(const char* file, const char* pass, const char* entry, const char* profile,
                             const char* preset = nullptr) {
    const PostSource* source = nullptr;
    for (const auto& s : kPostSources)
        if (!strcmp(s.name, file)) source = &s;
    if (!source) Fatal("post-processing source %s missing", file);
    PostInclude include;
    const D3D_SHADER_MACRO defines[] = {{pass, "1"}, {preset ? preset : "POST_NO_PRESET", "1"}, {nullptr, nullptr}};
    ComPtr<ID3DBlob> code, err;
    if (FAILED(D3DCompile(source->text, strlen(source->text), file, defines, &include, entry, profile,
                          D3DCOMPILE_OPTIMIZATION_LEVEL3, 0, &code, &err)))
        Fatal("post-processing shader %s: %s", pass, err ? (const char*)err->GetBufferPointer() : "?");
    return code;
}
ComPtr<ID3D11ComputeShader> Compute(const char* file, const char* pass, const char* preset = nullptr) {
    const auto code = CompileCode(file, pass, "main", "cs_5_0", preset);
    ComPtr<ID3D11ComputeShader> cs;
    g_post.dev->CreateComputeShader(code->GetBufferPointer(), code->GetBufferSize(), nullptr, &cs);
    return cs;
}
ComPtr<ID3D11ComputeShader> Compile(const char* pass, int aa) {
    return Compute("post.hlsl", pass, aa == 2 ? "SMAA_PRESET_ULTRA" : "SMAA_PRESET_HIGH");
}

ComPtr<ID3D11ShaderResourceView> Lookup(const void* bytes, u32 w, u32 h, u32 pitch, DXGI_FORMAT fmt) {
    D3D11_TEXTURE2D_DESC td{};
    td.Width = w, td.Height = h, td.MipLevels = td.ArraySize = 1;
    td.Format = fmt;
    td.SampleDesc.Count = 1;
    td.Usage = D3D11_USAGE_IMMUTABLE;
    td.BindFlags = D3D11_BIND_SHADER_RESOURCE;
    const D3D11_SUBRESOURCE_DATA data{bytes, pitch, 0};
    ComPtr<ID3D11Texture2D> tex;
    ComPtr<ID3D11ShaderResourceView> srv;
    if (FAILED(g_post.dev->CreateTexture2D(&td, &data, &tex)) ||
        FAILED(g_post.dev->CreateShaderResourceView(tex.Get(), nullptr, &srv)))
        Fatal("SMAA lookup texture");
    return srv;
}

bool MakeTarget(Target& t, u32 w, u32 h, DXGI_FORMAT fmt) {
    D3D11_TEXTURE2D_DESC td{};
    td.Width = w, td.Height = h, td.MipLevels = td.ArraySize = 1;
    td.Format = fmt;
    td.SampleDesc.Count = 1;
    td.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS;
    t = {};
    return SUCCEEDED(g_post.dev->CreateTexture2D(&td, nullptr, &t.tex)) &&
           SUCCEEDED(g_post.dev->CreateShaderResourceView(t.tex.Get(), nullptr, &t.srv)) &&
           SUCCEEDED(g_post.dev->CreateUnorderedAccessView(t.tex.Get(), nullptr, &t.uav));
}

void InitGtao(ID3D11Device* dev) {
    g_gtao.level = std::clamp(ConfigInt("AO", 0), 0, 2);
    if (!g_gtao.level) return;
    g_gtao.prefilter = Compute("gtao.hlsl", "GTAO_PREFILTER");
    g_gtao.main = Compute("gtao.hlsl", "GTAO_MAIN", g_gtao.level == 2 ? "GTAO_ULTRA" : nullptr);
    g_gtao.denoise = Compute("gtao.hlsl", "GTAO_DENOISE");
    const auto vs = CompileCode("gtao.hlsl", "GTAO_APPLY", "vsmain", "vs_5_0");
    const auto ps = CompileCode("gtao.hlsl", "GTAO_APPLY", "main", "ps_5_0");
    dev->CreateVertexShader(vs->GetBufferPointer(), vs->GetBufferSize(), nullptr, &g_gtao.apply_vs);
    dev->CreatePixelShader(ps->GetBufferPointer(), ps->GetBufferSize(), nullptr, &g_gtao.apply_ps);
    const auto dps = CompileCode("gtao.hlsl", "GTAO_APPLY", "depthps", "ps_5_0");
    dev->CreatePixelShader(dps->GetBufferPointer(), dps->GetBufferSize(), nullptr, &g_gtao.depth_ps);
    const auto rps = CompileCode("gtao.hlsl", "GTAO_APPLY", "rawps", "ps_5_0");
    dev->CreatePixelShader(rps->GetBufferPointer(), rps->GetBufferSize(), nullptr, &g_gtao.raw_ps);
    D3D11_BLEND_DESC bd{};
    bd.RenderTarget[0].SrcBlend = bd.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ONE;
    bd.RenderTarget[0].DestBlend = bd.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_ZERO;
    bd.RenderTarget[0].BlendOp = bd.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;
    // the AO channel only (debug view 3: all channels)
    bd.RenderTarget[0].RenderTargetWriteMask =
        ConfigInt("AODebug", 0) == 3 ? D3D11_COLOR_WRITE_ENABLE_ALL : D3D11_COLOR_WRITE_ENABLE_RED;
    dev->CreateBlendState(&bd, &g_gtao.red_only);
    D3D11_RASTERIZER_DESC rd{};
    rd.FillMode = D3D11_FILL_SOLID;
    rd.CullMode = D3D11_CULL_NONE;
    rd.DepthClipEnable = TRUE;
    dev->CreateRasterizerState(&rd, &g_gtao.raster);
    D3D11_DEPTH_STENCIL_DESC dd{};
    dev->CreateDepthStencilState(&dd, &g_gtao.no_depth);
    D3D11_BUFFER_DESC cd{};
    cd.ByteWidth = (sizeof(GtaoCB) + 15) & ~15u;
    cd.Usage = D3D11_USAGE_DYNAMIC;
    cd.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
    cd.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    dev->CreateBuffer(&cd, nullptr, &g_gtao.cb);
    LOG("gfx: ambient occlusion: XeGTAO %s in place of the game's", g_gtao.level == 2 ? "ultra" : "high");
}

// Working textures of XeGTAO at the size of the game's AO pass
bool GtaoTargets(u32 w, u32 h) {
    if (g_gtao.w == w && g_gtao.h == h) return true;
    g_gtao.w = g_gtao.h = 0;
    D3D11_TEXTURE2D_DESC td{};
    td.Width = w, td.Height = h, td.MipLevels = XE_GTAO_DEPTH_MIP_LEVELS, td.ArraySize = 1;
    td.Format = DXGI_FORMAT_R32_FLOAT;
    td.SampleDesc.Count = 1;
    td.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS;
    g_gtao.depth_tex.Reset(), g_gtao.depth_srv.Reset();
    if (FAILED(g_post.dev->CreateTexture2D(&td, nullptr, &g_gtao.depth_tex)) ||
        FAILED(g_post.dev->CreateShaderResourceView(g_gtao.depth_tex.Get(), nullptr, &g_gtao.depth_srv)))
        return false;
    for (UINT mip = 0; mip < XE_GTAO_DEPTH_MIP_LEVELS; mip++) {
        D3D11_UNORDERED_ACCESS_VIEW_DESC ud{};
        ud.Format = DXGI_FORMAT_R32_FLOAT;
        ud.ViewDimension = D3D11_UAV_DIMENSION_TEXTURE2D;
        ud.Texture2D.MipSlice = mip;
        g_gtao.depth_uav[mip].Reset();
        if (FAILED(g_post.dev->CreateUnorderedAccessView(g_gtao.depth_tex.Get(), &ud, &g_gtao.depth_uav[mip])))
            return false;
    }
    // the final term is up to 1.5 x 255 before the apply pass clamps it (XE_GTAO_OCCLUSION_TERM_SCALE)
    if (!MakeTarget(g_gtao.term, w, h, DXGI_FORMAT_R8_UINT) || !MakeTarget(g_gtao.edges, w, h, DXGI_FORMAT_R8_UNORM) ||
        !MakeTarget(g_gtao.final_term, w, h, DXGI_FORMAT_R32_UINT))
        return false;
    g_gtao.w = w, g_gtao.h = h;
    return true;
}

void Dispatch(ID3D11ComputeShader* cs, std::initializer_list<ID3D11ShaderResourceView*> in,
              ID3D11UnorderedAccessView* out, UINT gx, UINT gy) {
    ID3D11ShaderResourceView* srv[3] = {};
    std::copy(in.begin(), in.end(), srv);
    g_post.ctx->CSSetShader(cs, nullptr, 0);
    g_post.ctx->CSSetShaderResources(0, 3, srv);
    g_post.ctx->CSSetUnorderedAccessViews(0, 1, &out, nullptr);
    g_post.ctx->Dispatch(gx, gy, 1);
    ID3D11ShaderResourceView* no_srv[3] = {};
    ID3D11UnorderedAccessView* no_uav = nullptr;
    g_post.ctx->CSSetShaderResources(0, 3, no_srv);
    g_post.ctx->CSSetUnorderedAccessViews(0, 1, &no_uav, nullptr);
}
} // namespace

void PostInit(ID3D11Device* dev, ID3D11DeviceContext* ctx) {
    g_post.dev = dev, g_post.ctx = ctx;
    // PostAA 3: no anti-aliasing at all. The game's FXAA pass must still draw (skipping it left
    // the scene black), so it draws the same copy as with SMAA and no SMAA follows.
    const int aa_setting = std::clamp(ConfigInt("PostAA", 0), 0, 3);
    g_post.aa = aa_setting == 3 ? 0 : aa_setting;
    g_post.sharpen = SharpenFromPercent(ConfigInt("Sharpen", 0));
    g_post.color[0] = Percent("Saturation", 100, 0, 200);
    g_post.color[1] = Percent("Contrast", 100, 50, 150);
    g_post.color[2] = Percent("Vibrance", 0, 0, 100);
    g_post.color[3] = Percent("Warmth", 0, -100, 100);
    g_post.color_on = g_post.color[0] != 1 || g_post.color[1] != 1 || g_post.color[2] != 0 || g_post.color[3] != 0;
    // CAS is always ready: Sharpness can be changed in the game menu while the game runs
    g_post.cas = Compile("PASS_CAS", 0);
    InitGtao(dev);
    if (g_post.aa) {
        g_post.edges = Compile("PASS_EDGES", g_post.aa);
        g_post.weights = Compile("PASS_WEIGHTS", g_post.aa);
        g_post.blend = Compile("PASS_BLEND", g_post.aa);
        g_post.area = Lookup(areaTexBytes, AREATEX_WIDTH, AREATEX_HEIGHT, AREATEX_PITCH, DXGI_FORMAT_R8G8_UNORM);
        g_post.search = Lookup(searchTexBytes, SEARCHTEX_WIDTH, SEARCHTEX_HEIGHT, SEARCHTEX_PITCH, DXGI_FORMAT_R8_UNORM);
    }
    if (aa_setting) {
        // the FXAA pass reads the scene (t0) at its own size and writes it at the same size
        static const char kCopy[] = "Texture2D scene : register(t0);\n"
                                    "float4 main(float4 pos : SV_Position) : SV_Target0 {\n"
                                    "    return scene.Load(int3(pos.xy, 0));\n"
                                    "}\n";
        ComPtr<ID3DBlob> code, err;
        if (FAILED(D3DCompile(kCopy, strlen(kCopy), "fxaa_copy", nullptr, nullptr, "main", "ps_5_0", 0, 0, &code,
                              &err)))
            Fatal("FXAA copy shader: %s", err ? (const char*)err->GetBufferPointer() : "?");
        dev->CreatePixelShader(code->GetBufferPointer(), code->GetBufferSize(), nullptr, &g_post.fxaa_copy);
    }
    if (g_post.color_on) g_post.color_cs = Compile("PASS_COLOR", 0);
    D3D11_BUFFER_DESC bd{};
    bd.ByteWidth = 48;
    bd.Usage = D3D11_USAGE_DYNAMIC;
    bd.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
    bd.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    dev->CreateBuffer(&bd, nullptr, &g_post.cb);
    D3D11_SAMPLER_DESC sd{};
    sd.Filter = D3D11_FILTER_MIN_MAG_LINEAR_MIP_POINT;
    sd.AddressU = sd.AddressV = sd.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
    sd.MaxLOD = D3D11_FLOAT32_MAX;
    dev->CreateSamplerState(&sd, &g_post.linear);
    sd.Filter = D3D11_FILTER_MIN_MAG_MIP_POINT;
    dev->CreateSamplerState(&sd, &g_post.point);
    LOG("gfx: scene post-processing: game FXAA %s, SMAA %s, colour %s (saturation %.2f contrast %.2f vibrance %.2f warmth %.2f), "
        "CAS %s %.2f",
        aa_setting ? "replaced by a copy" : "on", g_post.aa == 2 ? "ultra" : g_post.aa ? "high" : "off", g_post.color_on ? "on" : "off", g_post.color[0],
        g_post.color[1], g_post.color[2], g_post.color[3], g_post.sharpen >= 0 ? "on" : "off",
        std::max(g_post.sharpen, 0.0f));
}

ID3D11PixelShader* PostReplacement(const std::string& name) {
    if (!g_post.fxaa_copy || name.rfind("Fxaa_", 0) != 0) return nullptr;
    if (++g_post.fxaa_replaced <= 1 || g_post.fxaa_replaced % 3600 == 0)
        LOG("gfx: %s replaced by a copy (%s), %llu times", name.c_str(), g_post.aa ? "SMAA" : "anti-aliasing off",
            (unsigned long long)g_post.fxaa_replaced);
    return g_post.fxaa_copy.Get();
}

bool PostActive() {
    return g_post.aa || g_post.color_on || (g_post.cas && Sharpen() >= 0);
}

ID3D11ShaderResourceView* PostRun(ID3D11ShaderResourceView* scene, u32 w, u32 h, bool sharpen) {
    const float sharpness = Sharpen();
    const bool cas = sharpen && g_post.cas && sharpness >= 0;
    if (!g_post.aa && !g_post.color_on && !cas) return scene;
    if (g_post.w != w || g_post.h != h) {
        if (!MakeTarget(g_post.out[0], w, h, DXGI_FORMAT_R16G16B16A16_FLOAT) ||
            !MakeTarget(g_post.out[1], w, h, DXGI_FORMAT_R16G16B16A16_FLOAT) ||
            (g_post.aa && (!MakeTarget(g_post.edge_tex, w, h, DXGI_FORMAT_R8G8_UNORM) ||
                           !MakeTarget(g_post.blend_tex, w, h, DXGI_FORMAT_R8G8B8A8_UNORM)))) {
            LOG("gfx: scene post-processing targets %ux%u could not be created", w, h);
            g_post.w = g_post.h = 0;
            return scene;
        }
        g_post.w = w, g_post.h = h;
    }
    D3D11_MAPPED_SUBRESOURCE m;
    if (FAILED(g_post.ctx->Map(g_post.cb.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &m))) return scene;
    const float c[12] = {1.0f / w, 1.0f / h, (float)w, (float)h, g_post.color[0], g_post.color[1],
                         g_post.color[2], g_post.color[3], std::max(sharpness, 0.0f), 0, 0, 0};
    memcpy(m.pData, c, sizeof(c));
    g_post.ctx->Unmap(g_post.cb.Get(), 0);
    g_post.ctx->CSSetConstantBuffers(0, 1, g_post.cb.GetAddressOf());
    ID3D11SamplerState* samplers[2] = {g_post.linear.Get(), g_post.point.Get()};
    g_post.ctx->CSSetSamplers(0, 2, samplers);
    const UINT gx = (w + 7) / 8, gy = (h + 7) / 8;
    ID3D11ShaderResourceView* current = scene;
    int next = 0;
    auto output = [&](ID3D11ComputeShader* cs, std::initializer_list<ID3D11ShaderResourceView*> in, bool tiles16) {
        Dispatch(cs, in, g_post.out[next].uav.Get(), tiles16 ? (w + 15) / 16 : gx, tiles16 ? (h + 15) / 16 : gy);
        current = g_post.out[next].srv.Get();
        next ^= 1;
    };
    if (g_post.aa) {
        Dispatch(g_post.edges.Get(), {current}, g_post.edge_tex.uav.Get(), gx, gy);
        Dispatch(g_post.weights.Get(), {g_post.edge_tex.srv.Get(), g_post.area.Get(), g_post.search.Get()},
                 g_post.blend_tex.uav.Get(), gx, gy);
        output(g_post.blend.Get(), {current, g_post.blend_tex.srv.Get()}, false);
    }
    if (g_post.color_on) output(g_post.color_cs.Get(), {current}, false);
    if (cas) output(g_post.cas.Get(), {current}, true);
    g_post.ctx->CSSetShader(nullptr, nullptr, 0);
    if (++g_post.frames <= 3 || g_post.frames % 600 == 0)
        LOG("CTR_POST_DISPATCH frame=%llu size=%ux%u smaa=%d colour=%d cas=%d before_hud=1",
            (unsigned long long)g_post.frames, w, h, g_post.aa, int(g_post.color_on), int(cas));
    return current;
}

bool PostAmbientOcclusionFor(const std::string& name) {
    return g_gtao.level && name.rfind("AmbientOcclusion_", 0) == 0;
}

void PostAmbientOcclusion(ID3D11RenderTargetView* target, ID3D11ShaderResourceView* depth, const float* cb,
                          u32 floats) {
    // the game's AO pass constants (retail 1.21): dword 16 the occlusion radius in world units,
    // 24 the far plane, 28..29 the pass size, 36..39 the viewspace ray of a pixel (x = px * [36] + [38],
    // y = py * [37] + [39] at viewspace depth 1)
    ComPtr<ID3D11Resource> depth_res, target_res;
    if (!target || !depth || !cb || floats < 40) return;
    depth->GetResource(&depth_res);
    target->GetResource(&target_res);
    D3D11_TEXTURE2D_DESC dd{}, td{};
    static_cast<ID3D11Texture2D*>(depth_res.Get())->GetDesc(&dd);
    static_cast<ID3D11Texture2D*>(target_res.Get())->GetDesc(&td);
    D3D11_SHADER_RESOURCE_VIEW_DESC view{};
    depth->GetDesc(&view); // the texture may be typeless: its view says how it is read
    const float pass_w = cb[28], pass_h = cb[29];
    const float tan_x = cb[36] * pass_w * 0.5f, tan_y = -cb[37] * pass_h * 0.5f, radius = cb[16],
                far_plane = cb[24];
    // the depth is the full-resolution scene, the AO target the game's half-resolution pass
    const u32 ratio = td.Width ? dd.Width / td.Width : 0;
    if ((ratio != 1 && ratio != 2) || dd.Width != td.Width * ratio || dd.Height != td.Height * ratio ||
        view.Format != DXGI_FORMAT_R32_FLOAT ||
        !(tan_x > 0.05f && tan_x < 20) || !(tan_y > 0.05f && tan_y < 20) || !(radius > 0 && radius < 1e5f)
        || !(far_plane > radius && far_plane < 1e8f)) {
        if (++g_gtao.skipped <= 3)
            LOG("gfx: XeGTAO skipped: depth %ux%u format %d, target %ux%u, tan %.3f %.3f, radius %.1f", dd.Width,
                dd.Height, (int)view.Format, td.Width, td.Height, tan_x, tan_y, radius);
        return;
    }
    const u32 w = dd.Width, h = dd.Height;
    if (!GtaoTargets(w, h)) {
        if (++g_gtao.skipped <= 3) LOG("gfx: XeGTAO targets %ux%u could not be created", w, h);
        return;
    }
    GtaoCB c{};
    auto& k = c.consts;
    k.ViewportSize = {(int)w, (int)h};
    k.ViewportPixelSize = {1.0f / w, 1.0f / h};
    // the game's linear depth is viewspace z in world units, negative in front of the camera
    k.DepthUnpackConsts = {-1.0f, 0};
    k.CameraTanHalfFOV = {tan_x, tan_y};
    k.NDCToViewMul = {tan_x * 2.0f, tan_y * -2.0f};
    k.NDCToViewAdd = {-tan_x, tan_y};
    k.NDCToViewMul_x_PixelSize = {k.NDCToViewMul.x / w, k.NDCToViewMul.y / h};
    k.EffectRadius = radius;
    k.EffectFalloffRange = XE_GTAO_DEFAULT_FALLOFF_RANGE;
    k.RadiusMultiplier = XE_GTAO_DEFAULT_RADIUS_MULTIPLIER;
    k.FinalValuePower = XE_GTAO_DEFAULT_FINAL_VALUE_POWER;
    k.DenoiseBlurBeta = 1.2f;
    k.SampleDistributionPower = XE_GTAO_DEFAULT_SAMPLE_DISTRIBUTION_POWER;
    k.ThinOccluderCompensation = XE_GTAO_DEFAULT_THIN_OCCLUDER_COMPENSATION;
    k.DepthMIPSamplingOffset = XE_GTAO_DEFAULT_DEPTH_MIP_SAMPLING_OFFSET;
    k.NoiseIndex = 0;
    c.apply[0] = (float)ratio;
    c.apply[1] = far_plane * 0.9f;
    D3D11_MAPPED_SUBRESOURCE m;
    if (FAILED(g_post.ctx->Map(g_gtao.cb.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &m))) return;
    memcpy(m.pData, &c, sizeof(c));
    g_post.ctx->Unmap(g_gtao.cb.Get(), 0);

    auto* ctx = g_post.ctx;
    ID3D11ShaderResourceView* no_srv[2] = {};
    ID3D11UnorderedAccessView* no_uav[XE_GTAO_DEPTH_MIP_LEVELS] = {};
    ctx->CSSetConstantBuffers(0, 1, g_gtao.cb.GetAddressOf());
    ctx->CSSetSamplers(0, 1, g_post.point.GetAddressOf());
    // the game's depth into the viewspace depth MIP chain
    ID3D11UnorderedAccessView* mips[XE_GTAO_DEPTH_MIP_LEVELS];
    for (int i = 0; i < XE_GTAO_DEPTH_MIP_LEVELS; i++) mips[i] = g_gtao.depth_uav[i].Get();
    ctx->CSSetShader(g_gtao.prefilter.Get(), nullptr, 0);
    ctx->CSSetShaderResources(0, 1, &depth);
    ctx->CSSetUnorderedAccessViews(0, XE_GTAO_DEPTH_MIP_LEVELS, mips, nullptr);
    ctx->Dispatch((w + 15) / 16, (h + 15) / 16, 1);
    ctx->CSSetUnorderedAccessViews(0, XE_GTAO_DEPTH_MIP_LEVELS, no_uav, nullptr);
    // occlusion and depth edges
    ID3D11UnorderedAccessView* main_out[2] = {g_gtao.term.uav.Get(), g_gtao.edges.uav.Get()};
    ctx->CSSetShader(g_gtao.main.Get(), nullptr, 0);
    ctx->CSSetShaderResources(0, 1, g_gtao.depth_srv.GetAddressOf());
    ctx->CSSetUnorderedAccessViews(0, 2, main_out, nullptr);
    ctx->Dispatch((w + XE_GTAO_NUMTHREADS_X - 1) / XE_GTAO_NUMTHREADS_X,
                  (h + XE_GTAO_NUMTHREADS_Y - 1) / XE_GTAO_NUMTHREADS_Y, 1);
    ctx->CSSetUnorderedAccessViews(0, 2, no_uav, nullptr);
    // one edge-aware denoise pass (Intel's "sharp" setting)
    ID3D11ShaderResourceView* denoise_in[2] = {g_gtao.term.srv.Get(), g_gtao.edges.srv.Get()};
    ctx->CSSetShader(g_gtao.denoise.Get(), nullptr, 0);
    ctx->CSSetShaderResources(0, 2, denoise_in);
    ctx->CSSetUnorderedAccessViews(0, 1, g_gtao.final_term.uav.GetAddressOf(), nullptr);
    ctx->Dispatch((w + XE_GTAO_NUMTHREADS_X * 2 - 1) / (XE_GTAO_NUMTHREADS_X * 2),
                  (h + XE_GTAO_NUMTHREADS_Y - 1) / XE_GTAO_NUMTHREADS_Y, 1);
    ctx->CSSetUnorderedAccessViews(0, 1, no_uav, nullptr);
    ctx->CSSetShaderResources(0, 2, no_srv);
    ctx->CSSetShader(nullptr, nullptr, 0);
    // into the AO channel of the game's target; its blur and lighting read it from there
    const D3D11_VIEWPORT vp{0, 0, (float)td.Width, (float)td.Height, 0, 1};
    ctx->OMSetRenderTargets(1, &target, nullptr);
    ctx->OMSetBlendState(g_gtao.red_only.Get(), nullptr, 0xFFFFFFFF);
    ctx->OMSetDepthStencilState(g_gtao.no_depth.Get(), 0);
    ctx->RSSetState(g_gtao.raster.Get());
    ctx->RSSetViewports(1, &vp);
    ctx->IASetInputLayout(nullptr);
    ctx->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    ctx->VSSetShader(g_gtao.apply_vs.Get(), nullptr, 0);
    ctx->GSSetShader(nullptr, nullptr, 0);
    // debugging aid CTR_D3D_AODebug: 1 the term before denoising, 2 the viewspace depth, 3 the game's depth
    static const int debug = ConfigInt("AODebug", 0);
    ctx->PSSetShader((debug == 3 ? g_gtao.raw_ps : debug == 2 ? g_gtao.depth_ps : g_gtao.apply_ps).Get(), nullptr, 0);
    ctx->PSSetShaderResources(1, 1, debug == 2 ? g_gtao.depth_srv.GetAddressOf() : &depth);
    ctx->PSSetConstantBuffers(0, 1, g_gtao.cb.GetAddressOf());
    ctx->PSSetShaderResources(0, 1, (debug == 1 ? g_gtao.term : g_gtao.final_term).srv.GetAddressOf());
    ctx->Draw(3, 0);
    ctx->PSSetShaderResources(0, 2, no_srv);
    if (++g_gtao.frames <= 3 || g_gtao.frames % 600 == 0)
        LOG("CTR_GTAO_DISPATCH frame=%llu size=%ux%u tan=%.3f,%.3f radius=%.1f far=%.0f level=%d",
            (unsigned long long)g_gtao.frames, w, h, tan_x, tan_y, radius, far_plane, g_gtao.level);
}
