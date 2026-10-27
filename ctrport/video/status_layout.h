// SPDX-FileCopyrightText: Copyright 2024-2026 shadPS4 Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later
// Fixed-width layouts adapted from pinned public videoout/video_out.h.
// Layout definitions only; no guest pointer access or service implementation.
#pragma once
#include <stdint.h>
#include <stddef.h>
typedef struct CtrVideoPublicFlipStatus {
    uint64_t count, process_time, tsc;
    int64_t flip_arg;
    uint64_t submit_tsc, reserved0;
    int32_t gc_queue_num, flip_pending_num, current_buffer;
    uint32_t reserved1;
} CtrVideoPublicFlipStatus;
typedef struct CtrVideoPublicVblankStatus {
    uint64_t count, process_time, tsc, reserved;
    uint8_t flags, padding[7];
} CtrVideoPublicVblankStatus;
// Public resolution layout; values require explicit runtime display configuration.
typedef struct CtrVideoPublicResolutionStatus {
    int32_t full_width, full_height, pane_width, pane_height;
    uint64_t refresh_rate;
    float screen_size_in_inch;
    uint16_t flags, reserved0;
    uint32_t reserved1[3];
    uint32_t padding;
} CtrVideoPublicResolutionStatus;
#ifdef __cplusplus
#define CTR_VIDEO_LAYOUT_ASSERT static_assert
#else
#define CTR_VIDEO_LAYOUT_ASSERT _Static_assert
#endif
CTR_VIDEO_LAYOUT_ASSERT(sizeof(CtrVideoPublicFlipStatus) == 64, "flip size");
CTR_VIDEO_LAYOUT_ASSERT(offsetof(CtrVideoPublicFlipStatus, flip_arg) == 24, "flip arg offset");
CTR_VIDEO_LAYOUT_ASSERT(offsetof(CtrVideoPublicFlipStatus, submit_tsc) == 32, "submit offset");
CTR_VIDEO_LAYOUT_ASSERT(offsetof(CtrVideoPublicFlipStatus, gc_queue_num) == 48, "gc offset");
CTR_VIDEO_LAYOUT_ASSERT(offsetof(CtrVideoPublicFlipStatus, flip_pending_num) == 52, "pending offset");
CTR_VIDEO_LAYOUT_ASSERT(offsetof(CtrVideoPublicFlipStatus, current_buffer) == 56, "buffer offset");
CTR_VIDEO_LAYOUT_ASSERT(offsetof(CtrVideoPublicFlipStatus, reserved1) == 60, "reserved offset");
CTR_VIDEO_LAYOUT_ASSERT(sizeof(CtrVideoPublicVblankStatus) == 40, "vblank size");
CTR_VIDEO_LAYOUT_ASSERT(offsetof(CtrVideoPublicVblankStatus, process_time) == 8, "process offset");
CTR_VIDEO_LAYOUT_ASSERT(offsetof(CtrVideoPublicVblankStatus, tsc) == 16, "tsc offset");
CTR_VIDEO_LAYOUT_ASSERT(offsetof(CtrVideoPublicVblankStatus, reserved) == 24, "vblank reserved");
CTR_VIDEO_LAYOUT_ASSERT(offsetof(CtrVideoPublicVblankStatus, flags) == 32, "flags offset");
CTR_VIDEO_LAYOUT_ASSERT(offsetof(CtrVideoPublicVblankStatus, padding) == 33, "padding offset");
CTR_VIDEO_LAYOUT_ASSERT(sizeof(CtrVideoPublicResolutionStatus) == 48, "resolution size");
CTR_VIDEO_LAYOUT_ASSERT(offsetof(CtrVideoPublicResolutionStatus, refresh_rate) == 16, "refresh offset");
CTR_VIDEO_LAYOUT_ASSERT(offsetof(CtrVideoPublicResolutionStatus, screen_size_in_inch) == 24, "inches offset");
CTR_VIDEO_LAYOUT_ASSERT(offsetof(CtrVideoPublicResolutionStatus, flags) == 28, "resolution flags");
CTR_VIDEO_LAYOUT_ASSERT(offsetof(CtrVideoPublicResolutionStatus, reserved1) == 32, "resolution reserved");
CTR_VIDEO_LAYOUT_ASSERT(offsetof(CtrVideoPublicResolutionStatus, padding) == 44, "resolution padding");
#undef CTR_VIDEO_LAYOUT_ASSERT
