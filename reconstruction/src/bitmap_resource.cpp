#include "ctr/bitmap_resource.h"
#include "ctr/bitmap_resource_header.h"
#include <bit>
#include <cstring>
#include <stdexcept>
namespace ctr {
namespace {
template<class T> T load(std::span<std::byte> s,std::size_t offset){T v;std::memcpy(&v,s.data()+offset,sizeof(v));return v;}
template<class T> void store(std::span<std::byte> s,std::size_t offset,T value){std::memcpy(s.data()+offset,&value,sizeof(value));}
std::uint64_t validate(std::span<std::byte> s){
 if(reinterpret_cast<std::uintptr_t>(s.data())&7)throw std::invalid_argument("Bitmap resource alignment missing");
 if(s.size()<0x1988+16+24||s.size()>16384)throw std::invalid_argument("Bitmap resource extent outside bounded contract");
 return static_cast<std::uint64_t>((s.size()-0x1988-16)&~std::size_t{7});
}
}
std::uint64_t initialize_bitmap_resource_chunk(std::span<std::byte> s){
 const auto size=validate(s);const auto base=static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(s.data()));
 // This API only accepts the verified fresh header: no prior list ownership.
 for(std::size_t i=0x20;i<0x88;++i)if(s[i]!=std::byte{0})throw std::invalid_argument("Bitmap header maps not fresh");
 if(load<std::uint64_t>(s,0x10)!=base||load<std::uint64_t>(s,0x18)!=base)throw std::invalid_argument("Bitmap header sentinel not initialized");
 for(std::size_t i=0x88;i<0x1988;i+=8)if(load<std::uint64_t>(s,i)!=base)throw std::invalid_argument("Bitmap head not initialized");
 std::uint32_t first=0,second=static_cast<std::uint32_t>(size>>3);
 if(size>255){const auto msb=31u-static_cast<std::uint32_t>(std::countl_zero(static_cast<std::uint32_t>(size)));first=msb-7;second=static_cast<std::uint32_t>((size>>(msb-5))^0x20);}
 const auto chunk=base+0x1980;const auto head=0x88+static_cast<std::size_t>(first)*256+static_cast<std::size_t>(second)*8;
 store(s,0x1988,size|1);store(s,0x1990,base);store(s,0x1998,base);store(s,0x18,chunk);store(s,head,chunk);
 store(s,0x20,load<std::uint32_t>(s,0x20)|(std::uint32_t{1}<<first));
 const auto bit=std::uint32_t{1}<<second;store(s,0x24+static_cast<std::size_t>(first)*4,load<std::uint32_t>(s,0x24+static_cast<std::size_t>(first)*4)|bit);
 store(s,0x1988+static_cast<std::size_t>(size),chunk);store(s,0x1990+static_cast<std::size_t>(size),std::uint64_t{2});
 return bit; // EAX from original final bitmap OR, not a chunk pointer.
}
std::uint64_t create_bitmap_resource(std::span<std::byte> s){
 (void)validate(s);const auto result=initialize_bitmap_resource_header(s);(void)initialize_bitmap_resource_chunk(s);return result;
}
}
