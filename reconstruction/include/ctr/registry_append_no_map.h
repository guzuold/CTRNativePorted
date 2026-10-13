#pragma once
#include <cstddef>
#include <cstdint>
#include <span>
namespace ctr {
// Full014e4330 final storage, preallocated array, absent optional map.
void append_registry_without_map(std::span<std::byte> manager,
 std::span<std::byte> header,std::uint64_t header_token,
 std::span<std::byte> table,std::uint64_t table_token,
 std::span<std::byte> metadata,std::uint64_t metadata_token);
}
