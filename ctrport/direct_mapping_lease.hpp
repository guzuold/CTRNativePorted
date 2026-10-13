// SPDX-License-Identifier: GPL-2.0-only
#pragma once
#include "direct_memory_copy.hpp"
#include <future>
namespace ctrport {
// Runtime mapping lifetime for a renderer registration. No raw backing escapes
// without its shared owner. Current runtime has no successful unmap/free service;
// future revocation MUST invalidate/wait these leases before altering mappings.
// This lease does not serialize original guest stores or GPU work and therefore
// is never a frame-ready/coherent-snapshot token.
class DirectMappingLease {
    std::shared_ptr<DirectMemoryBacking> owner;
    std::uint64_t address,bytes;
public:
    DirectMappingLease(std::shared_ptr<DirectMemoryBacking> backing,std::uint64_t start,std::uint64_t size)
        :owner(std::move(backing)),address(start),bytes(size) {
        if(!valid())throw std::runtime_error("Direct mapping lease rejected");
    }
    bool valid() const{return owner && owner->contains_mapped_range(address,bytes);}
    // Actual aligned CPU publication only. Caller must first wait its genuine GPU
    // completion; this operation does not establish or simulate GPU completion.
    int store_u64_release(std::uint64_t offset,std::uint64_t value) const noexcept {
        if(offset>=bytes || 8>bytes-offset || ((address+offset)&7))return 1;
        try {
            auto transaction=owner->copy_transaction();
            if(!owner->contains_mapped_range(address+offset,8))return 1;
            LONG64 signed_value=0;std::memcpy(&signed_value,&value,8);
            InterlockedExchange64(reinterpret_cast<volatile LONG64*>(address+offset),signed_value);
            return 0;
        }catch(...){return 1;}
    }

    // Authority remains bounded to this range even when the backing has others.
    int read(std::uint64_t offset,void* output,std::uint64_t size) const noexcept {
        if(!size || offset>=bytes || size>bytes-offset)return 1;
        return direct_copy_read(owner.get(),address+offset,output,size);
    }
};
inline int direct_mapping_publication_control(){
 auto owner=std::make_shared<DirectMemoryBacking>(16384);std::weak_ptr<DirectMemoryBacking> weak=owner;
 std::int64_t physical=-1;void* mapping=nullptr;
 if(owner->allocate(0,16384,16384,0,0,&physical) || owner->map(&mapping,16384,51,0,physical,0))return 1;
 const auto address=reinterpret_cast<std::uint64_t>(mapping);
 unsigned rejected=0;
 {
  DirectMappingLease lease(owner,address,64);owner.reset();if(weak.expired())return 1;
  const uint64_t sentinel=0x123456789abcdef0ULL;
  if(lease.store_u64_release(0,sentinel))return 1;
  for(auto offset:{4ULL,60ULL,64ULL,UINT64_MAX})if(lease.store_u64_release(offset,1))++rejected;
  DWORD previous=0;if(!VirtualProtect(mapping,16384,PAGE_READONLY,&previous))return 1;
  if(lease.store_u64_release(0,1))++rejected;
  if(*static_cast<const uint64_t*>(mapping)!=sentinel)return 1;
  if(!VirtualProtect(mapping,16384,previous,&previous))return 1;
  std::promise<void> release;auto gate=release.get_future();int stored=1;
  std::thread worker([&]{gate.wait();stored=lease.store_u64_release(0,UINT64_MAX);});
  const bool unchanged=InterlockedCompareExchange64(static_cast<volatile LONG64*>(mapping),0,0)==static_cast<LONG64>(sentinel);
  release.set_value();worker.join();
  if(!unchanged || stored || InterlockedCompareExchange64(static_cast<volatile LONG64*>(mapping),0,0)!=-1)return 1;
 }
 if(!weak.expired() || rejected!=5)return 1;
 std::cout<<"{\"scope\":\"direct_mapping_atomic_publication_control\",\"negative_rejections\":5,\"owner_retained\":true,\"wait_before_store\":true,\"stored_u64_all_bits\":true,\"gpu_completion_claimed\":false}\n";return 0;
}
inline int direct_mapping_lease_control(){
    auto owner=std::make_shared<DirectMemoryBacking>(65536);std::weak_ptr<DirectMemoryBacking> weak=owner;
    std::int64_t physical=-1;void* mapping=nullptr;
    if(owner->allocate(0,65536,16384,0,0,&physical) || owner->map(&mapping,16384,51,0,physical,0))return 1;
    const auto address=reinterpret_cast<std::uint64_t>(mapping);
    unsigned rejected=0;
    for(const auto range:std::array<std::pair<uint64_t,uint64_t>,4>{{{address,0},{address+16384,1},{address+16380,8},{UINT64_MAX-3,8}}}) {
        try{DirectMappingLease bad(owner,range.first,range.second);}catch(const std::runtime_error&){++rejected;}
    }
    {
        DirectMappingLease lease(owner,address,64);owner.reset();
        if(weak.expired() || !lease.valid())return 1;
        unsigned char output[8];std::memset(output,0x79,sizeof(output));
        if(!lease.read(60,output,8))return 1;
        for(auto value:output)if(value!=0x79)return 1;
        ++rejected;
        if(lease.read(0,output,8))return 1;
        DWORD old=0;if(!VirtualProtect(mapping,16384,PAGE_READONLY,&old))return 1;
        if(lease.valid())return 1;++rejected;
        if(!VirtualProtect(mapping,16384,old,&old) || !lease.valid())return 1;
    }
    if(!weak.expired() || rejected!=6)return 1;
    // Actual observed two-buffer stride, synthetic owned mapping; no game pixels.
    constexpr uint64_t stride=8355840;
    auto large=std::make_shared<DirectMemoryBacking>(stride*2);physical=-1;mapping=nullptr;
    if(large->allocate(0,stride*2,stride*2,0,0,&physical) || large->map(&mapping,stride*2,51,0,physical,0))return 1;
    const auto base=reinterpret_cast<uint64_t>(mapping);
    DirectMappingLease first(large,base,stride),second(large,base+stride,stride);
    if(!first.valid() || !second.valid() || large->contains_mapped_range(base+stride,stride+1))return 1;
    auto threaded=std::make_shared<DirectMemoryBacking>(16384);physical=-1;mapping=nullptr;
    if(threaded->allocate(0,16384,16384,0,0,&physical) || threaded->map(&mapping,16384,51,0,physical,0))return 1;
    std::weak_ptr<DirectMemoryBacking> threaded_weak=threaded;
    std::promise<void> ready,release;auto ready_future=ready.get_future(),release_future=release.get_future();
    bool read_ok=false;
    std::thread worker([lease=DirectMappingLease(threaded,reinterpret_cast<uint64_t>(mapping),64),&ready,&release_future,&read_ok]{
        ready.set_value();release_future.wait();unsigned char bytes[8]{};read_ok=lease.valid() && !lease.read(0,bytes,sizeof(bytes));
    });
    ready_future.wait();threaded.reset();const bool kept=!threaded_weak.expired();release.set_value();worker.join();
    if(!kept || !read_ok || !threaded_weak.expired())return 1;
    std::cout<<"{\"scope\":\"direct_mapping_registration_lease_control\",\"retained_after_global_release\":true,\"released_after_lease\":true,\"owner_thread_retention\":true,\"negative_rejections\":6,\"failed_read_untouched\":true,\"two_buffer_stride_bytes\":8355840,\"synthetic_mapping\":true,\"GPU_coherence\":false,\"game_frame\":false}\n";return 0;
}
}
