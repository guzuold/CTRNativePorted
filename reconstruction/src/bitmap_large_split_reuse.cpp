#include "ctr/bitmap_large_split_reuse.h"
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
BitmapAllocation allocate_normal_bitmap_large_split_reuse(std::span<std::byte> p,SystemPoolBackend& b,std::uint64_t request,std::uint32_t alignment){
 if(p.size()!=192||request<129||request>192||alignment!=8)throw std::invalid_argument("Exact large reuse domain");
 auto bytes=b.memory.bytes();const auto base=reinterpret_cast<std::uintptr_t>(bytes.data()),object=reinterpret_cast<std::uintptr_t>(p.data());
 if(object<base||object-base>bytes.size()||192>bytes.size()-(object-base)||read<std::uint64_t>(p,0)!=0x025baa70||read<std::uint64_t>(p,0x10)||read<std::uint32_t>(p,0x1c)!=1||p[0x18]!=std::byte{1}||p[0x4b]!=std::byte{0})throw std::invalid_argument("Activated owned derived required");
 const auto h=read<std::uint64_t>(p,0xa8);
 if(h!=read<std::uint64_t>(p,0x20)||read<std::uint64_t>(p,0x28)!=8192||h<base||h-base>bytes.size()||8192>bytes.size()-(h-base))throw std::invalid_argument("Owned resource required");
 auto s=bytes.subspan(static_cast<std::size_t>(h-base),8192);const auto size=read<std::uint64_t>(s,0x1988)&~std::uint64_t{3};const auto allocated=(request+15)&~std::uint64_t{7};const auto remaining=size-allocated-8;const auto next=0x1980+static_cast<std::size_t>(size)+8,tail=next+272;const auto remain=1640-size-280;
 if(size<264||size>392||remaining<56||remaining>240)throw std::invalid_argument("Bounded original large size and split remainder required");
 const auto [g,i]=bin(size);const auto [tg,ti]=bin(remain);const auto head=0x88+g*256+i*8,thead=0x88+tg*256+ti*8;
 if(size<264||size>392||remaining<24||remaining>255||g!=1||(tg!=2&&tg!=3)||read<std::uint64_t>(p,0x50)!=1||read<std::uint64_t>(p,0x60)!=256||read<std::uint64_t>(p,0x70)!=0x1998+272||read<std::uint64_t>(s,0x1988)!=(size|1)||read<std::uint64_t>(s,next)!=h+0x1980||read<std::uint64_t>(s,next+8)!=266||read<std::uint64_t>(s,next+272)!=256||read<std::uint64_t>(s,tail+8)!=(remain|1))throw std::invalid_argument("Exact freed prefix and live successor required");
 if(read<std::uint32_t>(s,0x20)!=((1u<<g)|(1u<<tg))||read<std::uint32_t>(s,0x24+g*4)!=(1u<<i)||read<std::uint32_t>(s,0x24+tg*4)!=(1u<<ti)||read<std::uint64_t>(s,head)!=h+0x1980||read<std::uint64_t>(s,thead)!=h+tail||read<std::uint64_t>(s,0x18)!=h+0x1980)throw std::invalid_argument("Actual singleton maps required");
 for(unsigned j=0;j<25;++j)if(j!=g&&j!=tg&&read<std::uint32_t>(s,0x24+j*4))throw std::invalid_argument("Other maps unsupported");
 for(std::size_t j=0x88;j<0x1988;j+=8)if(j!=head&&j!=thead&&read<std::uint64_t>(s,j)!=h)throw std::invalid_argument("Other heads unsupported");
 for(auto j:{std::size_t{0x1990},std::size_t{0x1998},tail+16,tail+24})if(read<std::uint64_t>(s,j)!=h)throw std::invalid_argument("Actual singleton links required");
 if(read<std::uint64_t>(s,tail+static_cast<std::size_t>(remain)+8)!=h+tail||(read<std::uint64_t>(s,tail+static_cast<std::size_t>(remain)+16)&3)!=2)throw std::invalid_argument("Terminal guard required");
 // Unlink original large singleton, then split and insert genuine small remainder.
 write(s,0x18,h);write(s,0x10,h);write(s,head,h);write(s,0x24+g*4,0u);write(s,0x20,1u<<tg);write(s,0x1988,allocated);
 const auto split=0x1980+static_cast<std::size_t>(allocated)+8;const auto small=static_cast<unsigned>(remaining/8);
 write(s,split,h+0x1980);write(s,split+8,remaining|1);write(s,next,h+split);s[next+8]|=std::byte{2};
 write(s,split+16,h);write(s,split+24,h);write(s,0x18,h+split);write(s,0x88+small*8,h+split);write(s,0x24,1u<<small);write(s,0x20,(1u<<tg)|1u);
 write(s,split,request);
 const auto occupied=read<std::uint64_t>(p,0x70)+allocated+8,total=256+request;
 write(p,0x70,occupied);write(p,0x78,std::max(read<std::uint64_t>(p,0x78),occupied));write(p,0x60,total);write(p,0x68,std::max(read<std::uint64_t>(p,0x68),total));write(p,0x50,std::uint64_t{2});write(p,0x58,std::max(read<std::uint64_t>(p,0x58),std::uint64_t{2}));
 return {h+0x1990,s.subspan(0x1990,static_cast<std::size_t>(request))};
}
}

