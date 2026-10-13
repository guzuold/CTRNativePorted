#include "ctr/gameplay/input_lookup.h"
#include "ctr/object_nonterminal_release.h"
#include <cstring>
#include <stdexcept>
namespace ctr {
namespace {
void retain(std::array<std::byte,12>& h){std::uint32_t v;std::memcpy(&v,h.data()+8,4);++v;std::memcpy(h.data()+8,&v,4);}
}
InputLookupPayload* lookup_input(const std::array<InputLookupSlot*,8>& slots,std::uint32_t index){
 if(index>7||!slots[index])return nullptr;
 auto* slot=slots[index];retain(slot->header);
 auto* handle=slot->handle;InputLookupPayload* result=nullptr;
 for(unsigned depth=0;handle;++depth){
  if(depth>=64)throw std::invalid_argument("Missing unbounded handle chain");
  if(handle->flags&0x10000000u)throw std::invalid_argument("Missing mutating resolver");
  result=handle->payload;
  if(!(handle->flags&0x20000000u)||!result)break;
  handle=result->canonical;result=nullptr;
 }
 if(result){retain(result->header);retain(result->header);release_nonterminal_object_storage(result->header);}
 release_nonterminal_object_storage(slot->header);
 if(result)release_nonterminal_object_storage(result->header);
 return result;
}
}
