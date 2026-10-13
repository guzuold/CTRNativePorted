#include "ctr/data/igz_metadata_append.h"
#include <array>
#include <bit>
#include <cstring>
#include <stdexcept>
namespace ctr::data {
std::byte* igz_metadata_append(IgzMetadataList& list, std::int32_t added,
                               const std::byte* source) {
    const auto old_count = std::bit_cast<std::int32_t>(list.count);
    const auto total = list.count + std::bit_cast<std::uint32_t>(added);
    if (std::bit_cast<std::int32_t>(total) >
        std::bit_cast<std::int32_t>(list.capacity))
        throw std::runtime_error("missing original growth015455b0");
    auto* destination = list.data;
    list.count = total;
    if (added <= 0) return destination;
    destination += static_cast<std::int64_t>(old_count) * 16;
    for (std::int32_t i = 0; i < added; ++i) {
        // Original loads one XMM before each store. Forward overlap can
        // overwrite a subsequent source record; retain that ordering.
        std::array<std::byte, 16> record;
        std::memcpy(record.data(), source + static_cast<std::int64_t>(i) * 16, 16);
        std::memcpy(destination + static_cast<std::int64_t>(i) * 16, record.data(), 16);
    }
    return destination;
}
}
