// SPDX-License-Identifier: GPL-2.0-only
// Retail actual0xd1bb50 whole102bytes;eboot SHA256 ca9c94411375ebec52de70fbc62f605ac8e2cea95595c627029969b5b9d9b5b1
// Verification: reports/reconstruction/auto/harness/auto_00d1bb50/validation.json
#include "ctr/auto/auto_00d1bb50.hpp"
#include "ctr/core/component_query.h"
#include <stdexcept>
#include <cstring>
namespace ctr {void auto_00d1bb50(std::span<std::byte> image,std::uint64_t base,std::uint64_t object,std::uint64_t expected,std::uint64_t cached_type,std::uint64_t event) {
 const auto result=query_registered_component(image,base,object,expected,0,cached_type);
 if(!result)return;
 auto offset=[&](std::uint64_t address){if(address<base || address-base>image.size() || 4>image.size()-(address-base))throw std::invalid_argument("Counter/flag address extent missing");return static_cast<std::size_t>(address-base);};
 for(unsigned bit=0;bit<4;++bit){std::uint32_t flags;std::memcpy(&flags,image.data()+offset(event+64),4);if(flags&(1u<<bit)){const auto at=offset(result+72+bit*4);std::uint32_t value;std::memcpy(&value,image.data()+at,4);++value;std::memcpy(image.data()+at,&value,4);}}
}}
