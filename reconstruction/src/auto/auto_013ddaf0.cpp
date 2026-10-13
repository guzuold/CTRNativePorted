// SPDX-License-Identifier: GPL-2.0
// Retail entry:0x13ddaf0
// eboot SHA256:ca9c94411375ebec52de70fbc62f605ac8e2cea95595c627029969b5b9d9b5b1
// Verification:reports/reconstruction/auto/harness/auto_013ddaf0/validation.json
#include "ctr/auto/auto_013ddaf0.hpp"
#include <cstring>
namespace ctr {
void auto_013ddaf0(std::uint8_t* object,std::uint32_t condition){
 if(condition!=0){const std::uint32_t zero=0;std::memcpy(object+16,&zero,4);}
}
}
