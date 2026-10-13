// SPDX-License-Identifier: GPL-2.0
// Retail entry:0x10cff70
// eboot SHA256:ca9c94411375ebec52de70fbc62f605ac8e2cea95595c627029969b5b9d9b5b1
// Verification:reports/reconstruction/auto/harness/auto_010cff70/validation.json
#include "ctr/auto/auto_010cff70.hpp"
#include <cstring>
#include <xmmintrin.h>
namespace ctr {
std::uint32_t auto_010cff70(std::uint64_t,const std::uint8_t* key){float value;std::memcpy(&value,key,4);return static_cast<std::uint32_t>(_mm_cvttss_si64(_mm_set_ss(value)));}
}
