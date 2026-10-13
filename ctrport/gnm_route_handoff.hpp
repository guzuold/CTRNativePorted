// SPDX-License-Identifier: GPL-2.0-only
#pragma once
#include "graphics_companion_client.hpp"
namespace ctrport {
// Validate the live ordered bank against its retained packet-specific compiler
// inputs before a whole GPU handler may run. A future diagnostic route cannot
// be used with the wrong word, command contents or current register state.
// Callback zero means the WHOLE original packet has completed, not submission
// of asynchronous work; the callback must satisfy its own fence/writeback rules.
template<class Handler> int execute_owned_route(const GraphicsSubmissionOwner& owner,
 const CtrPm4::Packet& packet,const uint32_t* data,const CtrPm4::Registers& current,Handler&& handler){
 if(owner.status)return owner.status;
 if(!data)return 60;
 const GraphicsSubmissionOwner::Route* found=nullptr;
 for(const auto& route:owner.routes)if(route.word==packet.word){if(found)return 60;found=&route;}
 if(!found)return 88;
 if(found->opcode!=packet.opcode || found->packet.size()!=packet.words || !found->registers)return 112;
 if(!std::equal(found->packet.begin(),found->packet.end(),data) ||
    found->registers->words!=current.words || found->registers->compute!=current.compute)return 112;
 try{return handler(*found);}catch(...){return 112;}
}
inline int gnm_route_handoff_control(){
 auto bank=std::make_unique<CtrPm4::Registers>();GraphicsSubmissionOwner owner;
 uint32_t draw[]={0xc0012d00,3,2};if(owner.retain_route(255,0x2d,draw,3,*bank))return 1;
 unsigned calls=0;auto pending=[&](const GraphicsSubmissionOwner::Route& r){++calls;return r.word==255?88:112;};
 const CtrPm4::Packet packet{255,3,0x2d};
 if(execute_owned_route(owner,packet,draw,*bank,pending)!=88 || calls!=1)return 2;
 bank->compute[7]=1;if(execute_owned_route(owner,packet,draw,*bank,pending)!=112 || calls!=1)return 3;bank->compute[7]=0;
 bank->words[0xa001]=7;if(execute_owned_route(owner,packet,draw,*bank,pending)!=112 || calls!=1)return 4;bank->words[0xa001]=0;
 draw[1]=4;if(execute_owned_route(owner,packet,draw,*bank,pending)!=112 || calls!=1)return 5;draw[1]=3;
 if(execute_owned_route(owner,{256,3,0x2d},draw,*bank,pending)!=88 || calls!=1)return 6;
 auto throwing=[](const GraphicsSubmissionOwner::Route&)->int{throw 1;};
 if(execute_owned_route(owner,packet,draw,*bank,throwing)!=112)return 7;
 owner.routes.push_back(owner.routes.front());if(execute_owned_route(owner,packet,draw,*bank,pending)!=60 || calls!=1)return 8;
 std::cout<<"{\"scope\":\"gnm_route_handoff_control\",\"passed\":true,\"whole_bank_identity\":true,\"wrong_packet_rejected\":true,\"duplicate_rejected\":true,\"exception_guarded\":true,\"synthetic\":true,\"GPU_execution\":false}\n";return 0;
}
}
