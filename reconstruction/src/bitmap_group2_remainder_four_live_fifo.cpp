#include "ctr/bitmap_group2_remainder_four_live_fifo.h"
#include <algorithm>
#include <bit>
#include <cstring>
#include <stdexcept>
#include <initializer_list>
#include <utility>
// Original 014db6b0 NORMAL allocation, 014db250 getter, 014db270 NORMAL free -> 015c3a20 bitmap free.
// Write sets derived from genuine original snapshots (bitmap_group2_remainder_four_live_fifo_original.bin).
namespace ctr {
namespace {
template<class T>T get(std::span<const std::byte> s,std::size_t n){T v;std::memcpy(&v,s.data()+n,sizeof(v));return v;}
template<class T>void put(std::span<std::byte> s,std::size_t n,T v){std::memcpy(s.data()+n,&v,sizeof(v));}
auto bin(std::uint64_t n){auto m=31u-static_cast<unsigned>(std::countl_zero(static_cast<std::uint32_t>(n)));return std::pair{m-7,static_cast<unsigned>((n>>(m-5))^32)};}
std::size_t head_of(std::uint64_t n){const auto [g,i]=bin(n);return 0x88+static_cast<std::size_t>(g)*256+i*8;}
std::span<std::byte> owned_resource(std::span<std::byte> p,SystemPoolBackend& b,std::uint64_t expected){
 auto bytes=b.memory.bytes();const auto base=reinterpret_cast<std::uintptr_t>(bytes.data()),object=reinterpret_cast<std::uintptr_t>(p.data());
 if(p.size()!=192||object<base||object-base>bytes.size()||192>bytes.size()-(object-base)||get<std::uint64_t>(p,0)!=0x025baa70||get<std::uint64_t>(p,0x10)||get<std::uint32_t>(p,0x1c)!=1||p[0x18]!=std::byte{1}||p[0x4b]!=std::byte{0})throw std::invalid_argument("Activated same-creator derived owner required");
 const auto h=get<std::uint64_t>(p,0xa8);
 if((expected&&h!=expected)||h!=get<std::uint64_t>(p,0x20)||get<std::uint64_t>(p,0x28)!=8192||h<base||h-base>bytes.size()||8192>bytes.size()-(h-base))throw std::invalid_argument("Owned8192 resource required");
 return bytes.subspan(static_cast<std::size_t>(h-base),8192);
}
struct FreeChunk{std::size_t offset;std::uint64_t size;};
// Free chunks are singletons in distinct bins; every other head is the sentinel and every other map zero.
void publish_bins(std::span<std::byte> s,std::uint64_t h,std::initializer_list<FreeChunk> chunks){
 for(std::size_t j=0x88;j<0x1988;j+=8)put(s,j,h);
 for(unsigned j=0;j<25;++j)put(s,0x24+j*4,0u);
 std::uint32_t groups=0;
 for(const auto& c:chunks){const auto [g,i]=bin(c.size);put(s,head_of(c.size),h+c.offset);put(s,0x24+static_cast<std::size_t>(g)*4,get<std::uint32_t>(s,0x24+static_cast<std::size_t>(g)*4)|(1u<<i));groups|=1u<<g;}
 put(s,0x20,groups);
}
void check_bins(std::span<const std::byte> s,std::uint64_t h,std::initializer_list<FreeChunk> chunks){
 std::uint32_t groups=0;std::array<std::uint32_t,25> maps{};
 for(const auto& c:chunks){const auto [g,i]=bin(c.size);if(g<1||g>3||(maps[g]&(1u<<i)))throw std::invalid_argument("Distinct large singleton bins required");maps[g]|=1u<<i;groups|=1u<<g;
  if(get<std::uint64_t>(s,head_of(c.size))!=h+c.offset||get<std::uint64_t>(s,c.offset+16)!=h||get<std::uint64_t>(s,c.offset+24)!=h)throw std::invalid_argument("Actual singleton links required");}
 if(get<std::uint32_t>(s,0x20)!=groups)throw std::invalid_argument("Actual group map required");
 for(unsigned j=0;j<25;++j)if(get<std::uint32_t>(s,0x24+j*4)!=maps[j])throw std::invalid_argument("Actual bin maps required");
 for(std::size_t j=0x88;j<0x1988;j+=8){bool used=false;for(const auto& c:chunks)used|=j==head_of(c.size);if(!used&&get<std::uint64_t>(s,j)!=h)throw std::invalid_argument("All actual bin heads required");}
}
}
BitmapGroup2RemainderFourLiveFifoCycle::BitmapGroup2RemainderFourLiveFifoCycle(std::span<std::byte> p,SystemPoolBackend& b):pool_(p),backend_(&b){
 (void)owned_resource(p,b,0);resource_=get<std::uint64_t>(p,0xa8);
 peak_request_=get<std::uint64_t>(p,0x68);peak_occupied_=get<std::uint64_t>(p,0x78);
 if(get<std::uint64_t>(p,0x58)!=4||peak_request_<768+129+129||peak_request_>896+192+192||peak_occupied_<0x1998+528+272+2*152||peak_occupied_>0x1998+656+272+2*208)throw std::invalid_argument("Measured four-live retained peaks required");
 validate();
}
void BitmapGroup2RemainderFourLiveFifoCycle::validate() const {
 auto s=owned_resource(pool_,*backend_,resource_);const auto h=resource_;
 const unsigned allocated=std::min(stage_,4u),freed=stage_>4?stage_-4:0;
 std::size_t off=0x1980;std::uint64_t requested=0,occupied=0x1998;
 // Freed FIFO blocks form one free prefix at the start of the resource.
 std::uint64_t prefix=0;for(unsigned k=0;k<freed;++k)prefix+=size_[k]+8;
 if(freed==4){
  if(get<std::uint64_t>(s,0x1988)!=1641||get<std::uint64_t>(s,0x1ff0)!=h+0x1980||get<std::uint64_t>(s,0x1ff8)!=2||get<std::uint64_t>(s,0x10)!=h||get<std::uint64_t>(s,0x18)!=h+0x1980)throw std::invalid_argument("Actual merged1640 free chunk and terminal guard required");
  check_bins(s,h,{{0x1980,1640}});
 } else {
  if(freed){if(get<std::uint64_t>(s,0x1988)!=((prefix-8)|1))throw std::invalid_argument("Actual free prefix header required");off+=static_cast<std::size_t>(prefix);}
  for(unsigned k=freed;k<allocated;++k){
   const bool afterfree=k==freed&&freed;
   if(address_[k]!=h+off+16||get<std::uint64_t>(s,off+8)!=(size_[k]|(afterfree?2u:0u))||(afterfree&&get<std::uint64_t>(s,off)!=h+0x1980)||get<std::uint64_t>(s,off+static_cast<std::size_t>(size_[k])+8)!=request_[k]||bitmap_requested_size_large(s,address_[k])!=request_[k])throw std::invalid_argument("Actual captured live chain with footers required");
   off+=static_cast<std::size_t>(size_[k])+8;requested+=request_[k];occupied+=size_[k]+8;
  }
  const auto remain=0x1ff0-8-static_cast<std::uint64_t>(off);
  if(remain<256||get<std::uint64_t>(s,off+8)!=(remain|1)||get<std::uint64_t>(s,0x1ff0)!=h+off||get<std::uint64_t>(s,0x1ff8)!=2||get<std::uint64_t>(s,0x10)!=h||get<std::uint64_t>(s,0x18)!=(freed?h+0x1980:h+off))throw std::invalid_argument("Actual singleton free tail and terminal guard required");
  if(freed)check_bins(s,h,{{0x1980,prefix-8},{off,remain}});else check_bins(s,h,{{off,remain}});
 }
 std::uint64_t maxrequest=0,maxoccupied=0x1998;for(unsigned k=0;k<allocated;++k){maxrequest+=request_[k];maxoccupied+=size_[k]+8;}
 const auto p=pool_;const auto live=allocated-freed;
 if(get<std::uint64_t>(p,0x50)!=live||get<std::uint64_t>(p,0x60)!=requested||get<std::uint64_t>(p,0x70)!=occupied||get<std::uint64_t>(p,0x58)!=4||get<std::uint64_t>(p,0x68)!=std::max(peak_request_,maxrequest)||get<std::uint64_t>(p,0x78)!=std::max(peak_occupied_,allocated?maxoccupied:0))throw std::invalid_argument("Measured current and retained peak statistics required");
}
BitmapAllocation BitmapGroup2RemainderFourLiveFifoCycle::allocate(std::uint64_t request,std::uint32_t alignment){
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
 publish_bins(s,h,{{tail,newremain}});put(s,0x18,h+tail);put(s,0x1ff0,h+tail);
 auto p=pool_;const auto count=get<std::uint64_t>(p,0x50)+1,requested=get<std::uint64_t>(p,0x60)+request,occupied=get<std::uint64_t>(p,0x70)+size+8;
 put(p,0x70,occupied);put(p,0x78,std::max(get<std::uint64_t>(p,0x78),occupied));
 put(p,0x60,requested);put(p,0x68,std::max(get<std::uint64_t>(p,0x68),requested));
 put(p,0x50,count);put(p,0x58,std::max(get<std::uint64_t>(p,0x58),count));
 request_[stage_]=request;address_[stage_]=h+off+16;size_[stage_]=size;++stage_;validate();
 return {h+off+16,s.subspan(off+16,static_cast<std::size_t>(request))};
}
std::uint32_t BitmapGroup2RemainderFourLiveFifoCycle::free(std::uint64_t pointer){
 if(stage_<4||stage_>=8)throw std::invalid_argument("FIFO free stage required");
 const unsigned k=stage_-4;if(pointer!=address_[k])throw std::invalid_argument("Actual captured FIFO allocation identity required");
 validate();
 auto s=owned_resource(pool_,*backend_,resource_);const auto h=resource_;
 const auto off=static_cast<std::size_t>(pointer-h-16),next=off+static_cast<std::size_t>(size_[k])+8;
 const auto merged=static_cast<std::uint64_t>(next-0x1980)-8;
 if(k>0)put(s,off+8,get<std::uint64_t>(s,off+8)|1);  // transient in-use clear left in the absorbed header
 if(k<3){
  // Successor stays live: free prefix (merged backward for k>0) gets prev-free pointer and bit1 on it.
  put(s,0x1988,merged|1);put(s,0x1990,h);put(s,0x1998,h);put(s,next,h+0x1980);put(s,next+8,get<std::uint64_t>(s,next+8)|2);
  const auto tail=static_cast<std::size_t>(get<std::uint64_t>(s,0x1ff0)-h);publish_bins(s,h,{{0x1980,merged},{tail,0x1ff0-8-static_cast<std::uint64_t>(tail)}});
 } else {
  // Successor is the free tail: backward and forward merge into the whole 1640 chunk.
  const auto remain=get<std::uint64_t>(s,next+8)&~std::uint64_t{3};
  put(s,next,h+0x1980);put(s,next+8,remain|3);put(s,0x1988,std::uint64_t{1641});
  publish_bins(s,h,{{0x1980,1640}});put(s,0x1ff0,h+0x1980);
 }
 put(s,0x18,h+0x1980);
 auto p=pool_;put(p,0x70,get<std::uint64_t>(p,0x70)-size_[k]-8);put(p,0x60,get<std::uint64_t>(p,0x60)-request_[k]);put(p,0x50,get<std::uint64_t>(p,0x50)-1);
 ++stage_;validate();return 0;
}
}
