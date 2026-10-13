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
inline int complete_event_metadata_flush(const uint32_t* data,size_t words,int(*provider)(void*),void* context){
 if(!data || words!=2)return 60;std::vector<CtrPm4::Packet> packets;
 const int decoded=CtrPm4::DecodePackets(data,words,packets);if(decoded)return decoded;
 if(packets.size()!=1 || packets[0].opcode!=0x46 || packets[0].words!=2)return 60;
 if(data[0]&255u)return 88;
 if(data[1]!=0x72cu && data[1]!=0x72eu)return 88;
 if(!provider)return 88;try{return provider(context);}catch(...){return 112;}
}
inline int gnm_event_metadata_flush_control(){
 uint32_t packet[]={0xc0004600u,0x72cu};unsigned calls=0;
 const auto fail=+[](void* raw)->int{++*static_cast<unsigned*>(raw);return 90;};
 const auto pass=+[](void* raw)->int{++*static_cast<unsigned*>(raw);return 0;};
 GnmCommandCursor cursor;if(cursor.capture(1,0,0,packet,2))return 1;
 if(complete_event_metadata_flush(packet,2,nullptr,nullptr)!=88)return 2;
 auto handler=[&](uint64_t,uint32_t,uint32_t,const CtrPm4::Packet& p,const uint32_t* data){return complete_event_metadata_flush(data,p.words,fail,&calls);};
 if(cursor.resume(handler)!=90 || cursor.completed() || calls!=1)return 3;
 if(cursor.resume([&](uint64_t,uint32_t,uint32_t,const CtrPm4::Packet& p,const uint32_t* data){return complete_event_metadata_flush(data,p.words,pass,&calls);}) || cursor.completed()!=1 || calls!=2)return 4;
 for(uint32_t control:{0x72du,0x72fu,0x12cu,0x10072cu}){packet[1]=control;if(complete_event_metadata_flush(packet,2,pass,&calls)!=88 || calls!=2)return 5;}
 packet[1]=0x72eu;if(complete_event_metadata_flush(packet,2,pass,&calls) || calls!=3)return 6;
 packet[0]|=1;if(complete_event_metadata_flush(packet,2,pass,&calls)!=88 || calls!=3)return 7;
 if(complete_event_metadata_flush(packet,1,pass,&calls)!=60 || calls!=3)return 8;
 packet[0]&=~1u;const auto throwing=+[](void*)->int{throw 1;};if(complete_event_metadata_flush(packet,2,throwing,nullptr)!=112)return 9;
 std::cout<<"{\"scope\":\"gnm_event_metadata_flush_control\",\"passed\":true,\"synthetic_provider\":true,\"failure_retains_cursor\":true,\"GPU_execution\":false}\n";return 0;
}
}
