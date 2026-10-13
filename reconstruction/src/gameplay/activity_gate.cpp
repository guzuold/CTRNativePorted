// SPDX-License-Identifier: GPL-2.0-only
#include "ctr/gameplay/activity_gate.h"
namespace ctr {
bool activity_gate(const GameplayHandle* first,const GameplayHandle* second,
                   std::uint32_t input_a,std::uint32_t input_b) {
 bool active=((input_a|input_b)&255u)!=0;
 if(const auto* p=gameplay_handle_payload(first))
  active=active || static_cast<const ActivityGatePayload*>(p)->kind==5;
 if(const auto* p=gameplay_handle_payload(second))
  active=active || static_cast<const ActivityGatePayload*>(p)->selection!=-1;
 return active;
}
}
