#pragma once
#include <cstdint>
#include <span>
namespace ctr::data {
struct IgzPublishedName {std::uint64_t name;std::uint32_t hash,padding;};
struct IgzDependencyPublisher {
    std::span<const std::uint64_t> keys;
    std::uint8_t first_initialized,second_initialized;
    std::span<IgzPublishedName> first,second;
    // Optional original40B headers, required when initializing the lists.
    std::span<std::byte> first_header{},second_header{};
};
// Whole01580770. Existing names needing release and growth are missing services.
void igz_dependency_publish(IgzDependencyPublisher&,std::uint64_t key,
                            const IgzPublishedName& first,const IgzPublishedName& second);
}
