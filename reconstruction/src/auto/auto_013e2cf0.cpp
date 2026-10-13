// SPDX-License-Identifier: GPL-2.0
// Retail entry:0x13e2cf0
// eboot SHA256:ca9c94411375ebec52de70fbc62f605ac8e2cea95595c627029969b5b9d9b5b1
// Verification:reports/reconstruction/auto/harness/auto_013e2cf0/validation.json
#include "ctr/auto/auto_013e2cf0.hpp"
#include <cstring>
namespace ctr {
std::uint32_t auto_013e2cf0(const std::uint8_t* object, std::uint64_t unused1, std::uint64_t unused2){(void)unused1;(void)unused2;std::uint32_t value;std::memcpy(&value,object+32,4);return ((value>>18u)&1u)-1u;}
}
