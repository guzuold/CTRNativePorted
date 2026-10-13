// SPDX-License-Identifier: GPL-2.0
// Retail entry:0xbc3b90
// eboot SHA256:ca9c94411375ebec52de70fbc62f605ac8e2cea95595c627029969b5b9d9b5b1
// Verification:reports/reconstruction/auto/harness/auto_00bc3b90/validation.json
#include "ctr/auto/auto_00bc3b90.hpp"
#include <cstring>
namespace ctr {
std::uint32_t auto_00bc3b90(const std::uint8_t* object){std::uint32_t value;std::memcpy(&value,object+12,4);return value*value;}
}
