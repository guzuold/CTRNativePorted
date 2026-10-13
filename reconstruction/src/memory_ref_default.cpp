#include "ctr/memory_ref_default.h"
#include <algorithm>
#include <bit>
#include <cstring>
#include <stdexcept>
namespace ctr {
void initialize_memory_ref_default(std::span<std::byte> storage,
    std::uint16_t offset, std::int32_t alignment,
    const std::array<std::optional<std::int32_t>,4>& pool_indices) {
    if (offset>storage.size() || storage.size()-offset<16 || storage.size()<12)
        throw std::out_of_range("Memory reference default outside storage");
    std::uint32_t ownership;std::uint64_t header,pointer;
    std::memcpy(&ownership,storage.data()+8,4);
    std::memcpy(&header,storage.data()+offset,8);
    std::memcpy(&pointer,storage.data()+offset+8,8);
    const auto slot=(ownership>>22)&0x3ffu;
    const auto& pool=pool_indices[slot==1?0:slot==2?2:slot==3?3:1];
    if (pointer==0) {
        const std::uint64_t token=pool ? static_cast<std::uint64_t>(static_cast<std::int64_t>(*pool)+1) : 0;
        header=(header&0xfff0000000000000ull)|(token&0x000fffffffffffffull);
    }
    if (alignment!=-1) {
        const auto value=std::max(4u,static_cast<std::uint32_t>(alignment));
        const auto code=(std::bit_width(value)-3u)&15u;
        header=(header&0xff0fffffffffffffull)|(static_cast<std::uint64_t>(code)<<52);
    }
    std::memcpy(storage.data()+offset,&header,8);
}
}
