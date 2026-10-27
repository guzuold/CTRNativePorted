// SPDX-License-Identifier: GPL-2.0-only
#ifndef CTRPORT_SERVICE_ABI_H
#define CTRPORT_SERVICE_ABI_H
#include <stdint.h>
#define CTRPORT_SERVICE_ABI_VERSION 1u
/* Win64 C ABI only. Guest SysV bridges remain resident in the executable.
   No STL, exceptions, allocation ownership or guest objects cross this ABI. */
typedef struct CtrHostApi {
    uint32_t size;
    uint32_t version;
    void* context;
    void (*log)(void* context, const char* bytes, uint32_t size);
} CtrHostApi;
/* Signature2: integer3 -> integer result with explicit host transport status.
   status0 produces *result (including guest errno bits). Any other status is
   unsupported/failure, handled by the resident bridge with caller diagnostics. */
typedef uint32_t (*CtrInteger3Transport)(uint64_t a, uint64_t b, uint64_t c, uint64_t* result);
/* Signature9: nine captured SysV integer arguments, copied into resident host
   storage. Transport status0 alone publishes *result; no borrowed guest stack. */
typedef uint32_t (*CtrInteger9Transport)(const uint64_t arguments[9], uint64_t caller, uint64_t* result);

typedef struct CtrServiceEntry {
    uint32_t size;
    uint32_t signature_id;
    char nid[32];
    char library[64];
    char module[64];
    const void* win64_adapter;
} CtrServiceEntry;
typedef struct CtrServiceManifest {
    uint32_t size;
    uint32_t version;
    uint32_t entry_size;
    uint32_t entry_count;
    const CtrServiceEntry* entries;
    char eboot_sha256[65];
    uint8_t reserved[7];
} CtrServiceManifest;
typedef const CtrServiceManifest* (*CtrGetServiceManifest)(const CtrHostApi* host);
#endif
