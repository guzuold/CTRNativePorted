#include "ctr/bitmap_normal_allocate.h"
#include "ctr/detail/bitmap_pool_allocate_impl.h"
#include <cstring>
#include <stdexcept>
namespace ctr {
BitmapAllocation allocate_normal_derived_bitmap_pool(std::span<std::byte> s,SystemPoolBackend& b,std::uint64_t request,std::uint32_t alignment){
 if(!request||request>112||alignment!=8)throw std::invalid_argument("Normal bitmap request1..112/align8 required");
 return detail::allocate_bitmap_pool_bounded(s,b,(request+15)&~std::uint64_t{7},request,alignment,true);
}
std::uint64_t bitmap_normal_requested_size(std::span<const std::byte> s,std::uint64_t pointer){
 const auto base=static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(s.data()));
 if(s.size()!=8192||pointer<base+0x1990||pointer-base>s.size()-8||pointer&7)throw std::invalid_argument("Actual positive normal bitmap pointer required");
 std::uint64_t marker;std::memcpy(&marker,s.data()+pointer-base-8,8);const auto size=marker&~std::uint64_t{3};
 if(marker&1||size<24||size>s.size()-(pointer-base))throw std::invalid_argument("Bounded allocated bitmap marker required");
 const auto footer=(pointer+size-8)&~std::uint64_t{7};std::uint64_t requested;std::memcpy(&requested,s.data()+footer-base,8);
 if(!requested||requested>112)throw std::invalid_argument("Real normal requested-size footer required");return requested;
}
}
