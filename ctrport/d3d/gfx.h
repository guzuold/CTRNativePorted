// SPDX-License-Identifier: MIT
// From FrankHUN88453/CTR-NitroFueled-PC 96b4aa5474b1d42b0d9f383349dc091ea9a2d385 port/src/gfx/gfx.h
// (MIT, ctrport/d3d/LICENSE.FrankHUN88453); hook installer returns whether both hooks were placed;
// ctrport additions: offset draws, dispatch report, counters.
#pragma once
// Native graphics backend (D3D11) for the engine's render layer.
//
// The engine's PS4 device context (igPS4GraphicsDeviceContext) keeps the complete render
// state as engine objects; at every draw this backend reads that state and issues the
// equivalent D3D11 draw with the ahead-of-time translated shaders (tools/gcn2hlsl.py).
#include "common.h"

// Called from the GNM draw entry points while the engine records a draw.
void GfxProbeDraw(const char* kind, u32 count, u64 index_addr);
void GfxDraw(u32 count, u64 index_addr, bool indexed);
// Presents the surface at `addr` (a registered VideoOut buffer). false = not rendered yet.
bool GfxPresent(u64 addr);
bool GfxActive();
// The translated shaders are there: embedded, or shaders.bin in the game folder (release builds).
bool GfxShadersAvailable();
// A video (Bink) was drawn within the last few frames.
bool GfxVideoPlaying();
// Where the game picture is in the window, as fractions of the client area: x0, y0, x1, y1
// (the 16:9 picture is letterboxed).
void GfxPictureRect(float r[4]);
// Installs hooks into engine functions (clear). Call after linking.
bool GfxInstallHooks();
// Saves the next frames at 1280x720 into %APPDATA%/CTRNF/capture/ (F12).
void GfxRecordFrames(int frames);

// ctrport additions
// Indexed draw from the context's index buffer, `index_offset` indices past its bound offset.
void GfxDrawIndexOffset(u32 index_offset, u32 count);
// A compute dispatch: program address, CS stage registers (COMPUTE_PGM_LO / HI, RSRC1 / 2,
// NUM_THREAD_X / Y / Z), group counts. Reported and saved for translation, not run yet.
void GfxDispatch(u64 program, const u32* regs, u32 x, u32 y, u32 z, u64 caller);
u64 GfxDrawCount();
u64 GnmSubmitCount();
