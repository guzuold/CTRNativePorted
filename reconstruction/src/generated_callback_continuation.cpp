#include "ctr/generated_callback_continuation.h"
#include <cstring>
#include <stdexcept>
namespace ctr {
void continue_generated_metadata_callback(ColdFieldTable& fields,std::span<std::byte> metadata,
 std::span<const std::byte> manager,std::span<std::byte> pool,SystemPoolBackend& backend,
 std::span<std::byte> names,const SystemPoolAllocation& buckets,std::span<const std::byte> cached_types,
 std::span<const std::byte> canonical,std::uint64_t metadata_token,std::uint64_t canonical_token,
 std::uint64_t element_type,std::uint64_t attribute_type,std::uint64_t base_type,
 std::span<std::byte> publications,bool poison){
 if(cached_types.size()!=160||canonical.size()!=80||std::memcmp(cached_types.data()+64,canonical.data(),32))throw std::invalid_argument("Shared original ObjectRef cache/canonical binding required");
 configure_generated_metaobject_fields(fields,manager,pool,metadata_token,canonical_token,canonical,element_type,attribute_type,base_type);
 register_generated_fields(fields,metadata,names,buckets,pool,backend,cached_types,generated_metadata_offsets,publications,poison);
}
}
