#include "ctr/object_nonterminal_release.h"
#include <cstdint>
#include <cstring>
#include <stdexcept>
namespace ctr {
void release_nonterminal_object_storage(std::span<std::byte> object){
 if(object.size()<12)throw std::invalid_argument("Object header12 required");
 std::uint32_t flags;std::memcpy(&flags,object.data()+8,4);
 const auto count=flags&0x7ffffu;
 const auto weak=(flags>>21)&1u;
 if(count<2 || ((flags&0x100000u)!=0 && count==weak+2) || (weak!=0 && count==2))
  throw std::invalid_argument("Object lifetime/weak callbacks missing");
 --flags;std::memcpy(object.data()+8,&flags,4);
}
}
