#pragma once
#include "ctr/bitmap_group2_large_remainder_second_free.h"
#include "ctr/bitmap_group2_reallocation_complete_free.h"
namespace ctr {
// Borrows the same creator and live derived storage for exactly one four-step cycle after the PEAK3 remainder history.
class BitmapGroup2RemainderPostCycle {
 std::span<std::byte> pool_;
 SystemPoolBackend* backend_;
 unsigned stage_{};
 std::uint64_t first_request_{},first_address_{},second_address_{},resource_{},peak_request_{},peak_occupied_{};
 void validate() const;
public:
 BitmapGroup2RemainderPostCycle(std::span<std::byte>,SystemPoolBackend&);
 BitmapGroup2RemainderPostCycle(const BitmapGroup2RemainderPostCycle&)=delete;
 BitmapGroup2RemainderPostCycle& operator=(const BitmapGroup2RemainderPostCycle&)=delete;
 BitmapAllocation allocate(std::uint64_t,std::uint32_t);
 std::uint32_t free(std::uint64_t);
};
}
