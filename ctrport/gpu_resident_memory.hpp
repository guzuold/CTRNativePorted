// SPDX-License-Identifier: GPL-2.0-only
#pragma once
#include "owned_guest_copy.hpp"
#include <memory>
#include <cstdio>
namespace ctrport {
// A CPU exclusion lease, not image tiling or a GPU image provider. Only whole
// owned RW pages are admitted: callers must establish any padded plane extent.
// The lease must outlive guest execution and retain the genuine GPU generation.
class GpuResidentMemory {
 struct Range {uint64_t address,bytes,image,generation;std::shared_ptr<void> owner;};
 std::vector<Range> ranges_;mutable std::mutex mutex_;
public:
 GpuResidentMemory()=default;GpuResidentMemory(const GpuResidentMemory&)=delete;
 GpuResidentMemory& operator=(const GpuResidentMemory&)=delete;
 ~GpuResidentMemory(){shutdown();}
 int protect(uint64_t address,uint64_t bytes,uint64_t image,uint64_t generation,std::shared_ptr<void> owner,std::vector<unsigned char>* prior=nullptr){
  std::lock_guard<std::mutex> state(mutex_);
  SYSTEM_INFO system{};GetSystemInfo(&system);const uint64_t page=system.dwPageSize;
  if(!page || !address || !bytes || !image || !generation || !owner || address%page || bytes%page || bytes>32*1024*1024 || address>UINT64_MAX-bytes)return 60;
  for(const auto& range:ranges_)if(address<range.address+range.bytes && range.address<address+bytes)return 88;
  std::lock_guard<std::mutex> lock(owned_output_lock);bool owned=false;
  for(const auto& item:owned_output_extents)for(const auto& extent:item.second)if(address>=extent.first && address+bytes<=extent.second)owned=true;
  if(!owned)return 62;
  for(uint64_t cursor=address;cursor<address+bytes;){MEMORY_BASIC_INFORMATION info{};
   if(!VirtualQuery(reinterpret_cast<void*>(cursor),&info,sizeof(info)) || info.State!=MEM_COMMIT || info.Protect!=PAGE_READWRITE)return 62;
   const uint64_t end=reinterpret_cast<uint64_t>(info.BaseAddress)+info.RegionSize;if(end<=cursor)return 62;cursor=end;
  }
  try{ranges_.reserve(ranges_.size()+1);}catch(...){return 112;}
  // Preserve original CPU backing before exclusion, solely as padding/prior-byte
  // authority. It is never a snapshot of the later GPU-written logical pixels.
  std::vector<unsigned char> snapshot;
  if(prior){try{snapshot.resize(static_cast<size_t>(bytes));}catch(...){return 112;}
   std::memcpy(snapshot.data(),reinterpret_cast<const void*>(address),static_cast<size_t>(bytes));}
  DWORD old=0;if(!VirtualProtect(reinterpret_cast<void*>(address),bytes,PAGE_NOACCESS,&old))return 62;
  if(prior)prior->swap(snapshot);
  ranges_.push_back({address,bytes,image,generation,std::move(owner)});return 0;
 }
 bool identify(uint64_t address,uint64_t& image,uint64_t& generation,uint64_t& begin,uint64_t& bytes)const{
  std::lock_guard<std::mutex> state(mutex_);
  for(const auto& range:ranges_)if(address>=range.address && address-range.address<range.bytes){image=range.image;generation=range.generation;begin=range.address;bytes=range.bytes;return true;}return false;
 }
 // Exact backing lookup only. Format/sample/layout matching remains the real
 // renderer provider's responsibility; partial overlaps cannot invent aliases.
 int resolve(uint64_t address,uint64_t bytes,uint64_t& image,uint64_t& generation,std::shared_ptr<void>& owner)const{
  if(!address || !bytes || address>UINT64_MAX-bytes)return 60;
  std::shared_ptr<void> selected;uint64_t selected_image=0,selected_generation=0;
  {std::lock_guard<std::mutex> state(mutex_);
   for(const auto& range:ranges_){if(address==range.address && bytes==range.bytes){selected_image=range.image;selected_generation=range.generation;selected=range.owner;break;}
    if(address<range.address+range.bytes && range.address<address+bytes)return 88;}
  }
  if(!selected)return 62;image=selected_image;generation=selected_generation;owner=std::move(selected);return 0;
 }
 // Called only after the real writer/fence. The page remains NOACCESS while
 // the same retained image advances; an obsolete expected generation fails.
 int advance(uint64_t address,uint64_t bytes,uint64_t image,uint64_t expected,uint64_t generation){
  if(!generation || generation<=expected)return 60;std::lock_guard<std::mutex> state(mutex_);
  for(auto& range:ranges_)if(range.address==address && range.bytes==bytes){if(range.image!=image || range.generation!=expected)return 88;range.generation=generation;return 0;}return 62;
 }
 // Swap a completed real GPU writer without ever exposing stale CPU pages.
 int replace(uint64_t address,uint64_t bytes,uint64_t prior,uint64_t expected,uint64_t image,uint64_t generation,std::shared_ptr<void> owner){
  if(!image || !owner || generation<=expected)return 60;
  std::shared_ptr<void> released;
  {std::lock_guard<std::mutex> state(mutex_);
   for(auto& range:ranges_)if(range.address==address && range.bytes==bytes){
    if(range.image!=prior || range.generation!=expected)return 88;
    released=std::move(range.owner);range.owner=std::move(owner);range.image=image;range.generation=generation;return 0;
   }
  }return 62;
 }
 // Publish a fully fenced staged result overlapping only the beginning of
 // one exclusion lease. The submitting guest is blocked in GnmSubmit; other
 // guest consumers must obey the game's GPU/EOS synchronization. This mutex
 // serializes host mappings, not unsynchronized guest CPU instructions, and
 // this transfer is NOT claimed atomic against such instructions.
 int publish_overlap_prefix(uint64_t address,const std::vector<unsigned char>& initial,
                            const std::vector<unsigned char>& result,
                            uint64_t guard,uint64_t image,uint64_t generation){
  SYSTEM_INFO system{};GetSystemInfo(&system);const uint64_t page=system.dwPageSize,bytes=result.size();
  if(!page || !address || !guard || !image || !generation || generation==UINT64_MAX || initial.size()!=bytes || !bytes || bytes>32*1024*1024 || address>UINT64_MAX-bytes || address>=guard || address+bytes<=guard || guard%page || (address+bytes)%page)return 60;
  const uint64_t prefix=guard-address,overlap=bytes-prefix;
  const auto host=reinterpret_cast<uint64_t>(result.data());
  if(host>UINT64_MAX-bytes || (host<address+bytes && address<host+bytes))return 60;
  std::lock_guard<std::mutex> state(mutex_);
  Range* found=nullptr;for(auto& range:ranges_)if(address<range.address+range.bytes && range.address<address+bytes){if(found || range.address!=guard)return 88;found=&range;}
  if(!found)return 62;
  if(found->image!=image || found->generation!=generation || !found->owner || overlap>=found->bytes)return 88;
  std::lock_guard<std::mutex> lock(owned_output_lock);bool owned=false;
  for(const auto& item:owned_output_extents)for(const auto& extent:item.second)if(address>=extent.first && address+bytes<=extent.second)owned=true;
  if(!owned)return 62;
  for(unsigned part=0;part<2;++part){const uint64_t begin=part?guard:address,end=part?found->address+found->bytes:guard;const DWORD protection=part?PAGE_NOACCESS:PAGE_READWRITE;
   for(uint64_t cursor=begin;cursor<end;){MEMORY_BASIC_INFORMATION info{};
    if(!VirtualQuery(reinterpret_cast<void*>(cursor),&info,sizeof(info)) || info.State!=MEM_COMMIT || info.Protect!=protection)return 62;
    const uint64_t next=reinterpret_cast<uint64_t>(info.BaseAddress)+info.RegionSize;if(next<=cursor)return 62;cursor=std::min(next,end);
   }
  }
  if(std::memcmp(reinterpret_cast<const void*>(address),initial.data(),static_cast<size_t>(prefix)))return 88;
  // All fallible allocation/conversion/ownership checks precede permission
  // change. No stale excluded bytes are read. Retain the exact suffix owner.
  DWORD old=0;if(!VirtualProtect(reinterpret_cast<void*>(guard),overlap,PAGE_READWRITE,&old))return 62;
  std::memcpy(reinterpret_cast<void*>(address),result.data(),static_cast<size_t>(bytes));
  found->address+=overlap;found->bytes-=overlap;++found->generation;
  // Old full-range lookup now fails; remaining pages stay excluded. A failure
  // after this point is a poisoned publication, never an EOS/success receipt.
  return std::memcmp(reinterpret_cast<const void*>(address),result.data(),static_cast<size_t>(bytes))?112:0;
 }
 // A completed whole GPU writer can replace an excluded suffix and extend
 // exclusion into adjacent owned RW pages. No CPU publication or old pixel reuse.
 int replace_extend(uint64_t address,uint64_t bytes,uint64_t prior,uint64_t expected,
                    uint64_t image,uint64_t generation,std::shared_ptr<void> owner,
                    const std::vector<unsigned char>& prefix_prior,std::vector<unsigned char>& output){
  SYSTEM_INFO system{};GetSystemInfo(&system);const uint64_t page=system.dwPageSize;
  if(!page || !address || !bytes || address%page || bytes%page || bytes>32*1024*1024 || address>UINT64_MAX-bytes || !image || !owner || generation<=expected)return 60;
  std::shared_ptr<void> released;
  {std::lock_guard<std::mutex> state(mutex_);Range* found=nullptr;
   for(auto& range:ranges_)if(address<range.address+range.bytes && range.address<address+bytes){if(found || range.address!=address)return 88;found=&range;}
   if(!found || found->image!=prior || found->generation!=expected || !found->owner || found->bytes>=bytes || prefix_prior.size()!=found->bytes)return 88;
   std::lock_guard<std::mutex> owned_lock(owned_output_lock);bool owned=false;
   for(const auto& item:owned_output_extents)for(const auto& extent:item.second)if(address>=extent.first && address+bytes<=extent.second)owned=true;
   if(!owned)return 62;
   for(unsigned part=0;part<2;++part){const uint64_t begin=part?address+found->bytes:address,end=part?address+bytes:address+found->bytes;
    for(uint64_t cursor=begin;cursor<end;){MEMORY_BASIC_INFORMATION info{};
     if(!VirtualQuery(reinterpret_cast<void*>(cursor),&info,sizeof(info)) || info.State!=MEM_COMMIT || info.Protect!=DWORD(part?PAGE_READWRITE:PAGE_NOACCESS))return 62;
     const uint64_t next=reinterpret_cast<uint64_t>(info.BaseAddress)+info.RegionSize;if(next<=cursor)return 62;cursor=std::min(next,end);
    }
   }
   std::vector<unsigned char> backing;try{backing.resize(static_cast<size_t>(bytes));}catch(...){return 112;}
   std::copy(prefix_prior.begin(),prefix_prior.end(),backing.begin());
   std::memcpy(backing.data()+found->bytes,reinterpret_cast<const void*>(address+found->bytes),static_cast<size_t>(bytes-found->bytes));
   DWORD old=0;if(!VirtualProtect(reinterpret_cast<void*>(address+found->bytes),bytes-found->bytes,PAGE_NOACCESS,&old))return 62;
   released=std::move(found->owner);found->owner=std::move(owner);found->bytes=bytes;found->image=image;found->generation=generation;output.swap(backing);
  }released.reset();return 0;
 }
 // Shutdown only: after guest execution has joined. This restores allocation
 // permissions for destruction; it never grants a guest-visible publication.
 int shutdown(){std::vector<std::shared_ptr<void>> released;int status=0;
  {std::lock_guard<std::mutex> state(mutex_);std::lock_guard<std::mutex> lock(owned_output_lock);
   try{released.reserve(ranges_.size());}catch(...){return 112;}
   for(auto it=ranges_.begin();it!=ranges_.end();){DWORD old=0;if(!VirtualProtect(reinterpret_cast<void*>(it->address),it->bytes,PAGE_READWRITE,&old)){status=62;++it;}else {released.push_back(std::move(it->owner));it=ranges_.erase(it);}}
  }
  // Provider destructors may revoke owned memory or query this registry.
  // Release them only after both runtime ownership locks have been dropped.
  released.clear();return status;
 }
 bool diagnose(EXCEPTION_POINTERS* exception)const{
  if(!exception || !exception->ExceptionRecord || !exception->ContextRecord)return false;
  const auto& record=*exception->ExceptionRecord;if(record.ExceptionCode!=EXCEPTION_ACCESS_VIOLATION || record.NumberParameters<2 || record.ExceptionInformation[0]>1)return false;
  uint64_t image=0,generation=0,begin=0,bytes=0;const uint64_t address=record.ExceptionInformation[1];if(!identify(address,image,generation,begin,bytes))return false;
  char output[512];const int count=snprintf(output,sizeof(output),"{\"scope\":\"GPU_resident_CPU_access_missing\",\"address\":%llu,\"image\":%llu,\"generation\":%llu,\"caller\":%llu,\"operation\":%llu,\"status\":88}\n",static_cast<unsigned long long>(address),static_cast<unsigned long long>(image),static_cast<unsigned long long>(generation),static_cast<unsigned long long>(exception->ContextRecord->Rip),static_cast<unsigned long long>(record.ExceptionInformation[0]));
  DWORD written=0;if(count>0 && count<int(sizeof(output)))WriteFile(GetStdHandle(STD_ERROR_HANDLE),output,DWORD(count),&written,nullptr);return true;
 }
 size_t size()const{std::lock_guard<std::mutex> state(mutex_);return ranges_.size();}
};
inline GpuResidentMemory runtime_gpu_resident_memory;
// Actual SEH read/write probes; no repaired data or instruction-pointer changes.
inline int gpu_resident_access_probe(volatile unsigned char* pointer,bool write,const GpuResidentMemory* resident,uint64_t* address,uint64_t* caller,uint64_t* operation){
 __try {if(write)*pointer=0x99;else {const volatile auto value=*pointer;(void)value;}}
 __except((GetExceptionCode()==EXCEPTION_ACCESS_VIOLATION && (!resident || resident->diagnose(GetExceptionInformation())))?(*address=GetExceptionInformation()->ExceptionRecord->ExceptionInformation[1],*caller=GetExceptionInformation()->ContextRecord->Rip,*operation=GetExceptionInformation()->ExceptionRecord->ExceptionInformation[0],EXCEPTION_EXECUTE_HANDLER):EXCEPTION_CONTINUE_SEARCH){return 62;}return 0;
}
inline int gpu_replace_extend_control(){
 GuestStack allocation(32768,4096);auto* data=static_cast<unsigned char*>(allocation.bottom());std::fill_n(data,8192,static_cast<unsigned char>(0x25));std::fill_n(data+8192,4096,static_cast<unsigned char>(0x39));
 const uint64_t address=reinterpret_cast<uint64_t>(data);GpuResidentMemory resident;
 auto old=std::make_shared<int>(7);std::weak_ptr<int> released=old;auto next=std::make_shared<int>(9);
 if(resident.protect(address,8192,7,5,old))return 1;old.reset();
 std::vector<unsigned char> prior(8192,0x25),output{0x79};unsigned rejected=0;
 if(resident.replace_extend(address,12288,7,4,9,6,next,prior,output)!=88)return 2;++rejected;
 if(resident.replace_extend(address,12288,8,5,9,6,next,prior,output)!=88)return 3;++rejected;
 if(resident.replace_extend(address,8192,7,5,9,6,next,prior,output)!=88)return 4;++rejected;
 auto short_prior=prior;short_prior.pop_back();if(resident.replace_extend(address,12288,7,5,9,6,next,short_prior,output)!=88)return 5;++rejected;
 if(resident.replace_extend(address+1,12288,7,5,9,6,next,prior,output)!=60)return 6;++rejected;
 if(output!=std::vector<unsigned char>{0x79} || released.expired() || data[8192]!=0x39)return 7;
 if(resident.replace_extend(address,12288,7,5,9,6,next,prior,output) || !released.expired() || output.size()!=12288)return 8;
 if(!std::all_of(output.begin(),output.begin()+8192,[](auto b){return b==0x25;}) || !std::all_of(output.begin()+8192,output.end(),[](auto b){return b==0x39;}))return 9;
 uint64_t image=0,generation=0;std::shared_ptr<void> owner;
 if(resident.resolve(address,8192,image,generation,owner)!=88 || owner || resident.resolve(address,12288,image,generation,owner) || image!=9 || generation!=6 || owner!=next)return 10;
 for(auto offset:{0,8192,12287})for(bool write:{false,true}){uint64_t fault=0,caller=0,operation=9;if(gpu_resident_access_probe(data+offset,write,&resident,&fault,&caller,&operation)!=62 || fault!=address+offset)return 11;}
 if(resident.shutdown() || data[0]!=0x25 || data[8192]!=0x39)return 12;
 std::cout<<"{\"scope\":\"GPU_replace_extend_control\",\"passed\":true,\"negative_rejections\":"<<rejected<<",\"real_owned_pages\":true,\"read_write_faults\":6,\"old_owner_released\":true,\"old_extent_rejected\":true,\"prior_padding_only\":true,\"guest_publication\":false,\"GPU_execution\":false}\n";return 0;
}
inline int gpu_overlap_prefix_publication_control(){
 GuestStack allocation(32768,4096);auto* data=static_cast<unsigned char*>(allocation.bottom());std::fill_n(data,16384,static_cast<unsigned char>(0x25));
 const uint64_t address=reinterpret_cast<uint64_t>(data),guard=address+4096;
 GpuResidentMemory resident;auto lease=std::make_shared<int>(7);std::weak_ptr<int> retained=lease;
 if(resident.protect(guard,12288,7,4,lease))return 1;lease.reset();
 std::vector<unsigned char> initial(8192,0x25),result(8192,0x72);unsigned rejected=0;
 if(resident.publish_overlap_prefix(address,initial,result,guard,7,3)!=88)return 2;++rejected;
 if(resident.publish_overlap_prefix(address,initial,result,guard,8,4)!=88)return 3;++rejected;
 auto partial=result;partial.pop_back();if(resident.publish_overlap_prefix(address,initial,partial,guard,7,4)!=60)return 4;++rejected;
 auto changed=initial;changed[0]=0x26;if(resident.publish_overlap_prefix(address,changed,result,guard,7,4)!=88)return 5;++rejected;
 auto full=std::vector<unsigned char>(16384,0x72);if(resident.publish_overlap_prefix(address,full,full,guard,7,4)!=88)return 6;++rejected;
 if(resident.publish_overlap_prefix(address,initial,result,guard+4096,7,4)!=60)return 7;++rejected;
 uint64_t image=0,generation=0,begin=0,length=0;
 if(!resident.identify(guard,image,generation,begin,length) || generation!=4 || begin!=guard || length!=12288 || data[0]!=0x25)return 8;
 if(resident.publish_overlap_prefix(address,initial,result,guard,7,4) || !std::all_of(data,data+8192,[](auto b){return b==0x72;}))return 9;
 if(resident.identify(guard,image,generation,begin,length) || !resident.identify(guard+4096,image,generation,begin,length) || generation!=5 || begin!=guard+4096 || length!=8192 || retained.expired())return 10;
 std::shared_ptr<void> owner;if(resident.resolve(guard,12288,image,generation,owner)!=88 || owner)return 11;
 for(bool write:{false,true}){uint64_t fault=0,caller=0,operation=9;if(gpu_resident_access_probe(data+8192,write,&resident,&fault,&caller,&operation)!=62 || fault!=address+8192)return 12;}
 if(resident.shutdown() || !retained.expired() || data[8192]!=0x25 || data[16383]!=0x25)return 13;
 std::cout<<"{\"scope\":\"GPU_overlap_prefix_publication_control\",\"passed\":true,\"rejections\":"<<rejected<<",\"real_owned_pages\":true,\"suffix_read_write_faults\":2,\"old_full_alias_rejected\":true,\"suffix_owner_retained\":true,\"synthetic_result_bytes\":true,\"GPU_execution\":false,\"atomic_against_unsynchronized_guest\":false}\n";return 0;
}
inline int gpu_resident_memory_control(){
 GuestStack allocation(32768,4096);auto* bytes=static_cast<unsigned char*>(allocation.bottom());std::fill_n(bytes,8192,static_cast<unsigned char>(0x25));
 const uint64_t address=reinterpret_cast<uint64_t>(bytes);auto lease=std::make_shared<int>(7);std::weak_ptr<int> retained=lease;GpuResidentMemory resident;
 if(resident.protect(address+1,4096,7,1,lease)!=60 || resident.protect(address,4095,7,1,lease)!=60 || resident.size())return 1;
 std::vector<unsigned char> prior{0x79};
 if(resident.protect(address,4096,7,1,lease,&prior) || prior.size()!=4096 || !std::all_of(prior.begin(),prior.end(),[](unsigned char b){return b==0x25;}))return 2;
 auto rejected_prior=prior;if(resident.protect(address,4096,8,1,lease,&rejected_prior)!=88 || rejected_prior!=prior)return 2;
 std::cout<<"{\"scope\":\"GPU_resident_prior_backing_control\",\"passed\":true,\"owned_bytes\":4096,\"capture_before_exclusion\":true,\"failed_output_unchanged\":true,\"GPU_coherence\":false}\n";

 lease.reset();if(retained.expired())return 3;
 for(bool write:{false,true}){uint64_t fault=0,caller=0,operation=9,image=0,generation=0,begin=0,length=0;
  if(gpu_resident_access_probe(bytes,write,&resident,&fault,&caller,&operation)!=62 || fault!=address || !caller || operation!=uint64_t(write) || !resident.identify(fault,image,generation,begin,length) || image!=7 || generation!=1 || begin!=address || length!=4096)return 4;
  std::cout<<"{\"scope\":\"GPU_resident_CPU_access_rejected\",\"address\":"<<fault<<",\"caller\":"<<caller<<",\"image\":"<<image<<",\"generation\":"<<generation<<",\"write\":"<<(write?"true":"false")<<",\"actual_SEH\":true}\n";
 }
 uint64_t resolved_image=99,resolved_generation=99;std::shared_ptr<void> resolved;
 if(resident.resolve(address,4095,resolved_image,resolved_generation,resolved)!=88 || resolved_image!=99 || resolved || resident.resolve(address+4096,4096,resolved_image,resolved_generation,resolved)!=62)return 7;
 if(resident.resolve(address,4096,resolved_image,resolved_generation,resolved) || resolved_image!=7 || resolved_generation!=1 || !resolved)return 8;resolved.reset();
 if(resident.advance(address,4096,7,1,2) || resident.advance(address,4096,7,1,3)!=88 || resident.advance(address,4096,7,2,2)!=60)return 9;
 uint64_t token=0,generation=0,begin=0,length=0;if(!resident.identify(address,token,generation,begin,length) || token!=7 || generation!=2)return 10;
 auto replacement=std::make_shared<int>(9);
 if(resident.replace(address,4096,7,1,9,3,replacement)!=88 || resident.replace(address,4096,8,2,9,3,replacement)!=88 || resident.replace(address,4095,7,2,9,3,replacement)!=62)return 13;
 if(resident.replace(address,4096,7,2,9,3,replacement) || !retained.expired() || !resident.identify(address,token,generation,begin,length) || token!=9 || generation!=3)return 14;
 for(bool write:{false,true}){uint64_t fault=0,caller=0,operation=9;if(gpu_resident_access_probe(bytes,write,&resident,&fault,&caller,&operation)!=62 || fault!=address)return 15;}
 unsigned char output=0x79;const auto memory=make_owned_guest_copy_api();if(!memory.read(memory.context,address,&output,1) || output!=0x79)return 5;
 bool destructor_reentered=false;auto reentrant=std::shared_ptr<void>(new int(8),[&](void* pointer){destructor_reentered=resident.size()==0;delete static_cast<int*>(pointer);});
 if(resident.protect(address+4096,4096,8,1,reentrant))return 11;reentrant.reset();
 if(resident.shutdown() || !destructor_reentered || resident.size() || !retained.expired() || bytes[0]!=0x25 || bytes[4096]!=0x25)return 12;
 if(bytes[4096]!=0x25 || resident.shutdown() || resident.size() || !retained.expired() || bytes[0]!=0x25 || resident.shutdown())return 6;
 std::cout<<"{\"scope\":\"GPU_resident_memory_control\",\"passed\":true,\"actual_read_and_write_faults\":true,\"lease_retains_generation\":true,\"partial_pages_rejected\":true,\"overlap_rejected\":true,\"host_copy_rejected\":true,\"exact_GPU_lookup\":true,\"stale_generation_rejected\":true,\"lease_destructor_reentrancy\":true,\"neighbor_untouched\":true,\"shutdown_restores_without_publication\":true,\"synthetic_memory\":true,\"actual_GPU_owner\":false}\n";return 0;
}
}
