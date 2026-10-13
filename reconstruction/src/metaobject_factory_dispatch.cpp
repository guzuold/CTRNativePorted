#include "ctr/metaobject_factory_dispatch.h"
#include <stdexcept>
namespace ctr {
MetaobjectFactoryDispatch dispatch_metaobject_creation(std::span<const std::byte> descriptor,
 std::span<const std::byte> manager,std::span<std::byte> pool,SystemPoolBackend& backend,bool poison){
 if(descriptor.size()!=176||pool.size()!=176)throw std::invalid_argument("Descriptor176/actual pool176 required");
 const auto address=static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(pool.data()));
 if(descriptor[0]==std::byte{0})
  return {MetaobjectFactoryState::ColdObjectCreated,create_cold_metaobject(descriptor,manager,pool,backend,poison),0,0,address};
 return {MetaobjectFactoryState::WarmMetadataFactoryMissing,{},0x026ee198,0x01ab61d6,address};
}
}
