#pragma once
#include "ctr/bitmap_group2_remainder_four_live_cycle.h"
namespace ctr {
// Borrows the same creator and live derived storage for exactly one eight-step cycle after the
// four-live LIFO cycle (retained peak count4): the same four in-order tail splits, then FIFO frees.
// FIRST becomes a free prefix singleton beside the live successor, SECOND and the first small block
// merge backward into it, and the last block merges backward and forward into the free tail.
class BitmapGroup2RemainderFourLiveFifoCycle {
 std::span<std::byte> pool_;
 SystemPoolBackend* backend_;
 unsigned stage_{};
 std::array<std::uint64_t,4> request_{},address_{},size_{};
 std::uint64_t resource_{},peak_request_{},peak_occupied_{};
 void validate() const;
public:
 BitmapGroup2RemainderFourLiveFifoCycle(std::span<std::byte>,SystemPoolBackend&);
 BitmapGroup2RemainderFourLiveFifoCycle(const BitmapGroup2RemainderFourLiveFifoCycle&)=delete;
 BitmapGroup2RemainderFourLiveFifoCycle& operator=(const BitmapGroup2RemainderFourLiveFifoCycle&)=delete;
 BitmapAllocation allocate(std::uint64_t,std::uint32_t);
 std::uint32_t free(std::uint64_t);
};
}
