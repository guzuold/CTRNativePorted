// SPDX-License-Identifier: GPL-2.0
// Retail entry:0x33df50
// eboot SHA256:ca9c94411375ebec52de70fbc62f605ac8e2cea95595c627029969b5b9d9b5b1
// Verification:reports/reconstruction/auto/harness/auto_0033df50/validation.json
#include "ctr/auto/auto_0033df50.hpp"
#include <cstring>
namespace ctr {
void auto_0033df50(std::uint8_t* object,std::uint32_t mask){std::uint32_t value;std::memcpy(&value,object+276,4);value|=mask;std::memcpy(object+276,&value,4);}
}
