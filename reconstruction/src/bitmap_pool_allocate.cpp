#include "ctr/bitmap_pool_allocate.h"
#include "ctr/detail/bitmap_pool_allocate_impl.h"
#include <algorithm>
#include <cstring>
#include <stdexcept>
namespace ctr {
BitmapAllocation detail::allocate_bitmap_pool_bounded(std::span<std::byte> s,SystemPoolBackend& b,std::uint64_t request,std::uint64_t original_request,std::uint32_t alignment,bool requested_footer){
 if(s.size()!=192||!request||request>128||alignment!=8)throw std::invalid_argument("Concrete bitmap pool positive1..128/align8 contract required");
 auto get=[&](std::size_t off){std::uint64_t v;std::memcpy(&v,s.data()+off,8);return v;};
 auto put=[&](std::size_t off,std::uint64_t v){std::memcpy(s.data()+off,&v,8);};
 const auto bytes=b.memory.bytes();const auto base=static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(bytes.data()));
 const auto address=static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(s.data()));std::uint32_t type;std::memcpy(&type,s.data()+0x1c,4);
 if(address<base||address-base>bytes.size()||s.size()>bytes.size()-(address-base)||get(0)!=0x025baa70||get(0x10)||type!=1||s[0x18]!=std::byte{1}||s[0x4b]!=std::byte{0})
  throw std::invalid_argument("Actual activated derived type1 debug0 required");
 const auto header=get(0xa8);
 if(header!=get(0x20)||get(0x28)!=8192||header<base||header-base>bytes.size()||8192>bytes.size()-(header-base))throw std::invalid_argument("Actual8192 bitmap resource required");
 const auto allocation=allocate_bitmap_singleton(bytes.subspan(static_cast<std::size_t>(header-base),8192),request,alignment);
 std::uint64_t marker;std::memcpy(&marker,bytes.data()+allocation.address-base-8,8);const auto charged=(marker+8)&~std::uint64_t{3};
 if(requested_footer){const auto footer=(allocation.address+(marker&~std::uint64_t{3})-8)&~std::uint64_t{7};std::memcpy(bytes.data()+footer-base,&original_request,8);}
 const auto occupied=get(0x70)+charged;put(0x70,occupied);put(0x78,std::max(get(0x78),occupied));
 const auto requested=get(0x60)+original_request;put(0x60,requested);put(0x68,std::max(get(0x68),requested));
 const auto count=get(0x50)+1;put(0x50,count);put(0x58,std::max(get(0x58),count));return {allocation.address,allocation.payload.first(static_cast<std::size_t>(original_request))};
}
BitmapAllocation allocate_derived_bitmap_pool(std::span<std::byte> s,SystemPoolBackend& b,std::uint64_t request,std::uint32_t alignment){return detail::allocate_bitmap_pool_bounded(s,b,request,request,alignment,false);}
}
