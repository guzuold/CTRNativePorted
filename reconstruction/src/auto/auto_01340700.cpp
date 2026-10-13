// SPDX-License-Identifier: GPL-2.0
// Retail entry:0x1340700
// eboot SHA256:ca9c94411375ebec52de70fbc62f605ac8e2cea95595c627029969b5b9d9b5b1
// Verification:reports/reconstruction/auto/harness/auto_01340700/validation.json
#include "ctr/auto/auto_01340700.hpp"
#include <cstring>
namespace ctr {
std::uint64_t auto_01340700(const std::uint8_t* object, std::uint32_t index){std::uint64_t value;std::memcpy(&value,object+5840+static_cast<std::size_t>(index)*8,8);return value;}
}
