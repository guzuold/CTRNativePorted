#include "ctr/bitmap_allocate.h"
#include <algorithm>
#include <bit>
#include <cstring>
#include <stdexcept>
namespace ctr {
namespace {
template<class T> T get(std::span<std::byte> s,std::size_t off){T v;std::memcpy(&v,s.data()+off,sizeof(v));return v;}
template<class T> void put(std::span<std::byte> s,std::size_t off,T value){std::memcpy(s.data()+off,&value,sizeof(value));}
std::pair<std::uint32_t,std::uint32_t> index(std::uint64_t size){const auto msb=31u-static_cast<std::uint32_t>(std::countl_zero(static_cast<std::uint32_t>(size)));return {msb-7,static_cast<std::uint32_t>((size>>(msb-5))^0x20)};}
}
BitmapAllocation allocate_bitmap_singleton(std::span<std::byte> s,std::uint64_t request,std::uint32_t alignment){
 if(s.size()!=8192||reinterpret_cast<std::uintptr_t>(s.data())&7||!request||request>128||alignment!=8)
  throw std::invalid_argument("Bitmap singleton positive1..128/align8/8192 contract required");
 const auto base=static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(s.data()));const auto map=get<std::uint32_t>(s,0x20);
 if(!map||std::popcount(map)!=1)throw std::invalid_argument("Exactly one bitmap group required");
 const auto first=static_cast<std::uint32_t>(std::countr_zero(map));
 if(first!=3)throw std::invalid_argument("Verified large group3 required");
 const auto secondmap=get<std::uint32_t>(s,0x24+first*4);
 if(!secondmap||std::popcount(secondmap)!=1)throw std::invalid_argument("Exactly one free bin required");
 for(std::size_t i=0;i<25;++i)if(i!=first&&get<std::uint32_t>(s,0x24+i*4))throw std::invalid_argument("Other free groups unsupported");
 const auto second=static_cast<std::uint32_t>(std::countr_zero(secondmap));const auto head=0x88+static_cast<std::size_t>(first)*256+second*8;
 const auto token=get<std::uint64_t>(s,head);
 if(token<base+0x1980||token-base>s.size()-32||(token&7))throw std::invalid_argument("Bitmap free chunk outside resource");
 const auto chunk=static_cast<std::size_t>(token-base);const auto marker=get<std::uint64_t>(s,chunk+8),size=marker&~std::uint64_t{3};
 const auto normalized=std::max(std::uint64_t{24},(request+7)&~std::uint64_t{7});
 if((marker&3)!=1||size>s.size()-chunk-24||size<normalized+32||size-normalized-8<=255||get<std::uint64_t>(s,chunk+16)!=base||get<std::uint64_t>(s,chunk+24)!=base||index(size)!=std::pair{first,second})
  throw std::invalid_argument("Consistent singleton large free chunk and split required");
 for(std::size_t off=0x88;off<0x1988;off+=8)if(off!=head&&get<std::uint64_t>(s,off)!=base)throw std::invalid_argument("Other free heads unsupported");
 const auto remaining=size-normalized-8;const auto next=chunk+static_cast<std::size_t>(normalized)+8;const auto [newfirst,newsecond]=index(remaining);
 const auto newhead=0x88+static_cast<std::size_t>(newfirst)*256+newsecond*8;
 // Unlink singleton. This internal bitmap writes the head back to sentinel.
 put(s,0x18,base);put(s,0x10,base);put(s,head,base);put(s,0x20,std::uint32_t{0});put(s,0x24+first*4,std::uint32_t{0});
 put(s,chunk+8,normalized);put(s,next,token);put(s,next+8,remaining|1);
 put(s,chunk+static_cast<std::size_t>(size)+8,base+next);s[chunk+static_cast<std::size_t>(size)+16]|=std::byte{2};
 put(s,next+16,base);put(s,next+24,base);put(s,0x18,base+next);put(s,newhead,base+next);
 put(s,0x20,std::uint32_t{1}<<newfirst);put(s,0x24+newfirst*4,std::uint32_t{1}<<newsecond);
 return {token+16,s.subspan(chunk+16,static_cast<std::size_t>(request))};
}
}
