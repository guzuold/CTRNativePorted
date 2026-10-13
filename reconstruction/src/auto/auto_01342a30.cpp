// SPDX-License-Identifier: GPL-2.0
// Retail entry:0x1342a30
// eboot SHA256:ca9c94411375ebec52de70fbc62f605ac8e2cea95595c627029969b5b9d9b5b1
// Verification:reports/reconstruction/auto/harness/auto_01342a30/validation.json
#include "ctr/auto/auto_01342a30.hpp"
#include <cstring>
namespace ctr {
float auto_01342a30(const std::uint8_t* object){float value;std::memcpy(&value,object+5732,4);return value;}
}
