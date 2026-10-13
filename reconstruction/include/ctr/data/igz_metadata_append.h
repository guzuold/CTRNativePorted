#pragma once
#include <cstddef>
#include <cstdint>
namespace ctr::data {
// Whole01580a90, pending differential verification. Original header fields
// count+c, capacity+10, data+20; growth015455b0 remains explicitly missing.
struct IgzMetadataList {
    std::uint32_t count;
    std::uint32_t capacity;
    std::byte* data;
};
std::byte* igz_metadata_append(IgzMetadataList& list, std::int32_t added,
                               const std::byte* source);
}
