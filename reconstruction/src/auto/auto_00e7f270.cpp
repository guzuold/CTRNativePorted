// SPDX-License-Identifier: GPL-2.0
// Retail entry:0xe7f270
// eboot SHA256:ca9c94411375ebec52de70fbc62f605ac8e2cea95595c627029969b5b9d9b5b1
// Verification:reports/reconstruction/auto/harness/auto_00e7f270/validation.json
#include "ctr/auto/auto_00e7f270.hpp"
#include <cstring>
namespace ctr {
float auto_00e7f270(const std::uint8_t* object, std::uint32_t index){float value;std::memcpy(&value,object+92+static_cast<std::size_t>(index)*4,4);return value;}
}
