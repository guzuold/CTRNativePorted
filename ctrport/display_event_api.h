// SPDX-License-Identifier: GPL-2.0-only
// Host C ABI for VideoOut. Opaque queue tokens belong to kernel host registry.
// Return 0 only after operation performed; nonzero is host-boundary rejection,
// not a PS4 errno. No guest syscall binding is implied.
#ifndef CTRPORT_DISPLAY_EVENT_API_H
#define CTRPORT_DISPLAY_EVENT_API_H
#include <stdint.h>
typedef struct CtrDisplayEventApi {
    uint32_t size;
    uint32_t version;
    void* context;
    int (*add)(void*, uint64_t queue, uint64_t ident, uint64_t udata);
    int (*remove)(void*, uint64_t queue, uint64_t ident);
    int (*trigger)(void*, uint64_t queue, uint64_t ident, uint64_t hint);
    int (*clock)(void*, uint64_t* runtime_microseconds, uint64_t* native_tsc);
} CtrDisplayEventApi;
#endif
