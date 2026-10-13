#include "ctr/bitmap_normal_free.h"
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
std::uint32_t free_last_normal_derived_bitmap_pool(std::span<std::byte> pool,SystemPoolBackend& b,std::uint64_t pointer){
 if(pool.size()!=192)throw std::invalid_argument("Actual derived192 required");
 auto bytes=b.memory.bytes();const auto base=static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(bytes.data()));
 const auto object=static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(pool.data()));
 if(object<base||object-base>bytes.size()||192>bytes.size()-(object-base)||get<std::uint64_t>(pool,0)!=0x025baa70||get<std::uint64_t>(pool,0x10)||get<std::uint32_t>(pool,0x1c)!=1||pool[0x18]!=std::byte{1}||pool[0x4b]!=std::byte{0})throw std::invalid_argument("Concrete activated owner0/debug0 derived required");
 const auto header=get<std::uint64_t>(pool,0xa8);
 if(header!=get<std::uint64_t>(pool,0x20)||get<std::uint64_t>(pool,0x28)!=8192||header<base||header-base>bytes.size()||8192>bytes.size()-(header-base))throw std::invalid_argument("Actual8192 bitmap resource required");
 auto s=bytes.subspan(static_cast<std::size_t>(header-base),8192);
 const auto requested=bitmap_normal_requested_size(s,pointer);const auto chunk=static_cast<std::size_t>(pointer-header)-16;
 const auto marker=get<std::uint64_t>(s,chunk+8),size=marker&~std::uint64_t{3};
 if((marker&3)!=0||size<24||size>120||size>s.size()-chunk-40)throw std::invalid_argument("Normal allocated last chunk without backward free required");
 const auto next=chunk+static_cast<std::size_t>(size)+8;const auto nextmarker=get<std::uint64_t>(s,next+8),nextsize=nextmarker&~std::uint64_t{3};
 if((nextmarker&3)!=1||nextsize<=255||nextsize>s.size()-next-24||get<std::uint64_t>(s,next+16)!=header||get<std::uint64_t>(s,next+24)!=header)throw std::invalid_argument("Forward singleton large free chunk required");
 const auto [first,second]=index(nextsize);const auto head=0x88+first*256+second*8;
 if(first!=3||get<std::uint32_t>(s,0x20)!=(1u<<first)||get<std::uint32_t>(s,0x24+first*4)!=(1u<<second)||get<std::uint64_t>(s,head)!=header+next)throw std::invalid_argument("Consistent singleton group3 bitmap required");
 for(unsigned i=0;i<25;++i)if(i!=first&&get<std::uint32_t>(s,0x24+i*4))throw std::invalid_argument("Other groups unsupported");
 for(std::size_t off=0x88;off<0x1988;off+=8)if(off!=head&&get<std::uint64_t>(s,off)!=header)throw std::invalid_argument("Other bins unsupported");
 const auto merged=size+nextsize+8;const auto [newfirst,newsecond]=index(merged);const auto newhead=0x88+newfirst*256+newsecond*8;
 const auto charge=size+8,count=get<std::uint64_t>(pool,0x50),totalrequest=get<std::uint64_t>(pool,0x60),occupied=get<std::uint64_t>(pool,0x70);
 if(newfirst!=3||!count||requested>totalrequest||charge>occupied)throw std::invalid_argument("Consistent normal pool statistics required");
 // Preserve original stores into the old next chunk even though it becomes interior memory.
 put(s,next,header+chunk);s[next+8]|=std::byte{2};put(s,chunk+8,marker|1);
 put(s,0x18,header);put(s,0x10,header);put(s,head,header);put(s,0x20,std::uint32_t{0});put(s,0x24+first*4,std::uint32_t{0});
 put(s,chunk+8,merged|1);put(s,chunk+static_cast<std::size_t>(merged)+8,header+chunk);
 put(s,chunk+16,header);put(s,chunk+24,header);put(s,0x18,header+chunk);put(s,newhead,header+chunk);
 put(s,0x20,1u<<newfirst);put(s,0x24+newfirst*4,1u<<newsecond);
 put(pool,0x70,occupied-charge);put(pool,0x78,std::max(get<std::uint64_t>(pool,0x78),occupied-charge));
 put(pool,0x60,totalrequest-requested);put(pool,0x68,std::max(get<std::uint64_t>(pool,0x68),totalrequest-requested));put(pool,0x50,count-1);
 return 0;
}
}
