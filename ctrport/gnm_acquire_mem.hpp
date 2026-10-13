// SPDX-License-Identifier: GPL-2.0-only
#pragma once
#include "gnm_command_cursor.hpp"
namespace ctrport {
// Complete public PM4CmdAcquireMem field transport, pinned shadPS4
// e3ce810f3a653f43ac64ebab63023de281a4103a pm4_cmds.h (GPL-2.0-or-later).
// Upstream Liverpool ignores this packet; we do not promote that to real
// cache/fence semantics. A provider must implement the requested visibility.
struct GnmAcquireMemRequest {uint32_t control=0,size_low=0,size_high=0,base_low=0,base_high=0,poll_interval=0;};
inline int prepare_acquire_mem(const uint32_t* data,size_t words,GnmAcquireMemRequest& output){
 if(!data || words!=7)return 60;std::vector<CtrPm4::Packet> packets;
 const int status=CtrPm4::DecodePackets(data,words,packets);if(status)return status;
 if(packets.size()!=1 || packets[0].opcode!=0x58 || packets[0].words!=7)return 60;
 if(data[0]&255u)return 88;
 output={data[1],data[2],data[3],data[4],data[5],data[6]};return 0;
}
using GnmAcquireMemProvider=int(*)(void*,const GnmAcquireMemRequest&);
inline int complete_acquire_mem(const uint32_t* data,size_t words,GnmAcquireMemProvider provider,void* context){
 GnmAcquireMemRequest request;const int status=prepare_acquire_mem(data,words,request);if(status)return status;
 if(!provider)return 88;
 try{return provider(context,request);}catch(...){return 112;}
}
// Exact observed combinations of public Mesa gfx6.json CP_COHER_CNTL
// (MIT): CB/DB destination and action, TC/TCL1 action and TC writeback.
// Admission grants a conservative whole-queue memory barrier, never an image
// download, sampled-owner refresh, cache emulation, or guest publication.
inline bool acquire_queue_visibility_supported(const GnmAcquireMemRequest& r){
 if(r.size_high || r.base_high || r.poll_interval!=10)return false;
 // Reached second-submit word0 adds SH_KCACHE_ACTION_ENA (gfx6 bit27).
 // Translated constants are Vulkan buffer reads, covered by the same actual
 // ALL_COMMANDS memory barrier/fence. This grants no CPU image download.
 if(r.control==0x80c40000u || r.control==0x88c40000u)return r.size_low==1 && !r.base_low;
 // Original second-submit word12 requests all-address CB/DB, TC/TCL1,
 // shader constant/instruction visibility. Shader modules are immutable owned
 // translations; actual queue-wide barrier/fence covers their resource reads.
 if(r.control==0x2ec47fc0u)return r.size_low==UINT32_MAX && !r.base_low;
 if(!r.size_low || !r.base_low)return false;
 return r.control==0x82003fc0u || r.control==0x86007fc0u ||
        r.control==0x84c44000u || r.control==0x82c43fc0u;
}
inline int gnm_acquire_mem_control(){
 uint32_t packet[]={0xc0055800,0x80000040,0x12345678,0x9abcdef0,0x87654321,0x10203040,0x55};
 GnmAcquireMemRequest request;if(prepare_acquire_mem(packet,7,request) || request.control!=0x80000040 || request.size_low!=0x12345678 || request.size_high!=0x9abcdef0 || request.base_low!=0x87654321 || request.base_high!=0x10203040 || request.poll_interval!=0x55)return 1;
 if(complete_acquire_mem(packet,7,nullptr,nullptr)!=88)return 2;
 struct State {unsigned calls=0;int status=88;};State state;
 auto provider=+[](void* raw,const GnmAcquireMemRequest& r)->int{auto& s=*static_cast<State*>(raw);++s.calls;return r.control==0x80000040 && r.poll_interval==0x55?s.status:112;};
 GnmCommandCursor cursor;if(cursor.capture(1,0,0,packet,7))return 3;
 auto callback=[&](uint64_t,uint32_t,uint32_t,const CtrPm4::Packet& p,const uint32_t* data){return complete_acquire_mem(data,p.words,provider,&state);};
 if(cursor.resume(callback)!=88 || cursor.completed() || state.calls!=1)return 4;
 state.status=90;if(cursor.resume(callback)!=90 || cursor.completed() || state.calls!=2)return 5;
 // Explicitly synthetic provider success tests cursor transport, not a GPU barrier.
 state.status=0;if(cursor.resume(callback) || cursor.completed()!=1 || state.calls!=3)return 6;
 packet[0]|=1;if(complete_acquire_mem(packet,7,provider,&state)!=88 || state.calls!=3)return 7;
 packet[0]&=~1u;if(complete_acquire_mem(packet,6,provider,&state)!=60 || state.calls!=3)return 8;
 auto throwing=+[](void*,const GnmAcquireMemRequest&)->int{throw 1;};if(complete_acquire_mem(packet,7,throwing,nullptr)!=112)return 9;
 for(const auto control:{0x82003fc0u,0x86007fc0u,0x84c44000u,0x82c43fc0u}){
  GnmAcquireMemRequest r{control,32640,0,285942784,0,10};if(!acquire_queue_visibility_supported(r))return 10;
  r.size_high=1;if(acquire_queue_visibility_supported(r))return 11;r.size_high=0;
  r.base_high=1;if(acquire_queue_visibility_supported(r))return 12;r.base_high=0;
  r.poll_interval=11;if(acquire_queue_visibility_supported(r))return 13;r.poll_interval=10;
  r.control^=1u<<27;if(acquire_queue_visibility_supported(r))return 14;
 }
 GnmAcquireMemRequest constants{0x88c40000u,1,0,0,0,10};
 if(!acquire_queue_visibility_supported(constants))return 15;
 constants.size_low=2;if(acquire_queue_visibility_supported(constants))return 16;
 constants.size_low=1;constants.base_low=1;if(acquire_queue_visibility_supported(constants))return 17;
 constants.base_low=0;constants.control|=1u<<29;if(acquire_queue_visibility_supported(constants))return 18;
 GnmAcquireMemRequest global{0x2ec47fc0u,UINT32_MAX,0,0,0,10};
 if(!acquire_queue_visibility_supported(global))return 19;
 global.size_low=1;if(acquire_queue_visibility_supported(global))return 20;
 global.size_low=UINT32_MAX;global.base_low=1;if(acquire_queue_visibility_supported(global))return 21;
 global.base_low=0;global.control|=1u<<28;if(acquire_queue_visibility_supported(global))return 22;
 std::cout<<"{\"scope\":\"gnm_acquire_mem_control\",\"passed\":true,\"all_raw_fields_preserved\":true,\"missing_provider_pending\":true,\"pending_error_keeps_cursor\":true,\"synthetic_provider_only\":true,\"GPU_execution\":false}\n";return 0;
}
}
