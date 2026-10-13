// SPDX-License-Identifier: GPL-2.0
// Retail entry:0x32f340
// eboot SHA256:ca9c94411375ebec52de70fbc62f605ac8e2cea95595c627029969b5b9d9b5b1
// Verification:reports/reconstruction/auto/harness/auto_0032f340/validation.json
#include "ctr/auto/auto_0032f340.hpp"
#include <cstring>
namespace ctr {
void auto_0032f340(std::uint8_t* object){std::uint32_t value;std::memcpy(&value,object+104,4);value-=1u;std::memcpy(object+104,&value,4);}
}
