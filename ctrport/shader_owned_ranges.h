// SPDX-License-Identifier: GPL-2.0-only
#pragma once
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
typedef struct CtrShaderRangeLeaseV1 { uint64_t guest_address,bytes,token; } CtrShaderRangeLeaseV1;
// Host-only synchronous callbacks. Begin pins mapping ledger on this thread;
// end must run on the same thread. Caller retains owner/API through all leases.
// Capture via CtrShaderInputsV1 while lease is live, release snapshots then end.
// No source-writer serialization, GPU coherence or resource admission promised.
typedef struct CtrShaderOwnedRangesV1 {
 uint32_t version,size;void* context;
 int (*begin)(void*,uint64_t address,uint64_t cap,CtrShaderRangeLeaseV1*);
 int (*end)(void*,const CtrShaderRangeLeaseV1*);
} CtrShaderOwnedRangesV1;
#ifdef __cplusplus
}
#endif
