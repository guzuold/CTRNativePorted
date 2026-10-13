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
inline int complete_eop_writeback(const uint32_t* packet,size_t words,const CtrGuestCopyApi& memory,
 GnmCompletePriorGpuWork complete,void* context){
 CtrPm4::Eop eop{};const int decoded=CtrPm4::DecodeEop(packet,words,eop);if(decoded)return decoded;
 if(packet[0]&255u)return 88;
 // Public fields cover event type/index,48-bit address,interrupt/data select.
 // Unhandled cache actions/reserved bits must not silently signal completion.
 if(packet[1]!=0x504u || (packet[3]&~0xe300ffffu))return 88;
 if(eop.interruptSelect)return 88;
 if(memory.size!=sizeof(memory) || memory.version!=1 || !memory.write)return 60;
 if(!complete)return 88;
 int status=112;try{status=complete(context);}catch(...){return 112;}if(status)return status;
 if(!eop.writeBytes)return 0;
 return memory.write(memory.context,eop.address,&eop.value,eop.writeBytes);
}
inline int gnm_eop_completion_control(){
 GuestStack storage(32768,4096);const auto address=reinterpret_cast<uint64_t>(storage.bottom());
 auto* value=reinterpret_cast<uint64_t*>(address);*value=0xa5a5a5a5a5a5a5a5ull;
 uint32_t packet[]={0xc0044700,0x504,uint32_t(address),uint32_t(address>>32)|(1u<<29),0x12345678,0xffffffff};
 const auto memory=make_owned_guest_copy_api();
 struct State{unsigned calls=0;int status=88;};State state;
 const auto fence=+[](void* raw)->int{auto& s=*static_cast<State*>(raw);++s.calls;return s.status;};
 if(complete_eop_writeback(packet,6,memory,nullptr,nullptr)!=88 || *value!=0xa5a5a5a5a5a5a5a5ull)return 1;
 if(complete_eop_writeback(packet,6,memory,fence,&state)!=88 || state.calls!=1 || *value!=0xa5a5a5a5a5a5a5a5ull)return 2;
 state.status=0;if(complete_eop_writeback(packet,6,memory,fence,&state) || *value!=0xa5a5a5a512345678ull || state.calls!=2)return 3;
 packet[3]=(uint32_t(address>>32))|(2u<<29);packet[5]=0x87654321;
 if(complete_eop_writeback(packet,6,memory,fence,&state) || *value!=0x8765432112345678ull)return 4;
 const auto before=*value;const auto calls=state.calls;
 packet[3]|=2u<<24;if(complete_eop_writeback(packet,6,memory,fence,&state)!=88 || state.calls!=calls || *value!=before)return 5;
 packet[3]=(uint32_t(address>>32))|(3u<<29);if(complete_eop_writeback(packet,6,memory,fence,&state)!=88 || state.calls!=calls || *value!=before)return 6;
 packet[3]=(uint32_t(address>>32))|(2u<<29);packet[2]|=1;if(complete_eop_writeback(packet,6,memory,fence,&state)!=60 || state.calls!=calls || *value!=before)return 7;
 packet[2]&=~3u;packet[3]=uint32_t(address>>32)|(2u<<29);packet[1]|=1u<<16;
 if(complete_eop_writeback(packet,6,memory,fence,&state)!=88 || state.calls!=calls || *value!=before)return 8;
 packet[1]=0x504;packet[3]|=1u<<16;
 if(complete_eop_writeback(packet,6,memory,fence,&state)!=88 || state.calls!=calls || *value!=before)return 9;
 std::cout<<"{\"scope\":\"gnm_eop_completion_control\",\"passed\":true,\"actual_owned_guest_writes\":true,\"pending_fence_preserves_memory\":true,\"unsupported_preflight_atomic\":true,\"synthetic_completion_provider\":true,\"GPU_execution\":false}\n";return 0;
}
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
inline int gnm_eop_transaction_control(){
 GuestStack stack(32768,4096);auto* value=reinterpret_cast<uint64_t*>(stack.bottom());*value=0;
 const auto address=reinterpret_cast<uint64_t>(value);const auto memory=make_owned_guest_copy_api();
 uint32_t packet[]={0xc0044700,0x504,uint32_t(address),uint32_t(address>>32)|(2u<<29)|(2u<<24),0x12345678,0x87654321};
 struct State{int completion=88,irq=0;unsigned finishes=0,signals=0;uint64_t* label;};State state{88,0,0,0,value};
 const auto finish=+[](void* raw)->int{auto& s=*static_cast<State*>(raw);++s.finishes;return s.completion;};
 const auto signal=+[](void* raw)->int{auto& s=*static_cast<State*>(raw);++s.signals;if(*s.label!=0x8765432112345678ull)return 112;return s.irq;};
 GnmEopImageGate gate{0,2,finish,&state};GnmEopTransaction guarded;
 if(guarded.commit(packet,6,memory,GnmEopImageGate::complete,&gate,signal,&state)!=88 || state.finishes || state.signals || *value || guarded.poisoned)return 7;
 gate.depth_dirty=0;gate.color_dirty=1;
 if(guarded.commit(packet,6,memory,GnmEopImageGate::complete,&gate,signal,&state)!=88 || state.finishes || state.signals || *value)return 8;
 gate.color_dirty=0;gate.visibility=nullptr;
 if(guarded.commit(packet,6,memory,GnmEopImageGate::complete,&gate,signal,&state)!=88 || state.finishes || *value)return 9;
 gate.visibility=finish;packet[1]=0x505u;
 if(guarded.commit(packet,6,memory,GnmEopImageGate::complete,&gate,signal,&state)!=88 || state.finishes || state.signals || *value)return 10;
 packet[1]=0x504u;
 GnmEopTransaction owner;
 if(owner.commit(packet,6,memory,finish,&state,nullptr,nullptr)!=88 || state.finishes || *value)return 1;
 if(owner.commit(packet,6,memory,finish,&state,signal,&state)!=88 || owner.poisoned || owner.completed || state.signals || *value)return 2;
 state.completion=0;if(owner.commit(packet,6,memory,finish,&state,signal,&state) || !owner.completed || owner.poisoned || state.signals!=1)return 3;
 if(owner.commit(packet,6,memory,finish,&state,signal,&state)!=91 || state.signals!=1)return 4;
 GnmEopTransaction failed;state.irq=62;
 if(failed.commit(packet,6,memory,finish,&state,signal,&state)!=62 || !failed.poisoned || failed.completed)return 5;
 const auto calls=state.finishes,signals=state.signals;
 if(failed.commit(packet,6,memory,finish,&state,signal,&state)!=91 || state.finishes!=calls || state.signals!=signals)return 6;
 // A guest write can succeed while readback fails or another CPU writer
 // changes the label. Neither case may deliver an IRQ or permit a retry.
 struct Fault{CtrGuestCopyApi real;bool mismatch=false;}fault{memory};auto faulty=memory;faulty.context=&fault;
 faulty.write=+[](void* raw,uint64_t address,const void* source,uint64_t bytes)->int{auto& f=*static_cast<Fault*>(raw);return f.real.write(f.real.context,address,source,bytes);};
 faulty.read=+[](void* raw,uint64_t address,void* target,uint64_t bytes)->int{auto& f=*static_cast<Fault*>(raw);if(!f.mismatch)return 62;const int status=f.real.read(f.real.context,address,target,bytes);if(!status)static_cast<unsigned char*>(target)[0]^=1;return status;};
 state.irq=0;
 for(bool mismatch:{false,true}){fault.mismatch=mismatch;GnmEopTransaction verification;const auto before_signals=state.signals;
  if(verification.commit(packet,6,faulty,finish,&state,signal,&state)!=(mismatch?112:62) || !verification.poisoned || verification.completed || state.signals!=before_signals)return 11;
  const auto before_finishes=state.finishes;
  if(verification.commit(packet,6,memory,finish,&state,signal,&state)!=91 || state.finishes!=before_finishes || state.signals!=before_signals)return 12;
 }
 std::cout<<"{\"scope\":\"gnm_EOP_transaction_control\",\"passed\":true,\"write_verified_before_IRQ\":true,\"dirty_images_block_completion\":true,\"unknown_event_rejected_before_provider\":true,\"readback_failure_blocks_IRQ_and_retry\":true,\"pending_preserves_memory\":true,\"failed_IRQ_poisoned\":true,\"duplicate_signal_prevented\":true,\"synthetic_completion_and_IRQ\":true,\"runtime_bound\":false}\n";return 0;
}

}
