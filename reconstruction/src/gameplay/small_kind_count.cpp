// SPDX-License-Identifier: GPL-2.0-only
#include "ctr/gameplay/small_kind_count.h"
#include <stdexcept>
namespace ctr { std::uint32_t small_kind_count(std::int32_t count,std::span<const std::uint32_t> kinds){if(count<=0)return 0;if(static_cast<std::size_t>(count)>kinds.size())throw std::invalid_argument("Missing original kind records");std::uint32_t result=0;for(std::int32_t i=0;i<count;++i)result+=kinds[i]<8u;return result;} }
