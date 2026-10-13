#pragma once
#include <cstdint>
namespace ctr::data {
struct IgzWordArray {
    std::uint32_t count;
    std::uint32_t padding;
    std::uint64_t capacity_bytes;
    std::uint64_t* data;
};
// Whole 01583710. Allocation growth remains an explicit missing dependency.
void igz_append_words(IgzWordArray& array, const std::uint64_t* begin,
                      const std::uint64_t* end);
}
