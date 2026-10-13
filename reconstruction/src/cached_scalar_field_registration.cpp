#include "ctr/cached_scalar_field_registration.h"
#include <cstring>
#include <stdexcept>
namespace ctr {
void register_single_cached_scalar_field(std::span<std::byte> field,std::uint64_t field_token,
 std::span<std::byte> metadata,std::span<const std::byte> context,
 const SystemPoolAllocation& table,const PooledNameInsertion& name,std::string_view text,
 std::uint16_t offset,std::uint64_t cached_type_token,std::span<const std::byte> cached_type,
 std::span<std::byte> publication){
 if(field.size()!=64||metadata.size()!=152||cached_type.size()!=32||publication.size()!=8||!field_token||!cached_type_token)throw std::invalid_argument("Explicit single scalar field/nonnull cached type/publication bindings required");
 std::uint32_t count;std::memcpy(&count,metadata.data()+24,4);
 if(count!=1)throw std::invalid_argument("Only inherited0/single field registration covered");
 // Real original014eee00 directly loads global026ee118, then0153f453 reads cached metadata+10.
 // Cache absence/factory/query fallback is not implemented or treated as success.
 bind_first_registered_scalar_field_name(field,metadata,context,table,name,text,offset);
 std::uint16_t type_id,size=4,previous;
 std::memcpy(&type_id,cached_type.data()+16,2);std::memcpy(field.data()+14,&type_id,2);
 std::memcpy(field.data()+18,&size,2);std::memcpy(publication.data(),&field_token,8);
 std::memcpy(&previous,metadata.data()+90,2);
 // Original compares the full32bit end before storing AX; retain uint16 wrap behavior.
 const auto end=static_cast<std::uint32_t>(offset)+size;
 const auto result=static_cast<std::uint16_t>(end<previous?previous:end);
 std::memcpy(metadata.data()+90,&result,2);
}
}
