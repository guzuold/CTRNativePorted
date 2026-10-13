// SPDX-License-Identifier: GPL-2.0
// Retail entry:0x144a950
// eboot SHA256:ca9c94411375ebec52de70fbc62f605ac8e2cea95595c627029969b5b9d9b5b1
// Verification:reports/reconstruction/auto/harness/auto_0144a950/validation.json
#include "ctr/auto/auto_0144a950.hpp"
#include <cstring>
namespace ctr {
void auto_0144a950(std::uint8_t* object,std::uint64_t needle,const std::uint8_t* keys,const std::uint8_t* values,std::uint32_t count){for(std::uint32_t index=0;index<count;++index){std::uint64_t key;std::memcpy(&key,keys+static_cast<std::size_t>(index)*8,8);if(key==needle){std::uint64_t value;std::memcpy(&value,values+static_cast<std::size_t>(index)*8,8);std::memcpy(object+72,&value,8);return;}}}
}
