#include "ctr/gameplay/boost_blend.h"
#include "ctr/gameplay/boost_interpolation.h"
#include <stdexcept>
namespace ctr {
void blend_boost_record(BoostBlendView& target,const BoostBlendView* source,float factor){
 if(!source)return;
 if(target.nested&&!source->nested)throw std::invalid_argument("Missing nested boost source record");
 for(unsigned i=0;i<5;++i)target.values[i]+=(source->values[i]-target.values[i])*factor;
 if(target.nested)interpolate_boost_values(*target.nested,*source->nested,factor);
}
}
