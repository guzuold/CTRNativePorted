// SPDX-License-Identifier: GPL-2.0
// Retail entry:0x13d7fe0
// eboot SHA256:ca9c94411375ebec52de70fbc62f605ac8e2cea95595c627029969b5b9d9b5b1
// Verification:reports/reconstruction/auto/harness/auto_013d7fe0/validation.json
#include "ctr/auto/auto_013d7fe0.hpp"
#include <cstring>
namespace ctr {
std::uint64_t auto_013d7fe0(const std::uint8_t* object){if(!object[56])return 0;std::uint64_t value;std::memcpy(&value,object+16,8);return value;}
}
