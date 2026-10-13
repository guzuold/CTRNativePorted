// SPDX-License-Identifier: GPL-2.0-only
#pragma once
#ifdef __cplusplus
extern "C" {
#endif
// Owner-thread synchronous transition to the live game presentation window.
// Runtime has no separate console splash. Verify/reveal actual HWND; no frame
// or GPU completion credit. Null60, wrong thread116, closed/invalid91.
int ctr_video_hide_system_splash_v1(void* session);
#ifdef __cplusplus
}
#endif
