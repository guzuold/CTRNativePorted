#include "ctr/bitmap_align16_suffix_consume.h"
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
BitmapAllocation allocate_normal_bitmap_align16_suffix(std::span<std::byte> p,SystemPoolBackend& b,std::uint64_t requested,std::uint32_t alignment){
 if(requested<32||requested>160||alignment!=8)throw std::invalid_argument("Exact suffix request/alignment8 required");
 if(p.size()!=192)throw std::invalid_argument("Bounded leading-gap request required");
 auto bytes=b.memory.bytes();const auto base=reinterpret_cast<std::uintptr_t>(bytes.data()),object=reinterpret_cast<std::uintptr_t>(p.data());
 if(object<base||object-base>bytes.size()||192>bytes.size()-(object-base)||read<std::uint64_t>(p,0)!=0x025baa70||read<std::uint64_t>(p,0x10)||read<std::uint32_t>(p,0x1c)!=1||p[0x18]!=std::byte{1}||p[0x4b]!=std::byte{0})throw std::invalid_argument("Activated owned derived required");
 const auto h=read<std::uint64_t>(p,0xa8);
 if((h&15)||h!=read<std::uint64_t>(p,0x20)||read<std::uint64_t>(p,0x28)!=8192||h<base||h-base>bytes.size()||8192>bytes.size()-(h-base))throw std::invalid_argument("Actual naturally aligned resource required");
 auto s=bytes.subspan(static_cast<std::size_t>(h-base),8192);const auto first=read<std::uint64_t>(s,0x1988);
 if(first<144||first>192||(first&15))throw std::invalid_argument("Actual declared allocated prefix required");
 const auto split=0x1980+static_cast<std::size_t>(first)+8,active=split+40;
 const auto marker=read<std::uint64_t>(s,active+8),allocated=marker&~std::uint64_t{3};
 if(allocated<24||allocated>40||(allocated&7)||marker!=allocated||((h+active+16)&15)||read<std::uint64_t>(s,split+8)!=32||read<std::uint64_t>(s,active)!=24)throw std::invalid_argument("Observed owned aligned small block required");
 const auto suffix=active+static_cast<std::size_t>(allocated)+8,suffixmarker=read<std::uint64_t>(s,suffix+8),suffixsize=suffixmarker&~std::uint64_t{3},request=read<std::uint64_t>(s,suffix);
 if(suffixsize<40||suffixsize>168||(suffixsize&7)||suffixmarker!=(suffixsize|1)||request<8||request>32||std::max(std::uint64_t{24},(request+15)&~std::uint64_t{7})!=allocated)throw std::invalid_argument("Actual requested footer and small suffix required");
 const auto size=40+allocated+8+suffixsize,original=first+size+8,next=split+static_cast<std::size_t>(size)+8,tail=next+272,remain=1640-original-280,prior=read<std::uint64_t>(s,split);
 if(size<128||size>240||original<328||original>392||prior<129||prior>177||((prior-129)&15)||((prior+15)&~std::uint64_t{7})!=first||read<std::uint64_t>(s,next)!=h+suffix||read<std::uint64_t>(s,next+8)!=266||read<std::uint64_t>(s,next+272)!=256||read<std::uint64_t>(s,tail+8)!=(remain|1)||read<std::uint64_t>(p,0x50)!=4||read<std::uint64_t>(p,0x58)!=4||read<std::uint64_t>(p,0x60)!=256+prior+request+24||read<std::uint64_t>(p,0x70)!=0x1998+first+280+allocated+48)throw std::invalid_argument("Actual bounded source layout and statistics required");
 const auto suffixindex=static_cast<unsigned>(suffixsize/8);const auto [tg,ti]=bin(remain);const auto suffixhead=0x88+suffixindex*8,thead=0x88+tg*256+ti*8;
 if((tg!=2&&tg!=3)||read<std::uint32_t>(s,0x20)!=((1u<<tg)|1u)||read<std::uint32_t>(s,0x24)!=(1u<<suffixindex)||read<std::uint32_t>(s,0x24+tg*4)!=(1u<<ti)||read<std::uint64_t>(s,0xa8)!=h||read<std::uint64_t>(s,suffixhead)!=h+suffix||read<std::uint64_t>(s,thead)!=h+tail||read<std::uint64_t>(s,0x18)!=h||read<std::uint64_t>(s,0x10)!=h)throw std::invalid_argument("Actual two small singletons and large tail required");
 for(unsigned j=1;j<25;++j)if(j!=tg&&read<std::uint32_t>(s,0x24+j*4))throw std::invalid_argument("Other groups unsupported");
 for(std::size_t j=0x88;j<0x1988;j+=8)if(j!=0xa8&&j!=suffixhead&&j!=thead&&read<std::uint64_t>(s,j)!=h)throw std::invalid_argument("Other heads unsupported");
 for(auto j:{suffix+16,suffix+24,tail+16,tail+24})if(read<std::uint64_t>(s,j)!=h)throw std::invalid_argument("Actual singleton links required");
 if(read<std::uint64_t>(s,0x1ff0)!=h+tail||(read<std::uint64_t>(s,0x1ff8)&3)!=2)throw std::invalid_argument("Actual terminal guard required");
 if(requested!=suffixsize-8)throw std::invalid_argument("Exact genuine suffix consume required");
 write(s,suffixhead,h);write(s,0x10,h);write(s,0x18,h);write(s,0x24,0u);write(s,0x20,1u<<tg);
 write(s,suffix+8,suffixsize);write(s,next,requested);write(s,next+8,std::uint64_t{264});
 const auto occupied=read<std::uint64_t>(p,0x70)+suffixsize+8,total=read<std::uint64_t>(p,0x60)+requested;
 write(p,0x70,occupied);write(p,0x78,std::max(read<std::uint64_t>(p,0x78),occupied));write(p,0x60,total);write(p,0x68,std::max(read<std::uint64_t>(p,0x68),total));write(p,0x50,std::uint64_t{5});write(p,0x58,std::uint64_t{5});
 return {h+suffix+16,s.subspan(suffix+16,static_cast<std::size_t>(requested))};
}
std::uint64_t bitmap_align16_suffix_requested_size(std::span<std::byte> p,SystemPoolBackend& b,std::uint64_t pointer){
 if(p.size()!=192)throw std::invalid_argument("Bounded leading-gap request required");
 auto bytes=b.memory.bytes();const auto base=reinterpret_cast<std::uintptr_t>(bytes.data()),object=reinterpret_cast<std::uintptr_t>(p.data());
 if(object<base||object-base>bytes.size()||192>bytes.size()-(object-base)||read<std::uint64_t>(p,0)!=0x025baa70||read<std::uint64_t>(p,0x10)||read<std::uint32_t>(p,0x1c)!=1||p[0x18]!=std::byte{1}||p[0x4b]!=std::byte{0})throw std::invalid_argument("Activated owned derived required");
 const auto h=read<std::uint64_t>(p,0xa8);
 if((h&15)||h!=read<std::uint64_t>(p,0x20)||read<std::uint64_t>(p,0x28)!=8192||h<base||h-base>bytes.size()||8192>bytes.size()-(h-base))throw std::invalid_argument("Actual naturally aligned resource required");
 auto s=bytes.subspan(static_cast<std::size_t>(h-base),8192);const auto first=read<std::uint64_t>(s,0x1988);
 if(first<144||first>192||(first&15))throw std::invalid_argument("Actual declared allocated prefix required");
 const auto split=0x1980+static_cast<std::size_t>(first)+8,active=split+40;
 const auto marker=read<std::uint64_t>(s,active+8),allocated=marker&~std::uint64_t{3};
 if(allocated<24||allocated>40||(allocated&7)||marker!=allocated||((h+active+16)&15)||read<std::uint64_t>(s,split+8)!=32||read<std::uint64_t>(s,active)!=24)throw std::invalid_argument("Observed owned aligned small block required");
 const auto suffix=active+static_cast<std::size_t>(allocated)+8,suffixmarker=read<std::uint64_t>(s,suffix+8),suffixsize=suffixmarker&~std::uint64_t{3},request=read<std::uint64_t>(s,suffix);
 if(suffixsize<40||suffixsize>168||(suffixsize&7)||suffixmarker!=suffixsize||request<8||request>32||std::max(std::uint64_t{24},(request+15)&~std::uint64_t{7})!=allocated)throw std::invalid_argument("Actual requested footer and small suffix required");
 const auto size=40+allocated+8+suffixsize,original=first+size+8,next=split+static_cast<std::size_t>(size)+8,tail=next+272,remain=1640-original-280,prior=read<std::uint64_t>(s,split);
 if(size<128||size>240||original<328||original>392||prior<129||prior>177||((prior-129)&15)||((prior+15)&~std::uint64_t{7})!=first||read<std::uint64_t>(s,next)!=suffixsize-8||read<std::uint64_t>(s,next+8)!=264||read<std::uint64_t>(s,next+272)!=256||read<std::uint64_t>(s,tail+8)!=(remain|1)||read<std::uint64_t>(p,0x50)!=5||read<std::uint64_t>(p,0x58)!=5||read<std::uint64_t>(p,0x60)!=256+prior+request+24+suffixsize-8||read<std::uint64_t>(p,0x70)!=0x1998+first+280+allocated+48+suffixsize+8)throw std::invalid_argument("Actual bounded source layout and statistics required");
 const auto suffixindex=static_cast<unsigned>(suffixsize/8);const auto [tg,ti]=bin(remain);const auto suffixhead=0x88+suffixindex*8,thead=0x88+tg*256+ti*8;
 if((tg!=2&&tg!=3)||read<std::uint32_t>(s,0x20)!=(1u<<tg)||read<std::uint32_t>(s,0x24)!=0u||read<std::uint32_t>(s,0x24+tg*4)!=(1u<<ti)||read<std::uint64_t>(s,0xa8)!=h||read<std::uint64_t>(s,suffixhead)!=h||read<std::uint64_t>(s,thead)!=h+tail||read<std::uint64_t>(s,0x18)!=h||read<std::uint64_t>(s,0x10)!=h)throw std::invalid_argument("Actual two small singletons and large tail required");
 for(unsigned j=1;j<25;++j)if(j!=tg&&read<std::uint32_t>(s,0x24+j*4))throw std::invalid_argument("Other groups unsupported");
 for(std::size_t j=0x88;j<0x1988;j+=8)if(j!=0xa8&&j!=suffixhead&&j!=thead&&read<std::uint64_t>(s,j)!=h)throw std::invalid_argument("Other heads unsupported");
 for(auto j:{tail+16,tail+24})if(read<std::uint64_t>(s,j)!=h)throw std::invalid_argument("Actual singleton links required");
 if(read<std::uint64_t>(s,0x1ff0)!=h+tail||(read<std::uint64_t>(s,0x1ff8)&3)!=2)throw std::invalid_argument("Actual terminal guard required");
 if(pointer!=h+suffix+16)throw std::invalid_argument("Actual owned consumed suffix pointer required");
 return read<std::uint64_t>(s,next);
}
}
