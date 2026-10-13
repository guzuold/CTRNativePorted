#include "ctr/system_pool_allocate.h"
#include "ctr/detail/system_pool_allocate_impl.h"
#include "ctr/mspace_allocate.h"
#include "ctr/mspace_usable_size.h"
#include <algorithm>
#include <cstring>
#include <stdexcept>
namespace ctr {
SystemPoolAllocation detail::allocate_system_pool_bounded(std::span<std::byte> pool,SystemPoolBackend& b,
 std::uint64_t request,std::uint32_t alignment,bool poison,std::uint64_t limit){
 if(pool.size()!=176 || !request || request>limit || alignment<8 || alignment>256 || (alignment&(alignment-1)))
  throw std::invalid_argument("Positive bounded request and power-of-two alignment required");
 auto get=[&](std::size_t offset){std::uint64_t v;std::memcpy(&v,pool.data()+offset,8);return v;};
 auto put=[&](std::size_t offset,std::uint64_t v){std::memcpy(pool.data()+offset,&v,8);};
 std::uint32_t type;std::memcpy(&type,pool.data()+0x1c,4);
 if(get(0)!=0x025b9ca8 || get(0xa8)!=b.heap.header_token || pool[0x18]!=std::byte{1} ||
    pool[0x4b]!=std::byte{0} || type!=1)
  throw std::invalid_argument("Constructed concrete system pool with real heap required");
 const auto bytes=b.memory.bytes();const auto base=reinterpret_cast<std::uint64_t>(bytes.data());
 const auto pool_address=reinterpret_cast<std::uint64_t>(pool.data());
 if(pool_address<base || pool_address-base>bytes.size() || pool.size()>bytes.size()-(pool_address-base))
  throw std::invalid_argument("Pool not owned by actual backend");
 const auto total=request+16+(alignment>8?alignment-1:0);
 const MspaceTopAllocationContext c{base,b.heap.header_token,poison,b.heap.mutex.get()};
 const auto raw=allocate_mspace_top(bytes,c,total);
 if(!raw.address_token)throw std::invalid_argument("Original allocator signature rejection outside successful wrapper contract");
 const auto address=(raw.address_token+alignment-1+16)&~static_cast<std::uint64_t>(alignment-1);
 const auto offset=static_cast<std::uint32_t>(address-raw.address_token);
 if(address-(raw.address_token+16)>3){
  std::uint32_t first;std::memcpy(&first,raw.payload.data(),4);first=(first&0x7ff)|(offset<<11);std::memcpy(raw.payload.data(),&first,4);
 }
 const std::uint32_t metadata=(type&0x3ff)|(alignment>8?0x400:0)|(offset<<11);
 std::memcpy(bytes.data()+address-base-16,&metadata,4);std::memcpy(bytes.data()+address-base-8,&request,8);
 const auto usable=mspace_usable_size(bytes,static_cast<std::size_t>(raw.address_token-base));
 const auto occupied=get(0x70)+usable;put(0x70,occupied);put(0x78,std::max(get(0x78),occupied));
 const auto requested=get(0x60)+request;put(0x60,requested);put(0x68,std::max(get(0x68),requested));
 const auto count=get(0x50)+1;put(0x50,count);put(0x58,std::max(get(0x58),count));
 return {raw.address_token,address,usable,bytes.subspan(static_cast<std::size_t>(address-base),static_cast<std::size_t>(request))};
}
SystemPoolAllocation allocate_system_pool(std::span<std::byte> pool,SystemPoolBackend& b,
 std::uint64_t request,std::uint32_t alignment,bool poison){
 return detail::allocate_system_pool_bounded(pool,b,request,alignment,poison,512);
}
}
