// SPDX-License-Identifier: GPL-2.0
// Retail entry:0x13b7730
// eboot SHA256:ca9c94411375ebec52de70fbc62f605ac8e2cea95595c627029969b5b9d9b5b1
// Verification:reports/reconstruction/auto/harness/auto_013b7730/validation.json
#include "ctr/auto/auto_013b7730.hpp"
#include <cstring>
#include <xmmintrin.h>
namespace ctr {
__declspec(noinline) std::uint8_t auto_013b7730(const std::uint8_t* object){float v0;std::memcpy(&v0,object+24,4);return static_cast<std::uint8_t>(_mm_ucomigt_ss(_mm_setzero_ps(),_mm_load_ss(&v0)));}
}
