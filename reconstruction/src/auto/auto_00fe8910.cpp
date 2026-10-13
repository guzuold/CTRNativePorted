// SPDX-License-Identifier: GPL-2.0-only
// Retail actual0xfe8910 whole34bytes; eboot SHA256 ca9c94411375ebec52de70fbc62f605ac8e2cea95595c627029969b5b9d9b5b1
// Verification: reports/reconstruction/auto/harness/auto_00fe8910/validation.json
#include "ctr/auto/auto_00fe8910.hpp"
#include "ctr/core/component_query.h"
#include <stdexcept>
namespace ctr {void auto_00fe8910(std::span<std::byte> image,std::uint64_t base,std::uint64_t object,std::uint64_t expected,std::uint64_t cached_type) {
if(!object)return;const auto result=query_registered_component(image,base,object,expected,0,cached_type);
if(result){if(result<base || result-base>image.size() || 65>image.size()-(result-base))throw std::invalid_argument("Component write extent missing");image[static_cast<std::size_t>(result-base)+64]^=std::byte{1};}
}}
