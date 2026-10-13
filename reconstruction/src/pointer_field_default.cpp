#include "ctr/pointer_field_default.h"
#include <cstring>
#include <stdexcept>
namespace ctr {
void set_prepared_pointer_field_default(std::span<std::byte> field,
 std::uint64_t backing_token,std::span<std::byte> backing,
 std::span<const std::byte> value,bool has_value){
 if(field.size()!=72&&field.size()!=80)throw std::invalid_argument("Known pointer field extent required");
 std::uint64_t table,flags,pointer;std::memcpy(&table,field.data(),8);std::memcpy(&flags,field.data()+48,8);std::memcpy(&pointer,field.data()+56,8);
 if(!((table==0x025c2918&&field.size()==72)||(table==0x025bba10&&field.size()==80)))throw std::invalid_argument("Uncovered pointer default type");
 if((flags&(std::uint64_t(1)<<56))||(flags&0xfffffffffffffull)<8||!pointer||pointer!=backing_token||backing.size()<8)throw std::invalid_argument("Existing default backing binding required; allocation missing");
 if(has_value&&value.size()!=8)throw std::invalid_argument("Pointer default value extent");
 const std::uint16_t width=8;std::memcpy(field.data()+18,&width,2);
 if(has_value)std::memcpy(backing.data(),value.data(),8);
}
}
