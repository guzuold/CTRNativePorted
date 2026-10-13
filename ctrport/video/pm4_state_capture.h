// SPDX-License-Identifier: GPL-2.0-only
#pragma once
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
// Synchronous owner-thread copy into HOST storage. Exact flat0xD000/compute80
// DWORD capacities, separate nonoverlapping output arrays. Not guest addresses.
// Returns91 before first completed init or after poisoned state;116wrongthread.
int ctr_video_capture_pm4_state_v1(void* session,uint32_t* flat,uint32_t flat_words,
    uint32_t* compute,uint32_t compute_words,uint64_t* completed_init);
#ifdef __cplusplus
}
#endif
