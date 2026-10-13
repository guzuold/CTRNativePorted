// SPDX-License-Identifier: GPL-2.0
// Retail entry:0x12db010
// eboot SHA256:ca9c94411375ebec52de70fbc62f605ac8e2cea95595c627029969b5b9d9b5b1
// Verification:reports/reconstruction/auto/harness/auto_012db010/validation.json
#include "ctr/auto/auto_012db010.hpp"
#include <cstring>
namespace ctr {
void auto_012db010(const std::uint8_t* source,std::uint8_t* destination){std::uint32_t value;std::memcpy(&value,source+20,4);std::memcpy(destination+1288,&value,4);}
}
