#include "ctr/mspace_bin_select.h"
#include <bit>
#include <stdexcept>
namespace ctr {
LargerSmallBin select_larger_small_bin(std::uint32_t bitmap,std::uint32_t size) {
    if (size<32 || size>224 || (size&31))
        throw std::invalid_argument("Invalid required small chunk size");
    const auto index=size>>3;
    if ((bitmap>>index)&3)
        throw std::invalid_argument("Exact/adjacent small-bin branch must precede larger selection");
    const auto candidates=bitmap&(~0u<<(index+1));
    if (!candidates) throw std::invalid_argument("No larger small bin; dv/tree/top fallback not reconstructed here");
    const auto selected=static_cast<std::uint32_t>(std::countr_zero(candidates));
    return {selected,selected*8,0x40+selected*16,0x50+selected*16};
}
}
