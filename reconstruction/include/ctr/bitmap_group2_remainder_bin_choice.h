#pragma once
#include "ctr/bitmap_group2_remainder_bin_split.h"
namespace ctr {
// Original 014db250 NORMAL getter for a chunk handed out whole: the footer holds the request while the
// chunk may exceed the rounded request by under 32 bytes (no split). The strict getter stays unchanged.
std::uint64_t bitmap_requested_size_large_slack(std::span<const std::byte> resource,std::uint64_t pointer);
// Borrows the same creator and live derived storage for exactly one twelve-step cycle after the bin-split
// cycle (retained peak count5): FIRST256, A129..192, X129..192, SECOND256, C129..192 split in order from the
// tail; A (SMALL bin) and SECOND (group1 chunk264) freed apart; D129..192 then takes the free chunk in the
// smallest fitting bin (A when it fits, else SECOND's chunk, never the tail here), split when at least 32
// bytes remain, otherwise handed out whole; then D, C, X and FIRST are freed back to 1640.
class BitmapGroup2RemainderBinChoiceCycle {
public:
 struct ModelChunk{std::size_t offset;std::uint64_t size;bool free;int block;};
private:
 std::span<std::byte> pool_;
 SystemPoolBackend* backend_;
 unsigned stage_{};
 std::array<std::uint64_t,6> request_{},address_{};
 std::vector<ModelChunk> model_;
 std::uint64_t resource_{},peak_request_{},peak_occupied_{},sentinel_prev_{0x1980},max_requested_{},max_occupied_{};
 void validate() const;
 void account_allocation(std::uint64_t,std::uint64_t);
 BitmapAllocation split(std::size_t index,std::uint64_t request,unsigned block);
 BitmapAllocation whole(std::size_t index,std::uint64_t request,unsigned block);
public:
 BitmapGroup2RemainderBinChoiceCycle(std::span<std::byte>,SystemPoolBackend&);
 BitmapGroup2RemainderBinChoiceCycle(const BitmapGroup2RemainderBinChoiceCycle&)=delete;
 BitmapGroup2RemainderBinChoiceCycle& operator=(const BitmapGroup2RemainderBinChoiceCycle&)=delete;
 BitmapAllocation allocate(std::uint64_t,std::uint32_t);
 std::uint32_t free(std::uint64_t);
};
}
