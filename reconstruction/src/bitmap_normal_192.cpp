#include "ctr/bitmap_normal_192.h"
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
static BitmapAllocation allocate_bitmap_singleton_200(std::span<std::byte> s,std::uint64_t request,std::uint32_t alignment){
 if(s.size()!=8192||reinterpret_cast<std::uintptr_t>(s.data())&7||request!=200||alignment!=8)
  throw std::invalid_argument("Bitmap singleton exact200/align8/8192 contract required");
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
 const auto chunk=static_cast<std::size_t>(token-base);const auto marker=get<std::uint64_t>(s,chunk+8),size=marker&~std::uint64_t{3};
 const auto normalized=std::max(std::uint64_t{24},(request+7)&~std::uint64_t{7});
 if((marker&3)!=1||size>s.size()-chunk-24||size<normalized+32||size-normalized-8<=255||get<std::uint64_t>(s,chunk+16)!=base||get<std::uint64_t>(s,chunk+24)!=base||index(size)!=std::pair{first,second})
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
BitmapAllocation allocate_normal_bitmap_192(std::span<std::byte> s,SystemPoolBackend& b,std::uint64_t request,std::uint32_t alignment){
 if(s.size()!=192||request!=192||alignment!=8)throw std::invalid_argument("Concrete bitmap pool positive1..128/align8 contract required");
 auto get=[&](std::size_t off){std::uint64_t v;std::memcpy(&v,s.data()+off,8);return v;};
 auto put=[&](std::size_t off,std::uint64_t v){std::memcpy(s.data()+off,&v,8);};
 const auto bytes=b.memory.bytes();const auto base=static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(bytes.data()));
 const auto address=static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(s.data()));std::uint32_t type;std::memcpy(&type,s.data()+0x1c,4);
 if(address<base||address-base>bytes.size()||s.size()>bytes.size()-(address-base)||get(0)!=0x025baa70||get(0x10)||type!=1||s[0x18]!=std::byte{1}||s[0x4b]!=std::byte{0})
  throw std::invalid_argument("Actual activated derived type1 debug0 required");
 const auto header=get(0xa8);
 if(header!=get(0x20)||get(0x28)!=8192||header<base||header-base>bytes.size()||8192>bytes.size()-(header-base))throw std::invalid_argument("Actual8192 bitmap resource required");
 const auto allocation=allocate_bitmap_singleton_200(bytes.subspan(static_cast<std::size_t>(header-base),8192),200,alignment);
 std::uint64_t marker;std::memcpy(&marker,bytes.data()+allocation.address-base-8,8);const auto charged=(marker+8)&~std::uint64_t{3};
 {const auto footer=(allocation.address+(marker&~std::uint64_t{3})-8)&~std::uint64_t{7};std::memcpy(bytes.data()+footer-base,&request,8);}
 const auto occupied=get(0x70)+charged;put(0x70,occupied);put(0x78,std::max(get(0x78),occupied));
 const auto requested=get(0x60)+request;put(0x60,requested);put(0x68,std::max(get(0x68),requested));
 const auto count=get(0x50)+1;put(0x50,count);put(0x58,std::max(get(0x58),count));return {allocation.address,allocation.payload.first(static_cast<std::size_t>(request))};
}
std::uint64_t bitmap_requested_size_192(std::span<const std::byte> s,std::uint64_t pointer){
 const auto base=static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(s.data()));
 if(s.size()!=8192||pointer<base+0x1990||pointer-base>s.size()-8||pointer&7)throw std::invalid_argument("Actual positive normal bitmap pointer required");
 std::uint64_t marker;std::memcpy(&marker,s.data()+pointer-base-8,8);const auto size=marker&~std::uint64_t{3};
 if(marker&1||size!=200||size>s.size()-(pointer-base))throw std::invalid_argument("Bounded allocated bitmap marker required");
 const auto footer=(pointer+size-8)&~std::uint64_t{7};std::uint64_t requested;std::memcpy(&requested,s.data()+footer-base,8);
 if(requested!=192)throw std::invalid_argument("Real normal requested-size footer required");return requested;
}
std::uint32_t free_last_normal_bitmap_192(std::span<std::byte> pool,SystemPoolBackend& b,std::uint64_t pointer){
 if(pool.size()!=192)throw std::invalid_argument("Actual derived192 required");
 auto bytes=b.memory.bytes();const auto base=static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(bytes.data()));
 const auto object=static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(pool.data()));
 if(object<base||object-base>bytes.size()||192>bytes.size()-(object-base)||get<std::uint64_t>(pool,0)!=0x025baa70||get<std::uint64_t>(pool,0x10)||get<std::uint32_t>(pool,0x1c)!=1||pool[0x18]!=std::byte{1}||pool[0x4b]!=std::byte{0})throw std::invalid_argument("Concrete activated owner0/debug0 derived required");
 const auto header=get<std::uint64_t>(pool,0xa8);
 if(header!=get<std::uint64_t>(pool,0x20)||get<std::uint64_t>(pool,0x28)!=8192||header<base||header-base>bytes.size()||8192>bytes.size()-(header-base))throw std::invalid_argument("Actual8192 bitmap resource required");
 auto s=bytes.subspan(static_cast<std::size_t>(header-base),8192);
 const auto requested=bitmap_requested_size_192(s,pointer);const auto chunk=static_cast<std::size_t>(pointer-header)-16;
 const auto marker=get<std::uint64_t>(s,chunk+8),size=marker&~std::uint64_t{3};
 if((marker&3)!=0||size!=200||size>s.size()-chunk-40)throw std::invalid_argument("Normal allocated last chunk without backward free required");
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
