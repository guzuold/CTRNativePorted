// SPDX-License-Identifier: GPL-2.0-only
#pragma once
#include "compute_guest_writeback.hpp"
#include <array>
namespace ctrport {
// Transaction boundary for separately stored depth and stencil planes. The
// provider must download/retile the actual GPU generation; this owner supplies
// no PS4 layout formula and cannot grant completion without every plane.
struct DepthGuestWriteback {
 uint64_t token=0;std::array<ComputeGuestWriteback,2> planes{};
 uint32_t count=0;bool completed=false,poisoned=false;
 // Capture before the first GPU initialization/writer. The caller must supply
 // spans validated by its public layout provider; this method derives no layout.
 // Commit the owner only after every read completes, preserving caller state
 // on malformed spans, inaccessible second planes, or overlapping guest memory.
 static int capture(const CtrGuestCopyApi& memory,uint64_t owner,const uint64_t* addresses,const uint64_t* lengths,uint32_t planes_count,DepthGuestWriteback& output){
  if(!owner || !addresses || !lengths || !planes_count || planes_count>2)return 60;
  if(output.token || output.count || output.completed || output.poisoned)return 91;
  for(uint32_t i=0;i<planes_count;++i){
   if(!addresses[i] || !lengths[i] || lengths[i]>32*1024*1024 || addresses[i]>UINT64_MAX-lengths[i])return 60;
   for(uint32_t j=0;j<i;++j)if(addresses[i]<addresses[j]+lengths[j] && addresses[j]<addresses[i]+lengths[i])return 60;
  }
  try{DepthGuestWriteback pending;pending.token=owner;pending.count=planes_count;
   for(uint32_t i=0;i<planes_count;++i){auto& plane=pending.planes[i];plane.address=addresses[i];plane.initial.resize(static_cast<size_t>(lengths[i]));
    const int status=image_guest_copy(memory,plane.address,plane.initial.data(),lengths[i],false);if(status)return status;
   }
   // Optimistic group consistency: a CPU writer may alter the first plane
   // while the second is read. No owner is published until all spans still
   // match. This is validation, not a lock on guest threads; commit rechecks.
   for(uint32_t i=0;i<planes_count;++i){const auto& plane=pending.planes[i];std::vector<unsigned char> check(plane.initial.size());
    const int status=image_guest_copy(memory,plane.address,check.data(),check.size(),false);if(status)return status;
    if(check!=plane.initial)return 88;
   }
   output=std::move(pending);return 0;
  }catch(...){return 112;}
 }
 using Download=int(*)(void*,uint32_t,const unsigned char*,uint64_t,unsigned char*,uint64_t);
 int commit(const CtrGuestCopyApi& memory,Download download,void* context){
  if(completed || poisoned)return 91;
  if(!token || !count || count>2 || memory.version!=1 || memory.size!=sizeof(memory) || !memory.read || !memory.write)return 60;
  if(!download)return 88;
  std::array<std::vector<unsigned char>,2> outputs;
  try{
   for(uint32_t i=0;i<count;++i){const auto& p=planes[i];
    if(p.completed || p.poisoned || !p.address || p.initial.empty() || p.initial.size()>32*1024*1024 || p.address>UINT64_MAX-p.initial.size())return 60;
    for(uint32_t j=0;j<i;++j)if(p.address<planes[j].address+planes[j].initial.size() && planes[j].address<p.address+p.initial.size())return 60;
   }
   // Finish all GPU transfers before beginning either guest publication.
   for(uint32_t i=0;i<count;++i){const auto& p=planes[i];outputs[i].resize(p.initial.size());
    const int status=download(context,i,p.initial.data(),p.initial.size(),outputs[i].data(),outputs[i].size());if(status)return status;
   }
   for(uint32_t i=0;i<count;++i){std::vector<unsigned char> current(planes[i].initial.size());
    const int status=image_guest_copy(memory,planes[i].address,current.data(),current.size(),false);if(status)return status;
    if(current!=planes[i].initial)return 88;
   }
   // Once any publication is attempted, a partial result may never release
   // the dirty owner or signal EOP. Failure deliberately forbids retry.
   poisoned=true;
   for(uint32_t i=0;i<count;++i){
    const auto copy=+[](void* raw,const unsigned char*,uint64_t bytes,unsigned char* result,uint64_t capacity)->int{
     const auto& data=*static_cast<const std::vector<unsigned char>*>(raw);
     if(bytes!=data.size() || capacity!=bytes)return 60;std::copy(data.begin(),data.end(),result);return 0;
    };
    const int status=planes[i].commit(memory,copy,&outputs[i]);if(status)return status;
   }
   // Recheck the group after its last write: a concurrent writer may have
   // changed the first plane while the second was being published.
   for(uint32_t i=0;i<count;++i){std::vector<unsigned char> current(planes[i].initial.size());
    const int status=image_guest_copy(memory,planes[i].address,current.data(),current.size(),false);if(status)return status;
    if(current!=planes[i].initial)return 112;
   }
   completed=true;poisoned=false;return 0;
  }catch(...){return 112;}
 }
};
inline int depth_guest_writeback_control(){
 GuestStack allocation(32768,4096);auto* data=static_cast<unsigned char*>(allocation.bottom());
 const auto memory=make_owned_guest_copy_api();
 const auto owner=[&](){std::fill_n(data,128,static_cast<unsigned char>(0x25));DepthGuestWriteback w;w.token=7;w.count=2;
  for(uint32_t i=0;i<2;++i){w.planes[i].address=reinterpret_cast<uint64_t>(data+64*i);w.planes[i].initial.assign(data+64*i,data+64*i+64);}return w;};
 struct Provider{uint32_t calls=0;int fail_plane=-1;};Provider provider;
 const auto download=+[](void* raw,uint32_t plane,const unsigned char* prior,uint64_t bytes,unsigned char* output,uint64_t capacity)->int{
  auto& p=*static_cast<Provider*>(raw);++p.calls;if(int(plane)==p.fail_plane)return 88;
  if(bytes!=64 || capacity!=bytes)return 60;std::copy_n(prior,64,output);std::fill_n(output,48,static_cast<unsigned char>(plane?0x72:0x61));return 0;
 };
 auto w=owner();if(w.commit(memory,nullptr,nullptr)!=88 || provider.calls)return 9;
 auto invalid=memory;invalid.version=0;if(w.commit(invalid,download,&provider)!=60 || provider.calls)return 10;
 w.count=1;if(w.commit(memory,download,&provider) || !w.completed || data[0]!=0x61 || data[64]!=0x25)return 11;
 w=owner();provider.fail_plane=1;if(w.commit(memory,download,&provider)!=88 || w.poisoned || w.completed || data[0]!=0x25 || data[64]!=0x25)return 1;
 provider.fail_plane=-1;data[65]=0x99;if(w.commit(memory,download,&provider)!=88 || w.completed || data[0]!=0x25)return 2;
 w=owner();w.planes[1].address=w.planes[0].address+32;const auto calls=provider.calls;if(w.commit(memory,download,&provider)!=60 || provider.calls!=calls)return 3;
 w=owner();if(w.commit(memory,download,&provider) || !w.completed || w.poisoned)return 4;
 for(uint32_t i=0;i<128;++i)if(data[i]!=(i%64<48?(i<64?0x61:0x72):0x25))return 5;
 if(w.commit(memory,download,&provider)!=91)return 6;
 struct Fault{CtrGuestCopyApi real;uint64_t second;bool race=false;};Fault fault{memory,reinterpret_cast<uint64_t>(data+64)};
 CtrGuestCopyApi failing=memory;failing.context=&fault;
 failing.read=+[](void* raw,uint64_t address,void* output,uint64_t bytes)->int{auto& f=*static_cast<Fault*>(raw);return f.real.read(f.real.context,address,output,bytes);};
 failing.write=+[](void* raw,uint64_t address,const void* source,uint64_t bytes)->int{auto& f=*static_cast<Fault*>(raw);if(address==f.second){if(!f.race)return 62;const unsigned char changed=0x99;const int status=f.real.write(f.real.context,f.second-64,&changed,1);if(status)return status;}return f.real.write(f.real.context,address,source,bytes);};
 w=owner();if(w.commit(failing,download,&provider)!=62 || !w.poisoned || w.completed || !w.planes[0].completed || w.planes[1].completed || data[0]!=0x61 || data[64]!=0x25)return 7;
 const auto stopped=provider.calls;if(w.commit(memory,download,&provider)!=91 || provider.calls!=stopped)return 8;
 w=owner();fault.race=true;if(w.commit(failing,download,&provider)!=112 || !w.poisoned || w.completed || data[0]!=0x99)return 12;
 DepthGuestWriteback captured;uint64_t addresses[]={reinterpret_cast<uint64_t>(data),reinterpret_cast<uint64_t>(data+64)},lengths[]={64,64};
 if(DepthGuestWriteback::capture(memory,9,addresses,lengths,2,captured) || captured.token!=9 || captured.count!=2 || captured.planes[0].initial[0]!=data[0] || captured.planes[1].initial[0]!=data[64])return 13;
 if(DepthGuestWriteback::capture(memory,9,addresses,lengths,2,captured)!=91)return 14;
 captured=DepthGuestWriteback{};addresses[1]=addresses[0]+32;if(DepthGuestWriteback::capture(memory,9,addresses,lengths,2,captured)!=60 || captured.token)return 15;
 addresses[1]=UINT64_MAX-31;if(DepthGuestWriteback::capture(memory,9,addresses,lengths,2,captured)!=60 || captured.token)return 16;
 addresses[1]=reinterpret_cast<uint64_t>(data+64);CtrGuestCopyApi denied=memory;denied.context=&fault;
 denied.read=+[](void* raw,uint64_t address,void* target,uint64_t bytes)->int{auto& f=*static_cast<Fault*>(raw);if(address==f.second)return 62;return f.real.read(f.real.context,address,target,bytes);};
 if(DepthGuestWriteback::capture(denied,9,addresses,lengths,2,captured)!=62 || captured.token || captured.count)return 17;
 struct CaptureRace{CtrGuestCopyApi real;uint64_t first,second;bool changed=false;}race{memory,addresses[0],addresses[1]};
 auto racing=memory;racing.context=&race;
 racing.read=+[](void* raw,uint64_t address,void* target,uint64_t count)->int{auto& r=*static_cast<CaptureRace*>(raw);if(address==r.second && !r.changed){const unsigned char changed=0x55;const int status=r.real.write(r.real.context,r.first,&changed,1);if(status)return status;r.changed=true;}return r.real.read(r.real.context,address,target,count);};
 if(DepthGuestWriteback::capture(racing,9,addresses,lengths,2,captured)!=88 || !race.changed || captured.token || captured.count)return 18;
  std::cout<<"{\"scope\":\"depth_guest_writeback_control\",\"passed\":true,\"actual_owned_guest_bytes\":128,\"atomic_pre_writer_snapshot_capture\":true,\"cross_plane_capture_CPU_race_rejected\":true,\"all_transfers_before_publication\":true,\"partial_second_plane_poisoned\":true,\"overlap_rejected\":true,\"concurrent_CPU_change_rejected\":true,\"synthetic_download\":true,\"GPU_execution\":false}\n";return 0;
}
// Full-sized two-plane transport control. Bytes are authored, not retail GPU
// content; it proves chunked copies and all-plane publication at actual lengths.
inline int depth_large_guest_writeback_control(){
 constexpr uint64_t zbytes=17694720,sbytes=2073600;
 GuestStack allocation(zbytes+sbytes+4096,4096);auto* bytes=static_cast<unsigned char*>(allocation.bottom());
 std::fill_n(bytes,zbytes+sbytes,static_cast<unsigned char>(0x25));
 const auto memory=make_owned_guest_copy_api();uint64_t addresses[]={reinterpret_cast<uint64_t>(bytes),reinterpret_cast<uint64_t>(bytes+zbytes)},lengths[]={zbytes,sbytes};
 DepthGuestWriteback owner;if(DepthGuestWriteback::capture(memory,19,addresses,lengths,2,owner))return 1;
 struct State{uint32_t calls=0;}state;
 const auto download=+[](void* raw,uint32_t plane,const unsigned char* prior,uint64_t count,unsigned char* output,uint64_t capacity)->int{
  ++static_cast<State*>(raw)->calls;if(count!=capacity || count!=(plane?2073600ull:17694720ull))return 60;
  std::copy_n(prior,static_cast<size_t>(count),output);std::fill_n(output,static_cast<size_t>(count-256),static_cast<unsigned char>(plane?0x72:0x61));return 0;
 };
 if(owner.commit(memory,download,&state) || !owner.completed || owner.poisoned || state.calls!=2)return 2;
 for(uint32_t plane=0;plane<2;++plane)for(uint64_t i=0;i<lengths[plane];++i)if(reinterpret_cast<unsigned char*>(addresses[plane])[i]!=(i<lengths[plane]-256?(plane?0x72:0x61):0x25))return 3;
 std::fill_n(bytes,zbytes+sbytes,static_cast<unsigned char>(0x25));owner=DepthGuestWriteback{};
 if(DepthGuestWriteback::capture(memory,20,addresses,lengths,2,owner))return 4;
 struct Fault{CtrGuestCopyApi real;uint64_t stencil;uint32_t writes=0;uint64_t largest=0;}fault{memory,addresses[1]};
 auto failing=memory;failing.context=&fault;
 failing.read=+[](void* raw,uint64_t address,void* out,uint64_t count)->int{auto& f=*static_cast<Fault*>(raw);return f.real.read(f.real.context,address,out,count);};
 failing.write=+[](void* raw,uint64_t address,const void* in,uint64_t count)->int{auto& f=*static_cast<Fault*>(raw);++f.writes;f.largest=std::max(f.largest,count);if(address==f.stencil)return 62;return f.real.write(f.real.context,address,in,count);};
 if(owner.commit(failing,download,&state)!=62 || !owner.poisoned || owner.completed || !owner.planes[0].completed || owner.planes[1].completed || fault.writes!=4 || fault.largest!=8388608 || bytes[zbytes]!=0x25)return 5;
 const auto calls=state.calls;if(owner.commit(memory,download,&state)!=91 || state.calls!=calls)return 6;
 std::cout<<"{\"scope\":\"depth_large_guest_writeback_control\",\"passed\":true,\"depth_bytes\":17694720,\"stencil_bytes\":2073600,\"full_plane_verification\":true,\"padding_preserved\":true,\"partial_second_plane_poisoned\":true,\"maximum_copy_chunk\":8388608,\"synthetic_download\":true,\"GPU_execution\":false}\n";return 0;
}

}
