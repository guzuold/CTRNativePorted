#include "ctr/descriptor_metadata_registration.h"
#include <cstring>
#include <stdexcept>
namespace ctr {
MetaobjectRegistrationName complete_descriptor_metadata_registration(
 std::span<std::byte> descriptor,std::span<const std::byte> pool_manager,
 std::span<std::byte> strings,std::span<std::byte> buckets,std::uint64_t buckets_address,
 std::span<std::byte> pool,SystemPoolBackend& backend,std::string_view name,
 std::uint32_t instance_size,bool poison,std::uint64_t& cell,
 const SystemPoolAllocation& descriptor_array){
 if(descriptor.size()!=176||descriptor_array.payload.size()!=24)
  throw std::invalid_argument("Descriptor176/actual array24 required");
 std::uint64_t header_token,table_token,capacity;
 std::memcpy(&header_token,descriptor.data()+0x20,8);
 std::memcpy(&table_token,descriptor_array.payload.data()+16,8);
 std::memcpy(&capacity,descriptor_array.payload.data()+8,8);
 auto backing=backend.memory.bytes();const auto backing_token=reinterpret_cast<std::uintptr_t>(backing.data());
 if(header_token!=descriptor_array.address||capacity!=0x0010000000002000ull||table_token<backing_token||table_token-backing_token>backing.size()||8192>backing.size()-(table_token-backing_token))
  throw std::invalid_argument("Actual tagged descriptor backing required");
 auto table=backing.subspan(static_cast<std::size_t>(table_token-backing_token),8192);
 return complete_prepared_metaobject_registration(descriptor,pool_manager,strings,buckets,buckets_address,pool,backend,name,instance_size,poison,cell,descriptor.first(128),descriptor_array.payload,header_token,table,table_token);
}
}
