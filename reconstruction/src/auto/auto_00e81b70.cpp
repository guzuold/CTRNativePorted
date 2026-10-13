// SPDX-License-Identifier: GPL-2.0
// Retail entry:0xe81b70
// eboot SHA256:ca9c94411375ebec52de70fbc62f605ac8e2cea95595c627029969b5b9d9b5b1
// Verification:reports/reconstruction/auto/harness/auto_00e81b70/validation.json
#include "ctr/auto/auto_00e81b70.hpp"
#include <cstring>
namespace ctr {
std::uint8_t auto_00e81b70(const std::uint8_t* object,float){std::uint32_t value;std::memcpy(&value,object+324,4);return value!=3;}
}
