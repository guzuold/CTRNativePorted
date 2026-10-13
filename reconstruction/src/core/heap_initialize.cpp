// SPDX-License-Identifier: GPL-2.0-only
#include "ctr/core/heap_initialize.h"
#include <algorithm>
#include <cstring>
#include <stdexcept>
namespace ctr {
std::uintptr_t heap_initialize(std::span<std::byte> heap, std::uintptr_t identity) {
 if ((identity & 7) != 0) throw std::runtime_error("Missing original misalignment diagnostic 0x1ada3d0");
 if (heap.size() < 0x1988) throw std::runtime_error("Missing bounded heap extent");
 std::memcpy(heap.data()+0x10, &identity, 8);
 std::memcpy(heap.data()+0x18, &identity, 8);
 std::fill(heap.begin()+0x20, heap.begin()+0x88, std::byte{});
 for (std::size_t offset=0x88; offset<0x1988; offset+=8) std::memcpy(heap.data()+offset, &identity, 8);
 return identity;
}
}
