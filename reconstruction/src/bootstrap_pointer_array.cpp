#include "ctr/bootstrap_pointer_array.h"
#include "ctr/detail/system_pool_allocate_impl.h"
#include <cstring>
namespace ctr {
SystemPoolAllocation create_bootstrap_pointer_array(std::span<std::byte> pool,
 SystemPoolBackend& backend,bool poison){
 auto header=dispatch_null_reallocation_system_pool(pool,backend,24,8,poison);
 const std::uint32_t zero=0;const std::uint64_t initial=0x0010000000000002,empty=0;
 std::memcpy(header.payload.data(),&zero,4);
 std::memcpy(header.payload.data()+8,&initial,8);std::memcpy(header.payload.data()+16,&empty,8);
 // Second original014dabb0 dispatch uses the same concrete pool and null old pointer.
 std::uint32_t calls;std::memcpy(&calls,pool.data()+0x80,4);++calls;std::memcpy(pool.data()+0x80,&calls,4);
 auto buffer=detail::allocate_system_pool_bounded(pool,backend,1024,8,poison,1024);
 const std::uint64_t metadata=0x0010000000000400;
 std::memcpy(header.payload.data()+8,&metadata,8);
 std::memcpy(header.payload.data()+16,&buffer.address,8);
 return header;
}
}
