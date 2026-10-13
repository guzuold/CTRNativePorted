#pragma once
#include <cstddef>
#include <cstdint>
#include <span>
namespace ctr {
// Full013c6860 return with already allocated capacity; no reserve service.
void append_registry_array_storage(std::span<std::byte> header,
 std::span<std::byte> table,std::uint64_t table_token,std::span<const std::byte> source);
}
