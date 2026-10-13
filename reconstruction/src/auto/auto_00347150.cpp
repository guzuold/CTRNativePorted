// SPDX-License-Identifier: GPL-2.0
// Retail entry:0x347150
// eboot SHA256:ca9c94411375ebec52de70fbc62f605ac8e2cea95595c627029969b5b9d9b5b1
// Verification:reports/reconstruction/auto/harness/auto_00347150/validation.json
#include "ctr/auto/auto_00347150.hpp"
#include <cstring>
namespace ctr {
std::uint8_t auto_00347150(const std::uint8_t* object,std::uint32_t mask){
 const std::uint8_t* previous;
 do { previous=object;std::uintptr_t next;std::memcpy(&next,previous+24,8);object=reinterpret_cast<const std::uint8_t*>(next); }while(object!=nullptr);
 return (previous[33]&mask)==0;
}
}
