// SPDX-License-Identifier: GPL-2.0
// Retail entry:0x143c4e0
// eboot SHA256:ca9c94411375ebec52de70fbc62f605ac8e2cea95595c627029969b5b9d9b5b1
// Verification:reports/reconstruction/auto/harness/auto_0143c4e0/validation.json
#include "ctr/auto/auto_0143c4e0.hpp"
#include <cstring>
namespace ctr {
std::int32_t auto_0143c4e0(const std::uint8_t* object,std::uint32_t needle){std::int32_t count;std::memcpy(&count,object+56,4);if(count<=0)return -1;std::uintptr_t pointer;std::memcpy(&pointer,object+72,8);const auto* data=reinterpret_cast<const std::uint8_t*>(pointer);for(std::int32_t index=0;index<count;++index){std::uint32_t value;std::memcpy(&value,data+static_cast<std::size_t>(index)*4,4);if(value==needle)return index;}return -1;}
}
