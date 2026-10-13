// SPDX-License-Identifier: GPL-2.0
// Retail entry:0xf143b0
// eboot SHA256:ca9c94411375ebec52de70fbc62f605ac8e2cea95595c627029969b5b9d9b5b1
// Verification:reports/reconstruction/auto/harness/auto_00f143b0/validation.json
#include "ctr/auto/auto_00f143b0.hpp"
#include <cstring>
namespace ctr {
float auto_00f143b0(const std::uint8_t* object, std::uint8_t ignored0, std::uint8_t ignored1){(void)ignored0;(void)ignored1;float value;std::memcpy(&value,object+1028,4);return value;}
}
