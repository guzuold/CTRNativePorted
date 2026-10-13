// SPDX-License-Identifier: GPL-2.0-only
#include "ctr/gameplay/event_selection.h"
#include <stdexcept>
namespace ctr {
void select_event(EventSelection& s,std::uint64_t bits,const EventSelectionServices& services){
 if(s.state-3u<2u){s.selected=0x33;s.timestamp=-1;return;}
 if(!services.lookup)throw std::invalid_argument("Missing genuine singleton event table lookup");
 const auto indexes=services.lookup(services.context);
 std::uint32_t chosen;
 if((bits>>(indexes.first&63u))&1u)chosen=indexes.first;
 else if((bits>>(indexes.second&63u))&1u)chosen=indexes.second;
 else return;
 if(!services.clock)throw std::invalid_argument("Missing genuine event clock");
 s.selected=static_cast<std::int32_t>(chosen);
 s.timestamp=services.clock(services.context);
}
}
