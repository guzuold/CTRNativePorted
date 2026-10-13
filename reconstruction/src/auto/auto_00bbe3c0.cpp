// SPDX-License-Identifier: GPL-2.0
// Retail entry:0xbbe3c0
// eboot SHA256:ca9c94411375ebec52de70fbc62f605ac8e2cea95595c627029969b5b9d9b5b1
// Verification:reports/reconstruction/auto/harness/auto_00bbe3c0/validation.json
#include "ctr/auto/auto_00bbe3c0.hpp"
#include <cstring>
#include <xmmintrin.h>
namespace ctr {
__declspec(noinline) float auto_00bbe3c0(const std::uint8_t* object,const std::uint8_t*,float){float v0;std::memcpy(&v0,object+24,4);float v1;std::memcpy(&v1,object+28,4);float v2;std::memcpy(&v2,object+32,4);__m128 product=_mm_mul_ss(_mm_load_ss(&v0),_mm_load_ss(&v1));return _mm_cvtss_f32(_mm_add_ss(product,_mm_load_ss(&v2)));}
}
