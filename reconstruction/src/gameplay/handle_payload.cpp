#include "ctr/gameplay/handle_payload.h"
#include <stdexcept>
namespace ctr {
const GameplayPayload* gameplay_handle_payload(const GameplayHandle* handle){
 for(unsigned depth=0;depth<64;++depth){
  if(!handle)return nullptr;
  if(handle->flags&0x10000000u)throw std::invalid_argument("Missing mutating handle resolver");
  const auto* payload=handle->payload;
  if(!(handle->flags&0x20000000u)||!payload)return payload;
  handle=payload->canonical;
 }
 throw std::invalid_argument("Unbounded or cyclic handle chain");
}
}
