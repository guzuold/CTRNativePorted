#include "ctr/metaobject_field_append.h"
#include "ctr/object_nonterminal_release.h"
#include <cstring>
#include <stdexcept>
namespace ctr {
void append_metaobject_field_no_growth(std::span<std::byte> metadata,
 std::span<std::byte> table,std::uint64_t table_token,
 std::span<std::byte> field,std::uint64_t field_token){
 if(metadata.size()!=152||table.size()!=64||(!field.empty()&&field.size()!=32))throw std::invalid_argument("Resolved metadata152/table64/field32 required");
 if((field_token==0)!=field.empty())throw std::invalid_argument("Null field token/storage differs");
 std::int32_t count;std::uint64_t capacity,token;std::uint32_t refs=0;
 std::memcpy(&count,metadata.data()+0x18,4);std::memcpy(&capacity,metadata.data()+0x20,8);std::memcpy(&token,metadata.data()+0x28,8);
 if(count<0||count>=8||token!=table_token||!token||(capacity!=64&&capacity!=0x0010000000000040ull))throw std::invalid_argument("Field reserve/growth or backing missing");
 if(field_token){
  std::memcpy(&refs,field.data()+8,4);
  if(refs<1||refs>0x7fffc)throw std::invalid_argument("Weak/lifetime/overflow services missing");
  ++refs;std::memcpy(field.data()+8,&refs,4);
 }
 std::memcpy(table.data()+static_cast<std::size_t>(count)*8,&field_token,8);
 if(field_token){++refs;std::memcpy(field.data()+8,&refs,4);}
 ++count;std::memcpy(metadata.data()+0x18,&count,4);
 if(field_token)release_nonterminal_object_storage(field);
}
}
