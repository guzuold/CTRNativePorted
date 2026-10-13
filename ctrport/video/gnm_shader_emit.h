// SPDX-License-Identifier: GPL-2.0-only
#pragma once
#include "../guest_copy_api.h"
#ifdef __cplusplus
extern "C" {
#endif
// Whole public emitter; no shader/GPU execution implied. Synchronous snapshot/RMW.
// Caller serializes source/destination through entire transaction. Hoststatus0
// means public_result contains original0/-1. Copy failure112 preserves result.
int ctr_gnm_set_vs_shader(const CtrGuestCopyApi* memory,uint64_t command_address,
    uint32_t capacity_dwords,uint64_t registers_address,uint32_t modifier,int32_t* public_result);
int ctr_gnm_set_ps_shader_350(const CtrGuestCopyApi* memory,uint64_t command_address,
    uint32_t capacity_dwords,uint64_t registers_address,int32_t* public_result);
int ctr_gnm_update_vs_shader(const CtrGuestCopyApi* memory,uint64_t command_address,
    uint32_t capacity_dwords,uint64_t registers_address,uint32_t modifier,int32_t* public_result);
int ctr_gnm_update_ps_shader_350(const CtrGuestCopyApi* memory,uint64_t command_address,
    uint32_t capacity_dwords,uint64_t registers_address,int32_t* public_result);
int ctr_gnm_set_vgt_control(const CtrGuestCopyApi* memory,uint64_t command_address,
    uint32_t size_dwords,uint32_t group_minus_one,uint32_t partial_wave,uint32_t switch_eop,
    int neo_mode,int neo_compat,int32_t* public_result);
int ctr_gnm_set_cs_shader_modifier(const CtrGuestCopyApi* memory,uint64_t command_address,
    uint32_t capacity_dwords,uint64_t registers_address,uint32_t modifier,int32_t* public_result);
int ctr_gnm_dispatch_direct(const CtrGuestCopyApi* memory,uint64_t command_address,
    uint32_t size_dwords,uint32_t x,uint32_t y,uint32_t z,uint32_t flags,int32_t* public_result);
// Marker follows pinned public write extent: size_dwords*4 including zero padding.
// Caller retains source/destination authority for entire call; alias domain rejected88.
int ctr_gnm_insert_push_marker(const CtrGuestCopyApi* memory,uint64_t command_address,
    uint32_t size_dwords,uint64_t marker_address,int32_t* public_result);
int ctr_gnm_insert_set_marker(const CtrGuestCopyApi* memory,uint64_t command_address,
    uint32_t size_dwords,uint64_t marker_address,int32_t* public_result);
int ctr_gnm_insert_pop_marker(const CtrGuestCopyApi* memory,uint64_t command_address,
    uint32_t size_dwords,int32_t* public_result);
int ctr_gnm_draw_index(const CtrGuestCopyApi* memory,uint64_t command_address,
    uint32_t size_dwords,uint32_t index_count,uint64_t index_address,uint32_t flags,
    uint32_t type,int neo_mode,int32_t* public_result);
// Whole public DrawIndexAuto emitter only; neo_mode must be configured0/1.
int ctr_gnm_draw_index_auto(const CtrGuestCopyApi* memory,uint64_t command_address,
    uint32_t size_dwords,uint32_t index_count,uint32_t flags,int neo_mode,int32_t* public_result);
#ifdef __cplusplus
}
#endif
