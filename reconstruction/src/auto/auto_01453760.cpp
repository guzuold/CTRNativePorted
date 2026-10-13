// SPDX-License-Identifier: GPL-2.0
// Retail entry:0x1453760
// eboot SHA256:ca9c94411375ebec52de70fbc62f605ac8e2cea95595c627029969b5b9d9b5b1
// Verification:reports/reconstruction/auto/harness/auto_01453760/validation.json
#include "ctr/auto/auto_01453760.hpp"
#include <cstring>
namespace ctr {
std::uint64_t auto_01453760(const std::uint8_t* object,std::uint32_t ignored){(void)ignored;std::uint64_t first;std::memcpy(&first,object+96,8);if(first)return first;std::uint64_t second;std::memcpy(&second,object+72,8);return second;}
}
