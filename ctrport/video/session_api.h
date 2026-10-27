// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include <stdint.h>
#include <stddef.h>
#include "../display_event_api.h"
#ifdef __cplusplus
extern "C" {
#endif
// Host Windows C ABI, not PS4 SysV. All calls on the session owner thread.
// Borrowed context/table expire before session destruction or DLL unload.
// Source is readable host memory, copied synchronously, never a guest pointer.
typedef struct CtrVideoLinearFrameV1 {
    uint32_t size, format, width, height, pitch_pixels;
    const void* bytes;
    size_t byte_length;
} CtrVideoLinearFrameV1;
// Public40B layout adapted from shadPS4 videoout/buffer.h, GPL-2.0-or-later,
// revision e3ce810f3a653f43ac64ebab63023de281a4103a. Captured values only.
typedef struct CtrVideoBufferAttributesV1 {
    uint32_t format;
    int32_t tiling, aspect;
    uint32_t width, height, pitch_pixels, option, reserved0;
    uint64_t reserved1;
} CtrVideoBufferAttributesV1;
typedef struct CtrVideoHostBufferV1 {
    const void* bytes;
    size_t byte_length, pitch_bytes;
} CtrVideoHostBufferV1;

typedef struct CtrVideoCompletionV1 {
    uint64_t count, token, argument;
} CtrVideoCompletionV1;
// Host counters only, not the PS4 FlipStatus layout or timestamps.
typedef struct CtrVideoVblankV1 {
    uint64_t count, process_microseconds, native_tsc;
} CtrVideoVblankV1;
typedef struct CtrVideoProgressV1 {
    uint64_t completed, token, argument, pending;
    int64_t current_slot;
    uint64_t submit_tsc, completion_microseconds, completion_tsc;
    uint32_t timing_valid, reserved;
} CtrVideoProgressV1;
typedef struct CtrVideoSessionApiV1 {
    uint32_t size, version;
    void* context;
    int (*register_frame)(void*, uint32_t, const void*, size_t, size_t, uint64_t*);
    int (*unregister_frame)(void*, uint32_t, uint64_t);
    int (*enqueue)(void*, uint32_t, uint64_t, uint64_t);
    int (*start_pacing)(void*);
    int (*stop_pacing)(void*);
    int (*present_next)(void*, uint32_t, CtrVideoCompletionV1*);
    int (*refresh_frame)(void*, uint32_t, uint64_t, const void*, size_t, size_t);
    int (*submit_linear_frame)(void*, uint32_t slot, uint64_t token, const CtrVideoLinearFrameV1*, uint64_t argument);
    int (*refresh_linear_frame)(void*, uint32_t slot, uint64_t token, const CtrVideoLinearFrameV1*);
    int (*register_linear_frame)(void*, uint32_t, const CtrVideoLinearFrameV1*, uint64_t*);
    // Kernel clock context/code borrowed until SESSION DESTRUCTION, even after
    // detach/remove. Stop pacing before combined detach; destroy before DLL unload.
    int (*attach_events)(void*, const CtrDisplayEventApi*, uint64_t queue, uint64_t flip_data, uint64_t vblank_data);
    int (*detach_events)(void*);
    int (*add_event)(void*, const CtrDisplayEventApi*, uint64_t queue, uint64_t ident, uint64_t userdata);
    int (*remove_event)(void*, uint64_t queue, uint64_t ident);
    int (*get_public_flip)(void*, void* host_output, size_t bytes);
    int (*get_public_vblank)(void*, void* host_output, size_t bytes);
    int (*get_vblank)(void*, CtrVideoVblankV1*);
    int (*get_progress)(void*, CtrVideoProgressV1*);
    // Packed host bytes in swapchain format; group index is not a frame token.
    int (*register_group)(void*, uint32_t first, uint32_t count, const CtrVideoHostBufferV1*, uint32_t* group,
                          uint64_t* tokens, size_t token_capacity);
    int (*register_linear_group)(void*, uint32_t first, uint32_t count, const CtrVideoBufferAttributesV1*,
                                 const CtrVideoHostBufferV1*, uint32_t* group, uint64_t* tokens, size_t token_capacity);
    int (*unregister_group)(void*, uint32_t group);
    // Bounded host wait for next clock tick, independent of frame queue.
    // timeout <=5000ms; 52 stopped/timeout, 91 callback failure.
    int (*wait_vblank)(void*, uint32_t timeout_ms);
    int (*get_public_capability)(void*, uint64_t*); // actual SDR swapchain only; HDR missing88
    int (*set_flip_rate)(void*, int32_t);           // supported0..2; owner-thread cadence, others88
    int (*pump_messages)(void*);                    // 80: window close requested; return from body to clean up.
} CtrVideoSessionApiV1;
typedef int (*CtrVideoSessionBodyV1)(const CtrVideoSessionApiV1*, uint32_t width, uint32_t height,
                                     uint32_t host_vk_format, void* user);
// Creates hidden Win32/Vulkan resources, calls body exactly once on this owner thread,
// then stops clock and destroys resources. The body may run its service loop here.
// Table/context must not escape body. Nonzero callback result propagates unchanged.
int ctr_video_run_session_v1(CtrVideoSessionBodyV1 body, void* user);
typedef struct CtrVideoWindowOptionsV1 {
    uint32_t size, version, width, height, visible, reserved;
} CtrVideoWindowOptionsV1;
int ctr_video_run_window_session_v1(const CtrVideoWindowOptionsV1*, CtrVideoSessionBodyV1, void* user);
// Only a live, initialized internal PresentationSession may be supplied.
int ctr_video_session_api_v1(void* borrowed_session, CtrVideoSessionApiV1* output);
#ifdef __cplusplus
}
#endif
