#pragma once
#include "ctr/bitmap_group2_remainder_head_unlink.h"
namespace ctr {
// Borrows the same creator and live derived storage for exactly one ten-step cycle after the head-unlink
// cycle (retained peak count5): FIRST256, A129..192, SECOND256, C129..192 split in order from the tail,
// A freed into its SMALL bin between live neighbours, then a request equal to A's reuses A exactly from
// that bin (unlink, in-use header, footer, successor prev-free bit cleared), then C, SECOND, reused A and
// FIRST are freed back to 1640. Bins are sentinel-based doubly linked lists as in the original.
class BitmapGroup2RemainderBinReuseCycle {
 std::span<std::byte> pool_;
 SystemPoolBackend* backend_;
 unsigned stage_{};
 std::array<std::uint64_t,4> request_{},address_{},size_{};
 std::array<bool,4> freed_{};
 std::uint64_t resource_{},peak_request_{},peak_occupied_{},sentinel_prev_{0x1980},max_requested_{},max_occupied_{};
 void validate() const;
 void account_allocation(std::uint64_t,std::uint64_t);
public:
 BitmapGroup2RemainderBinReuseCycle(std::span<std::byte>,SystemPoolBackend&);
 BitmapGroup2RemainderBinReuseCycle(const BitmapGroup2RemainderBinReuseCycle&)=delete;
 BitmapGroup2RemainderBinReuseCycle& operator=(const BitmapGroup2RemainderBinReuseCycle&)=delete;
 BitmapAllocation allocate(std::uint64_t,std::uint32_t);
 std::uint32_t free(std::uint64_t);
};
}
