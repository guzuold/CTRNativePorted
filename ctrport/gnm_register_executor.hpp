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
// Authored integration fixture for the newly connected DrawIndex2 transaction.
// Providers are synthetic; this verifies cursor/register commit, not GPU output.
inline int indexed_ordered_transaction_control(){
 auto initial=std::make_unique<CtrPm4::Registers>();initial->words.fill(0xa5a5a5a5);initial->compute[7]=123;
 GnmRegisterExecutor executor(*initial);GnmCommandCursor cursor;
 const uint32_t packet[]={0xc0042700,102,0x1235,0x20,102,0};
 if(cursor.capture(7,0,0,packet,6))return 1;
 unsigned calls=0;int provider_status=88;
 const auto handler=[&](uint64_t,uint32_t,uint32_t,const CtrPm4::Packet& p,const uint32_t* data){
  if(p.opcode!=0x27)return 112;
  return executor.execute_draw(data,p.words,[&](const CtrPm4::Registers& prepared){
   ++calls;if(prepared.words[0xA29E]!=102 || prepared.words[0xA1F9]!=0x1235 || prepared.words[0xA1FA]!=0x20 || prepared.words[0xC24C]!=102 || prepared.words[0xA1FC] || prepared.compute!=initial->compute)return 112;
   return provider_status;
  });
 };
 for(int status:{88,90,112}){provider_status=status;if(cursor.resume(handler)!=status || cursor.completed() || !cursor.pending() || executor.applied() || executor.state().words!=initial->words || executor.state().compute!=initial->compute)return 2;}
 provider_status=0;if(cursor.resume(handler) || cursor.completed()!=1 || cursor.pending() || executor.applied()!=1 || calls!=4 || executor.state().words[0xC24C]!=102)return 3;
 if(cursor.resume(handler) || calls!=4 || executor.applied()!=1)return 4;
 std::cout<<"{\"scope\":\"indexed_ordered_transaction_control\",\"passed\":true,\"failure_statuses\":[88,90,112],\"indexed_register_fields_preserved\":true,\"commit_once\":true,\"synthetic_provider\":true,\"GPU_execution\":false}\n";return 0;
}
inline int gnm_register_executor_control(){
 auto initial=std::make_unique<CtrPm4::Registers>();initial->words.fill(0xa5a5a5a5);
 GnmRegisterExecutor executor(*initial);
 const uint32_t context[]={0xc0026900,0xd,7,9};
 if(executor.execute({0,4,0x69},context) || executor.state().words[0xa00d]!=7 || executor.state().words[0xa00e]!=9 || executor.state().words[0xa00f]!=0xa5a5a5a5)return 1;
 const uint32_t malformed[]={0xc0026900,0xfff,3,4};
 if(executor.execute({4,4,0x69},malformed)!=60 || executor.applied()!=1 || executor.state().words[0xafff]!=0xa5a5a5a5)return 2;
 const uint32_t predicated[]={0xc0016901,0xd,11};
 if(executor.execute({4,3,0x69},predicated)!=88 || executor.state().words[0xa00d]!=7)return 3;
 const uint32_t compute[]={0xc0017602,0x216,21};
 if(executor.execute({4,3,0x76},compute) || executor.state().compute[22]!=21 || executor.state().words[0x2e16]!=0xa5a5a5a5)return 4;
 const uint32_t barrier[]={0xc0005800,0};
 if(executor.execute({4,2,0x58},barrier)!=88 || executor.applied()!=2)return 5;
 const uint32_t nop[]={0xc0001000,0},hint[]={0xc0011000,0x68750001,0};
 if(executor.execute({5,2,0x10},nop) || executor.applied()!=3 || GnmRegisterExecutor(*initial).execute({7,3,0x10},hint) || executor.applied()!=3)return 6;
 const uint32_t long_padding[]={0xc0021000,0,17,19};
 if(executor.execute({9,4,0x10},long_padding) || executor.applied()!=4)return 7;
 const uint32_t marker_padding[]={0xc0001000,0x68750004};
 if(executor.execute({13,2,0x10},marker_padding) || executor.applied()!=5)return 8;
 const uint32_t dma_ignored[]={0xc0055000,0x60000000,0x1234,0x10,0x3022c,0,188};
 if(executor.execute({15,7,0x50},dma_ignored) || executor.applied()!=6)return 9;
 uint32_t dma_other[7];std::copy(std::begin(dma_ignored),std::end(dma_ignored),dma_other);dma_other[4]=0x30230;
 if(executor.execute({22,7,0x50},dma_other)!=88 || executor.applied()!=6)return 10;
 if(executor.execute({22,6,0x50},dma_other)!=60 || executor.applied()!=6)return 11;
 const uint32_t draw[]={0xc0012d00,3,2};const auto previous=executor.state().words;unsigned calls=0;
 if(executor.execute_draw(draw,3,[&](const CtrPm4::Registers& prepared){++calls;return prepared.words[0xC24C]==3?88:112;})!=88 || executor.state().words!=previous || executor.applied()!=6 || calls!=1)return 12;
 if(executor.execute_draw(draw,3,[&](const CtrPm4::Registers&){++calls;return 0;}) || executor.state().words[0xC24C]!=3 || executor.state().words[0xA1FC]!=2 || executor.applied()!=7 || calls!=2)return 13;
 const uint32_t dispatch[]={0xc0031502,240,135,1,0x80000001};const auto before_dispatch=executor.state().compute;const auto before_graphics=executor.state().words;
 if(executor.execute_dispatch(dispatch,5,[&](const CtrPm4::Registers& prepared){++calls;return prepared.compute[0]==0x80000001 && prepared.compute[1]==240 && prepared.compute[2]==135 && prepared.compute[3]==1 && prepared.words==before_graphics?88:112;})!=88 || executor.state().compute!=before_dispatch || executor.applied()!=7 || calls!=3)return 14;
 if(executor.execute_dispatch(dispatch,5,[&](const CtrPm4::Registers&){++calls;return 90;})!=90 || executor.state().compute!=before_dispatch || executor.applied()!=7 || calls!=4)return 15;
 if(executor.execute_dispatch(dispatch,5,[&](const CtrPm4::Registers&){++calls;return 0;}) || executor.state().compute[0]!=0x80000001 || executor.state().compute[1]!=240 || executor.state().compute[2]!=135 || executor.state().compute[3]!=1 || executor.state().words!=before_graphics || executor.applied()!=8 || calls!=5)return 16;
 const auto completed_compute=executor.state().compute;
 if(executor.execute_dispatch(dispatch,4,[&](const CtrPm4::Registers&){++calls;return 0;})!=60 || executor.state().compute!=completed_compute || calls!=5)return 17;
 uint32_t predicated_dispatch[5];std::copy(std::begin(dispatch),std::end(dispatch),predicated_dispatch);predicated_dispatch[0]|=1;
 if(executor.execute_dispatch(predicated_dispatch,5,[&](const CtrPm4::Registers&){++calls;return 0;})!=88 || executor.state().compute!=completed_compute || calls!=5)return 18;
 GnmRegisterExecutor hints(*initial);uint32_t metadata_hint[]={0xc0011000,0,0};
 for(uint32_t marker:{0x68750004u,0x68750005u,0x68750006u,0x68753000u,0x68752000u}){metadata_hint[1]=marker;if(hints.execute({0,3,0x10},metadata_hint))return 19;}
 if(hints.applied()!=5 || hints.state().words!=initial->words || hints.state().compute!=initial->compute)return 20;
 metadata_hint[1]=0x68750776u;if(hints.execute({0,3,0x10},metadata_hint)!=88 || hints.applied()!=5)return 21;
 GnmRegisterExecutor padding(*initial);const uint32_t type2[]={0x80000000u};
 if(padding.execute({0,1,258},type2) || padding.applied()!=1 || padding.state().words!=initial->words || padding.state().compute!=initial->compute)return 21;
 if(padding.execute({0,1,0x10},type2)!=60 || padding.applied()!=1)return 22;
 GnmRegisterExecutor debug(*initial);uint32_t marker[]={0xc00f1000,0x68750001u,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0};
 for(uint32_t value:{0x68750001u,0x68750002u,0x6875000eu}){marker[1]=value;if(debug.execute({0,17,0x10},marker))return 23;}
 if(debug.applied()!=3 || debug.state().words!=initial->words || debug.state().compute!=initial->compute)return 24;
 for(uint32_t value:{0x68750776u,0x68750777u,0x68750778u,0x68750780u,0x68750781u}){marker[1]=value;if(debug.execute({0,17,0x10},marker)!=88 || debug.applied()!=3)return 25;}
 GnmRegisterExecutor context_control(*initial);
 uint32_t disable_shadow[]={0xc0012800,0x80000000u,0x80000000u};
 if(context_control.execute({0,3,0x28},disable_shadow) || context_control.applied()!=1 || context_control.state().words!=initial->words || context_control.state().compute!=initial->compute)return 26;
 for(unsigned field:{1u,2u})for(uint32_t bit:{1u,2u,0x8000u,0x10000u,0x1000000u,0x40000000u}){
  disable_shadow[field]|=bit;
  if(context_control.execute({0,3,0x28},disable_shadow)!=88 || context_control.applied()!=1 || context_control.state().words!=initial->words)return 27;
  disable_shadow[field]&=~bit;
 }
 if(context_control.execute({0,2,0x28},disable_shadow)!=60 || context_control.applied()!=1)return 28;
 // ClearState uses the complete pinned public register defaults, not zero
 // initialization. Compute SH bank is separate and must survive this packet.
 GnmRegisterExecutor clear(*initial);uint32_t clear_packet[]={0xc0001200,1};
 if(clear.execute({0,2,0x12},clear_packet)!=60 || clear.applied() || clear.state().words!=initial->words)return 29;
 clear_packet[1]=0;
 if(clear.execute({0,2,0x12},clear_packet) || clear.applied()!=1 || clear.state().words[0xa00d]!=0x40004000u || clear.state().words[0xa1b6]!=2 || clear.state().words[0xa204]!=0x90000u || clear.state().words[0xa205]!=4 || clear.state().words[0x2000] || clear.state().compute!=initial->compute)return 30;
 std::cout<<"{\"scope\":\"authored_ordered_register_executor\",\"passed\":true,\"writes\":2,\"invalid_atomic\":true,\"predicate_and_barrier_pending\":true,\"dispatch_provider_pending_atomic\":true,\"synthetic\":true,\"GPU_execution\":false}\n";return 0;
}
}
