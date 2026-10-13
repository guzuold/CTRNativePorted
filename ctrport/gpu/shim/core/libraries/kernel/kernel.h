// SPDX-License-Identifier: GPL-2.0-or-later
// ctrport: the parts of shadPS4's kernel.h used by the vendored libraries
// (adapted from bbport gpu/shim/core/libraries/kernel/kernel.h, GPL-2.0).
#pragma once
#include "common/types.h"
#include "core/libraries/kernel/orbis_error.h"
namespace Libraries::Kernel {
int* PS4_SYSV_ABI __Error();
} // namespace Libraries::Kernel
