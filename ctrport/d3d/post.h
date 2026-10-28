// SPDX-License-Identifier: GPL-2.0-or-later
// Scene post-processing of the Direct3D 11 renderer (post.cpp): SMAA anti-aliasing, colour
// adjustment and CAS sharpening on the finished 3D scene, before the HUD is drawn over it.
#pragma once
#include <d3d11.h>

#include <string>

#include "common.h"

// Reads the settings (CTR_D3D_PostAA, CTR_D3D_Sharpen, CTR_D3D_Saturation, CTR_D3D_Contrast,
// CTR_D3D_Vibrance, CTR_D3D_Warmth) and compiles the passes they need.
void PostInit(ID3D11Device* dev, ID3D11DeviceContext* ctx);
// True when any pass is selected.
bool PostActive();
// Runs the selected passes on the scene (w x h physical pixels) and returns the result, which
// has the same size; `sharpen` false skips CAS (FSR 1 RCAS sharpens an upscaled scene instead).
// Returns `scene` itself when nothing ran. Uses compute only: the caller's graphics state stays.
ID3D11ShaderResourceView* PostRun(ID3D11ShaderResourceView* scene, u32 w, u32 h, bool sharpen);
// The pixel shader to draw in place of the game's shader `name`, or null to keep it: with SMAA
// selected, the game's FXAA pass copies its input unchanged, so SMAA works on unblurred edges.
ID3D11PixelShader* PostReplacement(const std::string& name);

// XeGTAO ambient occlusion (CTR_D3D_AO 1 high, 2 ultra) in place of the game's: true when selected
// and `name` is the game's AO pixel shader.
bool PostAmbientOcclusionFor(const std::string& name);
// Right after the game's AO draw: computes XeGTAO from the game's full-resolution linear depth
// (`depth`, its LinearizeDepth target) with the camera of the AO pass's constants (`cb`, `floats` of
// them) and writes it, averaged down to the AO pass size, into the AO channel (red) of `target`.
// Changes graphics state; the next game draw sets its own.
void PostAmbientOcclusion(ID3D11RenderTargetView* target, ID3D11ShaderResourceView* depth, const float* cb,
                          u32 floats);
