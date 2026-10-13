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
inline int gnm_wait_reg_mem_control(){
 auto bank=std::make_unique<CtrPm4::Registers>();GuestStack stack(32768,4096);
 auto* value=reinterpret_cast<uint32_t*>(stack.bottom());*value=2;const auto address=reinterpret_cast<uint64_t>(value);const auto memory=make_owned_guest_copy_api();
 uint32_t packet[]={0xc0053c00,0x13,uint32_t(address),uint32_t(address>>32),3,0xff,10};
 if(poll_wait_reg_mem(packet,7,*bank,memory)!=88 || *value!=2)return 1;
 GnmCommandCursor cursor;if(cursor.capture(1,0,0,packet,7))return 2;
 auto poll=[&](uint64_t,uint32_t,uint32_t,const CtrPm4::Packet& p,const uint32_t* data){return poll_wait_reg_mem(data,p.words,*bank,memory);};
 if(cursor.resume(poll)!=88 || cursor.completed())return 3;
 *value=3;if(cursor.resume(poll) || cursor.completed()!=1)return 4;
 // All seven original comparison branches, including unmasked reference.
 const bool expected[]={true,true,true,false,true,false,false};*value=0x102;
 for(uint32_t f=0;f<7;++f){packet[1]=16|f;if(poll_wait_reg_mem(packet,7,*bank,memory)!=(expected[f]?0:88))return 5;}
 packet[1]=0x17;if(poll_wait_reg_mem(packet,7,*bank,memory)!=88)return 6;
 packet[1]=0x113;if(poll_wait_reg_mem(packet,7,*bank,memory)!=88)return 7;
 packet[1]=3;packet[2]=0xA100;packet[3]=0;bank->words[0xA100]=3;
 if(poll_wait_reg_mem(packet,7,*bank,memory))return 8;
 packet[2]=0xA101;bank->words[0xA101]=3;if(poll_wait_reg_mem(packet,7,*bank,memory))return 10;
 packet[2]=0xD000;if(poll_wait_reg_mem(packet,7,*bank,memory)!=60)return 9;
 std::cout<<"{\"scope\":\"gnm_wait_reg_mem_control\",\"passed\":true,\"pending_cursor_preserved\":true,\"real_owned_memory_read\":true,\"comparison_branches\":7,\"synthetic_CPU_writer\":true,\"GPU_execution\":false}\n";return 0;
}
}
