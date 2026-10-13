#pragma once
#include <cstddef>
#include <cstdint>
#include <span>
namespace ctr {
// Full014e40d0 with original lookup-disabled manager byte0; map/mutex absent.
std::uint32_t register_metadata_lookup_disabled(std::span<std::byte> manager,
 std::span<std::byte> header,std::uint64_t header_token,
 std::span<std::byte> table,std::uint64_t table_token,
 std::span<std::byte> metadata,std::uint64_t metadata_token);
}
