// SPDX-License-Identifier: GPL-2.0-only
#pragma once
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
// Live owner only. Proves range has NO retained guest-backed renderer resource.
// Does not invalidate or flush overlapping resources: returns88 for overlap.
// Caller must keep owner serialization through actual mapping/type mutation.
// 60 zero/overflow/malformed,91 failed session. No GPU/CPU coherence asserted.
int ctr_video_admit_memory_change_v1(void* session, uint64_t address, uint64_t bytes);
#ifdef __cplusplus
}
#endif
