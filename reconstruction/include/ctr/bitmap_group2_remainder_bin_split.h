#pragma once
#include "ctr/bitmap_group2_remainder_bin_reuse.h"
#include <vector>
namespace ctr {
// Borrows the same creator and live derived storage for exactly one ten-step cycle after the bin-reuse
// cycle (retained peak count5): FIRST256, A129..192, SECOND256, C129..192 split in order from the tail;
// SECOND freed into its group1 bin between live A and C; a NORMAL129..192 request D is then served by
// SPLITTING that non-tail chunk (D at SECOND's address, remainder 56..112 into a SMALL bin); then D, C,
// A and FIRST are freed back to 1640. The class keeps an explicit physical chunk model updated by every
// split and merge and validates memory against it.
class BitmapGroup2RemainderBinSplitCycle {
public:
 struct ModelChunk{std::size_t offset;std::uint64_t size;bool free;int block;};
private:
 std::span<std::byte> pool_;
 SystemPoolBackend* backend_;
 unsigned stage_{};
 std::array<std::uint64_t,5> request_{},address_{};
 std::vector<ModelChunk> model_;
 std::uint64_t resource_{},peak_request_{},peak_occupied_{},sentinel_prev_{0x1980},max_requested_{},max_occupied_{};
 void validate() const;
 void account_allocation(std::uint64_t,std::uint64_t);
 BitmapAllocation split(std::size_t index,std::uint64_t request,unsigned block);
public:
 BitmapGroup2RemainderBinSplitCycle(std::span<std::byte>,SystemPoolBackend&);
 BitmapGroup2RemainderBinSplitCycle(const BitmapGroup2RemainderBinSplitCycle&)=delete;
 BitmapGroup2RemainderBinSplitCycle& operator=(const BitmapGroup2RemainderBinSplitCycle&)=delete;
 BitmapAllocation allocate(std::uint64_t,std::uint32_t);
 std::uint32_t free(std::uint64_t);
};
}
