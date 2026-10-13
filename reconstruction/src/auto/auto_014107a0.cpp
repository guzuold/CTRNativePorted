// SPDX-License-Identifier: GPL-2.0
// Retail entry:0x14107a0
// eboot SHA256:ca9c94411375ebec52de70fbc62f605ac8e2cea95595c627029969b5b9d9b5b1
// Verification:reports/reconstruction/auto/harness/auto_014107a0/validation.json
#include "ctr/auto/auto_014107a0.hpp"
#include <cstring>
namespace ctr {
std::uint16_t auto_014107a0(const std::uint8_t* object){std::uint16_t value;std::memcpy(&value,object+72,2);return static_cast<std::uint16_t>(static_cast<std::uint32_t>(value)<<2);}
}
