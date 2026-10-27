// SPDX-License-Identifier: GPL-2.0-only
#pragma once
#include "session_api.h"
// Bounded adapter for observed retail Open(user255,bus0,index0,paramnull).
// Win64 owner-thread entry; runtime owns guest handles and service dispatch.
// Body starts only after real Win32/Vulkan session initialization. Runtime may
// signal Open completion there, then keep its owner service loop inside body.
// Returning destroys resources; borrowed API/context MUST NOT outlive body.
static inline int ctr_video_open_main_session_v1(int32_t user_id, int32_t bus, int32_t index, uint64_t parameter,
                                                 const CtrVideoWindowOptionsV1* options, CtrVideoSessionBodyV1 body,
                                                 void* context) {
    if (!body || !options) return 60;
    // Other cases remain explicitly unsupported host domain, not PS4 errno.
    if (user_id != 255 || bus != 0 || index != 0 || parameter != 0) return 88;
    return ctr_video_run_window_session_v1(options, body, context);
}
