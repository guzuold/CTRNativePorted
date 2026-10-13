#pragma once
#include <cstddef>
#include <cstdint>
#include <span>
namespace ctr {
struct MetaobjectParentLinkResult {
 std::uint32_t next_original_address;
 std::uint32_t reserve_count_bits;
};
// Original01518840 prefix. Resolved, separate152-byte nodes; no reservation call.
MetaobjectParentLinkResult link_metaobject_parent_prefix(
 std::span<std::byte> child, std::span<std::byte> parent,
 std::span<std::byte> previous_head, std::uint64_t child_address,
 std::uint64_t parent_address, std::uint64_t previous_head_address);
}
