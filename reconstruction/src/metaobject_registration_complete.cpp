#include "ctr/metaobject_registration_complete.h"
#include "ctr/metaobject_registration_finish.h"
#include <stdexcept>
namespace ctr {
MetaobjectRegistrationName complete_prepared_metaobject_registration(
 std::span<const std::byte> descriptor,std::span<const std::byte> pool_manager,
 std::span<std::byte> strings,std::span<std::byte> buckets,std::uint64_t buckets_address,
 std::span<std::byte> pool,SystemPoolBackend& backend,std::string_view name,
 std::uint32_t instance_size,bool poison,std::uint64_t& cell,
 std::span<std::byte> registry,std::span<std::byte> header,std::uint64_t header_token,
 std::span<std::byte> table,std::uint64_t table_token){
 auto result=begin_cold_metaobject_registration_name(descriptor,pool_manager,strings,buckets,buckets_address,pool,backend,name,instance_size,poison,cell);
 const auto base=finish_prepared_metaobject_registration(registry,header,header_token,table,table_token,result.registration.metadata.payload,result.registration.metadata.address,result.registration.base_record.address);
 if(base!=result.registration.base_record.address)throw std::runtime_error("Registration base return differs");
 return result;
}
}
