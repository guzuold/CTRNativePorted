// SPDX-License-Identifier: GPL-2.0-only
#pragma once
#include "gnm_command_cursor.hpp"
namespace ctrport {
// Complete public PM4CmdAcquireMem field transport, pinned shadPS4
// e3ce810f3a653f43ac64ebab63023de281a4103a pm4_cmds.h (GPL-2.0-or-later).
// Upstream Liverpool ignores this packet; we do not promote that to real
// cache/fence semantics. A provider must implement the requested visibility.
struct GnmAcquireMemRequest {
    uint32_t control = 0, size_low = 0, size_high = 0, base_low = 0, base_high = 0, poll_interval = 0;
};
inline int prepare_acquire_mem(const uint32_t* data, size_t words, GnmAcquireMemRequest& output) {
    if (!data || words != 7) return 60;
    std::vector<CtrPm4::Packet> packets;
    const int status = CtrPm4::DecodePackets(data, words, packets);
    if (status) return status;
    if (packets.size() != 1 || packets[0].opcode != 0x58 || packets[0].words != 7) return 60;
    if (data[0] & 255u) return 88;
    output = {data[1], data[2], data[3], data[4], data[5], data[6]};
    return 0;
}
using GnmAcquireMemProvider = int (*)(void*, const GnmAcquireMemRequest&);
inline int complete_acquire_mem(const uint32_t* data, size_t words, GnmAcquireMemProvider provider, void* context) {
    GnmAcquireMemRequest request;
    const int status = prepare_acquire_mem(data, words, request);
    if (status) return status;
    if (!provider) return 88;
    try {
        return provider(context, request);
    } catch (...) {
        return 112;
    }
}
// Exact observed combinations of public Mesa gfx6.json CP_COHER_CNTL
// (MIT): CB/DB destination and action, TC/TCL1 action and TC writeback.
// Admission grants a conservative whole-queue memory barrier, never an image
// download, sampled-owner refresh, cache emulation, or guest publication.
inline bool acquire_queue_visibility_supported(const GnmAcquireMemRequest& r) {
    if (r.size_high || r.base_high || r.poll_interval != 10) return false;
    // Reached second-submit word0 adds SH_KCACHE_ACTION_ENA (gfx6 bit27).
    // Translated constants are Vulkan buffer reads, covered by the same actual
    // ALL_COMMANDS memory barrier/fence. This grants no CPU image download.
    if (r.control == 0x80c40000u || r.control == 0x88c40000u) return r.size_low == 1 && !r.base_low;
    // Original second-submit word12 requests all-address CB/DB, TC/TCL1,
    // shader constant/instruction visibility. Shader modules are immutable owned
    // translations; actual queue-wide barrier/fence covers their resource reads.
    if (r.control == 0x2ec47fc0u) return r.size_low == UINT32_MAX && !r.base_low;
    if (!r.size_low || !r.base_low) return false;
    return r.control == 0x82003fc0u || r.control == 0x86007fc0u || r.control == 0x84c44000u || r.control == 0x82c43fc0u;
}
}
