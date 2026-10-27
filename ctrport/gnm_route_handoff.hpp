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
}
