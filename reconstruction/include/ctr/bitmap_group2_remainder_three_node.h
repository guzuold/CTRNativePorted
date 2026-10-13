#pragma once
#include "ctr/bitmap_group2_remainder_bin_choice.h"
namespace ctr {
// Borrows the same creator and live derived storage for exactly one fourteen-step cycle after the bin-choice
// cycle (retained peak count5): seven NORMAL129..160 blocks A, S0, S1, B, S2, C, S3 (B and C equal to A)
// split in order from the tail (live count7), then A, B, C freed into one SMALL bin as the three-node list
// C->B->A, S1 freed so its forward merge unlinks the MIDDLE node B, then S0, S2, S3 back to 1640.
class BitmapGroup2RemainderThreeNodeCycle {
public:
 struct ModelChunk{std::size_t offset;std::uint64_t size;bool free;int block;};
private:
 std::span<std::byte> pool_;
 SystemPoolBackend* backend_;
 unsigned stage_{},live_{},max_live_{};
 std::array<std::uint64_t,7> request_{},address_{};
 std::vector<ModelChunk> model_;
 std::uint64_t resource_{},peak_request_{},peak_occupied_{},sentinel_prev_{0x1980},max_requested_{},max_occupied_{};
 void validate() const;
 void account_allocation(std::uint64_t,std::uint64_t);
 BitmapAllocation split(std::size_t index,std::uint64_t request,unsigned block);
public:
 BitmapGroup2RemainderThreeNodeCycle(std::span<std::byte>,SystemPoolBackend&);
 BitmapGroup2RemainderThreeNodeCycle(const BitmapGroup2RemainderThreeNodeCycle&)=delete;
 BitmapGroup2RemainderThreeNodeCycle& operator=(const BitmapGroup2RemainderThreeNodeCycle&)=delete;
 BitmapAllocation allocate(std::uint64_t,std::uint32_t);
 std::uint32_t free(std::uint64_t);
};
}
