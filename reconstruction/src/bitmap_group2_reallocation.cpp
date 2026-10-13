#include "ctr/bitmap_group2_reallocation.h"
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
BitmapAllocation allocate_normal_bitmap_group2_reallocation(std::span<std::byte> p,SystemPoolBackend& b,std::uint64_t request,std::uint32_t alignment){
 if(p.size()!=192||request<512||request>640||alignment!=8)throw std::invalid_argument("Exact large reuse domain");
 auto bytes=b.memory.bytes();const auto base=reinterpret_cast<std::uintptr_t>(bytes.data()),object=reinterpret_cast<std::uintptr_t>(p.data());
 if(object<base||object-base>bytes.size()||192>bytes.size()-(object-base)||read<std::uint64_t>(p,0)!=0x025baa70||read<std::uint64_t>(p,0x10)||read<std::uint32_t>(p,0x1c)!=1||p[0x18]!=std::byte{1}||p[0x4b]!=std::byte{0})throw std::invalid_argument("Activated owned derived required");
 const auto h=read<std::uint64_t>(p,0xa8);
 if(h!=read<std::uint64_t>(p,0x20)||read<std::uint64_t>(p,0x28)!=8192||h<base||h-base>bytes.size()||8192>bytes.size()-(h-base))throw std::invalid_argument("Owned resource required");
 auto s=bytes.subspan(static_cast<std::size_t>(h-base),8192);const auto size=(request+15)&~std::uint64_t{7};const auto next=0x1980+static_cast<std::size_t>(size)+8,tail=next+272;const auto remain=1640-size-280;
 const auto [g,i]=bin(size);const auto [tg,ti]=bin(remain);const auto head=0x88+g*256+i*8,thead=0x88+tg*256+ti*8;
 if(g!=2||tg!=2||i==ti||read<std::uint64_t>(p,0x50)!=1||read<std::uint64_t>(p,0x60)!=256||read<std::uint64_t>(p,0x70)!=0x1998+272||read<std::uint64_t>(s,0x1988)!=(size|1)||read<std::uint64_t>(s,next)!=h+0x1980||read<std::uint64_t>(s,next+8)!=266||read<std::uint64_t>(s,next+272)!=256||read<std::uint64_t>(s,tail+8)!=(remain|1))throw std::invalid_argument("Exact freed prefix and live successor required");
 if(read<std::uint32_t>(s,0x20)!=4||read<std::uint32_t>(s,0x2c)!=((1u<<i)|(1u<<ti))||read<std::uint64_t>(s,head)!=h+0x1980||read<std::uint64_t>(s,thead)!=h+tail||read<std::uint64_t>(s,0x18)!=h+0x1980||read<std::uint64_t>(s,0x10)!=h)throw std::invalid_argument("Two actual group2 singleton maps required");
 if(read<std::uint64_t>(p,0x58)!=2||read<std::uint64_t>(p,0x68)!=request+256||read<std::uint64_t>(p,0x78)!=0x1998+size+280)throw std::invalid_argument("Original pair peak statistics required");
 for(unsigned j=0;j<25;++j)if(j!=g&&j!=tg&&read<std::uint32_t>(s,0x24+j*4))throw std::invalid_argument("Other maps unsupported");
 for(std::size_t j=0x88;j<0x1988;j+=8)if(j!=head&&j!=thead&&read<std::uint64_t>(s,j)!=h)throw std::invalid_argument("Other heads unsupported");
 for(auto j:{std::size_t{0x1990},std::size_t{0x1998},tail+16,tail+24})if(read<std::uint64_t>(s,j)!=h)throw std::invalid_argument("Actual singleton links required");
 if(read<std::uint64_t>(s,tail+static_cast<std::size_t>(remain)+8)!=h+tail||(read<std::uint64_t>(s,tail+static_cast<std::size_t>(remain)+16)&3)!=2)throw std::invalid_argument("Terminal guard required");
 // Original lookup rounds upward by the group2 quantum before filtering bin bits.
 const auto lookup=bin(size+15);if(lookup.first!=2)throw std::invalid_argument("Bounded group2 lookup required");
 const bool prefix=i>=lookup.second;
 if(!prefix&&ti<lookup.second)throw std::invalid_argument("Original lookup has no supported candidate");
 const auto chosen=prefix?std::size_t{0x1980}:tail;
 const auto split=tail+static_cast<std::size_t>(size)+8,remaining=remain-size-8;
 if(!prefix&&(remaining<56||remaining>312))throw std::invalid_argument("Observed tail split remainder required");
 // Validate the entire shape before unlinking the selected owned singleton.
 write(s,0x18,h);write(s,0x10,h);write(s,prefix?head:thead,h);write(s,0x2c,1u<<(prefix?ti:i));write(s,chosen+8,size);
 if(prefix){s[next+8]&=std::byte{0xfd};write(s,next,request);}
 else{
  write(s,split,h+tail);write(s,split+8,remaining|1);write(s,0x1ff0,h+split);s[0x1ff8]|=std::byte{2};
  write(s,split+16,h);write(s,split+24,h);write(s,0x18,h+split);
  const auto rg=remaining<256?0u:bin(remaining).first,ri=remaining<256?static_cast<unsigned>(remaining/8):bin(remaining).second;
  write(s,0x88+rg*256+ri*8,h+split);write(s,0x24+rg*4,1u<<ri);write(s,0x20,4u|(1u<<rg));write(s,split,request);
 }
 const auto occupied=read<std::uint64_t>(p,0x70)+size+8,total=256+request;
 write(p,0x70,occupied);write(p,0x78,std::max(read<std::uint64_t>(p,0x78),occupied));write(p,0x60,total);write(p,0x68,std::max(read<std::uint64_t>(p,0x68),total));write(p,0x50,std::uint64_t{2});write(p,0x58,std::max(read<std::uint64_t>(p,0x58),std::uint64_t{2}));
 return {h+chosen+16,s.subspan(chosen+16,static_cast<std::size_t>(request))};
}
}

