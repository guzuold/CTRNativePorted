#include "ctr/bitmap_group2_large_remainder_second_free.h"
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
std::uint32_t free_normal_bitmap_group2_large_remainder_second(std::span<std::byte> pool,SystemPoolBackend& b,std::uint64_t pointer){
 if(pool.size()!=192)throw std::invalid_argument("Actual derived192 required");auto bytes=b.memory.bytes();
 const auto base=static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(bytes.data())),object=static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(pool.data()));
 if(object<base||object-base>bytes.size()||192>bytes.size()-(object-base)||get<std::uint64_t>(pool,0)!=0x025baa70||get<std::uint64_t>(pool,0x10)||get<std::uint32_t>(pool,0x1c)!=1||pool[0x18]!=std::byte{1}||pool[0x4b]!=std::byte{0})throw std::invalid_argument("Actual activated derived owner required");
 const auto header=get<std::uint64_t>(pool,0xa8);
 if(header!=get<std::uint64_t>(pool,0x20)||get<std::uint64_t>(pool,0x28)!=8192||header<base||header-base>bytes.size()||8192>bytes.size()-(header-base))throw std::invalid_argument("Owned8192 resource required");
 auto s=bytes.subspan(static_cast<std::size_t>(header-base),8192);
 const auto prefixmarker=get<std::uint64_t>(s,0x1988),size=prefixmarker&~std::uint64_t{3};
 if(prefixmarker!=(size|1)||size<520||size>648)throw std::invalid_argument("Genuine free group2 prefix required");
 const auto next=0x1980+static_cast<std::size_t>(size)+8;
 if(pointer!=header+next+16||get<std::uint64_t>(s,next)!=header+0x1980||get<std::uint64_t>(s,next+8)!=266||bitmap_requested_size_large(s,pointer)!=256)throw std::invalid_argument("Only owned second256 with backward-free prefix supported");
 const auto tail=next+272,tailmarker=get<std::uint64_t>(s,tail+8),tailsize=tailmarker&~std::uint64_t{3};
 if(tailmarker!=((1640-size-280)|1)||get<std::uint64_t>(s,tail+16)!=header||get<std::uint64_t>(s,tail+24)!=header||get<std::uint64_t>(s,0x1990)!=header||get<std::uint64_t>(s,0x1998)!=header||get<std::uint64_t>(s,0x10)!=header||get<std::uint64_t>(s,0x18)!=header+0x1980||get<std::uint64_t>(s,0x1ff0)!=header+tail||get<std::uint64_t>(s,0x1ff8)!=2)throw std::invalid_argument("Two owned singleton links and terminal guard required");
 const auto [group,bin]=index(size);const auto [tailgroup,tailbin]=index(tailsize);
 if(group!=2||tailgroup!=2||bin==tailbin||get<std::uint32_t>(s,0x20)!=4||get<std::uint32_t>(s,0x2c)!=((1u<<tailbin)|(1u<<bin)))throw std::invalid_argument("Distinct bins within group2 required");
 for(unsigned i=0;i<25;++i)if(i!=2&&get<std::uint32_t>(s,0x24+i*4))throw std::invalid_argument("Other groups unsupported");
 const auto head=0x88+2*256+bin*8,tailhead=0x88+2*256+tailbin*8;
 for(std::size_t off=0x88;off<0x1988;off+=8)if(get<std::uint64_t>(s,off)!=(off==tailhead?header+tail:(off==head?header+0x1980:header)))throw std::invalid_argument("All actual bin heads required");
 const auto occupied=std::uint64_t{0x1998+272},peak=get<std::uint64_t>(pool,0x68);
 if(get<std::uint64_t>(pool,0x50)!=1||get<std::uint64_t>(pool,0x58)!=3||get<std::uint64_t>(pool,0x60)!=256||peak<768||peak>896||((peak-256+15)&~std::uint64_t{7})<size||get<std::uint64_t>(pool,0x70)!=occupied||get<std::uint64_t>(pool,0x78)!=0x1998+((peak-256+15)&~std::uint64_t{7})+280)throw std::invalid_argument("Remainder-cycle PEAK3 with coherent maximum pair statistics required");
 // Original 014db270->015c3a20 complete merge after genuine remainder consume/free and FIRST forward free (PEAK3 history).
 // Preserve stale interiors; unlink both original group2 singletons before merge.
 put(s,tail,header+next);put(s,tail+8,tailmarker|2);put(s,next+8,std::uint64_t{267});
 put(s,0x18,header);put(s,0x10,header);put(s,head,header);put(s,0x2c,1u<<tailbin);
 put(s,0x1988,(size+272)|1);put(s,tail,header+0x1980);
 put(s,0x18,header);put(s,0x10,header);put(s,tailhead,header);put(s,0x2c,0u);put(s,0x20,0u);
 put(s,0x1988,std::uint64_t{1641});put(s,0x1ff0,header+0x1980);
 put(s,0x1990,header);put(s,0x1998,header);put(s,0x18,header+0x1980);
 const auto [mergedgroup,mergedbin]=index(1640);put(s,0x88+mergedgroup*256+mergedbin*8,header+0x1980);put(s,0x20,1u<<mergedgroup);put(s,0x24+mergedgroup*4,1u<<mergedbin);
 put(pool,0x70,std::uint64_t{0x1998});put(pool,0x60,std::uint64_t{0});put(pool,0x50,std::uint64_t{0});return 0;
}
}
