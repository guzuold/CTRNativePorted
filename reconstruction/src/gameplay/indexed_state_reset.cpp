// SPDX-License-Identifier: GPL-2.0-only
#include "ctr/gameplay/indexed_state_reset.h"
#include <stdexcept>
namespace ctr {
void indexed_state_reset(IndexedState& state,const IndexedStateResetServices& services){
 if(!services.query_index)throw std::invalid_argument("Missing genuine index virtual method");
 const auto index=services.query_index(services.context);
 state.current=index;state.previous=index;state.flags=1;
 if(!services.find_component)throw std::invalid_argument("Missing whole component query");
 void* component=services.find_component(services.context);
 if(!component)return;
 if(!services.activate_component)throw std::invalid_argument("Missing whole component activation");
 services.activate_component(component,1u,services.context);
}
}
