#include "ctr/named_generated_root.h"
#include "ctr/activated_name_first_insert.h"
#include "ctr/metaobject_registration_finish.h"
#include <cstring>
#include <stdexcept>
namespace ctr {
MetaobjectRegistrationName register_named_generated_internal_object_base(
 std::span<std::byte> descriptor,std::span<const std::byte> manager,
 std::span<std::byte> strings,const SystemPoolAllocation& names_table,
 std::span<std::byte> pool,SystemPoolBackend& backend,bool poison,
 std::uint64_t& cell,const SystemPoolAllocation& descriptor_array){
 if(descriptor.size()!=176||descriptor_array.payload.size()!=24)
  throw std::invalid_argument("Descriptor176/actual array24 required");
 std::int32_t offset;std::memcpy(&offset,descriptor.data()+0xa8,4);
 if(offset!=0)throw std::invalid_argument("Generated root layout offset missing");
 std::uint64_t header_token,table_token,capacity;
 std::memcpy(&header_token,descriptor.data()+0x20,8);
 std::memcpy(&table_token,descriptor_array.payload.data()+16,8);
 std::memcpy(&capacity,descriptor_array.payload.data()+8,8);
 auto backing=backend.memory.bytes();const auto token=reinterpret_cast<std::uintptr_t>(backing.data());
 if(header_token!=descriptor_array.address||capacity!=0x0010000000002000ull||table_token<token||table_token-token>backing.size()||8192>backing.size()-(table_token-token))
  throw std::invalid_argument("Actual tagged descriptor backing required");
 auto registration=begin_cold_metaobject_registration_base(descriptor,manager,pool,backend,poison,cell);
 std::uint64_t old;std::memcpy(&old,registration.metadata.payload.data()+0x10,8);
 if(old)throw std::invalid_argument("Previous name release missing");
 auto inserted=insert_first_activated_name(strings,names_table,pool,backend,"__internalObjectBase",poison,1);
 std::memcpy(registration.metadata.payload.data()+0x10,&inserted.string_address,8);
 const std::uint16_t size=16;std::memcpy(registration.metadata.payload.data()+0x5a,&size,2);
 std::uint16_t flags;std::memcpy(&flags,registration.metadata.payload.data()+0x5c,2);flags|=0x410;
 std::memcpy(registration.metadata.payload.data()+0x5c,&flags,2);
 const auto base=finish_prepared_metaobject_registration(descriptor.first(128),descriptor_array.payload,header_token,backing.subspan(static_cast<std::size_t>(table_token-token),8192),table_token,registration.metadata.payload,registration.metadata.address,registration.base_record.address);
 if(base!=registration.base_record.address)throw std::runtime_error("Registration base return differs");
 // Bounded single-thread final-storage composition; original binds before registration.
 const std::uint64_t vtable=0x025ba3c0;std::memcpy(registration.metadata.payload.data()+0x38,&vtable,8);
 std::memcpy(&flags,registration.metadata.payload.data()+0x5c,2);flags=static_cast<std::uint16_t>(flags&0xffef);
 std::memcpy(registration.metadata.payload.data()+0x5c,&flags,2);
 return {registration,inserted};
}
}
