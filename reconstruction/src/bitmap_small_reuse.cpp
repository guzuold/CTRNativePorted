#include "ctr/bitmap_small_reuse.h"
#include <algorithm>
#include <bit>
#include <cstring>
#include <stdexcept>
namespace ctr {
namespace {
template<class T> T get(std::span<std::byte> s,std::size_t off){T v;std::memcpy(&v,s.data()+off,sizeof(v));return v;}
template<class T> void put(std::span<std::byte> s,std::size_t off,T v){std::memcpy(s.data()+off,&v,sizeof(v));}
}
BitmapAllocation reuse_normal_bitmap_exact_small(std::span<std::byte> pool,SystemPoolBackend& b,std::uint64_t request,std::uint32_t alignment){
 if(pool.size()!=192||!request||request>112||alignment!=8)throw std::invalid_argument("Normal exact-small positive1..112 align8 required");
 auto bytes=b.memory.bytes();const auto base=static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(bytes.data())),object=static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(pool.data()));
 if(object<base||object-base>bytes.size()||192>bytes.size()-(object-base)||get<std::uint64_t>(pool,0)!=0x025baa70||get<std::uint64_t>(pool,0x10)||get<std::uint32_t>(pool,0x1c)!=1||pool[0x18]!=std::byte{1}||pool[0x4b]!=std::byte{0})throw std::invalid_argument("Concrete activated owner0/debug0 derived required");
 const auto header=get<std::uint64_t>(pool,0xa8);
 if(header!=get<std::uint64_t>(pool,0x20)||get<std::uint64_t>(pool,0x28)!=8192||header<base||header-base>bytes.size()||8192>bytes.size()-(header-base))throw std::invalid_argument("Actual8192 bitmap resource required");
 auto s=bytes.subspan(static_cast<std::size_t>(header-base),8192);const auto normalized=std::max(std::uint64_t{24},(request+15)&~std::uint64_t{7});
 const auto second=static_cast<unsigned>(normalized>>3),smallhead=0x88+second*8;
 const auto largemap=get<std::uint32_t>(s,0x30);
 if(get<std::uint32_t>(s,0x20)!=9||get<std::uint32_t>(s,0x24)!=(1u<<second)||std::popcount(largemap)!=1)throw std::invalid_argument("Exact singleton small/group3 bins required");
 const auto largesecond=static_cast<unsigned>(std::countr_zero(largemap));const auto largehead=0x388+largesecond*8;
 const auto token=get<std::uint64_t>(s,smallhead);
 if(token<header+0x1980||token-header>s.size()-40||token&7)throw std::invalid_argument("Small free head outside resource");
 const auto chunk=static_cast<std::size_t>(token-header);const auto marker=get<std::uint64_t>(s,chunk+8);
 if(marker!=(normalized|1)||get<std::uint64_t>(s,chunk+16)!=header||get<std::uint64_t>(s,chunk+24)!=header||normalized>s.size()-chunk-40)throw std::invalid_argument("Exact small singleton size/links required");
 const auto next=chunk+static_cast<std::size_t>(normalized)+8;const auto nextmarker=get<std::uint64_t>(s,next+8),nextsize=nextmarker&~std::uint64_t{3};
 if((nextmarker&3)!=2||nextsize<24||nextsize>120||nextsize>s.size()-next-40||get<std::uint64_t>(s,next)!=token)throw std::invalid_argument("Allocated final neighbor with original previous-free footer required");
 const auto large=next+static_cast<std::size_t>(nextsize)+8;const auto largemarker=get<std::uint64_t>(s,large+8),largesize=largemarker&~std::uint64_t{3};
 if((largemarker&3)!=1||largesize<=255||largesize>s.size()-large-24||get<std::uint64_t>(s,largehead)!=header+large||get<std::uint64_t>(s,large+16)!=header||get<std::uint64_t>(s,large+24)!=header)throw std::invalid_argument("Consistent large singleton tail required");
 const auto msb=31u-static_cast<unsigned>(std::countl_zero(static_cast<std::uint32_t>(largesize)));
 if(msb-7!=3||static_cast<unsigned>((largesize>>(msb-5))^0x20)!=largesecond)throw std::invalid_argument("Consistent large index required");
 for(unsigned i=1;i<25;++i)if(i!=3&&get<std::uint32_t>(s,0x24+i*4))throw std::invalid_argument("Other groups unsupported");
 for(std::size_t off=0x88;off<0x1988;off+=8)if(off!=smallhead&&off!=largehead&&get<std::uint64_t>(s,off)!=header)throw std::invalid_argument("Other bins unsupported");
 // Exact-fit original priority/unlink and no-split tail; the large bin is preserved.
 put(s,0x18,header);put(s,0x10,header);put(s,smallhead,header);put(s,0x24,0u);put(s,0x20,8u);
 s[next+8]&=std::byte{0xfd};s[chunk+8]&=std::byte{0xfe};
 const auto address=token+16,footer=(address+normalized-8)&~std::uint64_t{7};put(s,static_cast<std::size_t>(footer-header),request);
 const auto occupied=get<std::uint64_t>(pool,0x70)+normalized+8;put(pool,0x70,occupied);put(pool,0x78,std::max(get<std::uint64_t>(pool,0x78),occupied));
 const auto requested=get<std::uint64_t>(pool,0x60)+request;put(pool,0x60,requested);put(pool,0x68,std::max(get<std::uint64_t>(pool,0x68),requested));
 const auto count=get<std::uint64_t>(pool,0x50)+1;put(pool,0x50,count);put(pool,0x58,std::max(get<std::uint64_t>(pool,0x58),count));
 return {address,s.subspan(chunk+16,static_cast<std::size_t>(request))};
}
}
