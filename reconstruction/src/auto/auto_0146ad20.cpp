// SPDX-License-Identifier: GPL-2.0
#include "ctr/auto/auto_0146ad20.hpp"
#include "ctr/core/allocation_dispatch.h"
#include <cstring>
namespace ctr {std::uint64_t auto_0146ad20(std::span<const std::byte> descriptor,std::uint64_t first,std::int32_t second){std::uint32_t fourth;std::memcpy(&fourth,descriptor.data()+48,4);(void)fourth;return dispatch_allocation(descriptor,first,static_cast<std::uint64_t>(static_cast<std::int64_t>(second)));}}
