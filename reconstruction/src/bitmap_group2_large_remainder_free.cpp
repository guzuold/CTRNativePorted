#include "ctr/bitmap_group2_large_remainder_free.h"
#include <algorithm>
#include <bit>
#include <cstring>
#include <stdexcept>
namespace ctr {
namespace {
template<class T>T read(std::span<const std::byte> s,std::size_t n){T v;std::memcpy(&v,s.data()+n,sizeof(v));return v;}
template<class T>void write(std::span<std::byte> s,std::size_t n,T v){std::memcpy(s.data()+n,&v,sizeof(v));}
auto bin(std::uint64_t n){auto m=31u-static_cast<unsigned>(std::countl_zero(static_cast<std::uint32_t>(n)));return std::pair{m-7,static_cast<unsigned>((n>>(m-5))^32)};}
}
std::uint32_t free_normal_bitmap_group2_large_remainder(std::span<std::byte> p,SystemPoolBackend& b,std::uint64_t pointer){
 if(p.size()!=192)throw std::invalid_argument("Bounded remainder request required");
 auto bytes=b.memory.bytes();const auto base=reinterpret_cast<std::uintptr_t>(bytes.data()),object=reinterpret_cast<std::uintptr_t>(p.data());
 if(object<base||object-base>bytes.size()||192>bytes.size()-(object-base)||read<std::uint64_t>(p,0)!=0x025baa70||read<std::uint64_t>(p,0x10)||read<std::uint32_t>(p,0x1c)!=1||p[0x18]!=std::byte{1}||p[0x4b]!=std::byte{0})throw std::invalid_argument("Activated owned derived required");
 const auto h=read<std::uint64_t>(p,0xa8);
 if(h!=read<std::uint64_t>(p,0x20)||read<std::uint64_t>(p,0x28)!=8192||h<base||h-base>bytes.size()||8192>bytes.size()-(h-base))throw std::invalid_argument("Owned resource required");
 auto s=bytes.subspan(static_cast<std::size_t>(h-base),8192);const auto first=read<std::uint64_t>(s,0x1988);
 if(first<144||first>200||(first&7))throw std::invalid_argument("Actual live split prefix required");
 const auto split=0x1980+static_cast<std::size_t>(first)+8;const auto marker=read<std::uint64_t>(s,split+8),size=marker&~std::uint64_t{3};
 if(size<312||size>496||(size&7)||marker!=size||pointer!=h+split+16)throw std::invalid_argument("Exact group1 large remainder consume required");
 const auto request=read<std::uint64_t>(s,split+static_cast<std::size_t>(size)+8);if(request!=size-8||bitmap_requested_size_large(s,pointer)!=request)throw std::invalid_argument("Actual NORMALfooter required");
 const auto original=first+size+8,next=split+static_cast<std::size_t>(size)+8,tail=next+272,remain=1640-original-280;
 const auto prior=read<std::uint64_t>(s,split);
 if(original<520||original>648||prior<129||prior>192||((prior+15)&~std::uint64_t{7})!=first||read<std::uint64_t>(s,next)!=request||read<std::uint64_t>(s,next+8)!=264||read<std::uint64_t>(s,next+272)!=256||read<std::uint64_t>(s,tail+8)!=(remain|1)||read<std::uint64_t>(p,0x50)!=3||read<std::uint64_t>(p,0x60)!=256+prior+request||read<std::uint64_t>(p,0x70)!=0x1998+original+280)throw std::invalid_argument("Actual bounded split layout and statistics required");
 const auto [rg,ri]=bin(size);const auto [tg,ti]=bin(remain);const auto head=0x88+rg*256+ri*8,thead=0x88+tg*256+ti*8;
 if(rg!=1||tg!=2||read<std::uint32_t>(s,0x20)!=4||read<std::uint32_t>(s,0x28)!=0||read<std::uint32_t>(s,0x2c)!=(1u<<ti)||read<std::uint64_t>(s,head)!=h||read<std::uint64_t>(s,thead)!=h+tail||read<std::uint64_t>(s,0x18)!=h||read<std::uint64_t>(s,0x10)!=h)throw std::invalid_argument("Actual group1 remainder and group2 tail bins required");
 const auto peak=read<std::uint64_t>(p,0x68);
 if(read<std::uint64_t>(p,0x58)!=3||peak<768||peak>896||((peak-256+15)&~std::uint64_t{7})<original||read<std::uint64_t>(p,0x78)!=0x1998+((peak-256+15)&~std::uint64_t{7})+280)throw std::invalid_argument("Coherent retainedmaximum pair peaks required");
 for(unsigned j=0;j<25;++j)if(j!=rg&&j!=tg&&read<std::uint32_t>(s,0x24+j*4))throw std::invalid_argument("Other groups unsupported");
 for(std::size_t j=0x88;j<0x1988;j+=8)if(j!=thead&&read<std::uint64_t>(s,j)!=h)throw std::invalid_argument("Other heads unsupported");
 for(auto j:{tail+16,tail+24})if(read<std::uint64_t>(s,j)!=h)throw std::invalid_argument("Actual singleton links required");
 if(read<std::uint64_t>(s,tail+static_cast<std::size_t>(remain)+8)!=h+tail||read<std::uint64_t>(s,tail+static_cast<std::size_t>(remain)+16)!=2)throw std::invalid_argument("Terminal guard required");
 // Original nonlast LARGE insertion with both physical neighbours still allocated.
 write(s,split+8,size|1);write(s,next,h+split);s[next+8]|=std::byte{2};
 write(s,split+16,h);write(s,split+24,h);write(s,0x18,h+split);write(s,head,h+split);write(s,0x28,1u<<ri);write(s,0x20,6u);
 write(p,0x70,std::uint64_t{0x1998+first+280});write(p,0x60,std::uint64_t{256+prior});write(p,0x50,std::uint64_t{2});return 0;
}
}
