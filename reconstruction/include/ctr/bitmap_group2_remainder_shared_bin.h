#pragma once
#include "ctr/bitmap_group2_remainder_small_bin.h"
namespace ctr {
// Borrows the same creator and live derived storage for exactly one ten-step cycle after the small-bin
// cycle (retained peak count4): FIRST256, A129..192, SECOND256, B (B.request==A.request) and C129..192
// split in order from the tail (live count5), then freed A, B, FIRST, SECOND, C. A and B share one SMALL
// bin as a two-node list (head insertion); FIRST's forward merge unlinks the non-head node A.
// Bins are doubly linked lists whose sentinel is the resource base: node+16 next, node+24 prev, with the
// sentinel's own next/prev at resource+0x10/+0x18 as in the original.
class BitmapGroup2RemainderSharedBinCycle {
 std::span<std::byte> pool_;
 SystemPoolBackend* backend_;
 unsigned stage_{};
 std::array<std::uint64_t,5> request_{},address_{},size_{};
 std::array<bool,5> freed_{};
 std::uint64_t resource_{},peak_request_{},peak_occupied_{},sentinel_prev_{0x1980};
 void validate() const;
public:
 BitmapGroup2RemainderSharedBinCycle(std::span<std::byte>,SystemPoolBackend&);
 BitmapGroup2RemainderSharedBinCycle(const BitmapGroup2RemainderSharedBinCycle&)=delete;
 BitmapGroup2RemainderSharedBinCycle& operator=(const BitmapGroup2RemainderSharedBinCycle&)=delete;
 BitmapAllocation allocate(std::uint64_t,std::uint32_t);
 std::uint32_t free(std::uint64_t);
};
}
