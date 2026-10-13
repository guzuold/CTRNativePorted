#pragma once
#include <cstddef>
#include <cstdint>
#include <span>
namespace ctr {
// Prepared419/null-parent/no-fields/null-property caller continuation.
std::uint64_t finish_prepared_metaobject_registration(std::span<std::byte> manager,
 std::span<std::byte> header,std::uint64_t header_token,
 std::span<std::byte> table,std::uint64_t table_token,
 std::span<std::byte> metadata,std::uint64_t metadata_token,std::uint64_t base_token);
}
