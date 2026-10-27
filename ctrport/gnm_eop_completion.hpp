// SPDX-License-Identifier: GPL-2.0-only
#pragma once
#include "video/pm4_decode.hpp"
#include "owned_guest_copy.hpp"
namespace ctrport {
// CPU writeback half of public PM4CmdEventWriteEop::SignalFence and Liverpool
// EventWriteEop, revision e3ce810f3a653f43ac64ebab63023de281a4103a GPL-2.0-or-later.
// The synchronous completion provider must drain the real preceding GPU work
// and image downloads. Missing provider, interrupts and GPU clock/perf fail88.
// This helper is not a completion provider and does not invent fence success.
using GnmCompletePriorGpuWork=int(*)(void*);
// A queue fence alone never releases unpublished image generations. Counts are
// sampled synchronously on the GPU owner thread immediately before EOP; the
// caller retains dirty owners until every guest plane has been verified.
struct GnmEopImageGate {
 size_t color_dirty=0,depth_dirty=0;GnmCompletePriorGpuWork visibility=nullptr;void* context=nullptr;
 static int complete(void* raw){
  if(!raw)return 60;auto& gate=*static_cast<GnmEopImageGate*>(raw);
  if(gate.color_dirty || gate.depth_dirty || !gate.visibility)return 88;
  try{return gate.visibility(gate.context);}catch(...){return 112;}
 }
};
using GnmSignalGraphicsIrq=int(*)(void*);
// Whole bounded SignalFence data32/data64/none + noIRQ/IRQ-only/write-confirm
// transaction. Completion must finish GPU AND image downloads before success.
// Publication/IRQ failures poison the transaction: retry must not duplicate IRQ.
struct GnmEopTransaction {
 bool completed=false,poisoned=false;
 int commit(const uint32_t* packet,size_t words,const CtrGuestCopyApi& memory,
   GnmCompletePriorGpuWork complete,void* completion_context,GnmSignalGraphicsIrq signal,void* irq_context){
  if(completed || poisoned)return 91;CtrPm4::Eop eop{};const int decoded=CtrPm4::DecodeEop(packet,words,eop);if(decoded)return decoded;
  if((packet[0]&255u) || packet[1]!=0x504u || (packet[3]&~0xe300ffffu))return 88;
  if(!complete || (eop.interruptSelect && !signal))return 88;
  if(memory.size!=sizeof(memory) || memory.version!=1 || (eop.writeBytes && (!memory.read || !memory.write)))return 60;
  try{
   const int status=complete(completion_context);if(status)return status;
   poisoned=true;
   if(eop.writeBytes){
    const int written=memory.write(memory.context,eop.address,&eop.value,eop.writeBytes);if(written)return written;
    uint64_t observed=0;const int read=memory.read(memory.context,eop.address,&observed,eop.writeBytes);if(read)return read;
    const uint64_t expected=eop.writeBytes==4?uint64_t(uint32_t(eop.value)):eop.value;if(observed!=expected)return 112;
   }
   if(eop.interruptSelect){const int delivered=signal(irq_context);if(delivered)return delivered;}
   completed=true;poisoned=false;return 0;
  }catch(...){return 112;}
 }
};

}
