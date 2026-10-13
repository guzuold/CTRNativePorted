// SPDX-License-Identifier: GPL-2.0-only
#include "ctr/core/metadata_initialize_empty.h"
namespace ctr {
// Retail v01.00 FDE13d7730/1B consists solely of RET. No memory or service effects.
void metadata_initialize_empty(std::uintptr_t,std::uintptr_t) noexcept {}
}
