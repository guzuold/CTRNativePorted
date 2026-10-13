#include "ctr/inherited_field_selection.h"
#include <cstring>
#include <stdexcept>
namespace ctr {
std::uint64_t select_inherited_highest_offset(std::span<const InheritedFieldBinding> fields,std::int32_t count){
 if(count<=0)return 0;
 if(count>16||static_cast<std::size_t>(count)>fields.size())throw std::invalid_argument("Bounded inherited prefix bindings required");
 std::size_t selected=0;
 for(std::int32_t i=0;i<count;++i){const auto& field=fields[static_cast<std::size_t>(i)];
  if(!field.token){if(i==0)continue;throw std::invalid_argument("Later null inherited field remains missing");}
  if(field.storage.size()<22)throw std::invalid_argument("Inherited offset storage required");
  std::uint16_t offset;std::memcpy(&offset,field.storage.data()+20,2);
  if(!fields[selected].token){selected=static_cast<std::size_t>(i);continue;}
  std::uint16_t previous;std::memcpy(&previous,fields[selected].storage.data()+20,2);
  if(offset>previous)selected=static_cast<std::size_t>(i);
 }
 return fields[selected].token;
}
}
