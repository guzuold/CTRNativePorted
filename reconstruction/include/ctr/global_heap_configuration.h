#pragma once
#include <cstddef>
#include <span>
namespace ctr {
// libc+5eea5..5ef8c, global block125a80..125ae0 inclusive.
void configure_global_heap_header(std::span<std::byte> header,std::span<std::byte> globals);
}
