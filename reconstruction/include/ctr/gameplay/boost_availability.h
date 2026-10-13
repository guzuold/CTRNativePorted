#pragma once
#include "ctr/gameplay/boost_phase.h"
namespace ctr {
// 00ee6300. Borrowed stable handles; mutating resolution remains missing.
bool boost_available(const BoostPhaseHandle* first,const BoostPhaseHandle* second,
 unsigned char input3a,unsigned char input3b);
}
