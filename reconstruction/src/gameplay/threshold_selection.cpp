// SPDX-License-Identifier: GPL-2.0-only
#include "ctr/gameplay/threshold_selection.h"
#include <stdexcept>
namespace ctr {
std::uint32_t threshold_selection(std::uint32_t state,std::uint32_t mode,
                                 float elapsed,const GameplayHandle* handle) {
 if(state-3u>1u || (mode!=1u && mode!=2u))return 0;
 const auto* p=static_cast<const ThresholdSelectionPayload*>(gameplay_handle_payload(handle));
 if(!p)throw std::invalid_argument("Missing threshold selection payload");
 const float threshold=(mode==1u?p->first:p->second)-1.0f;
 return elapsed>threshold?mode:0u;
}
}
