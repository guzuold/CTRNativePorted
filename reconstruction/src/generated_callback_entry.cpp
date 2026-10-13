#include "ctr/generated_callback_entry.h"
namespace ctr {
ColdFieldTable execute_generated_metadata_callback(std::span<std::byte> metadata,
 std::span<const std::byte> descriptor,std::span<const std::byte> manager,
 std::span<std::byte> pool,SystemPoolBackend& backend,std::span<std::byte> names,
 const SystemPoolAllocation& buckets,std::span<const std::byte> cached_types,
 std::span<const std::byte> canonical,std::uint64_t metadata_token,std::uint64_t canonical_token,
 std::uint64_t element_type,std::uint64_t attribute_type,std::uint64_t base_type,
 std::span<std::byte> publications,bool poison){
 auto fields=construct_cold_generated_field_table(metadata,descriptor,manager,pool,backend,poison);
 continue_generated_metadata_callback(fields,metadata,manager,pool,backend,names,buckets,cached_types,canonical,metadata_token,canonical_token,element_type,attribute_type,base_type,publications,poison);
 return fields;
}
}
