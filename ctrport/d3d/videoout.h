// SPDX-License-Identifier: GPL-2.0-or-later
// libSceVideoOut: the game window, flip bookkeeping, 60 Hz vblank and frame pacing.
#pragma once
#include "common.h"

struct HWND__;

// Display mode of the window (PC settings: CTR_SETTINGS_WINDOW_MODE / _DISPLAY_REFRESH, the
// launcher's CTR_GPU_FULLSCREEN for "original"). Exclusive: the swap chain takes the output.
struct DisplayConfig {
    enum Mode { Windowed, Borderless, Exclusive } mode = Windowed;
    int width = 1280, height = 720;  // window client size; exclusive: the display mode
    float refresh = 0;               // exclusive only: requested Hz, 0 = the desktop's
};
// Creates the window (ctr_gpu_init); the renderer's swap chain is bound to it.
void VideoOutStartWindow(const char* title, int width, int height);
const DisplayConfig& VideoOutDisplay();
// The game's display gamma (sceVideoOutAdjustColor_), 1 until it sets one.
float VideoOutGamma();
// A flip of the registered buffer `index` (SubmitFlip, Gnm submit-and-flip, flip requests).
void VideoOutFlip(int handle, int index, s64 arg);
bool VideoOutHasFocus();
u64 VideoOutFlipCount();
HWND__* VideoOutHwndRaw();
#define VideoOutHwnd() ((HWND)VideoOutHwndRaw())
// F12 recording (renderer.cpp)
void GfxRecordFrames(int frames);
