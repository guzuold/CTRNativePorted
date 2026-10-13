#include "ctr/bitmap_large_backward.h"
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
static BitmapAllocation allocate_bitmap_fresh_large(std::span<std::byte> s,std::uint64_t request,std::uint32_t alignment){
 if(s.size()!=8192||reinterpret_cast<std::uintptr_t>(s.data())&7||request<136||request>1032||alignment!=8)
  throw std::invalid_argument("Fresh bitmap underlying136..1032/align8/8192 required");
 const auto base=static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(s.data()));const auto map=get<std::uint32_t>(s,0x20);
 if(!map||std::popcount(map)!=1)throw std::invalid_argument("Exactly one bitmap group required");
 const auto first=static_cast<std::uint32_t>(std::countr_zero(map));
 if(first!=3)throw std::invalid_argument("Verified large group3 required");
 const auto secondmap=get<std::uint32_t>(s,0x24+first*4);
 if(!secondmap||std::popcount(secondmap)!=1)throw std::invalid_argument("Exactly one free bin required");
 for(std::size_t i=0;i<25;++i)if(i!=first&&get<std::uint32_t>(s,0x24+i*4))throw std::invalid_argument("Other free groups unsupported");
 const auto second=static_cast<std::uint32_t>(std::countr_zero(secondmap));const auto head=0x88+static_cast<std::size_t>(first)*256+second*8;
 const auto token=get<std::uint64_t>(s,head);
 if(token<base+0x1980||token-base>s.size()-32||(token&7))throw std::invalid_argument("Bitmap free chunk outside resource");
 const auto prefix=get<std::uint64_t>(s,0x1988);const auto prefixsize=prefix&~std::uint64_t{3};if(token!=base+0x1980&&((prefix&3)!=0||prefixsize<264||prefixsize>392||token!=base+0x1980+prefixsize+8))throw std::invalid_argument("Exact second adjacent large chunk required");
 const auto chunk=static_cast<std::size_t>(token-base);const auto marker=get<std::uint64_t>(s,chunk+8),size=marker&~std::uint64_t{3};
 const auto normalized=std::max(std::uint64_t{24},(request+7)&~std::uint64_t{7});
 if(marker!=((token==base+0x1980?1640:1640-prefixsize-8)|1)||size>s.size()-chunk-24||size<normalized+32||size-normalized-8<=255||get<std::uint64_t>(s,chunk+16)!=base||get<std::uint64_t>(s,chunk+24)!=base||index(size)!=std::pair{first,second})
  throw std::invalid_argument("Consistent singleton large free chunk and split required");
 for(std::size_t off=0x88;off<0x1988;off+=8)if(off!=head&&get<std::uint64_t>(s,off)!=base)throw std::invalid_argument("Other free heads unsupported");
 const auto remaining=size-normalized-8;const auto next=chunk+static_cast<std::size_t>(normalized)+8;const auto [newfirst,newsecond]=index(remaining);
 const auto newhead=0x88+static_cast<std::size_t>(newfirst)*256+newsecond*8;
 // Unlink singleton. This internal bitmap writes the head back to sentinel.
 put(s,0x18,base);put(s,0x10,base);put(s,head,base);put(s,0x20,std::uint32_t{0});put(s,0x24+first*4,std::uint32_t{0});
 put(s,chunk+8,normalized);put(s,next,token);put(s,next+8,remaining|1);
 put(s,chunk+static_cast<std::size_t>(size)+8,base+next);s[chunk+static_cast<std::size_t>(size)+16]|=std::byte{2};
 put(s,next+16,base);put(s,next+24,base);put(s,0x18,base+next);put(s,newhead,base+next);
 put(s,0x20,std::uint32_t{1}<<newfirst);put(s,0x24+newfirst*4,std::uint32_t{1}<<newsecond);
 return {token+16,s.subspan(chunk+16,static_cast<std::size_t>(request))};
}
BitmapAllocation allocate_normal_bitmap_large_pair(std::span<std::byte> s,SystemPoolBackend& b,std::uint64_t request,std::uint32_t alignment){
 if(s.size()!=192||request<256||request>384||alignment!=8)throw std::invalid_argument("Concrete normal129..1024/align8 required");
 auto get=[&](std::size_t off){std::uint64_t v;std::memcpy(&v,s.data()+off,8);return v;};
 auto put=[&](std::size_t off,std::uint64_t v){std::memcpy(s.data()+off,&v,8);};
 const auto bytes=b.memory.bytes();const auto base=static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(bytes.data()));
 const auto address=static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(s.data()));std::uint32_t type;std::memcpy(&type,s.data()+0x1c,4);
 if(address<base||address-base>bytes.size()||s.size()>bytes.size()-(address-base)||get(0)!=0x025baa70||get(0x10)||type!=1||s[0x18]!=std::byte{1}||s[0x4b]!=std::byte{0})
  throw std::invalid_argument("Actual activated derived type1 debug0 required");
 const auto header=get(0xa8);
 if(header!=get(0x20)||get(0x28)!=8192||header<base||header-base>bytes.size()||8192>bytes.size()-(header-base))throw std::invalid_argument("Actual8192 bitmap resource required");
 if(get(0x50)>1||(get(0x50)==1&&request!=256))throw std::invalid_argument("Exactly two declared normal allocations required");
 auto resource=bytes.subspan(static_cast<std::size_t>(header-base),8192);const auto initialmarker=ctr::get<std::uint64_t>(resource,0x1988);
 if(get(0x50)==0){if(initialmarker!=1641||get(0x60)||get(0x70)!=0x1998)throw std::invalid_argument("Fresh zero-count resource required");}
 else {const auto prior=bitmap_requested_size_large(resource,header+0x1990);if(prior<256||prior>384||(initialmarker&3)!=0||get(0x60)!=prior||get(0x70)!=(0x1998+initialmarker+8))throw std::invalid_argument("Exact first large allocation and statistics required");}
 const auto allocation=allocate_bitmap_fresh_large(bytes.subspan(static_cast<std::size_t>(header-base),8192),(request+15)&~std::uint64_t{7},alignment);
 std::uint64_t marker;std::memcpy(&marker,bytes.data()+allocation.address-base-8,8);const auto charged=(marker+8)&~std::uint64_t{3};
 {const auto footer=(allocation.address+(marker&~std::uint64_t{3})-8)&~std::uint64_t{7};std::memcpy(bytes.data()+footer-base,&request,8);}
 const auto occupied=get(0x70)+charged;put(0x70,occupied);put(0x78,std::max(get(0x78),occupied));
 const auto requested=get(0x60)+request;put(0x60,requested);put(0x68,std::max(get(0x68),requested));
 const auto count=get(0x50)+1;put(0x50,count);put(0x58,std::max(get(0x58),count));return {allocation.address,allocation.payload.first(static_cast<std::size_t>(request))};
}
std::uint32_t free_normal_bitmap_large_pair(std::span<std::byte> pool,SystemPoolBackend& b,std::uint64_t pointer){
 if(pool.size()!=192)throw std::invalid_argument("Actual derived192 required");auto bytes=b.memory.bytes();
 const auto base=static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(bytes.data())),object=static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(pool.data()));
 if(object<base||object-base>bytes.size()||192>bytes.size()-(object-base)||get<std::uint64_t>(pool,0)!=0x025baa70||get<std::uint64_t>(pool,0x10)||get<std::uint32_t>(pool,0x1c)!=1||pool[0x18]!=std::byte{1}||pool[0x4b]!=std::byte{0})throw std::invalid_argument("Concrete activated owner0/debug0 derived required");
 const auto header=get<std::uint64_t>(pool,0xa8);
 if(header!=get<std::uint64_t>(pool,0x20)||get<std::uint64_t>(pool,0x28)!=8192||header<base||header-base>bytes.size()||8192>bytes.size()-(header-base))throw std::invalid_argument("Actual8192 bitmap resource required");
 auto s=bytes.subspan(static_cast<std::size_t>(header-base),8192);const auto requested=bitmap_requested_size_large(s,pointer);
 const auto chunk=static_cast<std::size_t>(pointer-header)-16;const auto marker=get<std::uint64_t>(s,chunk+8),size=marker&~std::uint64_t{3};
 if(size<264||size>392||size>s.size()-chunk-40)throw std::invalid_argument("Bounded normal small allocated chunk required");
 const auto next=chunk+static_cast<std::size_t>(size)+8;const auto nextmarker=get<std::uint64_t>(s,next+8),nextsize=nextmarker&~std::uint64_t{3};
 const bool backward=(marker&3)==2;
 std::size_t previous=chunk,large=next;std::uint64_t previoussize=0;
 if(backward){
  const auto token=get<std::uint64_t>(s,chunk);
  if(token<header+0x1980||token>=header+chunk||token&7)throw std::invalid_argument("Previous free footer outside span");
  previous=static_cast<std::size_t>(token-header);const auto prevmarker=get<std::uint64_t>(s,previous+8);previoussize=prevmarker&~std::uint64_t{3};
  if((prevmarker&3)!=1||previoussize<264||previoussize>392||previous+previoussize+8!=chunk)throw std::invalid_argument("Adjacent small previous singleton required");
 }else{
  if((marker&3)!=0||(nextmarker&3)!=0||nextsize!=264||nextsize>s.size()-next-40)throw std::invalid_argument("Penultimate allocated neighbor required");
  large=next+static_cast<std::size_t>(nextsize)+8;
 }
 const auto largemarker=get<std::uint64_t>(s,large+8),largesize=largemarker&~std::uint64_t{3};
 if((largemarker&3)!=1||largesize<=255||largesize>s.size()-large-24||get<std::uint64_t>(s,large+16)!=header||get<std::uint64_t>(s,large+24)!=header)throw std::invalid_argument("Singleton large tail required");
 const auto [first,second]=index(largesize);const auto largehead=0x88+first*256+second*8;const auto [smallgroup,smallindex]=index(backward?previoussize:size);const auto smallhead=0x88+smallgroup*256+smallindex*8;
 if((first!=2&&first!=3)||smallgroup!=1||get<std::uint32_t>(s,0x20)!=((1u<<first)|(backward?(1u<<smallgroup):0u))||get<std::uint32_t>(s,0x24+first*4)!=(1u<<second)||get<std::uint32_t>(s,0x24+smallgroup*4)!=(backward?(1u<<smallindex):0u)||get<std::uint64_t>(s,largehead)!=header+large)throw std::invalid_argument("Declared small/large bitmap maps required");
 for(unsigned i=0;i<25;++i)if(i!=first&&i!=smallgroup&&get<std::uint32_t>(s,0x24+i*4))throw std::invalid_argument("Other groups unsupported");
 for(std::size_t off=0x88;off<0x1988;off+=8)if(off!=largehead&&!(backward&&off==smallhead)&&get<std::uint64_t>(s,off)!=header)throw std::invalid_argument("Other bins unsupported");
 if(backward&&(get<std::uint64_t>(s,smallhead)!=header+previous||get<std::uint64_t>(s,previous+16)!=header||get<std::uint64_t>(s,previous+24)!=header))throw std::invalid_argument("Previous small singleton links required");
 const auto charge=size+8,count=get<std::uint64_t>(pool,0x50),totalrequest=get<std::uint64_t>(pool,0x60),occupied=get<std::uint64_t>(pool,0x70);
 const auto merged=previoussize+size+largesize+16;const auto [newfirst,newsecond]=index(merged);
 if(!count||charge>occupied||requested>totalrequest||(backward&&newfirst!=3))throw std::invalid_argument("Consistent normal statistics/merge required");
 if((!backward&&(chunk!=0x1980||count!=2))||(backward&&(previous!=0x1980||count!=1||size!=264)))throw std::invalid_argument("Exact two-large prefix state required");
 // All supported shape and link checks precede the first original store.
 put(s,next,header+chunk);s[next+8]|=std::byte{2};put(s,chunk+8,marker|1);
 if(!backward){
  put(s,chunk+16,header);put(s,chunk+24,header);put(s,0x18,header+chunk);put(s,smallhead,header+chunk);
  put(s,0x20,(1u<<first)|(1u<<smallgroup));put(s,0x24+smallgroup*4,1u<<smallindex);
 }else{
  put(s,0x18,header);put(s,0x10,header);put(s,smallhead,header);put(s,0x24+smallgroup*4,0u);put(s,0x20,1u<<first);
  put(s,previous+8,(previoussize+size+8)|1);put(s,next,header+previous);
  put(s,0x18,header);put(s,0x10,header);put(s,largehead,header);put(s,0x24+first*4,0u);put(s,0x20,0u);
  put(s,previous+8,merged|1);put(s,previous+static_cast<std::size_t>(merged)+8,header+previous);
  put(s,previous+16,header);put(s,previous+24,header);put(s,0x18,header+previous);put(s,0x88+newfirst*256+newsecond*8,header+previous);
  put(s,0x20,1u<<newfirst);put(s,0x24+newfirst*4,1u<<newsecond);
 }
 put(pool,0x70,occupied-charge);put(pool,0x78,std::max(get<std::uint64_t>(pool,0x78),occupied-charge));
 put(pool,0x60,totalrequest-requested);put(pool,0x68,std::max(get<std::uint64_t>(pool,0x68),totalrequest-requested));put(pool,0x50,count-1);return 0;
}
}
