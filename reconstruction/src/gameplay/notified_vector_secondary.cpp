// SPDX-License-Identifier: GPL-2.0-only
#include "ctr/gameplay/notified_vector_secondary.h"
#include <cmath>
#include <stdexcept>
namespace ctr {
bool update_secondary_notified_vector(Vec3& current,Vec3 requested,VectorNotification notify,void* context){
 const auto finite=[](Vec3 v){return std::isfinite(v.x)&&std::isfinite(v.y)&&std::isfinite(v.z);};
 if(!finite(current)||!finite(requested))throw std::invalid_argument("Non-finite secondary vector contract");
 constexpr float epsilon=4.999999987376214e-7f;
 if(std::fabs(requested.x-current.x)<=epsilon&&std::fabs(requested.y-current.y)<=epsilon&&std::fabs(requested.z-current.z)<=epsilon)return false;
 if(!notify)throw std::invalid_argument("Missing genuine secondary vector virtual+b8");
 current=requested;
 notify(context,1);
 return true;
}
}
