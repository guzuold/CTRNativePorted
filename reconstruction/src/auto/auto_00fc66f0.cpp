// SPDX-License-Identifier: GPL-2.0-only
// Retail actual0xfc66f0 whole53bytes; eboot SHA256 ca9c94411375ebec52de70fbc62f605ac8e2cea95595c627029969b5b9d9b5b1
// Verification: reports/reconstruction/auto/harness/auto_00fc66f0/validation.json
#include "ctr/auto/auto_00fc66f0.hpp"
#include "ctr/core/component_query.h"
#include <stdexcept>
#include <cstring>
namespace ctr {void auto_00fc66f0(std::span<std::byte> image,std::uint64_t base,std::uint64_t object,std::uint64_t expected,std::uint64_t cached_type,std::uint32_t mask) {
if(!object)return;const auto result=query_registered_component(image,base,object,expected,0,cached_type);
if(result){if(result<base || result-base>image.size() || 100>image.size()-(result-base))throw std::invalid_argument("Component write extent missing");std::uint32_t value;const auto offset=static_cast<std::size_t>(result-base)+96;std::memcpy(&value,image.data()+offset,4);value|=mask==4?3:mask;std::memcpy(image.data()+offset,&value,4);}
}}
