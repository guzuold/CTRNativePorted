#include "ctr/metaobject_base_context.h"
#include <cstring>
#include <stdexcept>
namespace ctr {
SystemPoolAllocation create_metaobject_base_context(std::span<const std::byte> manager,
 std::span<std::byte> pool,SystemPoolBackend& backend,bool poison){
 if(manager.size()!=112||manager[0x6c]!=std::byte{0}||pool.size()!=176)throw std::invalid_argument("Nonthreaded manager112/concrete pool176 required");
 std::uint64_t selected;std::memcpy(&selected,manager.data()+0x10,8);
 if(selected!=reinterpret_cast<std::uintptr_t>(pool.data()))throw std::invalid_argument("Selector1 must resolve supplied pool");
 std::uint32_t alignment;std::memcpy(&alignment,pool.data()+0x30,4);
 auto result=dispatch_null_reallocation_system_pool(pool,backend,16,alignment,poison);
 std::memset(result.payload.data(),0,16);
 return result;
}
}
