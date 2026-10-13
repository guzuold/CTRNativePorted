// SPDX-License-Identifier: GPL-2.0
// Retail entry:0x12d7b90
// eboot SHA256:ca9c94411375ebec52de70fbc62f605ac8e2cea95595c627029969b5b9d9b5b1
// Verification:reports/reconstruction/auto/harness/auto_012d7b90/validation.json
#include "ctr/auto/auto_012d7b90.hpp"
#include <cstring>
namespace ctr {
void auto_012d7b90(const std::uint8_t* source,std::uint8_t* destination){std::uint64_t value;std::memcpy(&value,source+20,8);std::memcpy(destination+848,&value,8);}
}
