// SPDX-License-Identifier: GPL-2.0-only
#include "ctr/gameplay/countdown_dispatch.h"
#include <stdexcept>
namespace ctr {
void decrement_dispatch_countdown(std::uint32_t& counter,const CountdownDispatchServices& services){
 // Unsigned arithmetic preserves DEC wraparound, including zero -> ffffffff.
 --counter;
 if(counter!=0)return;
 if(!services.query)throw std::invalid_argument("Missing genuine countdown owner virtual+110");
 void* object=services.query(services.context);
 if(!object)return;
 if(!services.dispatch)throw std::invalid_argument("Missing genuine countdown dispatch00d28310 and retail token");
 services.dispatch(object,services.context);
}
}
