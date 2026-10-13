#include "ctr/mspace_layout.h"
#include <cstring>
#include <limits>
#include <stdexcept>
namespace ctr {
std::uint32_t initialize_mspace_layout(std::span<std::byte> backing,const MspaceLayoutParameters& p) {
    if (p.base_token>std::numeric_limits<std::uint64_t>::max()-0x600)
        throw std::invalid_argument("Mspace base address overflow");
    const auto padding=static_cast<std::uint32_t>((0u-(p.base_token+16u))&31u);
    const auto header=padding+16u;
    if (backing.size()<header+0x520u) throw std::out_of_range("Mspace header exceeds backing");
    auto put=[&](std::size_t offset,std::uint64_t value) {std::memcpy(backing.data()+offset,&value,8);};
    std::memset(backing.data()+header,0,0x520);
    put(padding+8,0x523);put(padding+0x28,p.base_token);put(padding+0x390,p.base_token);
    for (const auto offset:{0x368,0x360,0x398,0x408}) put(padding+offset,p.capacity);
    if (p.owns_backing) backing[header+0x398]|=std::byte{1};
    put(header+0x360,0x5a0);put(header+0x368,0x5a0);
    put(header+0x38,p.poison?0x58585858:0);
    const std::uint32_t flags=p.debug_flags?7u:0u;
    std::memcpy(backing.data()+header+0x370,&flags,4);
    put(header+0x3a0,static_cast<std::uint64_t>(p.page_scale)<<14);
    put(header+0x3a8,static_cast<std::uint64_t>(p.granularity_scale)<<16);
    return header;
}
}
