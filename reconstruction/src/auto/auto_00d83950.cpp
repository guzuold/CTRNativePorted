// SPDX-License-Identifier: GPL-2.0
// Retail entry:0xd83950
// eboot SHA256:ca9c94411375ebec52de70fbc62f605ac8e2cea95595c627029969b5b9d9b5b1
// Verification:reports/reconstruction/auto/harness/auto_00d83950/validation.json
#include "ctr/auto/auto_00d83950.hpp"
#include <cstring>
namespace ctr {
void auto_00d83950(std::uint8_t* object,std::uint64_t,const std::uint8_t* other,std::uint32_t,std::uint64_t){std::uint32_t value;std::memcpy(&value,other+44,4);object[96]=static_cast<std::uint8_t>(value==5);}
}
