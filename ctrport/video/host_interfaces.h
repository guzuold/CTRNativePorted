// SPDX-License-Identifier: GPL-2.0-only
#pragma once
#include <stddef.h>
#include <stdint.h>

// Proposed Win64 host boundary, NOT a PS4 service ABI. No implicit identity map.
// Version 2: callbacks return zero on success, nonzero on failure.
// No callbacks are installed or used by the current Vulkan control.
typedef struct CtrVideoMemoryLease {
    void* host_pointer;
    uint64_t byte_length;
    uint64_t owner_token;
    uint64_t guest_address;
} CtrVideoMemoryLease;

typedef struct CtrVideoHostV2 {
    uint32_t version;
    uint32_t byte_size;
    void* context;
    // Validate [guest_address,guest_address+size), including overflow, permissions
    // and live ownership. Pin backing until release, including through GPU fences.
    int (*acquire_memory)(void*, uint64_t guest_address, uint64_t size,
                          uint32_t write_access, CtrVideoMemoryLease* out);
    // Validate token and exact lease identity; reject stale/double release.
    // Failed release does not transfer ownership or silently unpin backing.
    int (*release_memory)(void*, const CtrVideoMemoryLease*);
    // Notify actual GPU writes only after completion and before release. Validate
    // live token, write permission, and subrange within the acquired guest range.
    // Reject stale tokens even if the same guest range has since been reused.
    int (*publish_gpu_write)(void*, const CtrVideoMemoryLease*,
                             uint64_t guest_address, uint64_t size);
    // Runtime owns worker creation/TLS and joins before teardown. Entry is Win64.
    int (*start_worker)(void*, void (*entry)(void*), void* argument, uint64_t* out_token);
    int (*join_worker)(void*, uint64_t token);
    // Event ownership/handle validation stays with runtime, no fabricated success.
    int (*signal_event)(void*, uint64_t queue_token, uint64_t event_id, uint64_t data);
    // Required diagnostic for absent guest service; runtime decides fail/termination.
    void (*missing_service)(void*, const char* library, const char* nid, uint64_t caller);
} CtrVideoHostV2;
