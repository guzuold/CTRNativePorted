#include "ctr/data/igz_append_words.h"
#include <array>
#include <bit>
#include <cstddef>
#include <stdexcept>
namespace ctr::data {
void igz_append_words(IgzWordArray& a, const std::uint64_t* begin,
                      const std::uint64_t* end) {
    const auto first = reinterpret_cast<std::uintptr_t>(begin);
    const auto last = reinterpret_cast<std::uintptr_t>(end);
    const auto added = (last - first) >> 3;
    const auto total = a.count + static_cast<std::uint32_t>(added);
    const auto signed_total = std::bit_cast<std::int32_t>(total);
    const auto capacity = a.data ? static_cast<std::uint32_t>(a.capacity_bytes >> 3) : 0u;
    if (signed_total > std::bit_cast<std::int32_t>(capacity)) {
        const auto bytes = total * 8u;
        const auto old_bytes = a.data ? static_cast<std::uint32_t>(a.capacity_bytes) : 0u;
        if (std::bit_cast<std::int32_t>(bytes) > std::bit_cast<std::int32_t>(old_bytes))
            throw std::runtime_error("missing original growth014f4d80");
    }
    if (begin != end) {
        auto* destination = a.data + std::bit_cast<std::int32_t>(a.count);
        const auto dest = reinterpret_cast<std::uintptr_t>(destination);
        const auto length = added * 8;
        std::uint64_t copied = 0;
        if (added > 15 && (dest >= last || dest + length <= first)) {
            const auto blocks = added & 0x3ffffffffffffff0ull;
            for (; copied != blocks; copied += 16) {
                // Original loads four YMM registers before storing the block.
                std::array<std::uint64_t,16> block;
                for (std::size_t i=0;i<16;++i) block[i]=begin[copied+i];
                for (std::size_t i=0;i<16;++i) destination[copied+i]=block[i];
            }
        }
        // Preserve the original ascending copy for overlapping input.
        for (; copied != added; ++copied) destination[copied]=begin[copied];
    }
    a.count = total;
}
}
