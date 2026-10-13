#pragma once
#include <cstdint>
#include <span>
namespace ctr {
// Original libc snprintf literal%lu, LP64 uint64 argument; not generic varargs.
std::uint32_t format_unsigned_long_decimal(std::span<char> destination,std::uint64_t value);
}
