// SPDX-License-Identifier: GPL-2.0-only
#ifndef CTR_GNM_EXECUTOR_HOST_H
#define CTR_GNM_EXECUTOR_HOST_H
#include <stdint.h>
#include "guest_copy_api.h"
#include "video/shader_inputs.h"
#include "shader_owned_ranges.h"
/* Host C ABI, not a PS4 service. CtrHostApi.context for the Gnm publisher
   points to this table. Snapshots own immutable command DWORDs until release.
   No resources, GPU coherence, completion, or draw success is implied.
   All callbacks are synchronous. DLL code must stay pinned through invocation.
   Tokens and borrowed command pointers must never outlive their release. */
typedef int (*CtrGnmOwnerActionV1)(void* argument, void* video_session_context);
typedef struct CtrGnmExecutorHostV1 {
    uint32_t size, version;
    void* context;
    int (*capture)(void*, uint32_t count, uint64_t draw_addresses, uint64_t draw_sizes, uint64_t constant_addresses,
                   uint64_t constant_sizes, uint64_t* token);
    int (*command)(void*, uint64_t token, uint32_t index, uint32_t constant_buffer, const uint32_t** words,
                   uint64_t* count, uint64_t* guest_address);
    /* Caller must synchronize all command consumers and GPU fences before release.
    A stale token fails, output arguments are unchanged on failure. */
    int (*release)(void*, uint64_t token);
    /* Runs on the actual selected video session owner, no Vulkan handle fabrication.
    Session context is borrowed ONLY during action. No asynchronous callbacks. */
    int (*on_video_owner)(void*, CtrGnmOwnerActionV1 action, void* argument);
    /* Resident checked CPU copies; GPU writes may be copied only after a real
    completion fence. This does not create source-writer/GPU coherence. */
    int (*guest_memory)(void*, CtrGuestCopyApi* output);
    /* Direct mapping owner is pinned on first successful query. APIs/leases must
    be released before host teardown; mapping replacement is rejected. */
    int (*shader_inputs)(void*, CtrShaderInputsV1*, CtrShaderOwnedRangesV1*);
    /* Exact commands captured before the resident wait. Lookup is bound to the
    trapped thread and caller. Release consumes the pending token as well. */
    int (*pending_commands)(void*, uint64_t caller, uint64_t* token);
} CtrGnmExecutorHostV1;
#endif
