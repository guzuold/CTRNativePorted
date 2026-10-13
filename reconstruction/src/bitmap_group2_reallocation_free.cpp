#include "ctr/bitmap_group2_reallocation_free.h"
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
std::uint32_t free_normal_bitmap_group2_reallocation(std::span<std::byte> pool,SystemPoolBackend& b,std::uint64_t pointer){
 if(pool.size()!=192)throw std::invalid_argument("Actual derived192 required");auto bytes=b.memory.bytes();
 const auto base=static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(bytes.data())),object=static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(pool.data()));
 if(object<base||object-base>bytes.size()||192>bytes.size()-(object-base)||get<std::uint64_t>(pool,0)!=0x025baa70||get<std::uint64_t>(pool,0x10)||get<std::uint32_t>(pool,0x1c)!=1||pool[0x18]!=std::byte{1}||pool[0x4b]!=std::byte{0})throw std::invalid_argument("Actual activated derived owner required");
 const auto header=get<std::uint64_t>(pool,0xa8);
 if(header!=get<std::uint64_t>(pool,0x20)||get<std::uint64_t>(pool,0x28)!=8192||header<base||header-base>bytes.size()||8192>bytes.size()-(header-base))throw std::invalid_argument("Owned8192 resource required");
 auto s=bytes.subspan(static_cast<std::size_t>(header-base),8192);
 const auto peak=get<std::uint64_t>(pool,0x68);
 if(peak<768||peak>896)throw std::invalid_argument("Same-request pair peak required");
 const auto request=peak-256,size=(request+15)&~std::uint64_t{7};
 const auto next=0x1980+static_cast<std::size_t>(size)+8,tail=next+272,remain=1640-size-280;
 const auto [group,bin]=index(size);const auto [tailgroup,tailbin]=index(remain);const bool prefix=bin>=index(size+15).second;
 const auto chunk=prefix?std::size_t{0x1980}:tail;
 if(pointer!=header+chunk+16||bitmap_requested_size_large(s,pointer)!=request||get<std::uint64_t>(s,chunk+8)!=size||get<std::uint64_t>(pool,0x50)!=2||get<std::uint64_t>(pool,0x58)!=2||get<std::uint64_t>(pool,0x60)!=peak||get<std::uint64_t>(pool,0x70)!=0x1998+size+280||get<std::uint64_t>(pool,0x78)!=0x1998+size+280||group!=2||tailgroup!=2||bin==tailbin)throw std::invalid_argument("Exact live reallocation pointer/footer/statistics required");
 const auto head=0x88+2*256+bin*8,tailhead=0x88+2*256+tailbin*8;
 if(get<std::uint64_t>(s,0x10)!=header||get<std::uint64_t>(s,0x1ff8)!=2)throw std::invalid_argument("Actual sentinel/terminal guard required");
 if(prefix){
  if(get<std::uint64_t>(s,next+8)!=264||bitmap_requested_size_large(s,header+next+16)!=256||get<std::uint64_t>(s,tail+8)!=(remain|1)||get<std::uint64_t>(s,tail+16)!=header||get<std::uint64_t>(s,tail+24)!=header||get<std::uint64_t>(s,0x18)!=header||get<std::uint64_t>(s,0x1ff0)!=header+tail||get<std::uint32_t>(s,0x20)!=4||get<std::uint32_t>(s,0x2c)!=(1u<<tailbin))throw std::invalid_argument("Reallocated prefix and owned tail required");
  for(unsigned i=0;i<25;++i)if(i!=2&&get<std::uint32_t>(s,0x24+i*4))throw std::invalid_argument("Other groups unsupported");
  for(std::size_t off=0x88;off<0x1988;off+=8)if(get<std::uint64_t>(s,off)!=(off==tailhead?header+tail:header))throw std::invalid_argument("Actual prefix-branch bin heads required");
  put(s,next,header+0x1980);put(s,next+8,std::uint64_t{266});put(s,0x1988,size|1);put(s,0x1990,header);put(s,0x1998,header);put(s,0x18,header+0x1980);put(s,head,header+0x1980);put(s,0x2c,(1u<<tailbin)|(1u<<bin));
 }else{
  const auto split=tail+static_cast<std::size_t>(size)+8,remaining=remain-size-8;
  if(remaining<56||remaining>312)throw std::invalid_argument("Observed tail remainder required");
  const auto rg=remaining<256?0u:index(remaining).first,ri=remaining<256?static_cast<unsigned>(remaining/8):index(remaining).second,rhead=0x88+rg*256+ri*8;
  if(get<std::uint64_t>(s,0x1988)!=(size|1)||get<std::uint64_t>(s,0x1990)!=header||get<std::uint64_t>(s,0x1998)!=header||get<std::uint64_t>(s,next)!=header+0x1980||get<std::uint64_t>(s,next+8)!=266||bitmap_requested_size_large(s,header+next+16)!=256||get<std::uint64_t>(s,split+8)!=(remaining|1)||get<std::uint64_t>(s,split+16)!=header||get<std::uint64_t>(s,split+24)!=header||get<std::uint64_t>(s,0x18)!=header+split||get<std::uint64_t>(s,0x1ff0)!=header+split||get<std::uint32_t>(s,0x20)!=(4u|(1u<<rg))||get<std::uint32_t>(s,0x2c)!=(1u<<bin)||get<std::uint32_t>(s,0x24+rg*4)!=(1u<<ri))throw std::invalid_argument("Actual prefix/second/tail/remainder chain required");
  for(unsigned i=0;i<25;++i)if(i!=2&&i!=rg&&get<std::uint32_t>(s,0x24+i*4))throw std::invalid_argument("Other groups unsupported");
  for(std::size_t off=0x88;off<0x1988;off+=8)if(get<std::uint64_t>(s,off)!=(off==head?header+0x1980:off==rhead?header+split:header))throw std::invalid_argument("Actual tail-branch bin heads required");
  // Original transient free stores remain in stale interior bytes after merge.
  put(s,split,header+tail);put(s,split+8,remaining|3);put(s,tail+8,size|1);
  put(s,0x18,header);put(s,0x10,header);put(s,rhead,header);put(s,0x24+rg*4,0u);put(s,0x20,4u);
  put(s,tail+8,remain|1);put(s,0x1ff0,header+tail);put(s,tail+16,header);put(s,tail+24,header);put(s,0x18,header+tail);put(s,tailhead,header+tail);put(s,0x2c,(1u<<bin)|(1u<<tailbin));
 }
 put(pool,0x70,std::uint64_t{0x1998+272});put(pool,0x60,std::uint64_t{256});put(pool,0x50,std::uint64_t{1});return 0;
}
}
