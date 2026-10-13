#include "ctr/bitmap_split_three_free.h"
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
std::uint32_t free_normal_bitmap_split_three(std::span<std::byte> p,SystemPoolBackend& b,std::uint64_t pointer){
 if(p.size()!=192)throw std::invalid_argument("Actual derived192 required");
 auto bytes=b.memory.bytes();const auto base=reinterpret_cast<std::uintptr_t>(bytes.data()),object=reinterpret_cast<std::uintptr_t>(p.data());
 if(object<base||object-base>bytes.size()||192>bytes.size()-(object-base)||read<std::uint64_t>(p,0)!=0x025baa70||read<std::uint64_t>(p,0x10)||read<std::uint32_t>(p,0x1c)!=1||p[0x18]!=std::byte{1}||p[0x4b]!=std::byte{0})throw std::invalid_argument("Activated owned derived required");
 const auto h=read<std::uint64_t>(p,0xa8);
 if(h!=read<std::uint64_t>(p,0x20)||read<std::uint64_t>(p,0x28)!=8192||h<base||h-base>bytes.size()||8192>bytes.size()-(h-base))throw std::invalid_argument("Owned resource required");
 const auto count=read<std::uint64_t>(p,0x50);
 // The final state is the previously verified actual two-large backward path.
 if(read<std::uint64_t>(p,0x58)!=3)throw std::invalid_argument("Measured three-block peak required");
 if(count==1){auto final_resource=bytes.subspan(static_cast<std::size_t>(h-base),8192);const auto prior_marker=read<std::uint64_t>(final_resource,0x1988),prior_size=prior_marker&~std::uint64_t{3};if(prior_size<264||prior_size>392||prior_marker!=(prior_size|1))throw std::invalid_argument("Actual merged prefix required");const auto second=0x1980+static_cast<std::size_t>(prior_size)+8,tail_offset=second+272;if(pointer!=h+second+16||read<std::uint64_t>(p,0x60)!=256||read<std::uint64_t>(p,0x70)!=0x1998+272||read<std::uint64_t>(final_resource,0x1ff0)!=h+tail_offset||(read<std::uint64_t>(final_resource,0x1ff8)&3)!=2)throw std::invalid_argument("Final owned chain/statistics/guard required");return free_normal_bitmap_large_pair(p,b,pointer);}

 if(count!=2&&count!=3)throw std::invalid_argument("Three-block free order required");
 auto s=bytes.subspan(static_cast<std::size_t>(h-base),8192);const auto first=read<std::uint64_t>(s,0x1988);
 if(first<144||first>200||(first&7))throw std::invalid_argument("Live split prefix required");
 const auto split=0x1980+static_cast<std::size_t>(first)+8,marker=read<std::uint64_t>(s,split+8),size=marker&~std::uint64_t{3};
 if(size<56||size>240||(size&7)||marker!=(size|(count==2?1u:0u)))throw std::invalid_argument("Bounded actual remainder required");
 const auto original=first+size+8,next=split+static_cast<std::size_t>(size)+8,tail=next+272,remain=1640-original-280,prior=read<std::uint64_t>(s,split);
 if(original<264||original>392||prior<129||prior>192||((prior+15)&~std::uint64_t{7})!=first||read<std::uint64_t>(s,next+8)!=(count==2?266u:264u)||read<std::uint64_t>(s,next+272)!=256||read<std::uint64_t>(s,tail+8)!=(remain|1))throw std::invalid_argument("Actual split and second/tail layout required");
 const auto requested=count==3?size-8:prior,charged=count==3?size+8:first+8;
 if(pointer!=(count==3?h+split+16:h+0x1990)||read<std::uint64_t>(p,0x60)!=256+prior+(count==3?size-8:0)||read<std::uint64_t>(p,0x70)!=0x1998+first+8+272+(count==3?size+8:0)||read<std::uint64_t>(s,next)!=(count==3?size-8:h+split))throw std::invalid_argument("Exact free identity/footer/statistics required");
 const auto small=static_cast<unsigned>(size/8);const auto [tg,ti]=bin(remain);const auto head=0x88+small*8,thead=0x88+tg*256+ti*8;
 if((tg!=2&&tg!=3)||read<std::uint32_t>(s,0x20)!=((1u<<tg)|(count==2?1u:0u))||read<std::uint32_t>(s,0x24)!=(count==2?1u<<small:0u)||read<std::uint32_t>(s,0x24+tg*4)!=(1u<<ti)||read<std::uint64_t>(s,thead)!=h+tail||read<std::uint64_t>(s,0x18)!=(count==2?h+split:h))throw std::invalid_argument("Actual bins required");
 for(unsigned j=1;j<25;++j)if(j!=tg&&read<std::uint32_t>(s,0x24+j*4))throw std::invalid_argument("Other groups unsupported");
 for(std::size_t j=0x88;j<0x1988;j+=8)if(j!=thead&&!(count==2&&j==head)&&read<std::uint64_t>(s,j)!=h)throw std::invalid_argument("Other heads unsupported");
 if(count==2&&(read<std::uint64_t>(s,head)!=h+split||read<std::uint64_t>(s,split+16)!=h||read<std::uint64_t>(s,split+24)!=h))throw std::invalid_argument("Actual small singleton required");
 if(read<std::uint64_t>(s,tail+16)!=h||read<std::uint64_t>(s,tail+24)!=h||read<std::uint64_t>(s,0x1ff0)!=h+tail||(read<std::uint64_t>(s,0x1ff8)&3)!=2)throw std::invalid_argument("Actual tail links/guard required");
 // All bounded checks precede original mutation order.
 if(count==3){
  write(s,next,h+split);s[next+8]|=std::byte{2};write(s,split+8,marker|1);write(s,split+16,h);write(s,split+24,h);write(s,0x18,h+split);write(s,head,h+split);write(s,0x24,1u<<small);write(s,0x20,(1u<<tg)|1u);
 }else{
  write(s,split,h+0x1980);s[split+8]|=std::byte{2};write(s,0x1988,first|1);
  write(s,0x18,h);write(s,0x10,h);write(s,head,h);write(s,0x24,0u);write(s,0x20,1u<<tg);
  write(s,0x1988,original|1);write(s,next,h+0x1980);
  const auto [g,i]=bin(original);write(s,0x1990,h);write(s,0x1998,h);write(s,0x18,h+0x1980);write(s,0x88+g*256+i*8,h+0x1980);write(s,0x24+g*4,1u<<i);write(s,0x20,(1u<<tg)|(1u<<g));
 }
 const auto occupied=read<std::uint64_t>(p,0x70)-charged,total=read<std::uint64_t>(p,0x60)-requested;
 write(p,0x70,occupied);write(p,0x78,std::max(read<std::uint64_t>(p,0x78),occupied));write(p,0x60,total);write(p,0x68,std::max(read<std::uint64_t>(p,0x68),total));write(p,0x50,count-1);return 0;
}
}
