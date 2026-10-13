#include "ctr/bitmap_align16_leading_gap.h"
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
BitmapAllocation allocate_normal_bitmap_align16_leading_gap(std::span<std::byte> p,SystemPoolBackend& b,std::uint64_t request,std::uint32_t alignment){
 if(p.size()!=192||request<8||request>32||alignment!=16)throw std::invalid_argument("Bounded leading-gap request required");
 auto bytes=b.memory.bytes();const auto base=reinterpret_cast<std::uintptr_t>(bytes.data()),object=reinterpret_cast<std::uintptr_t>(p.data());
 if(object<base||object-base>bytes.size()||192>bytes.size()-(object-base)||read<std::uint64_t>(p,0)!=0x025baa70||read<std::uint64_t>(p,0x10)||read<std::uint32_t>(p,0x1c)!=1||p[0x18]!=std::byte{1}||p[0x4b]!=std::byte{0})throw std::invalid_argument("Activated owned derived required");
 const auto h=read<std::uint64_t>(p,0xa8);
 if((h&15)||h!=read<std::uint64_t>(p,0x20)||read<std::uint64_t>(p,0x28)!=8192||h<base||h-base>bytes.size()||8192>bytes.size()-(h-base))throw std::invalid_argument("Actual naturally aligned resource required");
 auto s=bytes.subspan(static_cast<std::size_t>(h-base),8192);const auto first=read<std::uint64_t>(s,0x1988);
 if(first<144||first>192||(first&15))throw std::invalid_argument("Actual declared allocated prefix required");
 const auto split=0x1980+static_cast<std::size_t>(first)+8,marker=read<std::uint64_t>(s,split+8),size=marker&~std::uint64_t{3};
 if(size<128||size>240||(size&7)||marker!=(size|1)||((h+split+16)&15)!=8)throw std::invalid_argument("Actual misaligned small singleton required");
 const auto original=first+size+8,next=split+static_cast<std::size_t>(size)+8,tail=next+272,remain=1640-original-280,prior=read<std::uint64_t>(s,split);
 if(original<328||original>392||prior<129||prior>177||((prior-129)&15)||((prior+15)&~std::uint64_t{7})!=first||read<std::uint64_t>(s,next)!=h+split||read<std::uint64_t>(s,next+8)!=266||read<std::uint64_t>(s,next+272)!=256||read<std::uint64_t>(s,tail+8)!=(remain|1)||read<std::uint64_t>(p,0x50)!=2||read<std::uint64_t>(p,0x58)!=3||read<std::uint64_t>(p,0x60)!=256+prior||read<std::uint64_t>(p,0x70)!=0x1998+first+280)throw std::invalid_argument("Actual bounded source layout and statistics required");
 const auto allocated=std::max(std::uint64_t{24},(request+15)&~std::uint64_t{7}),lookup=(allocated+63)&~std::uint64_t{15};
 const auto initial_gap=(std::uint64_t{0}-(h+split+16))&15;const auto gap=((h+split+16+initial_gap+15+std::max(std::uint64_t{32}-initial_gap,std::uint64_t{16}))&~std::uint64_t{15})-(h+split+16);
 if(initial_gap!=8||gap!=40||size<lookup)throw std::invalid_argument("Observed initial8/adjusted40 gap required");
 const auto suffix_size=size-gap-allocated-8,suffix=split+static_cast<std::size_t>(gap+allocated)+8,active=split+static_cast<std::size_t>(gap);
 if(suffix_size<40||suffix_size>168)throw std::invalid_argument("Bounded small suffix required");
 const auto small=static_cast<unsigned>(size/8),suffix_index=static_cast<unsigned>(suffix_size/8);const auto [tg,ti]=bin(remain);const auto head=0x88+small*8,thead=0x88+tg*256+ti*8;
 if((tg!=2&&tg!=3)||read<std::uint32_t>(s,0x20)!=((1u<<tg)|1u)||read<std::uint32_t>(s,0x24)!=(1u<<small)||read<std::uint32_t>(s,0x24+tg*4)!=(1u<<ti)||read<std::uint64_t>(s,head)!=h+split||read<std::uint64_t>(s,thead)!=h+tail||read<std::uint64_t>(s,0x18)!=h+split)throw std::invalid_argument("Actual small and large bins required");
 for(unsigned j=1;j<25;++j)if(j!=tg&&read<std::uint32_t>(s,0x24+j*4))throw std::invalid_argument("Other groups unsupported");
 for(std::size_t j=0x88;j<0x1988;j+=8)if(j!=head&&j!=thead&&read<std::uint64_t>(s,j)!=h)throw std::invalid_argument("Other heads unsupported");
 for(auto j:{split+16,split+24,tail+16,tail+24})if(read<std::uint64_t>(s,j)!=h)throw std::invalid_argument("Actual singleton links required");
 if(read<std::uint64_t>(s,0x1ff0)!=h+tail||(read<std::uint64_t>(s,0x1ff8)&3)!=2)throw std::invalid_argument("Actual terminal guard required");
 // Original bounded unlink, leading prefix split, aligned consume and small suffix.
 write(s,head,h);write(s,0x10,h);write(s,0x18,h);write(s,0x24,0u);write(s,0x20,1u<<tg);
 write(s,split+8,std::uint64_t{33});write(s,active,h+split);write(s,split+16,h);write(s,split+24,h);write(s,0xa8,h+split);
 write(s,active+8,allocated|2);write(s,suffix,request);write(s,suffix+8,suffix_size|1);write(s,suffix+16,h);write(s,suffix+24,h);write(s,next,h+suffix);
 write(s,0x18,h+suffix);write(s,0x88+suffix_index*8,h+suffix);write(s,0x24,(1u<<4)|(1u<<suffix_index));write(s,0x20,(1u<<tg)|1u);
 const auto occupied=read<std::uint64_t>(p,0x70)+allocated+8,total=read<std::uint64_t>(p,0x60)+request;
 write(p,0x70,occupied);write(p,0x78,std::max(read<std::uint64_t>(p,0x78),occupied));write(p,0x60,total);write(p,0x68,std::max(read<std::uint64_t>(p,0x68),total));write(p,0x50,std::uint64_t{3});write(p,0x58,std::max(read<std::uint64_t>(p,0x58),std::uint64_t{3}));
 return {h+active+16,s.subspan(active+16,static_cast<std::size_t>(request))};
}
}
