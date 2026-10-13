#pragma once
#include "ctr/system_pool_backend.h"
#include <array>
namespace ctr {
// Borrowed backing, unregistered heap, exclusive mutex owner; logical caller frame.
std::array<std::uint64_t,16> destroy_borrowed_mspace(SystemPoolBackend& backend,
 const std::array<std::uint64_t,16>& entry,std::span<std::byte> stack);
}
