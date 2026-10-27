// SPDX-License-Identifier: GPL-2.0-only
#pragma once
#include <cstddef>
#include <cstdint>
#include <vector>
// Field interpretation from pinned shadPS4 pm4_cmds.h (GPL-2.0-or-later),
// e3ce810f3a653f43ac64ebab63023de281a4103a. Metadata decoder ONLY.
// Input is owned host DWORD snapshot. This never executes, writes or signals.
namespace CtrPm4 {
struct Packet {
    size_t word;
    uint32_t words, opcode;
};
struct Eop {
    uint32_t eventType, eventIndex, dataSelect, interruptSelect, writeBytes;
    uint64_t address, value;
};
inline int DecodePackets(const uint32_t* data, size_t words, std::vector<Packet>& output) {
    if (!data || !words || words > 4 * 1024 * 1024) return 60;
    std::vector<Packet> prepared;
    for (size_t at = 0; at < words;) {
        const uint32_t header = data[at];
        if ((header >> 30) == 2) {
            prepared.push_back({at, 1, 258});
            ++at;
            continue; // Type2 padding, metadata only.
        }
        if ((header >> 30) != 3) return 88;
        const uint32_t count = ((header >> 16) & 0x3fff) + 2;
        if (count > words - at) return 60;
        prepared.push_back({at, count, (header >> 8) & 255});
        at += count;
    }
    output.swap(prepared);
    return 0;
}
// Exact public PM4CmdDispatchDirect fields. Metadata only; no predicate,
// ordered-append, descriptor, pipeline, queue or completion effects are implied.
struct DispatchDirect {
    uint32_t x = 0, y = 0, z = 0, initiator = 0;
    bool predicate = false, compute = false;
};
inline int DecodeDispatchDirect(const uint32_t* data, size_t words, DispatchDirect& output) {
    if (!data || words != 5) return 60;
    if ((data[0] >> 30) != 3 || ((data[0] >> 16) & 0x3fff) != 3 || ((data[0] >> 8) & 255) != 0x15) return 60;
    if (data[0] & 0xfc) return 88; // Reserved header bits are outside admitted metadata.
    DispatchDirect prepared;
    prepared.x = data[1];
    prepared.y = data[2];
    prepared.z = data[3];
    prepared.initiator = data[4];
    prepared.predicate = (data[0] & 1) != 0;
    prepared.compute = (data[0] & 2) != 0;
    output = prepared;
    return 0;
}
inline int DecodeEop(const uint32_t* data, size_t words, Eop& output) {
    if (!data || words != 6) return 60;
    if ((data[0] >> 30) != 3 || ((data[0] >> 8) & 255) != 0x47 || ((data[0] >> 16) & 0x3fff) != 4) return 60;
    Eop value{};
    value.eventType = data[1] & 63;
    value.eventIndex = (data[1] >> 8) & 15;
    value.address = data[2] | (uint64_t(data[3] & 65535) << 32);
    value.dataSelect = data[3] >> 29;
    value.interruptSelect = (data[3] >> 24) & 3;
    value.value = data[4] | (uint64_t(data[5]) << 32);
    switch (value.dataSelect) {
    case 0: break;
    case 1:
        value.writeBytes = 4;
        value.value = data[4];
        break;
    case 2: value.writeBytes = 8; break;
    // GPU clock/perf require actual timing authority, not a host invented value.
    case 3:
    case 4: return 88;
    default: return 88;
    }
    if (value.interruptSelect == 3 || (value.interruptSelect == 1 && value.dataSelect != 0)) return 88;
    if (value.writeBytes && (value.address & 3)) return 60;
    output = value;
    return 0;
}
} // namespace CtrPm4
