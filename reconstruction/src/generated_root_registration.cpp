#include "ctr/generated_root_registration.h"
#include <cstring>
#include <stdexcept>
namespace ctr {
MetaobjectRegistrationName register_generated_internal_object_base(
 std::span<std::byte> descriptor,std::span<const std::byte> pool_manager,
 std::span<std::byte> strings,std::span<std::byte> buckets,std::uint64_t buckets_address,
 std::span<std::byte> pool,SystemPoolBackend& backend,bool poison,
 std::uint64_t& cell,const SystemPoolAllocation& descriptor_array){
 if(descriptor.size()!=176)throw std::invalid_argument("Descriptor176 required");
 std::int32_t object_vtable_offset;std::memcpy(&object_vtable_offset,descriptor.data()+0xa8,4);
 if(object_vtable_offset!=0)throw std::invalid_argument("Generated root object layout offset missing");
 auto result=complete_descriptor_metadata_registration(descriptor,pool_manager,strings,buckets,buckets_address,pool,backend,"__internalObjectBase",16,poison,cell,descriptor_array);
 // Single-thread final-storage composition: original binds before registration.
 // No callback observes this order in the bounded disabled-lookup domain.
 const std::uint64_t vtable=0x025ba3c0;
 std::memcpy(result.registration.metadata.payload.data()+0x38,&vtable,8);
 std::uint16_t flags;std::memcpy(&flags,result.registration.metadata.payload.data()+0x5c,2);
 flags=static_cast<std::uint16_t>(flags&0xffef);
 std::memcpy(result.registration.metadata.payload.data()+0x5c,&flags,2);
 return result;
}
}
