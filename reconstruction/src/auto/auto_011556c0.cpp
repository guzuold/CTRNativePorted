// SPDX-License-Identifier: GPL-2.0
// Retail entry:0x11556c0
// eboot SHA256:ca9c94411375ebec52de70fbc62f605ac8e2cea95595c627029969b5b9d9b5b1
// Verification:reports/reconstruction/auto/harness/auto_011556c0/validation.json
#include "ctr/auto/auto_011556c0.hpp"
#include <cstring>
namespace ctr {
void auto_011556c0(std::uint64_t,std::uint32_t index,std::uint8_t* keys,const std::uint8_t* key){std::uint32_t value;std::memcpy(&value,key,4);std::memcpy(keys+static_cast<std::uint64_t>(index)*4,&value,4);}
}
