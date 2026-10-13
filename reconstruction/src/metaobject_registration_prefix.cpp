#include "ctr/metaobject_registration_prefix.h"
#include <stdexcept>
#include <cstring>
namespace ctr {
std::uint32_t begin_metaobject_registration_flags(std::span<std::byte> metadata){
 if(metadata.size()!=152)throw std::invalid_argument("Metadata152 required");
 std::uint16_t flags;std::memcpy(&flags,metadata.data()+0x5c,2);
 if(flags&8)return 0x0154007b;
 const auto marked=static_cast<std::uint16_t>(flags|8);
 std::memcpy(metadata.data()+0x5c,&marked,2);
 if(flags&4)return 0x0153ff5a;
 if(flags&2)return 0x015402b6;
 if(flags&1)return 0x015402c1;
 return 0x0153fd5b; // BEFORE the unknown base initializer call r14.
}
SystemPoolAllocation begin_cold_metaobject_registration(std::span<const std::byte> descriptor,
 std::span<const std::byte> manager,std::span<std::byte> pool,SystemPoolBackend& backend,
 bool poison,std::uint64_t& registration_cell){
 if(registration_cell)throw std::invalid_argument("Empty registration cell required");
 auto allocation=create_cold_metaobject(descriptor,manager,pool,backend,poison);
 registration_cell=allocation.address;
 // Metadata still unready: original153fd20 name/parent/size setup follows this store.
 return allocation;
}
}
