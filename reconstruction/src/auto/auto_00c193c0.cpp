// SPDX-License-Identifier: GPL-2.0
// Retail entry:0xc193c0
// eboot SHA256:ca9c94411375ebec52de70fbc62f605ac8e2cea95595c627029969b5b9d9b5b1
// Verification:reports/reconstruction/auto/harness/auto_00c193c0/validation.json
#include "ctr/auto/auto_00c193c0.hpp"
#include <cstring>
namespace ctr {
void auto_00c193c0(std::uint64_t,std::uint32_t count,std::uint8_t* object){object[32]=static_cast<std::uint8_t>(count!=0);}
}
