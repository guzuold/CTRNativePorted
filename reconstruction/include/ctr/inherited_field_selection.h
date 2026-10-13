#pragma once
#include <cstddef>
#include <cstdint>
#include <span>
namespace ctr {
struct InheritedFieldBinding {std::uint64_t token;std::span<const std::byte> storage;};
// Original0153f260..0153f392 prefix only, first-null allowed; later null/query missing.
std::uint64_t select_inherited_highest_offset(std::span<const InheritedFieldBinding> fields,std::int32_t count);
}
