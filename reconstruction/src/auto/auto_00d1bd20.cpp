// SPDX-License-Identifier: GPL-2.0-only
// Retail actual0xd1bd20 whole34bytes; eboot SHA256 ca9c94411375ebec52de70fbc62f605ac8e2cea95595c627029969b5b9d9b5b1
// Verification: reports/reconstruction/auto/harness/auto_00d1bd20/validation.json
#include "ctr/auto/auto_00d1bd20.hpp"
#include "ctr/core/component_query.h"
#include <cstring>
#include <stdexcept>
namespace ctr {std::uint64_t auto_00d1bd20(std::span<std::byte> image,std::uint64_t base,std::uint64_t object,std::uint64_t expected,std::uint64_t cached_type) {
const auto result=query_registered_component(image,base,object,expected,0,cached_type);
if(result){if(result<base || result-base>image.size() || 108>image.size()-(result-base))throw std::invalid_argument("Component write extent missing");
std::uint32_t value;const auto offset=static_cast<std::size_t>(result-base)+104;std::memcpy(&value,image.data()+offset,4);--value;std::memcpy(image.data()+offset,&value,4);}
return result;}}
