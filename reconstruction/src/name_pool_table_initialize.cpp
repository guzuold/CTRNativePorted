#include "ctr/name_pool_table_initialize.h"
#include "ctr/detail/system_pool_allocate_impl.h"
#include <cstring>
#include <stdexcept>
namespace ctr {
SystemPoolAllocation initialize_created_name_pool_table(std::span<std::byte> object,
 std::span<const std::byte> manager,std::span<std::byte> pool,
 SystemPoolBackend& backend,bool poison){
 if(object.size()!=64||manager.size()!=112||manager[0x6c]!=std::byte{0}||pool.size()!=176)
  throw std::invalid_argument("Created context64/nonthreaded manager/System Pool required");
 std::uint64_t identity,old_table,selected,owner;std::uint32_t refs,buckets,alignment;
 std::memcpy(&identity,object.data(),8);std::memcpy(&refs,object.data()+8,4);std::memcpy(&old_table,object.data()+0x10,8);std::memcpy(&buckets,object.data()+0x18,4);
 std::memcpy(&selected,manager.data()+0x10,8);std::memcpy(&owner,pool.data()+0x10,8);std::memcpy(&alignment,pool.data()+0x30,4);
 const auto token=reinterpret_cast<std::uintptr_t>(pool.data());
 if(identity!=0x025c4510||(refs>>22)!=1||old_table||buckets!=1024||selected!=token||owner||alignment!=8||pool[0x4b]!=std::byte{0}||pool[0x4c]!=std::byte{0}||pool[0x4d]!=std::byte{0})
  throw std::invalid_argument("Initial1024 buckets/concrete pool1/default allocation required");
 std::memcpy(object.data()+0x38,&selected,8);
 std::uint32_t calls;std::memcpy(&calls,pool.data()+0x80,4);++calls;std::memcpy(pool.data()+0x80,&calls,4);
 auto result=detail::allocate_system_pool_bounded(pool,backend,8192,8,poison,8192);
 std::memset(result.payload.data(),0,8192);
 std::memcpy(object.data()+0x10,&result.address,8);
 return result; // Logical allocation output; original effects are object/table storage.
}
}
