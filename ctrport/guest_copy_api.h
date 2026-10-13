// SPDX-License-Identifier: GPL-2.0-only
#ifndef CTRPORT_GUEST_COPY_API_H
#define CTRPORT_GUEST_COPY_API_H
#include <stdint.h>
/* Windows C ABI v1. Context and callback code stay resident until consumers
   stop/join. Return0 means actual copy completed; nonzero is host rejection,
   never a guest PS4 errno. Host buffers must be owned/stable and non-overlapping
   with the guest range. Owner serializes unmap/reprotect and guest/GPU mutation.
   No coherent GPU capture or arbitrary process memory access is implied. */
typedef struct CtrGuestCopyApi {
 uint32_t size,version;
 void* context;
 int (*read)(void*,uint64_t guest_address,void* host_output,uint64_t size);
 int (*write)(void*,uint64_t guest_address,const void* host_input,uint64_t size);
} CtrGuestCopyApi;
#endif
