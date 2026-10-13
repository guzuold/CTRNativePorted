// SPDX-License-Identifier: GPL-2.0-only
#include "ctr/core/metadata_cached_type_resolve.h"
#include <stdexcept>
namespace ctr {
std::uintptr_t metadata_cached_type_resolve(std::uintptr_t cached,
 std::span<const std::byte> type) {
 if(!cached || type.size()<0x5d || (std::to_integer<unsigned>(type[0x5c])&4)==0)
  throw std::runtime_error("Missing original type registry/pool/name lookup");
 return cached;
}
}
