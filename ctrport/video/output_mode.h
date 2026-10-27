// SPDX-License-Identifier: GPL-2.0-only
#pragma once
#include <stdint.h>
// Public layout: pinned shadPS4 video_out.cpp Mode, GPL-2.0-or-later.
// Host snapshot, never a borrowed guest pointer. Synchronous live owner call.
typedef struct CtrVideoOutputModeV1 {
    uint32_t size;
    uint8_t encoding, range, colorimetry, depth;
    uint64_t refresh_rate, resolution;
    uint8_t reserved[8];
} CtrVideoOutputModeV1;
#ifdef __cplusplus
extern "C" {
#endif
// Supported exact retail Any request: reserved0, mode32, null options/size16.
// Validates actual active SDR swapchain. No HDR, resize or refresh invention.
// 60 malformed, 88 unsupported, 91 session failure. Runtime maps public errors.
int ctr_video_configure_output_any_v1(void* session, uint32_t reserved, const CtrVideoOutputModeV1* mode,
                                      const void* options, uint32_t size_mode, uint32_t size_options);
#ifdef __cplusplus
}
static_assert(sizeof(CtrVideoOutputModeV1) == 32, "public mode32");
#endif
