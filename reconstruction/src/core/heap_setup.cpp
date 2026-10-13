// SPDX-License-Identifier: GPL-2.0-only
#include "ctr/core/heap_setup.h"
#include "ctr/core/heap_initialize.h"
#include "ctr/core/heap_region_publish.h"
namespace ctr {
std::uintptr_t heap_setup(std::span<std::byte> memory,std::uintptr_t identity,std::uint64_t length){
 const auto heap=heap_initialize(memory,identity);
 heap_region_publish(memory,heap,0x1988,length-0x1988);
 return heap;
}
}
