#pragma once
#include <cstddef>
#include <cstdint>
#include <span>
namespace ctr {
// Whole original 0156b490. Backing corresponds to header+0x20.
void dependency_list_resize(std::span<std::byte> header,
                            std::span<std::byte> backing, std::int32_t requested);
}
