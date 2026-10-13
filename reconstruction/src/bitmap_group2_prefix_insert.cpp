#include "ctr/bitmap_group2_prefix_insert.h"
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
 if(first!=2&&first!=3)throw std::invalid_argument("Verified large group3 required");
 const auto secondmap=get<std::uint32_t>(s,0x24+first*4);
 if(!secondmap||std::popcount(secondmap)!=1)throw std::invalid_argument("Exactly one free bin required");
 for(std::size_t i=0;i<25;++i)if(i!=first&&get<std::uint32_t>(s,0x24+i*4))throw std::invalid_argument("Other free groups unsupported");
 const auto second=static_cast<std::uint32_t>(std::countr_zero(secondmap));const auto head=0x88+static_cast<std::size_t>(first)*256+second*8;
 const auto token=get<std::uint64_t>(s,head);
 if(token<base+0x1980||token-base>s.size()-32||(token&7))throw std::invalid_argument("Bitmap free chunk outside resource");
 const auto prefix=get<std::uint64_t>(s,0x1988);const auto prefixsize=prefix&~std::uint64_t{3};if(token!=base+0x1980&&((prefix&3)!=0||prefixsize<520||prefixsize>648||token!=base+0x1980+prefixsize+8))throw std::invalid_argument("Exact second adjacent large chunk required");
 const auto chunk=static_cast<std::size_t>(token-base);const auto marker=get<std::uint64_t>(s,chunk+8),size=marker&~std::uint64_t{3};
 const auto normalized=std::max(std::uint64_t{24},(request+7)&~std::uint64_t{7});
 if(marker!=((token==base+0x1980?1640:1640-prefixsize-8)|1)||size>s.size()-chunk-24||size<normalized+32||size-normalized-8<=255||get<std::uint64_t>(s,chunk+16)!=base||get<std::uint64_t>(s,chunk+24)!=base||index(size)!=std::pair{first,second})
  throw std::invalid_argument("Consistent singleton large free chunk and split required");
 for(std::size_t off=0x88;off<0x1988;off+=8)if(off!=head&&get<std::uint64_t>(s,off)!=base)throw std::invalid_argument("Other free heads unsupported");
 if(get<std::uint64_t>(s,0x1ff0)!=token||get<std::uint64_t>(s,0x1ff8)!=2||get<std::uint64_t>(s,0x10)!=base||get<std::uint64_t>(s,0x18)!=token)throw std::invalid_argument("Owned tail and terminal guard required");
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
BitmapAllocation allocate_normal_bitmap_group2_pair(std::span<std::byte> s,SystemPoolBackend& b,std::uint64_t request,std::uint32_t alignment){
 if(s.size()!=192||(request!=256&&(request<512||request>640))||alignment!=8)throw std::invalid_argument("Concrete normal129..1024/align8 required");
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
 if(get(0x50)==0){if(request<512||request>640)throw std::invalid_argument("First512..640 required");if(initialmarker!=1641||get(0x60)||get(0x70)!=0x1998)throw std::invalid_argument("Fresh zero-count resource required");}
 else {const auto prior=bitmap_requested_size_large(resource,header+0x1990);if(prior<512||prior>640||(initialmarker&3)!=0||get(0x60)!=prior||get(0x70)!=(0x1998+initialmarker+8))throw std::invalid_argument("Exact first large allocation and statistics required");}
 const auto allocation=allocate_bitmap_fresh_large(bytes.subspan(static_cast<std::size_t>(header-base),8192),(request+15)&~std::uint64_t{7},alignment);
 std::uint64_t marker;std::memcpy(&marker,bytes.data()+allocation.address-base-8,8);const auto charged=(marker+8)&~std::uint64_t{3};
 {const auto footer=(allocation.address+(marker&~std::uint64_t{3})-8)&~std::uint64_t{7};std::memcpy(bytes.data()+footer-base,&request,8);}
 const auto occupied=get(0x70)+charged;put(0x70,occupied);put(0x78,std::max(get(0x78),occupied));
 const auto requested=get(0x60)+request;put(0x60,requested);put(0x68,std::max(get(0x68),requested));
 const auto count=get(0x50)+1;put(0x50,count);put(0x58,std::max(get(0x58),count));return {allocation.address,allocation.payload.first(static_cast<std::size_t>(request))};
}
std::uint32_t free_normal_bitmap_group2_prefix(std::span<std::byte> pool,SystemPoolBackend& b,std::uint64_t pointer){
 if(pool.size()!=192)throw std::invalid_argument("Actual derived192 required");auto bytes=b.memory.bytes();
 const auto base=static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(bytes.data())),object=static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(pool.data()));
 if(object<base||object-base>bytes.size()||192>bytes.size()-(object-base)||get<std::uint64_t>(pool,0)!=0x025baa70||get<std::uint64_t>(pool,0x10)||get<std::uint32_t>(pool,0x1c)!=1||pool[0x18]!=std::byte{1}||pool[0x4b]!=std::byte{0})throw std::invalid_argument("Actual activated derived owner required");
 const auto header=get<std::uint64_t>(pool,0xa8);
 if(header!=get<std::uint64_t>(pool,0x20)||get<std::uint64_t>(pool,0x28)!=8192||header<base||header-base>bytes.size()||8192>bytes.size()-(header-base))throw std::invalid_argument("Owned8192 resource required");
 auto s=bytes.subspan(static_cast<std::size_t>(header-base),8192);
 if(pointer!=header+0x1990)throw std::invalid_argument("Only first prefix free supported");
 const auto requested=bitmap_requested_size_large(s,pointer),size=get<std::uint64_t>(s,0x1988);
 if(requested<512||requested>640||size!=((requested+15)&~std::uint64_t{7}))throw std::invalid_argument("Declared first512..640 required");
 const auto next=0x1980+static_cast<std::size_t>(size)+8;
 if(get<std::uint64_t>(s,next+8)!=264||bitmap_requested_size_large(s,header+next+16)!=256)throw std::invalid_argument("Actual second256 neighbor required");
 const auto tail=next+272,tailmarker=get<std::uint64_t>(s,tail+8),tailsize=tailmarker&~std::uint64_t{3};
 if(tailmarker!=(1640-size-280|1)||get<std::uint64_t>(s,tail+16)!=header||get<std::uint64_t>(s,tail+24)!=header||get<std::uint64_t>(s,0x10)!=header||get<std::uint64_t>(s,0x18)!=header+tail||get<std::uint64_t>(s,0x1ff0)!=header+tail||get<std::uint64_t>(s,0x1ff8)!=2)throw std::invalid_argument("Owned tail singleton/guard required");
 const auto [group,bin]=index(size);const auto [tailgroup,tailbin]=index(tailsize);
 if(group!=2||tailgroup!=2||bin==tailbin||get<std::uint32_t>(s,0x20)!=4||get<std::uint32_t>(s,0x2c)!=(1u<<tailbin))throw std::invalid_argument("Distinct bins within group2 required");
 for(unsigned i=0;i<25;++i)if(i!=2&&get<std::uint32_t>(s,0x24+i*4))throw std::invalid_argument("Other groups unsupported");
 const auto head=0x88+2*256+bin*8,tailhead=0x88+2*256+tailbin*8;
 for(std::size_t off=0x88;off<0x1988;off+=8)if(get<std::uint64_t>(s,off)!=(off==tailhead?header+tail:header))throw std::invalid_argument("All actual bin heads required");
 const auto occupied=0x1998+size+8+272;
 if(get<std::uint64_t>(pool,0x50)!=2||get<std::uint64_t>(pool,0x58)!=2||get<std::uint64_t>(pool,0x60)!=requested+256||get<std::uint64_t>(pool,0x68)!=requested+256||get<std::uint64_t>(pool,0x70)!=occupied||get<std::uint64_t>(pool,0x78)!=occupied)throw std::invalid_argument("Exact two-live statistics required");
 // All owned shape checks precede mutation. Preserve the other bin in this group.
 put(s,next,header+0x1980);put(s,next+8,std::uint64_t{266});put(s,0x1988,size|1);
 put(s,0x1990,header);put(s,0x1998,header);put(s,0x18,header+0x1980);put(s,head,header+0x1980);
 put(s,0x2c,(1u<<tailbin)|(1u<<bin));
 put(pool,0x70,std::uint64_t{0x1998+272});put(pool,0x60,std::uint64_t{256});put(pool,0x50,std::uint64_t{1});return 0;
}
}
