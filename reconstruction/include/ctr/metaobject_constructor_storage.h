#pragma once
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
namespace ctr {
// Original0153d2b0 storage effects; pool selection supplied, not implemented here.
void initialize_metaobject_constructor_storage(std::span<std::byte> storage,
 std::optional<std::int32_t> selected_pool_index);
}
