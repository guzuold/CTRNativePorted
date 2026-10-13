#include "ctr/vector_descriptor_default.h"
#include "ctr/object_nonterminal_release.h"
#include <array>
#include <cstring>
#include <stdexcept>
namespace ctr {
void set_prepared_vector_descriptor_default(std::span<std::byte> field,
 std::uint64_t backing_token,std::span<std::byte> backing,
 std::uint64_t old_token,std::span<std::byte> old_object,
 std::uint64_t new_token,std::span<std::byte> new_object){
 if(field.size()!=112||backing.size()<24)throw std::invalid_argument("Prepared vector default extent");
 std::uint64_t table,flags,pointer,old;std::memcpy(&table,field.data(),8);std::memcpy(&flags,field.data()+48,8);std::memcpy(&pointer,field.data()+56,8);std::memcpy(&old,backing.data(),8);
 if(table!=0x025c4220||(flags&(std::uint64_t(1)<<56))||(flags&0xfffffffffffffull)<24||!pointer||pointer!=backing_token)throw std::invalid_argument("Prepared vector backing required; allocation missing");
 const bool owned=field[67]!=std::byte{0};std::uint32_t new_flags=0;
 if(old!=old_token||(old&&!old_object.size())||(new_token&&new_object.size()<12)||(old&&new_token==old))throw std::invalid_argument("Explicit distinct object bindings required");
 if(owned&&new_token){std::memcpy(&new_flags,new_object.data()+8,4);auto count=new_flags&0x7ffffu;if(!count||count==0x7ffffu)throw std::invalid_argument("Object acquisition overflow/lifetime missing");}
 if(owned&&old){if(old_object.size()<12)throw std::invalid_argument("Old object header required");std::array<std::byte,12> check;std::memcpy(check.data(),old_object.data(),12);release_nonterminal_object_storage(check);}
 const std::uint16_t size=24;std::memcpy(field.data()+18,&size,2);std::memcpy(backing.data(),&new_token,8);
 // Single-thread fixtures only. Original uses lock inc before nonterminal release.
 if(owned&&new_token){++new_flags;std::memcpy(new_object.data()+8,&new_flags,4);}
 if(owned&&old)release_nonterminal_object_storage(old_object);
}
}
