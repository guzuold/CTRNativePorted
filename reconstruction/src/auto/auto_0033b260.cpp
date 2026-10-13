// SPDX-License-Identifier: GPL-2.0
// Retail entry:0x33b260
// eboot SHA256:ca9c94411375ebec52de70fbc62f605ac8e2cea95595c627029969b5b9d9b5b1
// Verification:reports/reconstruction/auto/harness/auto_0033b260/validation.json
#include "ctr/auto/auto_0033b260.hpp"
#include <cstring>
namespace ctr {
void auto_0033b260(std::uint8_t* object,std::uint32_t mask){std::uint32_t value;std::memcpy(&value,object+240,4);value|=mask;std::memcpy(object+240,&value,4);}
}
