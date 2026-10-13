#pragma once
#include "ctr/bitmap_group2_remainder_shared_bin.h"
namespace ctr {
// Borrows the same creator and live derived storage for exactly one ten-step cycle after the shared-bin
// cycle (retained peak count5): FIRST256, A129..192, SECOND256, B (B.request==A.request) and C129..192
// split in order from the tail, then freed A, B, C, FIRST, SECOND. A and B share one SMALL bin with B as
// head; C's backward merge unlinks the HEAD node B (bin head moves to A, A.prev to the sentinel).
// Bins are doubly linked lists whose sentinel is the resource base (node+16 next, node+24 prev).
class BitmapGroup2RemainderHeadUnlinkCycle {
 std::span<std::byte> pool_;
 SystemPoolBackend* backend_;
 unsigned stage_{};
 std::array<std::uint64_t,5> request_{},address_{},size_{};
 std::array<bool,5> freed_{};
 std::uint64_t resource_{},peak_request_{},peak_occupied_{},sentinel_prev_{0x1980};
 void validate() const;
public:
 BitmapGroup2RemainderHeadUnlinkCycle(std::span<std::byte>,SystemPoolBackend&);
 BitmapGroup2RemainderHeadUnlinkCycle(const BitmapGroup2RemainderHeadUnlinkCycle&)=delete;
 BitmapGroup2RemainderHeadUnlinkCycle& operator=(const BitmapGroup2RemainderHeadUnlinkCycle&)=delete;
 BitmapAllocation allocate(std::uint64_t,std::uint32_t);
 std::uint32_t free(std::uint64_t);
};
}
