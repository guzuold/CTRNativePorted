#include "ctr/gameplay/boost_availability.h"
#include <stdexcept>
namespace ctr {
bool boost_available(const BoostPhaseHandle* first,const BoostPhaseHandle* second,
 unsigned char input3a,unsigned char input3b){
 if(!boost_phase_complete(first,second,input3a,nullptr))return false;
 if(!input3b)return true;
 for(unsigned depth=0;depth<64;++depth){
  if(!first)return true;
  if(first->flags&0x10000000u)throw std::invalid_argument("Missing mutating canonical handle resolver");
  const auto* payload=first->payload;
  if(!payload)return true;
  if(!(first->flags&0x20000000u))return payload->enabled!=0;
  first=payload->canonical;
 }
 throw std::invalid_argument("Unbounded or cyclic handle chain");
}
}
