#include "ctr/bitmap_nonlast_free.h"
#include <algorithm>
#include <bit>
#include <cstring>
#include <stdexcept>
namespace ctr {
namespace {
template<class T> T get(std::span<std::byte> s,std::size_t off){T v;std::memcpy(&v,s.data()+off,sizeof(v));return v;}
template<class T> void put(std::span<std::byte> s,std::size_t off,T v){std::memcpy(s.data()+off,&v,sizeof(v));}
std::pair<unsigned,unsigned> index(std::uint64_t size){const auto msb=31u-static_cast<unsigned>(std::countl_zero(static_cast<std::uint32_t>(size)));return {msb-7,static_cast<unsigned>((size>>(msb-5))^0x20)};}
}
std::uint32_t free_normal_bitmap_adjacent_pair(std::span<std::byte> pool,SystemPoolBackend& b,std::uint64_t pointer){
 if(pool.size()!=192)throw std::invalid_argument("Actual derived192 required");auto bytes=b.memory.bytes();
 const auto base=static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(bytes.data())),object=static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(pool.data()));
 if(object<base||object-base>bytes.size()||192>bytes.size()-(object-base)||get<std::uint64_t>(pool,0)!=0x025baa70||get<std::uint64_t>(pool,0x10)||get<std::uint32_t>(pool,0x1c)!=1||pool[0x18]!=std::byte{1}||pool[0x4b]!=std::byte{0})throw std::invalid_argument("Concrete activated owner0/debug0 derived required");
 const auto header=get<std::uint64_t>(pool,0xa8);
 if(header!=get<std::uint64_t>(pool,0x20)||get<std::uint64_t>(pool,0x28)!=8192||header<base||header-base>bytes.size()||8192>bytes.size()-(header-base))throw std::invalid_argument("Actual8192 bitmap resource required");
 auto s=bytes.subspan(static_cast<std::size_t>(header-base),8192);const auto requested=bitmap_normal_requested_size(s,pointer);
 const auto chunk=static_cast<std::size_t>(pointer-header)-16;const auto marker=get<std::uint64_t>(s,chunk+8),size=marker&~std::uint64_t{3};
 if(size<24||size>120||size>s.size()-chunk-40)throw std::invalid_argument("Bounded normal small allocated chunk required");
 const auto next=chunk+static_cast<std::size_t>(size)+8;const auto nextmarker=get<std::uint64_t>(s,next+8),nextsize=nextmarker&~std::uint64_t{3};
 const bool backward=(marker&3)==2;
 std::size_t previous=chunk,large=next;std::uint64_t previoussize=0;
 if(backward){
  const auto token=get<std::uint64_t>(s,chunk);
  if(token<header+0x1980||token>=header+chunk||token&7)throw std::invalid_argument("Previous free footer outside span");
  previous=static_cast<std::size_t>(token-header);const auto prevmarker=get<std::uint64_t>(s,previous+8);previoussize=prevmarker&~std::uint64_t{3};
  if((prevmarker&3)!=1||previoussize<24||previoussize>120||previous+previoussize+8!=chunk)throw std::invalid_argument("Adjacent small previous singleton required");
 }else{
  if((marker&3)!=0||(nextmarker&3)!=0||nextsize<24||nextsize>120||nextsize>s.size()-next-40)throw std::invalid_argument("Penultimate allocated neighbor required");
  large=next+static_cast<std::size_t>(nextsize)+8;
 }
 const auto largemarker=get<std::uint64_t>(s,large+8),largesize=largemarker&~std::uint64_t{3};
 if((largemarker&3)!=1||largesize<=255||largesize>s.size()-large-24||get<std::uint64_t>(s,large+16)!=header||get<std::uint64_t>(s,large+24)!=header)throw std::invalid_argument("Singleton large tail required");
 const auto [first,second]=index(largesize);const auto largehead=0x88+first*256+second*8;const auto smallindex=static_cast<unsigned>((backward?previoussize:size)>>3),smallhead=0x88+smallindex*8;
 if(first!=3||get<std::uint32_t>(s,0x20)!=(8u|(backward?1u:0u))||get<std::uint32_t>(s,0x30)!=(1u<<second)||get<std::uint32_t>(s,0x24)!=(backward?(1u<<smallindex):0u)||get<std::uint64_t>(s,largehead)!=header+large)throw std::invalid_argument("Declared small/large bitmap maps required");
 for(unsigned i=1;i<25;++i)if(i!=3&&get<std::uint32_t>(s,0x24+i*4))throw std::invalid_argument("Other groups unsupported");
 for(std::size_t off=0x88;off<0x1988;off+=8)if(off!=largehead&&!(backward&&off==smallhead)&&get<std::uint64_t>(s,off)!=header)throw std::invalid_argument("Other bins unsupported");
 if(backward&&(get<std::uint64_t>(s,smallhead)!=header+previous||get<std::uint64_t>(s,previous+16)!=header||get<std::uint64_t>(s,previous+24)!=header))throw std::invalid_argument("Previous small singleton links required");
 const auto charge=size+8,count=get<std::uint64_t>(pool,0x50),totalrequest=get<std::uint64_t>(pool,0x60),occupied=get<std::uint64_t>(pool,0x70);
 const auto merged=previoussize+size+largesize+16;const auto [newfirst,newsecond]=index(merged);
 if(!count||charge>occupied||requested>totalrequest||(backward&&newfirst!=3))throw std::invalid_argument("Consistent normal statistics/merge required");
 // All supported shape and link checks precede the first original store.
 put(s,next,header+chunk);s[next+8]|=std::byte{2};put(s,chunk+8,marker|1);
 if(!backward){
  put(s,chunk+16,header);put(s,chunk+24,header);put(s,0x18,header+chunk);put(s,smallhead,header+chunk);
  put(s,0x20,9u);put(s,0x24,1u<<smallindex);
 }else{
  put(s,0x18,header);put(s,0x10,header);put(s,smallhead,header);put(s,0x24,0u);put(s,0x20,8u);
  put(s,previous+8,(previoussize+size+8)|1);put(s,next,header+previous);
  put(s,0x18,header);put(s,0x10,header);put(s,largehead,header);put(s,0x30,0u);put(s,0x20,0u);
  put(s,previous+8,merged|1);put(s,previous+static_cast<std::size_t>(merged)+8,header+previous);
  put(s,previous+16,header);put(s,previous+24,header);put(s,0x18,header+previous);put(s,0x88+newfirst*256+newsecond*8,header+previous);
  put(s,0x20,1u<<newfirst);put(s,0x24+newfirst*4,1u<<newsecond);
 }
 put(pool,0x70,occupied-charge);put(pool,0x78,std::max(get<std::uint64_t>(pool,0x78),occupied-charge));
 put(pool,0x60,totalrequest-requested);put(pool,0x68,std::max(get<std::uint64_t>(pool,0x68),totalrequest-requested));put(pool,0x50,count-1);return 0;
}
}
