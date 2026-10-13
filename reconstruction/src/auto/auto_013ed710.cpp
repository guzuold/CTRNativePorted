// SPDX-License-Identifier: GPL-2.0
// Retail entry:0x13ed710
// eboot SHA256:ca9c94411375ebec52de70fbc62f605ac8e2cea95595c627029969b5b9d9b5b1
// Verification:reports/reconstruction/auto/harness/auto_013ed710/validation.json
#include "ctr/auto/auto_013ed710.hpp"
#include <cstring>
namespace ctr {
std::uint8_t auto_013ed710(const std::uint8_t* object,std::uint64_t needle){std::uintptr_t pointer;std::memcpy(&pointer,object+192,8);while(pointer){const auto* node=reinterpret_cast<const std::uint8_t*>(pointer);std::uint64_t start,length;std::memcpy(&start,node,8);std::memcpy(&length,node+8,8);if(start<=needle && start+length>needle)return 1;std::memcpy(&pointer,node+16,8);}return 0;}
}
