// SPDX-License-Identifier: GPL-2.0-only
#pragma once
#include "gnm_command_cursor.hpp"
namespace ctrport {
// Complete raw PM4DmaData field decode, pinned public shadPS4 pm4_cmds.h,
// e3ce810f3a653f43ac64ebab63023de281a4103a GPL-2.0-or-later.
// A request is not a transfer. Provider owns GPU/GDS/coherence implementation.
struct GnmDmaRequest {uint32_t control=0,command=0,source_kind=0,destination_kind=0,bytes=0,data=0;uint64_t source=0,destination=0;};
inline int prepare_dma(const uint32_t* packet,size_t words,GnmDmaRequest& output){
 if(!packet || words!=7)return 60;std::vector<CtrPm4::Packet> packets;
 const int status=CtrPm4::DecodePackets(packet,words,packets);if(status)return status;
 if(packets.size()!=1 || packets[0].opcode!=0x50 || packets[0].words!=7)return 60;
 if(packet[0]&255u)return 88;
 GnmDmaRequest prepared{packet[1],packet[6],(packet[1]>>29)&3u,(packet[1]>>20)&3u,packet[6]&0x1fffffu,packet[2],uint64_t(packet[2])|(uint64_t(packet[3])<<32),uint64_t(packet[4])|(uint64_t(packet[5])<<32)};
 if(prepared.destination_kind==2)return 88;
 output=prepared;return 0;
}
using GnmDmaProvider=int(*)(void*,const GnmDmaRequest&);
}
