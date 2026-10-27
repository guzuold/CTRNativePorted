// SPDX-License-Identifier: GPL-2.0-only
#pragma once
#include "owned_guest_copy.hpp"
#include "video/pm4_registers.hpp"
#include "gnm_command_cursor.hpp"
namespace ctrport {
// Whole public PM4CmdWaitRegMem::Test comparison policy, shadPS4 revision
// e3ce810f3a653f43ac64ebab63023de281a4103a, pm4_cmds.h:643-713,
// GPL-2.0-or-later. One bounded poll replaces Liverpool's coroutine yield.
// Pending returns88 without advancing the cursor; never manufactures a fence.
inline int poll_wait_reg_mem(const uint32_t* packet,size_t words,const CtrPm4::Registers& registers,const CtrGuestCopyApi& memory){
 if(!packet || words!=7)return 60;
 std::vector<CtrPm4::Packet> decoded;const int decode=CtrPm4::DecodePackets(packet,words,decoded);if(decode)return decode;
 if(decoded.size()!=1 || decoded[0].opcode!=0x3c || decoded[0].words!=7)return 60;
 // Predicate, endian swap, PFP engine and reserved control bits unsupported.
 if((packet[0]&255u) || (packet[1]&~0x17u) || ((packet[1]&16u) && (packet[2]&3u)))return 88;
 const auto function=packet[1]&7u;if(function==7)return 88;
 uint32_t value=0;
 if(packet[1]&16u){
  if(memory.version!=1 || memory.size!=sizeof(memory) || !memory.read)return 60;
  const uint64_t address=uint64_t(packet[2])|(uint64_t(packet[3])<<32);
  const int status=memory.read(memory.context,address,&value,sizeof(value));if(status)return status;
 }else{
  if(packet[3] || packet[2]>65535 || packet[2]>=registers.words.size())return 60;
  value=registers.words[packet[2]];
 }
 const uint32_t reference=packet[4],mask=packet[5];bool passed=false;
 switch(function){
 case 0:passed=true;break;
 case 1:passed=(value&mask)<reference;break;
 case 2:passed=(value&mask)<=reference;break;
 case 3:passed=(value&mask)==reference;break;
 case 4:passed=(value&mask)!=reference;break;
 case 5:passed=(value&mask)>=reference;break;
 case 6:passed=(value&mask)>reference;break;
 default:return 88;
 }
 return passed?0:88;
}
}
