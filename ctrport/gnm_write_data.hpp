// SPDX-License-Identifier: GPL-2.0-only
#pragma once
#include "gpu_resident_memory.hpp"
#include "gnm_eop_completion.hpp"
#include "gnm_completion_batch.hpp"
#include "depth_publication_owners.hpp"
#include "gnm_command_cursor.hpp"
#include "video/session_api.h"
#include "video/graphics_companion.h"
namespace ctrport {
// Whole supported WriteData memory-copy branch from Liverpool::ProcessGraphics,
// engine/shadPS4/src/video_core/amdgpu/liverpool.cpp:764-778 and PM4CmdWriteData,
// revision e3ce810f3a653f43ac64ebab63023de281a4103a, GPL-2.0-or-later.
// Sequential address writes only; wr_one_addr and non-memory destinations fail.
// A genuine preceding-work completion provider is mandatory, including for
// label writes. This transport helper never produces a GPU fence or flip IRQ.
inline int complete_write_data(const uint32_t* packet, size_t words, const CtrGuestCopyApi& memory,
                               GnmCompletePriorGpuWork complete, void* context) {
    if (!packet || words < 5 || words > 16385) return 60;
    if ((packet[0] & 0xc000ff00u) != 0xc0003700u || size_t((packet[0] >> 16) & 0x3fffu) + 2 != words) return 60;
    if (packet[0] & 255u) return 88;
    const auto control = packet[1], destination = (control >> 8) & 15u;
    if ((destination != 2 && destination != 5) || (control & 0x10000u)) return 88;
    // Only public memory-copy controls have a handler here; reserved fields fail.
    if (control & ~0x40100f00u) return 88;
    const uint64_t address = uint64_t(packet[2]) | (uint64_t(packet[3]) << 32);
    const uint64_t bytes = uint64_t(words - 4) * 4;
    if (!address || (address & 3u) || address > UINT64_MAX - bytes) return 60;
    if (memory.size != sizeof(memory) || memory.version != 1 || !memory.write) return 60;
    if (!complete) return 88;
    try {
        const int status = complete(context);
        if (status) return status;
        return memory.write(memory.context, address, packet + 4, bytes);
    } catch (...) {
        return 112;
    }
}
// Both label transports require the same authoritative image generations.
// Residency satisfies completion without retiring GPU dirtiness or exposing
// stale CPU bytes. Color publication remains mandatory for current linear paths.
struct GnmImageCompletion {
    size_t colors;
    DepthPublicationOwners* depths;
    CtrGuestCopyApi memory;
    DepthPublicationOwners::Resident resident;
    void* resident_context;
    GnmCompletePriorGpuWork visibility;
    void* visibility_context;
    static int finish(void* raw) {
        if (!raw) return 60;
        auto& work = *static_cast<GnmImageCompletion*>(raw);
        if (!work.depths || !work.visibility) return 60;
        if (work.colors) return 88;
        const int status = work.depths->finish(work.memory, work.resident, work.resident_context);
        return status ? status : work.visibility(work.visibility_context);
    }
};

}
