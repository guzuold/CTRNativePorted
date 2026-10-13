// SPDX-License-Identifier: GPL-2.0
// Retail entry:0x13cd680
// eboot SHA256:ca9c94411375ebec52de70fbc62f605ac8e2cea95595c627029969b5b9d9b5b1
// Verification:reports/reconstruction/auto/harness/auto_013cd680/validation.json
#include "ctr/auto/auto_013cd680.hpp"
#include <cstring>
#include <xmmintrin.h>
namespace ctr {
__declspec(noinline) float auto_013cd680(const std::uint8_t* object){float v0;std::memcpy(&v0,object+56,4);float v1;std::memcpy(&v1,object+40,4);__m128 left=_mm_load_ss(&v0),right=_mm_load_ss(&v1);return _mm_cvtss_f32(_mm_div_ss(left,right));}
}
