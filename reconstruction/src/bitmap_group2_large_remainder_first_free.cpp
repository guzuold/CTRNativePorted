#include "ctr/bitmap_group2_large_remainder_first_free.h"
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
std::uint32_t free_normal_bitmap_group2_large_remainder_first(std::span<std::byte> p,SystemPoolBackend& b,std::uint64_t pointer){
 if(p.size()!=192)throw std::invalid_argument("Exact large reuse domain");
 auto bytes=b.memory.bytes();const auto base=reinterpret_cast<std::uintptr_t>(bytes.data()),object=reinterpret_cast<std::uintptr_t>(p.data());
 if(object<base||object-base>bytes.size()||192>bytes.size()-(object-base)||read<std::uint64_t>(p,0)!=0x025baa70||read<std::uint64_t>(p,0x10)||read<std::uint32_t>(p,0x1c)!=1||p[0x18]!=std::byte{1}||p[0x4b]!=std::byte{0})throw std::invalid_argument("Activated owned derived required");
 const auto h=read<std::uint64_t>(p,0xa8);
 if(h!=read<std::uint64_t>(p,0x20)||read<std::uint64_t>(p,0x28)!=8192||h<base||h-base>bytes.size()||8192>bytes.size()-(h-base))throw std::invalid_argument("Owned resource required");
 auto s=bytes.subspan(static_cast<std::size_t>(h-base),8192);
 const auto allocated=read<std::uint64_t>(s,0x1988);if(allocated<144||allocated>200||allocated%8||pointer!=h+0x1990)throw std::invalid_argument("Exact live prefix identity required");
 const auto split=0x1980+static_cast<std::size_t>(allocated)+8;const auto request=read<std::uint64_t>(s,split);
 const auto remaining=read<std::uint64_t>(s,split+8)&~std::uint64_t{3};
 if(request<129||request>192||((request+15)&~std::uint64_t{7})!=allocated||remaining<312||remaining>496||read<std::uint64_t>(s,split+8)!=(remaining|1))throw std::invalid_argument("Actual footer and forward large remainder required");
 const auto size=allocated+remaining+8,next=0x1980+static_cast<std::size_t>(size)+8,tail=next+272,remain=1640-size-280;
 if(size<520||size>648)throw std::invalid_argument("Bounded merged prefix required");
 const auto [g,i]=bin(size);const auto [tg,ti]=bin(remain);const auto [rg,ri]=bin(remaining);
 const auto head=0x88+g*256+i*8,thead=0x88+tg*256+ti*8,rhead=0x88+rg*256+ri*8;
 if(g!=2||tg!=2||rg!=1||i>=ti||read<std::uint64_t>(p,0x50)!=2||read<std::uint64_t>(p,0x60)!=256+request||read<std::uint64_t>(p,0x70)!=0x1998+allocated+280||read<std::uint64_t>(s,next)!=h+split||read<std::uint64_t>(s,next+8)!=266||read<std::uint64_t>(s,next+272)!=256||read<std::uint64_t>(s,tail+8)!=(remain|1))throw std::invalid_argument("Live prefix/second/forward tail chain required");
 if(read<std::uint32_t>(s,0x20)!=6||read<std::uint32_t>(s,0x28)!=(1u<<ri)||read<std::uint32_t>(s,0x2c)!=(1u<<ti)||read<std::uint64_t>(s,rhead)!=h+split||read<std::uint64_t>(s,thead)!=h+tail||read<std::uint64_t>(s,0x18)!=h+split||read<std::uint64_t>(s,0x10)!=h)throw std::invalid_argument("Actual group1/group2 singleton maps required");
 const auto peak=read<std::uint64_t>(p,0x68);
 if(read<std::uint64_t>(p,0x58)!=3||peak<768||peak>896||((peak-256+15)&~std::uint64_t{7})<size||read<std::uint64_t>(p,0x78)!=0x1998+((peak-256+15)&~std::uint64_t{7})+280)throw std::invalid_argument("Remainder-cycle PEAK3 with coherent maximum pair peaks required");
 for(unsigned j=0;j<25;++j)if(j!=rg&&j!=tg&&read<std::uint32_t>(s,0x24+j*4))throw std::invalid_argument("Other maps unsupported");
 for(std::size_t j=0x88;j<0x1988;j+=8)if(j!=rhead&&j!=thead&&read<std::uint64_t>(s,j)!=h)throw std::invalid_argument("Other heads unsupported");
 for(auto j:{split+16,split+24,tail+16,tail+24})if(read<std::uint64_t>(s,j)!=h)throw std::invalid_argument("Actual singleton links required");
 if(read<std::uint64_t>(s,tail+static_cast<std::size_t>(remain)+8)!=h+tail||read<std::uint64_t>(s,tail+static_cast<std::size_t>(remain)+16)!=2)throw std::invalid_argument("Terminal guard required");
 // Original 014db270->015c3a20 forward-large merge after genuine remainder consume/free (PEAK3 history).
 // Preserve original transient free writes in stale interior before forward-large unlink.
 write(s,0x1988,allocated|1);write(s,split,h+0x1980);write(s,split+8,remaining|3);
 write(s,0x18,h);write(s,0x10,h);write(s,rhead,h);write(s,0x28,0u);write(s,0x20,4u);
 write(s,0x1988,size|1);write(s,next,h+0x1980);s[next+8]|=std::byte{2};
 write(s,0x1990,h);write(s,0x1998,h);write(s,0x18,h+0x1980);write(s,head,h+0x1980);write(s,0x2c,(1u<<i)|(1u<<ti));
 write(p,0x70,std::uint64_t{0x1998+272});write(p,0x60,std::uint64_t{256});write(p,0x50,std::uint64_t{1});return 0;
}
}
