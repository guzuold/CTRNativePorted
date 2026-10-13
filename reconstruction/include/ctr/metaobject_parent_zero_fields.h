#pragma once
#include "ctr/metaobject_parent_link.h"
namespace ctr {
// Full01518840 return: resolved nodes, parent/child count0, child buffer null.
// No capacity reservation or inherited field-copy callback required in this domain.
void link_metaobject_parent_zero_fields(std::span<std::byte> child,
 std::span<std::byte> parent,std::span<std::byte> previous_head,
 std::uint64_t child_address,std::uint64_t parent_address,
 std::uint64_t previous_head_address);
}
