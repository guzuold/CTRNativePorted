// SPDX-License-Identifier: GPL-2.0-only
#pragma once
#include <memory>
#include "video/pm4_decode.hpp"
#include "video/pm4_registers.hpp"
namespace ctrport {
// Complete packet-field transport from public PM4CmdDrawIndexAuto/DrawIndex2
// and Liverpool graphics handlers, shadPS4 e3ce810f3a653f43ac64ebab63023de281a4103a
// (GPL-2.0-or-later). No register mutation or GPU completion is implied.
struct DrawPacketInputs {
    uint32_t opcode = 0, count = 0, initiator = 0, max_indices = 0, index_base_low = 0, index_base_high = 0;
    bool indexed = false;
};
inline int prepare_draw_packet(const uint32_t* data, size_t words, DrawPacketInputs& output) {
    if (!data || words < 2) return 60;
    std::vector<CtrPm4::Packet> packets;
    const int status = CtrPm4::DecodePackets(data, words, packets);
    if (status) return status;
    if (packets.size() != 1 || packets[0].words != words) return 60;
    if (data[0] & 255) return 88;
    DrawPacketInputs value;
    value.opcode = packets[0].opcode;
    if (value.opcode == 0x2d) {
        if (words != 3) return 60;
        value.count = data[1];
        value.initiator = data[2];
    } else if (value.opcode == 0x27) {
        if (words != 6) return 60;
        value.indexed = true;
        value.max_indices = data[1];
        value.index_base_low = data[2];
        value.index_base_high = data[3];
        value.count = data[4];
        value.initiator = data[5];
    } else
        return 88;
    output = value;
    return 0;
}
// Whole CPU input assignments of public Liverpool DrawIndexAuto/DrawIndex2.
// Private execution-bank copy only: never completes or advances the PM4 packet.
// Offsets: regs.cpp index_base A1F9, draw_initiator A1FC, index_size A29D,
// following max_index_size A29E; num_indices immediately before num_instances C24D.
inline int prepare_draw_execution_bank(const uint32_t* packet, size_t words, const CtrPm4::Registers& before,
                                       CtrPm4::Registers& output) {
    DrawPacketInputs draw;
    const int status = prepare_draw_packet(packet, words, draw);
    if (status) return status;
    auto result = std::make_unique<CtrPm4::Registers>(before);
    if (draw.indexed) {
        result->words[0xA29E] = draw.max_indices;
        result->words[0xA1F9] = draw.index_base_low;
        result->words[0xA1FA] = draw.index_base_high;
    }
    result->words[0xC24C] = draw.count;
    result->words[0xA1FC] = draw.initiator;
    output = *result;
    return 0;
}
inline std::string draw_packet_event(uint32_t word, int status, const DrawPacketInputs& value) {
    std::ostringstream log;
    log << "{\"scope\":\"original_GnmSubmitFlip_draw_packet_inputs\",\"word\":" << word << ",\"status\":" << status
        << ",\"opcode\":" << value.opcode << ",\"count\":" << value.count << ",\"initiator\":" << value.initiator
        << ",\"max_indices\":" << value.max_indices << ",\"indexed\":" << (value.indexed ? "true" : "false")
        << ",\"GPU_execution\":false}\n";
    return log.str();
}
}
