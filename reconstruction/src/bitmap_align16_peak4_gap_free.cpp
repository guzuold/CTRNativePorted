#include "ctr/bitmap_align16_peak4_gap_free.h"
#include <algorithm>
#include <bit>
#include <cstring>
#include <stdexcept>
namespace ctr {
namespace {
template<class T>T read(std::span<std::byte> s,std::size_t n){T v;std::memcpy(&v,s.data()+n,sizeof(v));return v;}
template<class T>void write(std::span<std::byte> s,std::size_t n,T v){std::memcpy(s.data()+n,&v,sizeof(v));}
auto bin(std::uint64_t n){auto m=31u-static_cast<unsigned>(std::countl_zero(static_cast<std::uint32_t>(n)));return std::pair{m-7,static_cast<unsigned>((n>>(m-5))^32)};}
}
std::uint32_t free_normal_bitmap_align16_peak4_gap(std::span<std::byte> p,SystemPoolBackend& b,std::uint64_t pointer){
 if(p.size()!=192)throw std::invalid_argument("Bounded leading-gap request required");
 auto bytes=b.memory.bytes();const auto base=reinterpret_cast<std::uintptr_t>(bytes.data()),object=reinterpret_cast<std::uintptr_t>(p.data());
 if(object<base||object-base>bytes.size()||192>bytes.size()-(object-base)||read<std::uint64_t>(p,0)!=0x025baa70||read<std::uint64_t>(p,0x10)||read<std::uint32_t>(p,0x1c)!=1||p[0x18]!=std::byte{1}||p[0x4b]!=std::byte{0})throw std::invalid_argument("Activated owned derived required");
 const auto h=read<std::uint64_t>(p,0xa8);
 if((h&15)||h!=read<std::uint64_t>(p,0x20)||read<std::uint64_t>(p,0x28)!=8192||h<base||h-base>bytes.size()||8192>bytes.size()-(h-base))throw std::invalid_argument("Actual naturally aligned resource required");
 auto s=bytes.subspan(static_cast<std::size_t>(h-base),8192);const auto first=read<std::uint64_t>(s,0x1988);
 if(first<144||first>192||(first&15))throw std::invalid_argument("Actual declared allocated prefix required");
 const auto split=0x1980+static_cast<std::size_t>(first)+8,active=split+40;
 const auto marker=read<std::uint64_t>(s,active+8),allocated=marker&~std::uint64_t{3};
 if(allocated<24||allocated>40||(allocated&7)||marker!=(allocated|2)||pointer!=h+active+16||(pointer&15)||read<std::uint64_t>(s,split+8)!=33||read<std::uint64_t>(s,active)!=h+split)throw std::invalid_argument("Observed owned aligned small block required");
 const auto suffix=active+static_cast<std::size_t>(allocated)+8,suffixmarker=read<std::uint64_t>(s,suffix+8),suffixsize=suffixmarker&~std::uint64_t{3},request=read<std::uint64_t>(s,suffix);
 if(suffixsize<40||suffixsize>168||(suffixsize&7)||suffixmarker!=(suffixsize|1)||request<8||request>32||std::max(std::uint64_t{24},(request+15)&~std::uint64_t{7})!=allocated)throw std::invalid_argument("Actual requested footer and small suffix required");
 const auto size=40+allocated+8+suffixsize,original=first+size+8,next=split+static_cast<std::size_t>(size)+8,tail=next+272,remain=1640-original-280,prior=read<std::uint64_t>(s,split);
 if(size<128||size>240||original<328||original>392||prior<129||prior>177||((prior-129)&15)||((prior+15)&~std::uint64_t{7})!=first||read<std::uint64_t>(s,next)!=h+suffix||read<std::uint64_t>(s,next+8)!=266||read<std::uint64_t>(s,next+272)!=256||read<std::uint64_t>(s,tail+8)!=(remain|1)||read<std::uint64_t>(p,0x50)!=3||read<std::uint64_t>(p,0x58)!=4||read<std::uint64_t>(p,0x60)!=256+prior+request||read<std::uint64_t>(p,0x70)!=0x1998+first+280+allocated+8)throw std::invalid_argument("Actual bounded source layout and statistics required");
 const auto suffixindex=static_cast<unsigned>(suffixsize/8),small=static_cast<unsigned>(size/8);const auto [tg,ti]=bin(remain);const auto suffixhead=0x88+suffixindex*8,thead=0x88+tg*256+ti*8;
 if((tg!=2&&tg!=3)||read<std::uint32_t>(s,0x20)!=((1u<<tg)|1u)||read<std::uint32_t>(s,0x24)!=((1u<<4)|(1u<<suffixindex))||read<std::uint32_t>(s,0x24+tg*4)!=(1u<<ti)||read<std::uint64_t>(s,0xa8)!=h+split||read<std::uint64_t>(s,suffixhead)!=h+suffix||read<std::uint64_t>(s,thead)!=h+tail||read<std::uint64_t>(s,0x18)!=h+split||read<std::uint64_t>(s,0x10)!=h)throw std::invalid_argument("Actual two small singletons and large tail required");
 for(unsigned j=1;j<25;++j)if(j!=tg&&read<std::uint32_t>(s,0x24+j*4))throw std::invalid_argument("Other groups unsupported");
 for(std::size_t j=0x88;j<0x1988;j+=8)if(j!=0xa8&&j!=suffixhead&&j!=thead&&read<std::uint64_t>(s,j)!=h)throw std::invalid_argument("Other heads unsupported");
 for(auto j:{split+16,split+24,suffix+16,suffix+24,tail+16,tail+24})if(read<std::uint64_t>(s,j)!=h)throw std::invalid_argument("Actual singleton links required");
 if(read<std::uint64_t>(s,0x1ff0)!=h+tail||(read<std::uint64_t>(s,0x1ff8)&3)!=2)throw std::invalid_argument("Actual terminal guard required");
 // Original free marks the active chunk and suffix before both small unlinks.
 write(s,active+8,marker|1);write(s,suffix,h+split);write(s,suffix+8,suffixmarker|2);
 write(s,0xa8,h);write(s,suffixhead,h);write(s,split+8,size|1);write(s,next,h+split);
 write(s,split+16,h);write(s,split+24,h);write(s,0x18,h+split);write(s,0x88+small*8,h+split);write(s,0x24,1u<<small);
 write(p,0x70,read<std::uint64_t>(p,0x70)-allocated-8);write(p,0x60,read<std::uint64_t>(p,0x60)-request);write(p,0x50,std::uint64_t{2});return 0;
}
}
