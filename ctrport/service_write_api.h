// SPDX-License-Identifier: GPL-2.0-only
#ifndef CTRPORT_SERVICE_WRITE_API_H
#define CTRPORT_SERVICE_WRITE_API_H
#include <stdint.h>
/* Optional typed CtrHostApi.context for a write service DLL. Host owns files,
   mutexes and callback code through final DLL retirement. No guest pointer is
   dereferenced by the DLL. transport0 means result was produced; transport1
   means unsupported/host failure and MUST NOT become guest success. */
typedef struct CtrWriteHostApi {
    uint32_t size;
    uint32_t version;
    void* context;
    uint32_t (*write)(void* context,int32_t fd,const void* bytes,uint64_t count,
        int64_t* result);
} CtrWriteHostApi;
#endif
