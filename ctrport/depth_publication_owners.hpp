// SPDX-License-Identifier: GPL-2.0-only
#pragma once
#include "depth_guest_writeback.hpp"
namespace ctrport {
// Submission-local GPU generations. Enrollment must precede the first writer;
// a token first observed dirty has no trustworthy pre-writer snapshot and cannot
// be retroactively enrolled. Only a full verified publication retires dirtiness.
class DepthPublicationOwners {
 struct Entry {uint64_t token;uint32_t writer;bool dirty;DepthGuestWriteback backing;DepthGuestWriteback::Download download;void* context;};
 std::vector<Entry> entries_;bool running_=false;
 static int verify(const Entry& entry,const CtrGuestCopyApi& memory){
  try{for(uint32_t i=0;i<entry.backing.count;++i){const auto& plane=entry.backing.planes[i];std::vector<unsigned char> current(plane.initial.size());
   const int read=image_guest_copy(memory,plane.address,current.data(),current.size(),false);if(read)return read;if(current!=plane.initial)return 88;
  }return 0;}catch(...){return 112;}
 }
public:
 int enroll(const CtrGuestCopyApi& memory,uint64_t token,const uint64_t* addresses,const uint64_t* lengths,uint32_t count,DepthGuestWriteback::Download download,void* context){
  if(running_)return 91;if(!token)return 60;for(const auto& entry:entries_)if(entry.token==token)return 91;
  // Guest overlap between distinct live GPU owners requires explicit alias
  // authority outside this class; never independently publish both snapshots.
  if(!addresses || !lengths || !count || count>2)return 60;
  for(uint32_t i=0;i<count;++i){if(!addresses[i] || !lengths[i] || addresses[i]>UINT64_MAX-lengths[i])return 60;
   for(const auto& entry:entries_)for(uint32_t j=0;j<entry.backing.count;++j){const auto& plane=entry.backing.planes[j];if(addresses[i]<plane.address+plane.initial.size() && plane.address<addresses[i]+lengths[i])return 88;}
  }
  Entry pending{token,0,false,{},download,context};const int status=DepthGuestWriteback::capture(memory,token,addresses,lengths,count,pending.backing);if(status)return status;
  try{entries_.push_back(std::move(pending));return 0;}catch(...){return 112;}
 }
 // A newly loaded real provider may bind to a pre-writer snapshot while
 // paused. Binding cannot manufacture a missing snapshot or repair poisoning.
 // Provider/module lifetime remains owned by the executor through finish().
 int bind(uint64_t token,DepthGuestWriteback::Download download,void* context){
  if(running_)return 91;if(!token || !download)return 60;
  for(auto& entry:entries_)if(entry.token==token){if(entry.backing.poisoned)return 91;if(!entry.backing.count)return 88;entry.download=download;entry.context=context;return 0;}
  return 62;
 }
 // Recheck the enrolled CPU generation before pages become GPU-resident.
 // No late snapshot may replace the bytes captured before the first clear.
 int verify_pre_writer(uint64_t token,const CtrGuestCopyApi& memory)const{
  if(running_)return 91;if(!token)return 60;
  for(const auto& entry:entries_)if(entry.token==token){if(entry.backing.poisoned)return 91;if(!entry.backing.count)return 88;return verify(entry,memory);}return 62;
 }
 int mark(uint64_t token,uint32_t writer){
  if(running_)return 91;if(!token)return 60;
  for(auto& entry:entries_)if(entry.token==token){
   if(entry.backing.poisoned)return 91;
   if(!entry.dirty && entry.backing.completed){entry.backing.completed=false;for(auto& plane:entry.backing.planes)plane.completed=false;}
   entry.dirty=true;entry.writer=writer;return 0;
  }
  try{entries_.push_back({token,writer,true,{},nullptr,nullptr});return 0;}catch(...){return 112;}
 }
 size_t size()const{size_t count=0;for(const auto& entry:entries_)if(entry.dirty)++count;return count;}
 bool empty()const{return !size();}
 size_t missing_snapshots()const{size_t count=0;for(const auto& entry:entries_)if(entry.dirty && !entry.backing.count)++count;return count;}
 template<class Observer>void observe(Observer&& observer)const{for(const auto& entry:entries_)observer(entry.token,entry.writer,entry.dirty,entry.backing.count,entry.backing.completed,entry.backing.poisoned);}
 using Resident=int(*)(void*,uint64_t,uint32_t);
 int finish(const CtrGuestCopyApi& memory,Resident resident=nullptr,void* context=nullptr){
  if(running_)return 91;running_=true;struct Reset{bool& state;~Reset(){state=false;}}reset{running_};
  // Validate prior completed publications on every resume, even if a later
  // owner still lacks a provider. A CPU mutation cannot silently signal EOP.
  for(const auto& entry:entries_)if(entry.backing.completed){const int status=verify(entry,memory);if(status)return status;}
  bool pending=false;
  for(auto& entry:entries_)if(entry.dirty){
   if(entry.backing.poisoned)return 91;
   // A genuine retained/protected GPU generation may satisfy completion while
   // remaining GPU-dirty. This does not publish or manufacture guest bytes.
   if(resident){const int status=resident(context,entry.token,entry.writer);if(!status)continue;if(status!=88)return status;}
   if(!entry.backing.count || !entry.download){pending=true;continue;}
   const int status=entry.backing.commit(memory,entry.download,entry.context);
   if(status==88 && !entry.backing.poisoned){pending=true;continue;}
   if(status)return status;entry.dirty=false;
  }
  for(const auto& entry:entries_)if(entry.backing.completed){const int status=verify(entry,memory);if(status)return status;}
  return pending?88:0;
 }
};
inline int depth_publication_owners_control(){
 GuestStack storage(32768,4096);auto* bytes=static_cast<unsigned char*>(storage.bottom());std::fill_n(bytes,256,static_cast<unsigned char>(0x25));
 const auto memory=make_owned_guest_copy_api();uint64_t addresses[]={reinterpret_cast<uint64_t>(bytes),reinterpret_cast<uint64_t>(bytes+64)},lengths[]={64,64};
 struct Provider {uint32_t calls=0;int result=0;DepthPublicationOwners* reenter=nullptr;bool guarded=false;}provider;
 const auto download=+[](void* raw,uint32_t plane,const unsigned char* prior,uint64_t length,unsigned char* output,uint64_t capacity)->int{auto& p=*static_cast<Provider*>(raw);++p.calls;if(p.reenter){const auto memory=make_owned_guest_copy_api();if(p.reenter->mark(999,3)!=91 || p.reenter->finish(memory)!=91)return 112;p.guarded=true;}if(p.result)return p.result;if(length!=64 || capacity!=64)return 60;std::copy_n(prior,64,output);std::fill_n(output,48,static_cast<unsigned char>(plane?0x72:0x61));return 0;};
 DepthPublicationOwners owners;
 if(owners.mark(0,1)!=60 || !owners.empty())return 1;
 if(owners.mark(7,1377) || owners.size()!=1 || owners.missing_snapshots()!=1 || owners.finish(memory)!=88 || provider.calls || bytes[0]!=0x25)return 2;
 if(owners.enroll(memory,7,addresses,lengths,2,download,&provider)!=91)return 3;
 DepthPublicationOwners ready;
 if(ready.enroll(memory,8,addresses,lengths,2,download,&provider) || !ready.empty() || ready.finish(memory) || provider.calls)return 4;
 if(ready.mark(8,1377) || ready.mark(8,2075) || ready.size()!=1)return 5;
 provider.result=88;if(ready.finish(memory)!=88 || ready.size()!=1 || bytes[0]!=0x25)return 6;
 provider.result=0;provider.reenter=&ready;if(ready.finish(memory) || !ready.empty() || bytes[0]!=0x61 || bytes[64]!=0x72)return 7;
 if(!provider.guarded)return 14;provider.reenter=nullptr;
 const auto calls=provider.calls;if(ready.finish(memory) || provider.calls!=calls)return 8;
 if(ready.mark(9,3242) || ready.finish(memory)!=88 || ready.size()!=1 || ready.missing_snapshots()!=1)return 9;
 bytes[0]=0x99;if(ready.finish(memory)!=88 || provider.calls!=calls || ready.empty())return 10;
 bytes[0]=0x61;
 DepthPublicationOwners aliases;if(aliases.enroll(memory,10,addresses,lengths,2,download,&provider))return 11;
 uint64_t overlap[]={addresses[0]+32};if(aliases.enroll(memory,11,overlap,lengths,1,download,&provider)!=88)return 12;
 // A fresh GPU generation reuses the verified last publication as its baseline.
 if(aliases.mark(10,1) || aliases.finish(memory) || aliases.mark(10,2) || aliases.size()!=1 || aliases.finish(memory) || !aliases.empty())return 13;
 // Real-provider binding after a pause preserves the original snapshot.
 std::fill_n(bytes,256,static_cast<unsigned char>(0x25));DepthPublicationOwners reload;
 if(reload.enroll(memory,12,addresses,lengths,2,nullptr,nullptr) || reload.mark(12,1) || reload.finish(memory)!=88 || reload.bind(12,download,&provider) || reload.finish(memory) || !reload.empty())return 15;
 if(owners.bind(7,download,&provider)!=88 || reload.bind(99,download,&provider)!=62 || reload.bind(12,nullptr,nullptr)!=60)return 16;
 std::fill_n(bytes,256,static_cast<unsigned char>(0x25));DepthPublicationOwners poisoned;
 if(poisoned.enroll(memory,13,addresses,lengths,2,download,&provider) || poisoned.mark(13,2))return 17;
 struct WriteFault{CtrGuestCopyApi real;uint64_t second;}fault{memory,addresses[1]};auto failing=memory;failing.context=&fault;
 failing.read=+[](void* raw,uint64_t address,void* output,uint64_t count)->int{auto& f=*static_cast<WriteFault*>(raw);return f.real.read(f.real.context,address,output,count);};
 failing.write=+[](void* raw,uint64_t address,const void* input,uint64_t count)->int{auto& f=*static_cast<WriteFault*>(raw);if(address==f.second)return 62;return f.real.write(f.real.context,address,input,count);};
 if(poisoned.finish(failing)!=62 || poisoned.size()!=1 || bytes[0]!=0x61 || bytes[64]!=0x25)return 18;
 const auto failed_calls=provider.calls;if(poisoned.mark(13,3)!=91 || poisoned.bind(13,download,&provider)!=91 || poisoned.finish(memory)!=91 || provider.calls!=failed_calls || poisoned.empty())return 19;
 // An earlier missing owner cannot prevent an independent captured owner publishing.
 std::fill_n(bytes,256,static_cast<unsigned char>(0x25));DepthPublicationOwners independent;
 if(independent.mark(20,1) || independent.enroll(memory,21,addresses,lengths,2,download,&provider) || independent.mark(21,2))return 20;
 if(independent.finish(memory)!=88 || independent.size()!=1 || bytes[0]!=0x61 || bytes[64]!=0x72)return 21;
 const auto independent_calls=provider.calls;if(independent.finish(memory)!=88 || provider.calls!=independent_calls)return 22;
 bytes[0]=0x99;if(independent.finish(memory)!=88 || provider.calls!=independent_calls)return 23;
 // Completion may retain a genuine GPU generation without declaring publication.
 DepthPublicationOwners resident; if(resident.mark(30,4))return 24;
 struct Lease {uint64_t token;uint32_t writer;} lease{30,4};
 const auto coverage=+[](void* raw,uint64_t token,uint32_t writer)->int{const auto& value=*static_cast<Lease*>(raw);return value.token==token && value.writer==writer?0:88;};
 if(resident.finish(memory,coverage,&lease) || resident.empty() || resident.missing_snapshots()!=1)return 25;
 lease.writer=3;if(resident.finish(memory,coverage,&lease)!=88)return 26;
 lease.writer=4;if(resident.mark(30,5) || resident.finish(memory,coverage,&lease)!=88)return 27;
 lease.writer=5;if(resident.finish(memory,coverage,&lease) || resident.size()!=1)return 28;
 DepthPublicationOwners captured;bytes[128]=0x25;uint64_t captured_address=reinterpret_cast<uint64_t>(bytes+128),captured_length=64;
 if(captured.verify_pre_writer(0,memory)!=60 || captured.verify_pre_writer(40,memory)!=62)return 31;
 if(captured.enroll(memory,40,&captured_address,&captured_length,1,nullptr,nullptr) || captured.verify_pre_writer(40,memory))return 32;
 bytes[128]=0x99;if(captured.verify_pre_writer(40,memory)!=88)return 33;
 bytes[128]=0x25;if(captured.verify_pre_writer(40,memory))return 34;
 const auto forged=+[](void*,uint64_t,uint32_t)->int{return 0;};
 if(poisoned.finish(memory,forged,nullptr)!=91)return 29;
 const auto failure=+[](void*,uint64_t,uint32_t)->int{return 112;};
 if(resident.finish(memory,failure,nullptr)!=112 || resident.empty())return 30;

  std::cout<<"{\"scope\":\"depth_publication_owners_control\",\"passed\":true,\"actual_owned_guest_memory\":true,\"late_snapshot_enrollment_rejected\":true,\"missing_snapshot_retains_dirty\":true,\"resume_rechecks_prior_publications\":true,\"overlapping_owners_rejected\":true,\"multiple_GPU_generations\":true,\"provider_reentrancy_rejected\":true,\"late_real_provider_binding_keeps_snapshot\":true,\"partial_publication_forbids_retry_or_rebind\":true,\"residency_keeps_dirty_generation\":true,\"stale_residency_rejected\":true,\"poison_cannot_be_bypassed\":true,\"residency_error_propagated\":true,\"synthetic_download\":true,\"GPU_execution\":false}\n";return 0;
}
}
