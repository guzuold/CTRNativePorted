#pragma once
#include "ctr/native_mutex.h"
#include <array>
namespace ctr {
struct GlobalHeapMutexStage {std::array<std::byte,32> name{};std::unique_ptr<NativeMutex> mutex;};
// Bounded libc+5ee10..5eea5 only, actual OS header; zero or matching selected cell.
GlobalHeapMutexStage initialize_global_heap_mutex_stage(std::span<std::byte> header,std::uint64_t& selected);
}
