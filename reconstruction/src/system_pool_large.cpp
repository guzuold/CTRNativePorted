#include "ctr/system_pool_large.h"
#include "ctr/detail/system_pool_allocate_impl.h"
#include <cstring>
#include <stdexcept>
namespace ctr {
SystemPoolAllocation dispatch_system_pool_resource_backing(std::span<std::byte> pool,
 SystemPoolBackend& b,std::uint64_t request,std::uint32_t alignment,bool poison){
 if(pool.size()!=176 || request!=8192 || alignment<8 || alignment>256 || (alignment&(alignment-1)))
  throw std::invalid_argument("Separate8192-byte resource backing contract required");
 std::uint64_t owner,identity,header;std::uint32_t minimum;
 std::memcpy(&owner,pool.data()+0x10,8);std::memcpy(&identity,pool.data(),8);std::memcpy(&header,pool.data()+0xa8,8);std::memcpy(&minimum,pool.data()+0x30,4);
 if(identity!=0x025b9ca8 || header!=b.heap.header_token || owner || minimum!=8 || pool[0x18]!=std::byte{1} ||
    pool[0x4b]!=std::byte{0} || pool[0x4c]!=std::byte{0} || pool[0x4d]!=std::byte{0})
  throw std::invalid_argument("Active concrete pool with absent owner/debug required");
 std::uint32_t calls;std::memcpy(&calls,pool.data()+0x80,4);++calls;std::memcpy(pool.data()+0x80,&calls,4);
 // Original concrete +130 ->014d81d0 null direct tail ->014d8070.
 return detail::allocate_system_pool_bounded(pool,b,request,alignment,poison,8192);
}
}
