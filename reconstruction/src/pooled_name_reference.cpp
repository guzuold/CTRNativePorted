#include "ctr/pooled_name_reference.h"
#include <cstring>
#include <stdexcept>
namespace ctr {
std::uint64_t retain_existing_created_name(std::span<const std::byte> context,
 const SystemPoolAllocation& table,const PooledNameInsertion& name,std::string_view text){
 if(context.size()!=64||table.payload.size()!=8192||context[40]!=std::byte{0}||text.size()>128||text.find('\0')!=std::string_view::npos||name.record.payload.size()!=17+text.size())throw std::invalid_argument("Known single existing unlocked name required");
 std::uint64_t identity,table_token,next,entry;std::uint32_t buckets,count,threshold,refs;
 std::memcpy(&identity,context.data(),8);std::memcpy(&table_token,context.data()+16,8);std::memcpy(&buckets,context.data()+24,4);std::memcpy(&count,context.data()+12,4);std::memcpy(&threshold,context.data()+28,4);
 std::memcpy(&next,name.record.payload.data(),8);std::memcpy(&refs,name.record.payload.data()+8,4);
 std::uint32_t hash=0x811c9dc5;for(unsigned char c:text)hash=(hash^c)*0x1000193u;
 std::memcpy(&entry,table.payload.data()+8*(hash%1024),8);
 if(identity!=0x025c4510||table_token!=table.address||buckets!=1024||count!=1||threshold!=1||next||entry!=name.record.address||name.string_address!=name.record.address+16||!refs||refs==0xffffffffu||name.record.payload.back()!=std::byte{0}||std::memcmp(name.record.payload.data()+16,text.data(),text.size()))throw std::invalid_argument("Existing name binding/bytes/lifetime required; search/growth missing");
 ++refs;std::memcpy(name.record.payload.data()+8,&refs,4);return name.string_address;
}
void bind_first_registered_scalar_field_name(std::span<std::byte> field,
 std::span<const std::byte> metadata,std::span<const std::byte> context,
 const SystemPoolAllocation& table,const PooledNameInsertion& name,
 std::string_view text,std::uint16_t offset){
 if(field.size()!=64||metadata.size()!=152||name.record.payload.size()<17)throw std::invalid_argument("First scalar field/metadata extent required");
 std::uint64_t identity,old;std::uint32_t refs;std::memcpy(&identity,field.data(),8);std::memcpy(&old,field.data()+40,8);std::memcpy(&refs,name.record.payload.data()+8,4);
 if(identity!=0x025bb470||old||(std::to_integer<unsigned>(field[35])&1)||refs>0xfffffffdu)throw std::invalid_argument("Fresh scalar name/registration/temporary refs required");
 auto string=retain_existing_created_name(context,table,name,text); //0153f3ee temporary
 retain_existing_created_name(context,table,name,text); //0153c720 field retained reference
 std::memcpy(field.data()+40,&string,8);std::memcpy(field.data()+20,&offset,2);
 std::memcpy(field.data()+12,metadata.data()+88,2); // actual +260 override014e35e0
 std::memcpy(&refs,name.record.payload.data()+8,4);--refs;std::memcpy(name.record.payload.data()+8,&refs,4); // nonterminal0154df00
}
}
