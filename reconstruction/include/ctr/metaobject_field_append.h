#pragma once
#include <span>
#include <cstddef>
#include <cstdint>
namespace ctr {
// Full01518e10/00a74a30 no-growth append and nonterminal release, resolved storage.
void append_metaobject_field_no_growth(std::span<std::byte> metadata,
 std::span<std::byte> table,std::uint64_t table_token,
 std::span<std::byte> field,std::uint64_t field_token);
}
