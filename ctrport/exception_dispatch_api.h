// SPDX-License-Identifier: GPL-2.0-only
#pragma once
#include <stdint.h>
// Windows host ABI. exception is EXCEPTION_POINTERS*, borrowed during callback.
// Callback returns1 only after actually handling that exact fault;0 continues search.
typedef int (*CtrExceptionCallback)(void* user,void* exception);
typedef struct CtrExceptionDispatchApi {
    uint32_t size,version;void* context;
    int (*add)(void*,uint32_t code,uint64_t rip_begin,uint64_t rip_end,int priority,
               CtrExceptionCallback,void* user,uint64_t* token);
    int (*remove)(void*,uint64_t token);
} CtrExceptionDispatchApi;
// Runtime owns exception delivery. Register only owned live generated-code ranges.
// Keep callback code/user/range alive until remove returns0. Removal waits active
// calls; queued snapshots then skip the disabled entry. Never remove/unload from
// inside ANY callback (rejected116). Reentrant dispatch continues native search.
// Smaller priority runs first; equal priorities retain insertion order. Duplicate
// exact registrations reject109. Unknown faults never become successful reads.
