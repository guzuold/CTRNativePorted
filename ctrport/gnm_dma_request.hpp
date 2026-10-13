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
inline int complete_dma(const uint32_t* packet,size_t words,GnmDmaProvider provider,void* context){
 GnmDmaRequest request;const int status=prepare_dma(packet,words,request);if(status)return status;
 if(!provider)return 88;
 try{return provider(context,request);}catch(...){return 112;}
}
inline int gnm_dma_request_control(){
 uint32_t packet[]={0xc0055000,0xc0100000,0x11223344,0x10,0x80,0,64};GnmDmaRequest request;
 if(prepare_dma(packet,7,request) || request.control!=0xc0100000 || request.source_kind!=2 || request.destination_kind!=1 || request.bytes!=64 || request.data!=0x11223344 || request.source!=0x1011223344ull || request.destination!=128)return 1;
 if(complete_dma(packet,7,nullptr,nullptr)!=88)return 2;
 struct State{unsigned calls=0;int result=88;};State state;
 auto provider=+[](void* raw,const GnmDmaRequest& r)->int{auto& s=*static_cast<State*>(raw);++s.calls;return r.data==0x11223344 && r.bytes==64?s.result:112;};
 GnmCommandCursor cursor;if(cursor.capture(1,0,0,packet,7))return 3;
 auto execute=[&](uint64_t,uint32_t,uint32_t,const CtrPm4::Packet& p,const uint32_t* data){return complete_dma(data,p.words,provider,&state);};
 if(cursor.resume(execute)!=88 || cursor.completed() || state.calls!=1)return 4;
 state.result=90;if(cursor.resume(execute)!=90 || cursor.completed() || state.calls!=2)return 5;
 state.result=0;if(cursor.resume(execute) || cursor.completed()!=1 || state.calls!=3)return 6;
 packet[0]|=1;if(complete_dma(packet,7,provider,&state)!=88 || state.calls!=3)return 7;packet[0]&=~1u;
 if(complete_dma(packet,6,provider,&state)!=60 || state.calls!=3)return 8;
 packet[1]=(packet[1]&~0x300000u)|0x200000u;if(complete_dma(packet,7,provider,&state)!=88 || state.calls!=3)return 9;
 auto throwing=+[](void*,const GnmDmaRequest&)->int{throw 1;};packet[1]=0xc0100000;
 if(complete_dma(packet,7,throwing,nullptr)!=112)return 10;
 std::cout<<"{\"scope\":\"gnm_dma_request_control\",\"passed\":true,\"all_raw_fields_preserved\":true,\"pending_error_keeps_cursor\":true,\"GDS_not_stubbed\":true,\"synthetic_provider_only\":true,\"GPU_execution\":false}\n";return 0;
}
}
