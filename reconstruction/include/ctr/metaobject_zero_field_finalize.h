#pragma once
#include <cstddef>
#include <span>
namespace ctr {
// Full01518640 storage-only no-field branch; no completed registration claim.
void finalize_zero_field_metaobject_storage(std::span<std::byte> metadata);
}
