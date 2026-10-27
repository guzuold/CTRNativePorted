// SPDX-License-Identifier: GPL-2.0-only
#pragma once
#include "gnm_eop_completion.hpp"
#include "gnm_command_cursor.hpp"
namespace ctrport {
// Whole public PM4CmdEventWriteEos packet and SignalFence CPU writeback branch,
// shadPS4 e3ce810f3a653f43ac64ebab63023de281a4103a, GPL-2.0-or-later.
// GDS is original GPU storage, never invented. Provider must finish prior GPU
// work AND downloads before a SignalFence value may reach guest memory.
struct GnmEosRequest {
    uint32_t event_type = 0, event_index = 0, command = 0, data = 0;
    uint64_t address = 0;
};
inline int prepare_eos(const uint32_t* packet, size_t words, GnmEosRequest& output) {
    if (!packet || words != 5) return 60;
    std::vector<CtrPm4::Packet> decoded;
    const int status = CtrPm4::DecodePackets(packet, words, decoded);
    if (status) return status;
    if (decoded.size() != 1 || decoded[0].opcode != 0x48 || decoded[0].words != 5) return 60;
    if (packet[0] & 253u) return 88;
    if ((packet[1] & ~0xf3fu) || (packet[3] & 0x1fff0000u)) return 88;
    GnmEosRequest prepared{packet[1] & 63u, (packet[1] >> 8) & 15u, packet[3] >> 29, packet[4],
                           uint64_t(packet[2]) | (uint64_t(packet[3] & 65535u) << 32)};
    if (prepared.command != 1 && prepared.command != 2) return 88;
    if (!prepared.address || (prepared.address & 3u)) return 60;
    output = prepared;
    return 0;
}
inline int complete_eos_writeback(const uint32_t* packet, size_t words, const CtrGuestCopyApi& memory,
                                  GnmCompletePriorGpuWork complete, void* context) {
    GnmEosRequest eos;
    const int decoded = prepare_eos(packet, words, eos);
    if (decoded) return decoded;
    if (eos.command != 2) return 88;
    if (memory.version != 1 || memory.size != sizeof(memory) || !memory.write) return 60;
    if (!complete) return 88;
    int status = 112;
    try {
        status = complete(context);
    } catch (...) {
        return 112;
    }
    if (status) return status;
    return memory.write(memory.context, eos.address, &eos.data, sizeof(eos.data));
}
}
