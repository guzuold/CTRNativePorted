#pragma once
#include "ctr/bitmap_group2_remainder_bin_choice.h"
#include <vector>
namespace ctr {
// General native NORMAL (129..1024, alignment 8) allocator over an activated derived bitmap resource
// (original 014db6b0 allocation, 014db250 getter, 014db270 free -> 015c3a20). Unlike the per-history
// classes it keeps no captured history: every call first validates the actual memory structurally
// (chunk chain, prev-free bits/pointers, footers, sentinel-based bin lists, maps, guard, statistics),
// then applies the original rules:
//   allocate: round the chunk size up to its bin width, take the first non-empty bin at or above that
//             index in its group, else the lowest bin of the next non-empty group; split when at least
//             32 bytes remain (remainder binned), otherwise hand the chunk out whole.
//   free:     merge with a free predecessor/successor, original transient writes, head insertion.
// Shapes never observed in the original are rejected before any write: alignment other than 8,
// allocation from a bin holding more than one chunk, no fitting chunk (the original falls back to the
// owner pool), whole handout of the last chunk, an allocated last chunk.
class BitmapNormalAllocator {
public:
 struct Chunk{std::size_t offset;std::uint64_t size;bool free;};
 BitmapNormalAllocator(std::span<std::byte> derived_pool,SystemPoolBackend& backend);
 BitmapNormalAllocator(const BitmapNormalAllocator&)=delete;
 BitmapNormalAllocator& operator=(const BitmapNormalAllocator&)=delete;
 BitmapAllocation allocate(std::uint64_t request,std::uint32_t alignment);
 std::uint32_t free(std::uint64_t pointer);
 std::uint64_t requested_size(std::uint64_t pointer) const;
 std::vector<Chunk> scan() const;
private:
 std::span<std::byte> pool_;
 SystemPoolBackend* backend_;
 std::uint64_t resource_{};
 std::span<std::byte> resource() const;
};
}
