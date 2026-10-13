#include "ctr/descriptor_manager_allocation.h"
#include <cstring>
#include <stdexcept>
namespace ctr {
SystemPoolAllocation allocate_warm_descriptor_manager(std::span<const std::byte> descriptor,
 std::span<const std::byte> manager,std::span<std::byte> pool,SystemPoolBackend& backend,bool poison){
 const auto selected=select_descriptor_manager_pool(descriptor,manager,1);
 if(pool.size()!=176||selected!=reinterpret_cast<std::uintptr_t>(pool.data()))throw std::invalid_argument("Original selected pool must be the supplied actual System Pool");
 std::uint32_t alignment;std::memcpy(&alignment,pool.data()+0x30,4);
 return dispatch_null_reallocation_system_pool(pool,backend,112,alignment,poison);
}
}
