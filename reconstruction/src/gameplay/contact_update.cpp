// SPDX-License-Identifier: GPL-2.0-only
#include "ctr/gameplay/contact_update.h"
#include <bit>
#include <cmath>
#include <stdexcept>
namespace ctr {
void update_contact_state(ContactUpdateState& s,const EntityLocalView& entity,Vec3 defaults,const ContactUpdateServices& svc){
 if(!svc.available)throw std::invalid_argument("Missing original contact availability query");
 if(!svc.available(svc.context))return;
 if(!svc.direction)throw std::invalid_argument("Missing original owner/contact direction query");
 const auto position=unparented_position(entity);Vec3 direction;
 if(!svc.direction(svc.context,position,direction))return;
 const auto a=unparented_angles(entity,defaults);
 const float cy=a.y==0.f?1.f:std::cos(a.y),sz=a.z==0.f?0.f:std::sin(a.z),cz=a.z==0.f?1.f:std::cos(a.z);
 const float nsy=a.y==0.f?-0.f:-std::sin(a.y);
 const float facing=((cy*cz*direction.x+0.f)+cy*sz*direction.y)+nsy*direction.z;
 const bool steep=facing<std::bit_cast<float>(std::uint32_t{0xbf34fdf4});
 auto clock=[&](){if(!svc.clock)throw std::invalid_argument("Missing original timer virtual+b0");return svc.clock(svc.context);};
 auto notify=[&](bool value){if(!svc.notification_component)throw std::invalid_argument("Missing original component query004d9a90");if(svc.notification_component(svc.context)){if(!svc.notify)throw std::invalid_argument("Missing original event factory/notification00efe370");svc.notify(svc.context,value);}};
 auto stop=[&](){if(!s.timer.stopped&&s.timer.running){s.timer.current=clock();s.timer.stopped=true;}};
 if(steep==s.steep){
  if(steep&&!s.timer.stopped){const auto now=s.timer.running?clock():s.timer.current;
   const auto elapsed=std::bit_cast<std::int32_t>(static_cast<std::uint32_t>(now)-static_cast<std::uint32_t>(s.timer.started));
   if(.25f<static_cast<float>(elapsed)*.0001220703125f){stop();notify(true);}
  }
 }else if(!steep){if(s.timer.running){if(!s.timer.stopped)s.timer.current=clock();s.timer.running=false;s.timer.stopped=false;}notify(false);}
 else {const auto now=clock();s.timer.started=now;s.timer.current=now;s.timer.running=true;s.timer.stopped=false;}
 s.steep=steep;
}
}
