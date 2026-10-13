// SPDX-License-Identifier: GPL-2.0
// Retail entry:0x133cca0
// eboot SHA256:ca9c94411375ebec52de70fbc62f605ac8e2cea95595c627029969b5b9d9b5b1
// Verification:reports/reconstruction/auto/harness/auto_0133cca0/validation.json
#include "ctr/auto/auto_0133cca0.hpp"
#include <cstring>
namespace ctr {
void auto_0133cca0(const std::uint8_t* unused,std::uint8_t* buffer,std::uint32_t count){(void)unused;const std::uint64_t zero64=0;const std::uint32_t zero32=0;for(std::uint32_t i=0;i<count;++i){auto* item=buffer+static_cast<std::size_t>(i)*16;std::memcpy(item,&zero64,8);std::memcpy(item+8,&zero32,4);}}
}
