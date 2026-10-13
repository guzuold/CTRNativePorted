#pragma once
#include <cstddef>
#include <cstdint>
#include <span>
#include <string_view>
namespace ctr {
// Full original libc+5ecd0 helper; prefix length <=15, destination32.
std::uint64_t make_heap_pointer_name(std::span<std::byte> name, std::string_view prefix, std::uint64_t token);
}
