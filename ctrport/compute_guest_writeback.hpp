// SPDX-License-Identifier: GPL-2.0-only
#pragma once
#include "graphics_companion_client.hpp"
#include "guest_copy_api.h"
namespace ctrport {
// Provider must finish the real GPU writer, download, and retile using the
// public format/layout path, preserving initial padding. Returning success is
// forbidden while any of those operations remains unsupported.
using ComputeTiledDownload=int(*)(void*,const unsigned char*,uint64_t,unsigned char*,uint64_t);
// Image generations can exceed the guest-copy ABI's 16 MiB call bound
// (retail1426 sampled Z32 MS2 spans17694720B). Bound the whole image at32MiB,
// then use8MiB calls; the transaction still validates the whole generation.
inline int image_guest_copy(const CtrGuestCopyApi& memory,uint64_t address,void* buffer,uint64_t bytes,bool write){
 if(memory.version!=1 || memory.size!=sizeof(memory) || !memory.read || !memory.write || !address || !buffer || !bytes || bytes>32*1024*1024 || address>UINT64_MAX-bytes)return 60;
 const auto host=reinterpret_cast<uintptr_t>(buffer);
 // Reject whole-span aliasing before the first chunk. Per-call checks alone
 // could publish an earlier chunk before a later shifted overlap is detected.
 if(host>UINTPTR_MAX-bytes || (host<address+bytes && address<host+bytes))return 60;
 auto* data=static_cast<unsigned char*>(buffer);
 for(uint64_t offset=0;offset<bytes;){const uint64_t count=std::min<uint64_t>(bytes-offset,8*1024*1024);const int status=write?memory.write(memory.context,address+offset,data+offset,count):memory.read(memory.context,address+offset,data+offset,count);if(status)return status;offset+=count;}
 return 0;
}
struct ComputeGuestWriteback {
 uint64_t address=0;std::vector<unsigned char> initial;bool completed=false,poisoned=false;
 int commit(const CtrGuestCopyApi& memory,ComputeTiledDownload download,void* context){
  if(completed || poisoned)return 91;
  if(memory.version!=1 || memory.size!=sizeof(memory) || !memory.read || !memory.write || !address || initial.empty() || initial.size()>32*1024*1024 || address>UINT64_MAX-initial.size())return 60;
  if(!download)return 88;
  try{
   std::vector<unsigned char> current(initial.size()),result(initial.size());
   int status=image_guest_copy(memory,address,current.data(),current.size(),false);if(status)return status;
   if(current!=initial)return 88; // Concurrent CPU change: do not overwrite it.
   status=download(context,initial.data(),initial.size(),result.data(),result.size());if(status)return status;
   status=image_guest_copy(memory,address,current.data(),current.size(),false);if(status)return status;
   if(current!=initial)return 88;
   poisoned=true; // Includes an exception after publication begins.
   status=image_guest_copy(memory,address,result.data(),result.size(),true);
   if(status){poisoned=true;return status;}
   status=image_guest_copy(memory,address,current.data(),current.size(),false);
   if(status || current!=result){poisoned=true;return status?status:112;}
   initial.swap(result);completed=true;poisoned=false;return 0;
  }catch(...){return 112;}
 }
};
inline int compute_guest_writeback_control(){
 GuestStack allocation(32768,4096);auto* bytes=reinterpret_cast<unsigned char*>(allocation.bottom());
 std::fill_n(bytes,64,static_cast<unsigned char>(0x25));ComputeGuestWriteback owner;owner.address=reinterpret_cast<uint64_t>(bytes);owner.initial.assign(bytes,bytes+64);
 const auto memory=make_owned_guest_copy_api();
 struct Provider{int status=88;unsigned calls=0;unsigned char* race=nullptr;} state;
 const auto download=+[](void* raw,const unsigned char* initial,uint64_t length,unsigned char* output,uint64_t capacity)->int{
  auto& provider=*static_cast<Provider*>(raw);++provider.calls;if(provider.status)return provider.status;
  if(length!=64 || capacity!=length)return 60;std::copy_n(initial,64,output);std::fill_n(output,48,static_cast<unsigned char>(0x72));if(provider.race)*provider.race=0x66;return 0;
 };
 if(owner.commit(memory,nullptr,nullptr)!=88 || owner.completed || owner.poisoned)return 1;
 if(owner.commit(memory,download,&state)!=88 || state.calls!=1 || bytes[0]!=0x25)return 2;
 state.status=90;if(owner.commit(memory,download,&state)!=90 || state.calls!=2 || bytes[0]!=0x25)return 3;
 state.status=0;bytes[2]=0x66;if(owner.commit(memory,download,&state)!=88 || state.calls!=2)return 4;bytes[2]=0x25;
 state.race=bytes+3;if(owner.commit(memory,download,&state)!=88 || state.calls!=3 || bytes[3]!=0x66 || bytes[0]!=0x25)return 5;bytes[3]=0x25;state.race=nullptr;
 if(owner.commit(memory,download,&state) || !owner.completed || owner.poisoned || state.calls!=4)return 6;
 for(unsigned i=0;i<64;++i)if(bytes[i]!=(i<48?0x72:0x25))return 7;
 if(owner.commit(memory,download,&state)!=91 || state.calls!=4)return 8;
 // Inject only API failures around real owned memory. A failed publication
 // or verification poisons this owner, preventing an EOS-ready completion.
 struct Fault{CtrGuestCopyApi real;bool wrote=false,fail_write=false,fail_readback=false,throw_readback=false;};
 Fault fault{memory};CtrGuestCopyApi failing=memory;failing.context=&fault;
 failing.read=+[](void* raw,uint64_t address,void* output,uint64_t count)->int{auto& f=*static_cast<Fault*>(raw);if(f.wrote && f.throw_readback)throw 1;if(f.wrote && f.fail_readback)return 62;return f.real.read(f.real.context,address,output,count);};
 failing.write=+[](void* raw,uint64_t address,const void* input,uint64_t count)->int{auto& f=*static_cast<Fault*>(raw);if(f.fail_write)return 62;const int status=f.real.write(f.real.context,address,input,count);f.wrote=!status;return status;};
 for(unsigned mode=0;mode<3;++mode){const bool after_write=mode!=0;std::fill_n(bytes,64,static_cast<unsigned char>(0x25));ComputeGuestWriteback failed;failed.address=owner.address;failed.initial.assign(bytes,bytes+64);fault.wrote=false;fault.fail_write=!after_write;fault.fail_readback=mode==1;fault.throw_readback=mode==2;
  if(failed.commit(failing,download,&state)!=(mode==2?112:62) || !failed.poisoned || failed.completed)return 9;
  const auto calls=state.calls;if(failed.commit(failing,download,&state)!=91 || state.calls!=calls)return 10;
  if(bytes[0]!=(after_write?0x72:0x25))return 11;
 }
 std::cout<<"{\"scope\":\"compute_guest_writeback_control\",\"passed\":true,\"actual_owned_guest_bytes\":64,\"synthetic_download_only\":true,\"padding_fixture_preserved\":true,\"concurrent_CPU_write_rejected\":true,\"write_failure_poisoned\":true,\"readback_failure_poisoned\":true,\"verification_exception_poisoned\":true,\"fault_callbacks_synthetic\":true,\"GPU_execution\":false}\n";return 0;
}
inline int compute_large_image_writeback_control(){
 constexpr size_t bytes=17694720,logical=1920*1080*4*2;
 GuestStack stack(bytes+4096,4096);auto* data=static_cast<unsigned char*>(stack.bottom());std::fill_n(data,bytes,static_cast<unsigned char>(0x25));
 ComputeGuestWriteback owner;owner.address=reinterpret_cast<uint64_t>(data);owner.initial.assign(data,data+bytes);
 const auto memory=make_owned_guest_copy_api();if(memory.read(memory.context,owner.address,owner.initial.data(),bytes)==0)return 1;
 struct Probe{unsigned calls=0;}probe;CtrGuestCopyApi preflight=memory;preflight.context=&probe;
 preflight.read=+[](void* raw,uint64_t,void*,uint64_t)->int{++static_cast<Probe*>(raw)->calls;return 112;};
 preflight.write=+[](void* raw,uint64_t,const void*,uint64_t)->int{++static_cast<Probe*>(raw)->calls;return 112;};
 for(bool write:{false,true}){
  if(image_guest_copy(preflight,owner.address,data+8388608,bytes,write)!=60 || probe.calls)return 8;
  if(image_guest_copy(preflight,owner.address,reinterpret_cast<void*>(UINTPTR_MAX-15),32,write)!=60 || probe.calls)return 9;
 }
 struct Provider{unsigned calls=0;}provider;
 const auto download=+[](void* raw,const unsigned char* prior,uint64_t length,unsigned char* output,uint64_t capacity)->int{auto& p=*static_cast<Provider*>(raw);++p.calls;if(length!=17694720 || capacity!=length)return 60;std::copy_n(prior,static_cast<size_t>(length),output);std::fill_n(output,1920*1080*4*2,static_cast<unsigned char>(0x72));return 0;};
 if(owner.commit(memory,download,&provider) || !owner.completed || owner.poisoned || provider.calls!=1)return 2;
 for(size_t i=0;i<bytes;++i)if(data[i]!=(i<logical?0x72:0x25))return 3;
 if(owner.commit(memory,download,&provider)!=91 || provider.calls!=1)return 4;
 // Real partial publication: first8MiB written, second chunk API failure.
 // No success/IRQ authority may escape this poisoned transaction.
 std::fill_n(data,bytes,static_cast<unsigned char>(0x25));ComputeGuestWriteback failed;failed.address=owner.address;failed.initial.assign(data,data+bytes);
 struct Fault{CtrGuestCopyApi real;unsigned writes=0;uint64_t maximum=0;}fault{memory};CtrGuestCopyApi failing=memory;failing.context=&fault;
 failing.read=+[](void* raw,uint64_t address,void* output,uint64_t count)->int{auto& f=*static_cast<Fault*>(raw);f.maximum=std::max(f.maximum,count);return f.real.read(f.real.context,address,output,count);};
 failing.write=+[](void* raw,uint64_t address,const void* input,uint64_t count)->int{auto& f=*static_cast<Fault*>(raw);f.maximum=std::max(f.maximum,count);if(++f.writes==2)return 62;return f.real.write(f.real.context,address,input,count);};
 if(failed.commit(failing,download,&provider)!=62 || !failed.poisoned || failed.completed || fault.writes!=2 || fault.maximum!=8388608)return 5;
 if(!std::all_of(data,data+8388608,[](unsigned char v){return v==0x72;}) || !std::all_of(data+8388608,data+bytes,[](unsigned char v){return v==0x25;}))return 6;
 const auto calls=provider.calls;if(failed.commit(failing,download,&provider)!=91 || provider.calls!=calls || fault.writes!=2)return 7;
 std::cout<<"{\"scope\":\"compute_large_image_writeback_control\",\"passed\":true,\"actual_owned_guest_bytes\":17694720,\"chunk_bound\":8388608,\"partial_second_chunk_failure_poisoned\":true,\"whole_span_alias_and_host_overflow_rejected_before_callbacks\":true,\"padding_preserved\":true,\"synthetic_download\":true,\"GPU_execution\":false}\n";return 0;
}

}
