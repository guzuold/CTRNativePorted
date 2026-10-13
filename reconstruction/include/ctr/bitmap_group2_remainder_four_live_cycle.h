#pragma once
#include "ctr/bitmap_group2_remainder_post_cycle.h"
#include <array>
namespace ctr {
// Borrows the same creator and live derived storage for exactly one eight-step cycle after the
// PEAK3 remainder history and its extra pair cycle: FIRST512..640, SECOND256 and two NORMAL129..192
// blocks split in order from the singleton free tail (live count 4 above retained peak count 3),
// then freed LIFO, each forward-merging into the free tail.
class BitmapGroup2RemainderFourLiveCycle {
 std::span<std::byte> pool_;
 SystemPoolBackend* backend_;
 unsigned stage_{};
 std::array<std::uint64_t,4> request_{},address_{},size_{};
 std::uint64_t resource_{},peak_request_{},peak_occupied_{};
 void validate() const;
public:
 BitmapGroup2RemainderFourLiveCycle(std::span<std::byte>,SystemPoolBackend&);
 BitmapGroup2RemainderFourLiveCycle(const BitmapGroup2RemainderFourLiveCycle&)=delete;
 BitmapGroup2RemainderFourLiveCycle& operator=(const BitmapGroup2RemainderFourLiveCycle&)=delete;
 BitmapAllocation allocate(std::uint64_t,std::uint32_t);
 std::uint32_t free(std::uint64_t);
};
}
