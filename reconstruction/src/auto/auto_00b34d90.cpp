// SPDX-License-Identifier: GPL-2.0
// Retail entry:0xb34d90
// eboot SHA256:ca9c94411375ebec52de70fbc62f605ac8e2cea95595c627029969b5b9d9b5b1
// Verification:reports/reconstruction/auto/harness/auto_00b34d90/validation.json
#include "ctr/auto/auto_00b34d90.hpp"
#include <cstring>
namespace ctr {
void auto_00b34d90(std::uint8_t* object){const std::uint32_t value=5u;std::memcpy(object+24,&value,4);}
}
