// SPDX-License-Identifier: GPL-2.0
// Retail entry:0xf03460
// eboot SHA256:ca9c94411375ebec52de70fbc62f605ac8e2cea95595c627029969b5b9d9b5b1
// Verification:reports/reconstruction/auto/harness/auto_00f03460/validation.json
#include "ctr/auto/auto_00f03460.hpp"
#include <cstring>
namespace ctr {
std::uint8_t auto_00f03460(const std::uint8_t* object){std::int32_t left,right;std::memcpy(&left,object+160,4);std::memcpy(&right,object+144,4);return left>=right;}
}
