#pragma once
#include "ctr/bitmap_group2_remainder_four_live_fifo.h"
namespace ctr {
// Borrows the same creator and live derived storage for exactly one eight-step cycle after the
// four-live FIFO cycle (retained peak count4): four in-order tail splits, then frees in the declared
// order FIRST, A, SECOND, B. A (NORMAL129..192, chunk144..200) is freed between two live neighbours
// and enters a SMALL group0 bin (index size/8); SECOND then merges backward and forward.
class BitmapGroup2RemainderSmallBinCycle {
 std::span<std::byte> pool_;
 SystemPoolBackend* backend_;
 unsigned stage_{};
 std::array<std::uint64_t,4> request_{},address_{},size_{};
 std::array<bool,4> freed_{};
 std::uint64_t resource_{},peak_request_{},peak_occupied_{},last_binned_{0x1980};
 void validate() const;
public:
 BitmapGroup2RemainderSmallBinCycle(std::span<std::byte>,SystemPoolBackend&);
 BitmapGroup2RemainderSmallBinCycle(const BitmapGroup2RemainderSmallBinCycle&)=delete;
 BitmapGroup2RemainderSmallBinCycle& operator=(const BitmapGroup2RemainderSmallBinCycle&)=delete;
 BitmapAllocation allocate(std::uint64_t,std::uint32_t);
 std::uint32_t free(std::uint64_t);
};
}
