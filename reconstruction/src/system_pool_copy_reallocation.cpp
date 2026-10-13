#include "ctr/system_pool_copy_reallocation.h"
#include "ctr/mspace_small_free.h"
#include <algorithm>
#include <array>
#include <cstring>
#include <stdexcept>
namespace ctr {
SystemPoolAllocation dispatch_copy_reallocation_system_pool(std::span<std::byte> pool,
 SystemPoolBackend& b,const SystemPoolAllocation& old,std::uint64_t request,
 std::uint32_t alignment,bool poison) {
 if(pool.size()!=176 || old.payload.empty() || old.payload.size()>192 || request<1 || request>192 ||
    (alignment!=16 && alignment!=32) || old.address!=old.raw_address+16 ||
    old.raw_address<0x60000010 || old.address-0x60000000+old.payload.size()>b.memory.bytes().size())
  throw std::invalid_argument("Copy reallocation requires one live small alignment8 allocation");
 auto get=[&](std::size_t at){std::uint64_t v;std::memcpy(&v,pool.data()+at,8);return v;};
 std::uint32_t minimum;std::memcpy(&minimum,pool.data()+0x30,4);
 if(get(0)!=0x025b9ca8 || get(0xa8)!=b.heap.header_token || get(0x10) || minimum!=8 ||
    pool[0x18]!=std::byte{1} || pool[0x4b]!=std::byte{0} || pool[0x4c]!=std::byte{0} || pool[0x4d]!=std::byte{0})
  throw std::invalid_argument("Default active concrete pool without owner/debug required");
 std::uint32_t metadata;std::uint64_t stored_request;
 auto raw=b.memory.bytes().data()+old.raw_address-0x60000000;
 std::uint64_t marker,top;std::memcpy(&marker,raw-8,8);
 std::memcpy(&top,b.memory.bytes().data()+b.heap.header_token-0x60000000+0x28,8);
 const auto chunk_size=marker&0xff00fffffffffff8ull;
 if(chunk_size>=256 || old.raw_address-16+chunk_size!=top)
  throw std::invalid_argument("Copy reallocation requires old small chunk directly before top");
 std::memcpy(&metadata,raw,4);std::memcpy(&stored_request,raw+8,8);
 if(metadata!=(16u<<11|1u) || stored_request!=old.payload.size() ||
    old.payload.data()!=raw+16 || get(0x50)!=1 || get(0x60)!=stored_request || get(0x70)!=old.usable_size)
  throw std::invalid_argument("Copy reallocation ownership or statistics differ");
 std::array<std::uint64_t,6> stats;std::memcpy(stats.data(),pool.data()+0x50,48);
 std::uint32_t calls;std::memcpy(&calls,pool.data()+0x80,4);++calls;std::memcpy(pool.data()+0x80,&calls,4);
 auto result=allocate_system_pool(pool,b,request,alignment,poison);
 std::memcpy(result.payload.data(),old.payload.data(),std::min<std::uint64_t>(stored_request,request));
 MspaceTopAllocationContext c{0x60000000,b.heap.header_token,poison,b.heap.mutex.get()};
 free_mspace_small(b.memory.bytes(),c,old.raw_address);
 // Original updates net totals after malloc/copy/free, without transient peaks.
 stats[2]=stats[2]-stored_request+request;stats[3]=std::max(stats[3],stats[2]);
 stats[4]=stats[4]-old.usable_size+result.usable_size;stats[5]=std::max(stats[5],stats[4]);
 std::memcpy(pool.data()+0x50,stats.data(),48);
 return result;
}
}
