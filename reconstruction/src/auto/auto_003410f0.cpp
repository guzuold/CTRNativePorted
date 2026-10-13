// SPDX-License-Identifier: GPL-2.0
// Retail entry:0x3410f0
// eboot SHA256:ca9c94411375ebec52de70fbc62f605ac8e2cea95595c627029969b5b9d9b5b1
// Verification:reports/reconstruction/auto/harness/auto_003410f0/validation.json
#include "ctr/auto/auto_003410f0.hpp"
#include <cstring>
namespace ctr {
void auto_003410f0(std::uint8_t* object,std::uint32_t mask){std::uint32_t value;std::memcpy(&value,object+260,4);value&=~mask;std::memcpy(object+260,&value,4);}
}
