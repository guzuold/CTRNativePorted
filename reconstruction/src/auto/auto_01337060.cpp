// SPDX-License-Identifier: GPL-2.0
// Retail entry:0x1337060
// eboot SHA256:ca9c94411375ebec52de70fbc62f605ac8e2cea95595c627029969b5b9d9b5b1
// Verification:reports/reconstruction/auto/harness/auto_01337060/validation.json
#include "ctr/auto/auto_01337060.hpp"
#include <cstring>
namespace ctr {
void auto_01337060(std::uint8_t* object,std::uint64_t value){std::memcpy(object+1232,&value,8);}
}
