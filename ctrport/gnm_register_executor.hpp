// SPDX-License-Identifier: GPL-2.0-only
#pragma once
#include "pm4_observed_state.hpp"
#include "draw_packet_inputs.hpp"
#include "gnm_command_cursor.hpp"
#include "dispatch_packet_inputs.hpp"
namespace ctrport {
// Software register-file effects only, from the public pinned PM4 handlers
// cited in pm4_observed_state.hpp and video/pm4_init_plan.hpp. This state belongs
// to one ordered command cursor; it is not the diagnostic future-draw bank.
// Ordinary NOP padding is consumed; proprietary hint markers,
// DMA, synchronization, GPU operations and completion remain pending.
class GnmRegisterExecutor {
 std::unique_ptr<CtrPm4::Registers> state_;
 size_t applied_=0;
public:
 explicit GnmRegisterExecutor(const CtrPm4::Registers& initial):state_(std::make_unique<CtrPm4::Registers>(initial)){}
 int execute(const CtrPm4::Packet& packet,const uint32_t* data){
  if(!data)return 60;
  // Whole public ProcessGraphics type2 branch: one padding DWORD, no effects.
  // liverpool.cpp:250, pinned e3ce810f3a653f43ac64ebab63023de281a4103a.
  if(packet.words==1 && packet.opcode==258 && (data[0]>>30)==2){++applied_;return 0;}
  if(packet.words<2)return 60;
  std::vector<CtrPm4::Packet> decoded;const int d=CtrPm4::DecodePackets(data,packet.words,decoded);if(d)return d;
  if(decoded.size()!=1 || decoded[0].opcode!=packet.opcode || decoded[0].words!=packet.words)return 60;
  if(data[0]&253u)return 88; // Shader-type bit1 is valid on compute register packets.
  // Whole public Liverpool graphics NOP handler returns immediately when
  // header.count==0 (two DWORDs), before interpreting even marker-valued payload.
  if(packet.opcode==0x10){
   // Complete public NOP switch default for resource descriptor hints and DbZInfo.
   // Their metadata is already interpreted during register capture; CP has no
   // execution effect. Flip/prepare/debug markers still require their handlers.
   // Whole public Liverpool NOP debug branches, liverpool.cpp:271-297,
   // e3ce810f3a653f43ac64ebab63023de281a4103a, GPL-2.0-or-later.
   // Runtime policy disables optional Vulkan guest debugging markers. Public
   // false branch reads no label/color and changes no GPU or register state.
   const bool disabled_debug_marker=data[1]==0x68750001u || data[1]==0x68750002u || data[1]==0x6875000eu;
   if(disabled_debug_marker){++applied_;return 0;}
   const bool metadata_hint=data[1]==0x68750004u || data[1]==0x68750005u || data[1]==0x68750006u || data[1]==0x68753000u || data[1]==0x68752000u;
   if(packet.words>2 && (data[1]&0xffff0000)==0x68750000 && !metadata_hint)return 88;++applied_;return 0;
  }
  // Whole pinned Liverpool graphics DMA handler's explicit destination rule
  // (liverpool.cpp, DmaData case): dst_addr_lo==0x3022C returns before any
  // rasterizer transfer. This is public handler behavior, not a missing-DMA
  // success fallback. Every other destination remains unsupported here.
  if(packet.opcode==0x50){if(packet.words!=7)return 60;if(data[4]!=0x3022c)return 88;++applied_;return 0;}
  // Public PM4 ContextControlEnable (pm4_cmds.h): bit31 enables the
  // control DWORD; all load/shadow register classes are disabled here.
  // This cursor owns an explicit register bank, with no hardware context
  // shadow backing. Disabling both mechanisms preserves that bank exactly.
  // Enabling any class requires a real shadow implementation and stays pending.
  if(packet.opcode==0x28){
   if(packet.words!=3)return 60;
   if(data[1]!=0x80000000u || data[2]!=0x80000000u)return 88;
   ++applied_;return 0;
  }
  switch(packet.opcode){case 0x68:case 0x69:case 0x76:case 0x79:case 0x2a:case 0x2f:case 0x12:break;default:return 88;}
  CtrPm4::InitOperation operation{packet.opcode,std::vector<uint32_t>(data+1,data+packet.words)};
  // Validate complete payload before committing any register write.
  const int status=observe_pm4_register(operation,*state_);if(status)return status;
  ++applied_;return 0;
 }
 template<class Handler> int execute_draw(const uint32_t* data,size_t words,Handler&& handler){
  auto prepared=std::make_unique<CtrPm4::Registers>();const int status=prepare_draw_execution_bank(data,words,*state_,*prepared);if(status)return status;
  const int completed=handler(*prepared);if(completed)return completed;
  state_.swap(prepared);++applied_;return 0;
 }
 // The whole synchronous dispatch provider must include its writeback/fence.
 // Pending/error leaves original registers intact; never commit lookahead inputs.
 template<class Handler> int execute_dispatch(const uint32_t* data,size_t words,Handler&& handler){
  auto prepared=std::make_unique<CtrPm4::Registers>();const int status=prepare_dispatch_execution_bank(data,words,*state_,*prepared);if(status)return status;
  const int completed=handler(*prepared);if(completed)return completed;
  state_.swap(prepared);++applied_;return 0;
 }
 const CtrPm4::Registers& state()const{return *state_;}
 size_t applied()const{return applied_;}
};
}
