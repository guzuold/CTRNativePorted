// SPDX-License-Identifier: GPL-2.0-only
#pragma once
#include "gnm_eop_completion.hpp"
#include "gnm_command_cursor.hpp"
namespace ctrport {
// Whole public PM4CmdEventWriteEos packet and SignalFence CPU writeback branch,
// shadPS4 e3ce810f3a653f43ac64ebab63023de281a4103a, GPL-2.0-or-later.
// GDS is original GPU storage, never invented. Provider must finish prior GPU
// work AND downloads before a SignalFence value may reach guest memory.
struct GnmEosRequest {uint32_t event_type=0,event_index=0,command=0,data=0;uint64_t address=0;};
inline int prepare_eos(const uint32_t* packet,size_t words,GnmEosRequest& output){
 if(!packet || words!=5)return 60;std::vector<CtrPm4::Packet> decoded;
 const int status=CtrPm4::DecodePackets(packet,words,decoded);if(status)return status;
 if(decoded.size()!=1 || decoded[0].opcode!=0x48 || decoded[0].words!=5)return 60;
 if(packet[0]&253u)return 88;
 if((packet[1]&~0xf3fu) || (packet[3]&0x1fff0000u))return 88;
 GnmEosRequest prepared{packet[1]&63u,(packet[1]>>8)&15u,packet[3]>>29,packet[4],uint64_t(packet[2])|(uint64_t(packet[3]&65535u)<<32)};
 if(prepared.command!=1 && prepared.command!=2)return 88;
 if(!prepared.address || (prepared.address&3u))return 60;
 output=prepared;return 0;
}
inline int complete_eos_writeback(const uint32_t* packet,size_t words,const CtrGuestCopyApi& memory,GnmCompletePriorGpuWork complete,void* context){
 GnmEosRequest eos;const int decoded=prepare_eos(packet,words,eos);if(decoded)return decoded;
 if(eos.command!=2)return 88;
 if(memory.version!=1 || memory.size!=sizeof(memory) || !memory.write)return 60;
 if(!complete)return 88;
 int status=112;try{status=complete(context);}catch(...){return 112;}if(status)return status;
 return memory.write(memory.context,eos.address,&eos.data,sizeof(eos.data));
}
inline int gnm_eos_completion_control(){
 GuestStack storage(32768,4096);auto* value=reinterpret_cast<uint64_t*>(storage.bottom());*value=0xabcdef0123456789ull;
 const auto address=reinterpret_cast<uint64_t>(value);const auto memory=make_owned_guest_copy_api();
 uint32_t packet[]={0xc0034800,0x62f,uint32_t(address),uint32_t(address>>32)|(2u<<29),0x76543210};
 struct State{int status=88;unsigned calls=0;};State state;
 const auto complete=+[](void* raw)->int{auto& s=*static_cast<State*>(raw);++s.calls;return s.status;};
 if(complete_eos_writeback(packet,5,memory,nullptr,nullptr)!=88 || *value!=0xabcdef0123456789ull)return 1;
 GnmCommandCursor cursor;if(cursor.capture(1,0,0,packet,5))return 2;
 auto execute=[&](uint64_t,uint32_t,uint32_t,const CtrPm4::Packet& p,const uint32_t* data){return complete_eos_writeback(data,p.words,memory,complete,&state);};
 if(cursor.resume(execute)!=88 || cursor.completed() || state.calls!=1 || *value!=0xabcdef0123456789ull)return 3;
 state.status=90;if(cursor.resume(execute)!=90 || cursor.completed() || *value!=0xabcdef0123456789ull)return 4;
 state.status=0;if(cursor.resume(execute) || cursor.completed()!=1 || *value!=0xabcdef0176543210ull)return 5;
 const auto calls=state.calls;packet[3]=uint32_t(address>>32)|(1u<<29);
 if(complete_eos_writeback(packet,5,memory,complete,&state)!=88 || state.calls!=calls)return 6;
 packet[3]=uint32_t(address>>32)|(2u<<29);packet[2]|=1;
 if(complete_eos_writeback(packet,5,memory,complete,&state)!=60 || state.calls!=calls)return 7;
 packet[2]&=~1u;packet[0]|=1;
 if(complete_eos_writeback(packet,5,memory,complete,&state)!=88 || state.calls!=calls)return 8;
 packet[0]&=~1u;packet[0]|=2u;if(complete_eos_writeback(packet,4,memory,complete,&state)!=60 || state.calls!=calls)return 9;
 auto throwing=+[](void*)->int{throw 1;};if(complete_eos_writeback(packet,5,memory,throwing,nullptr)!=112)return 10;
 std::cout<<"{\"scope\":\"gnm_eos_completion_control\",\"passed\":true,\"actual_owned_guest_write32\":true,\"pending_error_preserves_memory_cursor\":true,\"GDS_not_stubbed\":true,\"synthetic_completion_provider\":true,\"GPU_execution\":false}\n";return 0;
}
}
