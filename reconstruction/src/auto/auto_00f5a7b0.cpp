// SPDX-License-Identifier: GPL-2.0
// Retail entry:0xf5a7b0
// eboot SHA256:ca9c94411375ebec52de70fbc62f605ac8e2cea95595c627029969b5b9d9b5b1
// Verification:reports/reconstruction/auto/harness/auto_00f5a7b0/validation.json
#include "ctr/auto/auto_00f5a7b0.hpp"
#include <cstring>
namespace ctr {
void auto_00f5a7b0(std::uint8_t* object,float value){
 if(object!=nullptr)std::memcpy(object+96,&value,4);
}
}
