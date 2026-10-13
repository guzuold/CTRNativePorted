#include "ctr/system_pool_mode1.h"
#include <cstring>
#include <stdexcept>
namespace ctr {
SystemPoolAllocation reallocate_system_pool_from_null(std::span<std::byte> pool,
 SystemPoolBackend& b,std::uint64_t request,std::uint32_t alignment,bool poison){
 //014d81d0 old-null branch014d837e tail-jumps directly to014d8070.
 return allocate_system_pool(pool,b,request,alignment,poison);
}
SystemPoolAllocation dispatch_null_reallocation_system_pool(std::span<std::byte> pool,
 SystemPoolBackend& b,std::uint64_t request,std::uint32_t alignment,bool poison){
 if(pool.size()!=176 || !request || request>512 || alignment<8 || alignment>256 || (alignment&(alignment-1)))
  throw std::invalid_argument("Bounded positive new-allocation dispatcher contract required");
 std::uint64_t owner,identity,header;std::uint32_t minimum;
 std::memcpy(&owner,pool.data()+0x10,8);std::memcpy(&identity,pool.data(),8);std::memcpy(&header,pool.data()+0xa8,8);std::memcpy(&minimum,pool.data()+0x30,4);
 if(identity!=0x025b9ca8 || header!=b.heap.header_token || owner || minimum!=8 || pool[0x18]!=std::byte{1} ||
    pool[0x4b]!=std::byte{0} || pool[0x4c]!=std::byte{0} || pool[0x4d]!=std::byte{0})
  throw std::invalid_argument("Active concrete pool, absent owner/debug and default minimum alignment required");
 // Original owner provider014d7fe0 returns null whether +38 is clear or +10 is null.
 std::uint32_t calls;std::memcpy(&calls,pool.data()+0x80,4);++calls;std::memcpy(pool.data()+0x80,&calls,4);
 // Native direct call implements only the verified concrete +130 override and014d81d0 null tail, not a generic vtable.
 return reallocate_system_pool_from_null(pool,b,request,alignment,poison);
}
}

