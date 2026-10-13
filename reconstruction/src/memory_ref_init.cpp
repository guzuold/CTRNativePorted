#include "ctr/memory_ref_init.h"
#include <cstring>
#include <stdexcept>
namespace ctr {
std::uint16_t initialize_zero_memory_refs(std::span<std::byte> storage,
    std::uint16_t offset, bool array, std::uint16_t element_count) {
    const auto size=static_cast<std::uint16_t>(array ? static_cast<std::uint32_t>(element_count)<<4 : 16u);
    if (offset>storage.size() || size>storage.size()-offset)
        throw std::invalid_argument("Reflected memory field outside fresh storage");
    if (size) std::memset(storage.data()+offset,0,size);
    return size;
}
}
