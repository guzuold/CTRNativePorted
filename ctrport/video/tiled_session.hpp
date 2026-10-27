// SPDX-License-Identifier: GPL-2.0-only
#pragma once
#include "session_api.h"
#include <memory>
namespace ctrport {
class DirectMemoryBacking;
}
// Synchronous Win64/MSVC companion interface, NOT a portable C data ABI.
// shared_ptr pointer is borrowed for this call; registry copies owned range leases.
// All calls must run on the live session owner, using session.context.
// Runtime must stop original/GPU label users before destroying the session.
extern "C" int ctr_video_register_tiled_mapped_v1(void* session,
                                                  const std::shared_ptr<ctrport::DirectMemoryBacking>* owner,
                                                  uint32_t first, uint32_t count,
                                                  const CtrVideoBufferAttributesV1* attributes,
                                                  const uint64_t* addresses, uint32_t* group);
extern "C" int ctr_video_unregister_tiled_v1(void* session, uint32_t group);
extern "C" int ctr_video_tiled_label_address_v1(void* session, uint64_t* address);

// Host snapshot of the latest fenced guest publication, exact registered address/8355840B.
// Copies/detiles synchronously and enqueues an immutable frame; no flip completion yet.
// Use session present_next for genuine GPU presentation/completion and flip events.
extern "C" int ctr_video_submit_tiled_snapshot_v1(void* session, uint32_t slot, uint64_t registered_address,
                                                  const void* bytes, uint64_t length, uint64_t argument);

// Apply pinned VideoOut ProcessFlip label retirement after matching real present_next completion.
// First completion keeps current label locked1; later completion unlocks PREVIOUS slot64bit0
// and notifies registry waiters. Caller must execute original checked WriteData lock1 before enqueue.
extern "C" int ctr_video_complete_tiled_flip_v1(void* session, uint32_t slot, const CtrVideoCompletionV1* completion);

// Bounded synchronous end submission: real barrier/fence and device idle.
// Requires retired matching completion, no pending presentations; returns monotonic frame ordinal.
// Runtime must reset its DCB/CCB offsets/init packet state only after this succeeds.
// No guest downloads, resource destruction, label changes or synthetic GPU-idle IRQ.
extern "C" int ctr_video_end_tiled_frame_v1(void* session, const CtrVideoCompletionV1* completion, uint64_t* frame);

// Bind retained real clock before first enqueue, without event registration. Context must outlive session.
extern "C" int ctr_video_bind_clock_v1(void* session, const CtrDisplayEventApi* clock);

// Caller validates its VideoOut handle before dispatch to its owned session.
// Settings are copied by value; only exact identity gamma is supported.
struct CtrVideoColorSettingsV1 {
    float gamma;
    uint32_t reserved[3];
};
extern "C" int ctr_video_adjust_color_v1(void* session, const CtrVideoColorSettingsV1* settings);
