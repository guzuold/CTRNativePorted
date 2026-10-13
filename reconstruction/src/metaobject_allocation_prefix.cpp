#include "ctr/metaobject_allocation_prefix.h"
#include "ctr/metaobject_constructor_storage.h"
#include <cstring>
#include <stdexcept>
namespace ctr {
SystemPoolAllocation allocate_cold_metaobject_prefix(std::span<const std::byte> descriptor,
 std::span<const std::byte> manager,std::span<std::byte> pool,SystemPoolBackend& backend,bool poison){
 if(descriptor.size()!=176||descriptor[0]!=std::byte{0}||manager.size()!=112||manager[0x6c]!=std::byte{0}||pool.size()!=176)
  throw std::invalid_argument("Cold descriptor and nonthreaded prepared manager/pool required");
 std::uint64_t selected;std::memcpy(&selected,manager.data()+0x10,8);
 if(selected!=reinterpret_cast<std::uintptr_t>(pool.data()))throw std::invalid_argument("Constructor selector1 must resolve the supplied pool");
 std::int32_t index;std::memcpy(&index,pool.data()+0x1c,4);
 auto allocation=dispatch_null_reallocation_system_pool(pool,backend,152,8,poison);
 initialize_metaobject_constructor_storage(allocation.payload,index);
 return allocation;
}
SystemPoolAllocation create_cold_metaobject(std::span<const std::byte> descriptor,
 std::span<const std::byte> manager,std::span<std::byte> pool,SystemPoolBackend& backend,bool poison){
 auto allocation=allocate_cold_metaobject_prefix(descriptor,manager,pool,backend,poison);
 // Concrete original virtual+40 is the verified single RET014d7730.
 // No generic query/virtual factory service is represented by this base initializer.
 return allocation;
}
}
