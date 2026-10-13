// SPDX-License-Identifier: GPL-2.0
// Retail entry:0xe81ae0
// eboot SHA256:ca9c94411375ebec52de70fbc62f605ac8e2cea95595c627029969b5b9d9b5b1
// Verification:reports/reconstruction/auto/harness/auto_00e81ae0/validation.json
#include "ctr/auto/auto_00e81ae0.hpp"
#include <cstring>
namespace ctr {
float auto_00e81ae0(const std::uint8_t* object,std::uint32_t){float value;std::memcpy(&value,object+656,4);return value;}
}
