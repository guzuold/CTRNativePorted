#include "ctr/system_pool_free.h"
#include "ctr/mspace_free.h"
#include "ctr/mspace_usable_size.h"
#include <algorithm>
#include <cstring>
#include <stdexcept>
namespace ctr {
std::uint64_t system_pool_requested_size(std::span<const std::byte> bytes,std::uint64_t address){
 const auto base=reinterpret_cast<std::uint64_t>(bytes.data());
 if(address<base || address-base<8 || address-base>bytes.size())throw std::invalid_argument("Size getter address outside backing");
 std::uint64_t requested;std::memcpy(&requested,bytes.data()+address-base-8,8);return requested;
}
std::uint32_t free_system_pool_top(std::span<std::byte> pool,SystemPoolBackend& b,
 std::uint64_t address,bool poison){
 if(pool.size()!=176)throw std::invalid_argument("Concrete pool storage required");
 auto get=[&](std::size_t offset){std::uint64_t v;std::memcpy(&v,pool.data()+offset,8);return v;};
 auto put=[&](std::size_t offset,std::uint64_t v){std::memcpy(pool.data()+offset,&v,8);};
 const auto bytes=b.memory.bytes();const auto base=reinterpret_cast<std::uint64_t>(bytes.data());
 const auto pool_address=reinterpret_cast<std::uint64_t>(pool.data());
 if(pool_address<base || pool_address-base>bytes.size() || pool.size()>bytes.size()-(pool_address-base) ||
    get(0)!=0x025b9ca8 || get(0xa8)!=b.heap.header_token || pool[0x18]!=std::byte{1} || pool[0x4b]!=std::byte{0} ||
    address<base || address-base<16 || address-base>bytes.size())throw std::invalid_argument("Actual constructed pool and live payload required");
 const auto request=system_pool_requested_size(bytes,address);std::uint32_t metadata,type;
 std::memcpy(&metadata,bytes.data()+address-base-16,4);std::memcpy(&type,pool.data()+0x1c,4);
 const auto displacement=metadata>>11;
 if(!request || request>512 || type!=1 || (metadata&0x3ff)!=type || displacement<16 || displacement>address-base)
  throw std::invalid_argument("Live positive bounded wrapper metadata required");
 const auto raw=address-displacement;
 const auto usable=mspace_usable_size(bytes,static_cast<std::size_t>(raw-base));
 const MspaceTopAllocationContext c{base,b.heap.header_token,poison,b.heap.mutex.get()};
 free_mspace_top(bytes,c,raw); // Validates/commits actual heap free before wrapper statistic stores.
 const auto occupied=get(0x70)-usable;put(0x70,occupied);put(0x78,std::max(get(0x78),occupied));
 const auto requested=get(0x60)-request;put(0x60,requested);put(0x68,std::max(get(0x68),requested));
 put(0x50,get(0x50)-1);return 0;
}
}
