#pragma once
#include <cstdint>
namespace ctr::data {
struct IgzDependencyRecord {
    std::uint64_t first_name;
    std::uint32_t first_hash, first_padding;
    std::uint64_t second_name;
    std::uint32_t second_hash, second_padding;
    std::uint64_t value;
};
struct IgzDependencyArray {
    std::uint32_t count, padding;
    std::uint64_t packed_capacity;
    IgzDependencyRecord* data;
};
// Whole015835b0. Missing original014f4d80 is reported before any copy.
void igz_append_dependencies(IgzDependencyArray&, const IgzDependencyRecord* begin,
                             const IgzDependencyRecord* end);
}
