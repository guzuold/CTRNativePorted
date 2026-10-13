#include "ctr/metaobject_registration_base.h"
#include "ctr/metaobject_parent_zero_fields.h"
#include <stdexcept>
namespace ctr {
MetaobjectRegistrationBase begin_cold_metaobject_registration_base(
 std::span<const std::byte> descriptor,std::span<const std::byte> manager,
 std::span<std::byte> pool,SystemPoolBackend& backend,bool poison,
 std::uint64_t& cell){
 auto metadata=begin_cold_metaobject_registration(descriptor,manager,pool,backend,poison,cell);
 if(begin_metaobject_registration_flags(metadata.payload)!=0x0153fd5b)throw std::logic_error("Fresh metadata base route differs");
 auto base=create_metaobject_base_context(manager,pool,backend,poison);
 link_metaobject_parent_zero_fields(metadata.payload,{},{},metadata.address,0,0);
 metadata.payload[0x5c]|=std::byte{1};
 return {metadata,base}; // Flags9 remain unready; pooled-name/type/cleanup missing.
}
}
