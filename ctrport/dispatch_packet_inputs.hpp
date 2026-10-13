// SPDX-License-Identifier: GPL-2.0-only
#pragma once
#include <memory>
#include "video/pm4_registers.hpp"
#include "video/pm4_decode.hpp"
namespace ctrport {
// Complete public PM4CmdDispatchDirect field transport, shadPS4 revision
// e3ce810f3a653f43ac64ebab63023de281a4103a, GPL-2.0-or-later.
// This does not execute a dispatch or change COMPUTE_DISPATCH_INITIATOR.
struct DispatchPacketInputs {std::array<uint32_t,3> groups{};uint32_t initiator=0;bool compute=false;};
inline int prepare_dispatch_packet(const uint32_t* data,size_t words,DispatchPacketInputs& output){
 if(!data || words!=5)return 60;
 CtrPm4::DispatchDirect decoded;const int status=CtrPm4::DecodeDispatchDirect(data,words,decoded);if(status)return status;
 if(decoded.predicate)return 88;
 DispatchPacketInputs prepared{{decoded.x,decoded.y,decoded.z},decoded.initiator,decoded.compute};output=prepared;return 0;
}
// Whole public Liverpool DispatchDirect CPU assignments, regs_shader.h ComputeProgram.
// Private metadata bank only; no GPU dispatch or completion implied.
inline int prepare_dispatch_execution_bank(const uint32_t* packet,size_t words,const CtrPm4::Registers& before,CtrPm4::Registers& output){
 DispatchPacketInputs dispatch;const int status=prepare_dispatch_packet(packet,words,dispatch);if(status)return status;
 auto result=std::make_unique<CtrPm4::Registers>(before);result->compute[0]=dispatch.initiator;
 result->compute[1]=dispatch.groups[0];result->compute[2]=dispatch.groups[1];result->compute[3]=dispatch.groups[2];
 output=*result;return 0;
}
inline int dispatch_packet_inputs_control(){
 uint32_t packet[]={0xc0031502,2,3,4,0x80000001};DispatchPacketInputs output;
 if(prepare_dispatch_packet(packet,5,output) || output.groups!=std::array<uint32_t,3>{2,3,4} || output.initiator!=0x80000001 || !output.compute)return 1;
 packet[0]|=1;if(prepare_dispatch_packet(packet,5,output)!=88 || output.groups[0]!=2)return 2;packet[0]&=~1u;
 if(prepare_dispatch_packet(packet,4,output)!=60 || output.initiator!=0x80000001)return 3;
 packet[0]=0xc0031602;if(prepare_dispatch_packet(packet,5,output)!=60)return 4;
 packet[0]=0xc0031500;packet[1]=0;if(prepare_dispatch_packet(packet,5,output) || output.groups[0]!=0 || output.compute)return 5;
 auto bank=std::make_unique<CtrPm4::Registers>();auto prepared=std::make_unique<CtrPm4::Registers>();bank->words[7]=123;bank->compute[4]=99;
 if(prepare_dispatch_execution_bank(packet,5,*bank,*prepared) || prepared->compute[0]!=0x80000001 || prepared->compute[1] || prepared->compute[2]!=3 || prepared->compute[3]!=4 || prepared->compute[4]!=99 || prepared->words!=bank->words)return 6;
 const auto saved=prepared->compute;packet[0]|=1;if(prepare_dispatch_execution_bank(packet,5,*bank,*prepared)!=88 || prepared->compute!=saved || bank->compute[0])return 7;
 std::cout<<"{\"scope\":\"dispatch_packet_inputs_control\",\"passed\":true,\"raw_initiator_preserved\":true,\"shader_type_preserved\":true,\"zero_group_preserved\":true,\"failure_atomic\":true,\"synthetic\":true,\"GPU_execution\":false}\n";return 0;
}
}
