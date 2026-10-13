#pragma once
#include "ctr/bitmap_split_three_free.h"
namespace ctr {
// Borrows the same creator and live derived storage for exactly one four-step cycle.
class BitmapPostSplitCycle {
 std::span<std::byte> pool_;
 SystemPoolBackend* backend_;
 unsigned stage_{};
 std::uint64_t first_request_{},first_address_{},second_address_{};
 void validate() const;
public:
 BitmapPostSplitCycle(std::span<std::byte>,SystemPoolBackend&);
 BitmapPostSplitCycle(const BitmapPostSplitCycle&)=delete;
 BitmapPostSplitCycle& operator=(const BitmapPostSplitCycle&)=delete;
 BitmapAllocation allocate(std::uint64_t,std::uint32_t);
 std::uint32_t free(std::uint64_t);
};
}
