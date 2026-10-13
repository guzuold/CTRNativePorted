// SPDX-License-Identifier: GPL-2.0
// Retail entry:0xc9da10
// eboot SHA256:ca9c94411375ebec52de70fbc62f605ac8e2cea95595c627029969b5b9d9b5b1
// Verification:reports/reconstruction/auto/harness/auto_00c9da10/validation.json
#include "ctr/auto/auto_00c9da10.hpp"
#include <cstring>
namespace ctr {
void auto_00c9da10(std::uint64_t,std::uint8_t* other){std::uint16_t value;std::memcpy(&value,other+142,2);value=static_cast<std::uint16_t>(value - 1);std::memcpy(other+142,&value,2);}
}
