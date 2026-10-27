// SPDX-License-Identifier: GPL-2.0-only
#pragma once
#include "gnm_command_cursor.hpp"
namespace ctrport {
// Public PM4CmdEventWrite fields/event enums: shadPS4 pm4_cmds.h:358-437,
// e3ce810f3a653f43ac64ebab63023de281a4103a, GPL-2.0-or-later.
// Exact cache-index FlushAndInvDbMeta/FlushAndInvCbMeta without an address.
// Runtime has no emulated compression metadata cache. The conservative provider
// must execute a whole GPU queue memory barrier and fence. It grants no guest
// download, image layout transition, alias refresh or completion label write.
inline int complete_event_metadata_flush(const uint32_t* data, size_t words, int (*provider)(void*), void* context) {
    if (!data || words != 2) return 60;
    std::vector<CtrPm4::Packet> packets;
    const int decoded = CtrPm4::DecodePackets(data, words, packets);
    if (decoded) return decoded;
    if (packets.size() != 1 || packets[0].opcode != 0x46 || packets[0].words != 2) return 60;
    if (data[0] & 255u) return 88;
    if (data[1] != 0x72cu && data[1] != 0x72eu) return 88;
    if (!provider) return 88;
    try {
        return provider(context);
    } catch (...) {
        return 112;
    }
}
}
