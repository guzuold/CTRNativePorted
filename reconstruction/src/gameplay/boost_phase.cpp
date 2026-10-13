#include "ctr/gameplay/boost_phase.h"
#include "ctr/gameplay/boost_age.h"
#include <stdexcept>
namespace ctr {
namespace {
const BoostPhasePayload* stable_payload(const BoostPhaseHandle* handle){
 for(unsigned depth=0;depth<64;++depth){
  if(!handle)return nullptr;
  if(handle->flags&0x10000000u)throw std::invalid_argument("Missing mutating canonical handle resolver");
  const auto* payload=handle->payload;
  if(!(handle->flags&0x20000000u)||!payload)return payload;
  handle=payload->canonical;
 }
 throw std::invalid_argument("Unbounded or cyclic handle chain");
}
}
bool boost_phase_complete(const BoostPhaseHandle* first,const BoostPhaseHandle* second,
 unsigned char input3a,const BoostPhaseTimer* timer){
 const auto* payload=stable_payload(first);
 if(payload&&payload->enabled&&payload->gate35&&!payload->gate34&&!payload->gate36){
  if(input3a)return false;
  if(timer){
   float age=boost_age(timer->active,timer->now,timer->started,timer->finished,timer->delay);
   age+=timer->delay;
   age+=timer->duration;
   if(age<timer->threshold)return false;
  }
 }
 const auto* other=stable_payload(second);
 return !other||other->marker40==-1;
}
}
