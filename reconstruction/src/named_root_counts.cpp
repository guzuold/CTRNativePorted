#include "ctr/named_root_counts.h"
namespace ctr {
MetaobjectRegistrationName register_named_root_and_counts(
 std::span<std::byte> descriptor,std::span<const std::byte> manager,
 std::span<std::byte> strings,const SystemPoolAllocation& names_table,
 std::span<std::byte> pool,SystemPoolBackend& backend,bool poison,
 std::uint64_t& cell,const SystemPoolAllocation& registry_array,
 std::uint64_t& lazy_cell,SystemPoolAllocation& lazy_array){
 auto result=register_named_generated_internal_object_base(descriptor,manager,strings,names_table,pool,backend,poison,cell,registry_array);
 update_bootstrap_descriptor_counts(descriptor,registry_array,pool,backend,poison,lazy_cell,lazy_array);
 return result;
}
}
