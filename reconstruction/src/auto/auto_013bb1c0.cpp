// SPDX-License-Identifier: GPL-2.0
// Retail entry:0x13bb1c0
// eboot SHA256:ca9c94411375ebec52de70fbc62f605ac8e2cea95595c627029969b5b9d9b5b1
// Verification:reports/reconstruction/auto/harness/auto_013bb1c0/validation.json
#include "ctr/auto/auto_013bb1c0.hpp"
#include <cstring>
namespace ctr {
void auto_013bb1c0(std::uint8_t* object){std::uint32_t counter;std::memcpy(&counter,object+16,4);std::int32_t signedCounter;std::memcpy(&signedCounter,&counter,4);if(signedCounter<=0)return;counter+=1u;do{const std::uint32_t value=counter-2u;--counter;std::memcpy(object+16,&value,4);std::memcpy(&signedCounter,&counter,4);}while(signedCounter>1);}
}
