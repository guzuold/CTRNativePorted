#include "ctr/gameplay/registered_scalar.h"
#include "ctr/core/component_query.h"
#include <stdexcept>
namespace ctr {
SpeedResult advance_registered_scalar(float speed,float dt,float steering,
 const LinearVelocityProfile& profile,SpeedMode mode,const ScalarQueryView& query,
 float factor178,float factor180,float special184,bool handle98,bool handleb8){
 if(special184>0||handleb8)throw std::runtime_error("Missing special body handle path00eff4ff");
 if(factor178!=1.f||factor180!=1.f)throw std::runtime_error("Missing generalized scalar scale paths");
 const auto component=query_registered_component(query.image,query.base,query.object,query.expected,0,query.igobject_type);
 if(component)throw std::runtime_error("Missing queried component scalar contribution");
 if(handle98)throw std::runtime_error("Missing scalar override handle data");
 return advance_without_components(speed,dt,steering,profile,mode);
}
}
