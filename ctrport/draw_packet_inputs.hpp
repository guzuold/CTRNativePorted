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
 uint32_t opcode=0,count=0,initiator=0,max_indices=0,index_base_low=0,index_base_high=0;
 bool indexed=false;
};
inline int prepare_draw_packet(const uint32_t* data,size_t words,DrawPacketInputs& output){
 if(!data || words<2)return 60;
 std::vector<CtrPm4::Packet> packets;const int status=CtrPm4::DecodePackets(data,words,packets);if(status)return status;
 if(packets.size()!=1 || packets[0].words!=words)return 60;
 if(data[0]&255)return 88;
 DrawPacketInputs value;value.opcode=packets[0].opcode;
 if(value.opcode==0x2d){if(words!=3)return 60;value.count=data[1];value.initiator=data[2];}
 else if(value.opcode==0x27){if(words!=6)return 60;value.indexed=true;value.max_indices=data[1];value.index_base_low=data[2];value.index_base_high=data[3];value.count=data[4];value.initiator=data[5];}
 else return 88;
 output=value;return 0;
}
// Whole CPU input assignments of public Liverpool DrawIndexAuto/DrawIndex2.
// Private execution-bank copy only: never completes or advances the PM4 packet.
// Offsets: regs.cpp index_base A1F9, draw_initiator A1FC, index_size A29D,
// following max_index_size A29E; num_indices immediately before num_instances C24D.
inline int prepare_draw_execution_bank(const uint32_t* packet,size_t words,const CtrPm4::Registers& before,CtrPm4::Registers& output){
 DrawPacketInputs draw;const int status=prepare_draw_packet(packet,words,draw);if(status)return status;
 auto result=std::make_unique<CtrPm4::Registers>(before);
 if(draw.indexed){result->words[0xA29E]=draw.max_indices;result->words[0xA1F9]=draw.index_base_low;result->words[0xA1FA]=draw.index_base_high;}
 result->words[0xC24C]=draw.count;result->words[0xA1FC]=draw.initiator;
 output=*result;return 0;
}
inline std::string draw_packet_event(uint32_t word,int status,const DrawPacketInputs& value){
 std::ostringstream log;log<<"{\"scope\":\"original_GnmSubmitFlip_draw_packet_inputs\",\"word\":"<<word<<",\"status\":"<<status<<",\"opcode\":"<<value.opcode<<",\"count\":"<<value.count<<",\"initiator\":"<<value.initiator<<",\"max_indices\":"<<value.max_indices<<",\"indexed\":"<<(value.indexed?"true":"false")<<",\"GPU_execution\":false}\n";return log.str();
}
inline int draw_packet_inputs_control(){
 const uint32_t auto_draw[]={0xc0012d00,3,2};DrawPacketInputs value;
 if(prepare_draw_packet(auto_draw,3,value) || value.indexed || value.count!=3 || value.initiator!=2)return 1;
 const uint32_t indexed[]={0xc0042700,12,0x1235,0x20,6,0};
 if(prepare_draw_packet(indexed,6,value) || !value.indexed || value.count!=6 || value.max_indices!=12 || value.index_base_low!=0x1235 || value.index_base_high!=0x20)return 2;
 const uint32_t predicate[]={0xc0012d01,7,2};if(prepare_draw_packet(predicate,3,value)!=88 || value.count!=6)return 3;
 if(prepare_draw_packet(auto_draw,2,value)!=60 || value.count!=6)return 4;
 const uint32_t unsupported[]={0xc0001000,0};if(prepare_draw_packet(unsupported,2,value)!=88 || value.count!=6)return 5;
 const uint32_t zero[]={0xc0012d00,0,2};if(prepare_draw_packet(zero,3,value) || value.count)return 6;
 auto before=std::make_unique<CtrPm4::Registers>();auto after=std::make_unique<CtrPm4::Registers>();before->words[0xA29E]=99;before->compute[7]=123;
 if(prepare_draw_execution_bank(auto_draw,3,*before,*after) || after->words[0xC24C]!=3 || after->words[0xA1FC]!=2 || after->words[0xA29E]!=99 || after->compute!=before->compute)return 7;
 if(prepare_draw_execution_bank(indexed,6,*before,*after) || after->words[0xA29E]!=12 || after->words[0xA1F9]!=0x1235 || after->words[0xA1FA]!=0x20 || after->words[0xC24C]!=6)return 8;
 const auto saved=after->words;if(prepare_draw_execution_bank(predicate,3,*before,*after)!=88 || after->words!=saved || before->words[0xC24C])return 9;
 std::cout<<"{\"scope\":\"draw_packet_inputs_control\",\"passed\":true,\"indexed_raw_fields_preserved\":true,\"invalid_atomic\":true,\"zero_count_preserved\":true,\"synthetic\":true,\"GPU_execution\":false}\n";return 0;
}
}
