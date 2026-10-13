#include "ctr/metaobject_registration_name.h"
#include <cstring>
#include <stdexcept>
namespace ctr {
PooledNameInsertion assign_fresh_metaobject_name(std::span<std::byte> metadata,
 std::span<std::byte> strings,std::span<std::byte> buckets,std::uint64_t buckets_address,
 std::span<std::byte> pool,SystemPoolBackend& backend,std::string_view name,bool poison){
 if(metadata.size()!=152)throw std::invalid_argument("Metadata152 required");
 std::uint64_t old;std::memcpy(&old,metadata.data()+0x10,8);
 if(old)throw std::invalid_argument("Previous name release missing");
 auto result=insert_fresh_pooled_name(strings,buckets,buckets_address,pool,backend,name,poison);
 std::memcpy(metadata.data()+0x10,&result.string_address,8);
 return result;
}
MetaobjectRegistrationName begin_cold_metaobject_registration_name(
 std::span<const std::byte> descriptor,std::span<const std::byte> manager,
 std::span<std::byte> strings,std::span<std::byte> buckets,std::uint64_t buckets_address,
 std::span<std::byte> pool,SystemPoolBackend& backend,std::string_view name,
 std::uint32_t instance_size,bool poison,std::uint64_t& cell){
 auto registration=begin_cold_metaobject_registration_base(descriptor,manager,pool,backend,poison,cell);
 auto inserted=assign_fresh_metaobject_name(registration.metadata.payload,strings,buckets,buckets_address,pool,backend,name,poison);
 const auto size=static_cast<std::uint16_t>(instance_size);
 std::memcpy(registration.metadata.payload.data()+0x5a,&size,2);
 std::uint16_t flags;std::memcpy(&flags,registration.metadata.payload.data()+0x5c,2);flags|=0x410;
 std::memcpy(registration.metadata.payload.data()+0x5c,&flags,2);
 return {registration,inserted}; // Flags419, unready; no callback/ready cleanup.
}
}
