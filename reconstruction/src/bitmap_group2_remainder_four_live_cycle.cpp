#include "ctr/bitmap_group2_remainder_four_live_cycle.h"
#include <algorithm>
#include <bit>
#include <cstring>
#include <stdexcept>
// Original 014db6b0 NORMAL allocation, 014db250 getter, 014db270 NORMAL free -> 015c3a20 bitmap free.
// Write sets derived from genuine original snapshots (bitmap_group2_remainder_four_live_cycle_original.bin).
namespace ctr {
namespace {
template<class T>T get(std::span<const std::byte> s,std::size_t n){T v;std::memcpy(&v,s.data()+n,sizeof(v));return v;}
template<class T>void put(std::span<std::byte> s,std::size_t n,T v){std::memcpy(s.data()+n,&v,sizeof(v));}
auto bin(std::uint64_t n){auto m=31u-static_cast<unsigned>(std::countl_zero(static_cast<std::uint32_t>(n)));return std::pair{m-7,static_cast<unsigned>((n>>(m-5))^32)};}
std::size_t head_of(std::uint64_t n){const auto [g,i]=bin(n);return 0x88+static_cast<std::size_t>(g)*256+i*8;}
unsigned live_of(unsigned stage){return stage<=4?stage:8-stage;}
std::span<std::byte> owned_resource(std::span<std::byte> p,SystemPoolBackend& b,std::uint64_t expected){
 auto bytes=b.memory.bytes();const auto base=reinterpret_cast<std::uintptr_t>(bytes.data()),object=reinterpret_cast<std::uintptr_t>(p.data());
 if(p.size()!=192||object<base||object-base>bytes.size()||192>bytes.size()-(object-base)||get<std::uint64_t>(p,0)!=0x025baa70||get<std::uint64_t>(p,0x10)||get<std::uint32_t>(p,0x1c)!=1||p[0x18]!=std::byte{1}||p[0x4b]!=std::byte{0})throw std::invalid_argument("Activated same-creator derived owner required");
 const auto h=get<std::uint64_t>(p,0xa8);
 if((expected&&h!=expected)||h!=get<std::uint64_t>(p,0x20)||get<std::uint64_t>(p,0x28)!=8192||h<base||h-base>bytes.size()||8192>bytes.size()-(h-base))throw std::invalid_argument("Owned8192 resource required");
 return bytes.subspan(static_cast<std::size_t>(h-base),8192);
}
// Moves the singleton free tail from one bin to another, as the original does on split and merge.
void move_tail(std::span<std::byte> s,std::uint64_t h,std::uint64_t oldsize,std::size_t tail,std::uint64_t newsize){
 const auto [og,oi]=bin(oldsize);const auto [ng,ni]=bin(newsize);(void)oi;
 put(s,head_of(oldsize),h);put(s,0x24+static_cast<std::size_t>(og)*4,0u);
 put(s,head_of(newsize),h+tail);put(s,0x24+static_cast<std::size_t>(ng)*4,1u<<ni);put(s,0x20,1u<<ng);
 put(s,0x18,h+tail);put(s,0x1ff0,h+tail);
}
}
BitmapGroup2RemainderFourLiveCycle::BitmapGroup2RemainderFourLiveCycle(std::span<std::byte> p,SystemPoolBackend& b):pool_(p),backend_(&b){
 auto s=owned_resource(p,b,0);(void)s;resource_=get<std::uint64_t>(p,0xa8);
 peak_request_=get<std::uint64_t>(p,0x68);peak_occupied_=get<std::uint64_t>(p,0x78);
 if(get<std::uint64_t>(p,0x58)!=3||peak_request_<768||peak_request_>896||peak_occupied_!=0x1998+((peak_request_-256+15)&~std::uint64_t{7})+280)throw std::invalid_argument("Measured PEAK3 remainder-history pair peaks required");
 validate();
}
void BitmapGroup2RemainderFourLiveCycle::validate() const {
 auto s=owned_resource(pool_,*backend_,resource_);const auto h=resource_;const auto live=live_of(stage_);
 std::size_t off=0x1980;std::uint64_t requested=0,occupied=0x1998,remain=1640;
 for(unsigned k=0;k<live;++k){
  if(address_[k]!=h+off+16||get<std::uint64_t>(s,off+8)!=size_[k]||get<std::uint64_t>(s,off+static_cast<std::size_t>(size_[k])+8)!=request_[k]||bitmap_requested_size_large(s,address_[k])!=request_[k])throw std::invalid_argument("Actual captured live chain with footers required");
  off+=static_cast<std::size_t>(size_[k])+8;remain-=size_[k]+8;requested+=request_[k];occupied+=size_[k]+8;
 }
 if(remain<256||off+remain+8!=0x1ff0||get<std::uint64_t>(s,off+8)!=(remain|1)||get<std::uint64_t>(s,off+16)!=h||get<std::uint64_t>(s,off+24)!=h||get<std::uint64_t>(s,0x1ff0)!=h+off||get<std::uint64_t>(s,0x1ff8)!=2||get<std::uint64_t>(s,0x10)!=h||get<std::uint64_t>(s,0x18)!=h+off)throw std::invalid_argument("Actual singleton free tail and terminal guard required");
 const auto [g,i]=bin(remain);const auto head=head_of(remain);
 if(get<std::uint32_t>(s,0x20)!=(1u<<g))throw std::invalid_argument("Actual tail group map required");
 for(unsigned j=0;j<25;++j)if(get<std::uint32_t>(s,0x24+j*4)!=(j==g?1u<<i:0u))throw std::invalid_argument("Actual tail bin map required");
 for(std::size_t j=0x88;j<0x1988;j+=8)if(get<std::uint64_t>(s,j)!=(j==head?h+off:h))throw std::invalid_argument("All actual bin heads required");
 // Peaks: retained history maxima combined with this cycle's running maxima, never reset.
 const unsigned reached=std::min(stage_,4u);std::uint64_t maxrequest=0,maxoccupied=0x1998;
 for(unsigned k=0;k<reached;++k){maxrequest+=request_[k];maxoccupied+=size_[k]+8;}
 const auto p=pool_;
 if(get<std::uint64_t>(p,0x50)!=live||get<std::uint64_t>(p,0x60)!=requested||get<std::uint64_t>(p,0x70)!=occupied||get<std::uint64_t>(p,0x58)!=std::max<std::uint64_t>(3,reached)||get<std::uint64_t>(p,0x68)!=std::max(peak_request_,maxrequest)||get<std::uint64_t>(p,0x78)!=std::max(peak_occupied_,reached?maxoccupied:0))throw std::invalid_argument("Measured current and retained peak statistics required");
}
BitmapAllocation BitmapGroup2RemainderFourLiveCycle::allocate(std::uint64_t request,std::uint32_t alignment){
 const bool domain=stage_==0?request>=512&&request<=640:stage_==1?request==256:stage_<4&&request>=129&&request<=192;
 if(stage_>=4||alignment!=8||!domain)throw std::invalid_argument("Declared four-live allocation order/domain required");
 validate();
 auto s=owned_resource(pool_,*backend_,resource_);const auto h=resource_;
 std::size_t off=0x1980;std::uint64_t remain=1640;for(unsigned k=0;k<stage_;++k){off+=static_cast<std::size_t>(size_[k])+8;remain-=size_[k]+8;}
 const auto size=(request+15)&~std::uint64_t{7};
 if(remain<size+8+256)throw std::invalid_argument("Split must leave a large free tail");
 const auto newremain=remain-size-8;const auto tail=off+static_cast<std::size_t>(size)+8;
 // Original split at the tail start: header, footer in the next prev-size word, new singleton tail.
 put(s,off+8,size);put(s,tail,request);put(s,tail+8,newremain|1);put(s,tail+16,h);put(s,tail+24,h);
 move_tail(s,h,remain,tail,newremain);
 auto p=pool_;const auto count=get<std::uint64_t>(p,0x50)+1,requested=get<std::uint64_t>(p,0x60)+request,occupied=get<std::uint64_t>(p,0x70)+size+8;
 put(p,0x70,occupied);put(p,0x78,std::max(get<std::uint64_t>(p,0x78),occupied));
 put(p,0x60,requested);put(p,0x68,std::max(get<std::uint64_t>(p,0x68),requested));
 put(p,0x50,count);put(p,0x58,std::max(get<std::uint64_t>(p,0x58),count));
 request_[stage_]=request;address_[stage_]=h+off+16;size_[stage_]=size;++stage_;validate();
 return {h+off+16,s.subspan(off+16,static_cast<std::size_t>(request))};
}
std::uint32_t BitmapGroup2RemainderFourLiveCycle::free(std::uint64_t pointer){
 if(stage_<4||stage_>=8)throw std::invalid_argument("LIFO free stage required");
 const unsigned k=7-stage_;if(pointer!=address_[k])throw std::invalid_argument("Actual captured LIFO allocation identity required");
 validate();
 auto s=owned_resource(pool_,*backend_,resource_);const auto h=resource_;
 const auto off=static_cast<std::size_t>(pointer-h-16),tail=off+static_cast<std::size_t>(size_[k])+8;
 const auto remain=get<std::uint64_t>(s,tail+8)&~std::uint64_t{3},merged=size_[k]+8+remain;
 // Original transient writes into the absorbed tail stay in the stale interior.
 put(s,tail,h+off);put(s,tail+8,remain|3);
 put(s,off+8,merged|1);put(s,off+16,h);put(s,off+24,h);
 move_tail(s,h,remain,off,merged);
 auto p=pool_;put(p,0x70,get<std::uint64_t>(p,0x70)-size_[k]-8);put(p,0x60,get<std::uint64_t>(p,0x60)-request_[k]);put(p,0x50,get<std::uint64_t>(p,0x50)-1);
 ++stage_;validate();return 0;
}
}
