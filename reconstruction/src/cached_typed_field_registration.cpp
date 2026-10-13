#include "ctr/cached_typed_field_registration.h"
#include <cstring>
#include <stdexcept>
namespace ctr {
void register_single_cached_typed_field(std::span<std::byte> field,std::uint64_t field_token,
 std::span<std::byte> metadata,std::span<const std::byte> context,
 const SystemPoolAllocation& table,const PooledNameInsertion& name,std::string_view text,
 std::uint16_t offset,std::uint64_t cached_type_token,std::span<const std::byte> cached_type,
 std::span<std::byte> publication){
 if(field.size()<64||metadata.size()!=152||cached_type.size()!=32||publication.size()!=8||!field_token||!cached_type_token)throw std::invalid_argument("Explicit known field/type/publication bindings required");
 std::uint64_t identity,old;std::uint32_t count,refs;std::memcpy(&identity,field.data(),8);
 if(identity==0x025bb470){register_single_cached_scalar_field(field,field_token,metadata,context,table,name,text,offset,cached_type_token,cached_type,publication);return;}
 std::uint16_t size;
 if(identity==0x025c3090&&field.size()==64)size=2;
 else if(identity==0x025bba10&&field.size()==80)size=8;
 else if(identity==0x025c2918&&field.size()==72)size=8;
 else if(identity==0x025c4220&&field.size()==112)size=24;
 else throw std::invalid_argument("Unknown field type/extent remains missing");
 std::memcpy(&count,metadata.data()+24,4);std::memcpy(&old,field.data()+40,8);
 if(name.record.payload.size()<17)throw std::invalid_argument("Bound name record required");
 std::memcpy(&refs,name.record.payload.data()+8,4);
 if(count!=1||old||(std::to_integer<unsigned>(field[35])&1)||refs>0xfffffffdu)throw std::invalid_argument("Single fresh field/inherited0/name lifetime required");
 const auto string=retain_existing_created_name(context,table,name,text);
 retain_existing_created_name(context,table,name,text);
 std::memcpy(field.data()+40,&string,8);std::memcpy(field.data()+20,&offset,2);
 std::memcpy(field.data()+12,metadata.data()+88,2);
 std::memcpy(&refs,name.record.payload.data()+8,4);--refs;std::memcpy(name.record.payload.data()+8,&refs,4);
 // Known original +c0 direct cached getters and +110 constant-size getters only.
 std::memcpy(field.data()+14,cached_type.data()+16,2);std::memcpy(field.data()+18,&size,2);
 std::memcpy(publication.data(),&field_token,8);
 std::uint16_t previous;std::memcpy(&previous,metadata.data()+90,2);
 const auto end=static_cast<std::uint32_t>(offset)+size;
 const auto result=static_cast<std::uint16_t>(end<previous?previous:end);
 std::memcpy(metadata.data()+90,&result,2);
}
}
