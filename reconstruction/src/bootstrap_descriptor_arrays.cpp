#include "ctr/bootstrap_descriptor_arrays.h"
#include "ctr/detail/system_pool_allocate_impl.h"
#include <cstring>
#include <stdexcept>
namespace ctr {
SystemPoolAllocation create_bootstrap_secondary_array(std::span<std::byte> pool,
 SystemPoolBackend& backend,bool poison){
 auto second=dispatch_null_reallocation_system_pool(pool,backend,24,8,poison);
 const std::uint32_t zero=0;const std::uint64_t initial=0x0010000000000002,empty=0;
 std::memcpy(second.payload.data(),&zero,4);std::memcpy(second.payload.data()+8,&initial,8);std::memcpy(second.payload.data()+16,&empty,8);
 std::uint32_t calls;std::memcpy(&calls,pool.data()+0x80,4);++calls;std::memcpy(pool.data()+0x80,&calls,4);
 auto buffer=detail::allocate_system_pool_bounded(pool,backend,8192,8,poison,8192);
 const std::uint64_t metadata=0x0010000000002000;
 std::memcpy(second.payload.data()+8,&metadata,8);std::memcpy(second.payload.data()+16,&buffer.address,8);
 return second;
}
void publish_bootstrap_array(std::span<std::byte> descriptor,const SystemPoolAllocation& allocation,bool secondary){
 if(descriptor.size()!=176||allocation.payload.size()!=24)throw std::invalid_argument("Descriptor176/array24 required");
 std::memcpy(descriptor.data()+(secondary?0x20:0x28),&allocation.address,8);
}
SystemPoolAllocation construct_bootstrap_descriptor_arrays(std::span<std::byte> descriptor,
 SystemPoolBackend& backend,std::span<std::byte> pool,bool poison){
 if(descriptor.size()!=176)throw std::invalid_argument("Descriptor176 required");
 auto first=create_bootstrap_pointer_array(pool,backend,poison);publish_bootstrap_array(descriptor,first,false);
 auto second=create_bootstrap_secondary_array(pool,backend,poison);publish_bootstrap_array(descriptor,second,true);
 return second;
}
}
