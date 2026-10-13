// SPDX-License-Identifier: GPL-2.0-only
#pragma once
#include "../guest_copy_api.h"
#ifdef __cplusplus
extern "C" {
#endif
// Win64 checked adapter, not PS4 SysV. neo_mode must come from runtime policy.
// Caller serializes the entire read/modify/write operation against other writers.
// Return host status; on success result is public DWORD count (0 or256).
int ctr_gnm_draw_init_350(const CtrGuestCopyApi* memory,uint64_t address,
                          uint32_t capacity_dwords,int neo_mode,uint32_t* result);
#ifdef __cplusplus
}
#endif
