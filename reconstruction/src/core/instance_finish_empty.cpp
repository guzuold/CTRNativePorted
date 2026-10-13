// SPDX-License-Identifier: GPL-2.0-only
#include "ctr/core/instance_finish_empty.h"
namespace ctr {
// Retail v01.00 FDE13d7790/1B consists solely of RET. No memory or service effects.
void instance_finish_empty(std::uintptr_t,std::uintptr_t) noexcept {}
}
