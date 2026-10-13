// SPDX-License-Identifier: GPL-2.0-only
#pragma once
#include <stdint.h>
#include <stddef.h>
#include <string.h>

// Host-owned immutable input, not a guest pointer. No runtime binding yet.
typedef struct CtrShaderSnapshot {
    const void* bytes;
    uint64_t size;
    uint64_t guest_address;
    uint64_t snapshot_token;
} CtrShaderSnapshot;

typedef struct CtrShaderInputsV1 {
    uint32_t version;
    uint32_t byte_size;
    void* context;
    // Runtime validates overflow, readable ownership and a consistent capture.
    // Copies exactly the requested range to immutable host storage, retained
    // until release. Reject absent/partial input; never synthesize zero bytes.
    int (*capture)(void*, uint64_t guest_address, uint64_t size,
                   CtrShaderSnapshot* out);
    // Validate exact snapshot identity; reject stale/double release. Compilation
    // and nested descriptor traversal must finish before releasing any snapshot.
    int (*release)(void*, const CtrShaderSnapshot*);
} CtrShaderInputsV1;

// Caller retains a live captured snapshot; this checks ranges, not token lifetime.
// Output must be separate writable host storage. Failure leaves output untouched.
static inline int ctr_shader_snapshot_read(const CtrShaderSnapshot* snapshot,
                                           uint64_t guest_address, void* output,
                                           size_t size) {
    if (!snapshot || !snapshot->bytes || !snapshot->snapshot_token || !output ||
        snapshot->size > SIZE_MAX ||
        snapshot->guest_address > UINT64_MAX - snapshot->size ||
        guest_address < snapshot->guest_address) return 1;
    const uint64_t offset = guest_address - snapshot->guest_address;
    if (offset > snapshot->size || size > snapshot->size - offset) return 1;
    if (size) memcpy(output, (const unsigned char*)snapshot->bytes + (size_t)offset, size);
    return 0;
}

// Nested descriptor addresses require separate captures; host offsets must be
// bounds checked before every read. This interface does not authorize arbitrary
// info.h guest dereferences or SRT JIT fault recovery, and does not implement them.
