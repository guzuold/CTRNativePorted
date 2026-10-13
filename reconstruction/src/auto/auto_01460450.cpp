// SPDX-License-Identifier: GPL-2.0
// Retail entry:0x1460450
// eboot SHA256:ca9c94411375ebec52de70fbc62f605ac8e2cea95595c627029969b5b9d9b5b1
// Verification:reports/reconstruction/auto/harness/auto_01460450/validation.json
#include "ctr/auto/auto_01460450.hpp"
#include <cstring>
namespace ctr {
std::uint32_t auto_01460450(const std::uint8_t* object){std::uintptr_t pointer;std::memcpy(&pointer,object+40,8);std::uint16_t value;std::memcpy(&value,reinterpret_cast<const std::uint8_t*>(pointer)+90,2);return value;}
}
