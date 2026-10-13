#include "ctr/bitmap_group2_remainder_small_bin.h"
#include <algorithm>
#include <bit>
#include <cstring>
#include <stdexcept>
#include <utility>
#include <vector>
// Original 014db6b0 NORMAL allocation, 014db250 getter, 014db270 NORMAL free -> 015c3a20 bitmap free,
// 015c3c50 group-map clear, 015c3c7f small-bin index (size/8, group0). Write sets derived from genuine
// original snapshots (bitmap_group2_remainder_small_bin_original.bin).
namespace ctr {
namespace {
template<class T>T get(std::span<const std::byte> s,std::size_t n){T v;std::memcpy(&v,s.data()+n,sizeof(v));return v;}
template<class T>void put(std::span<std::byte> s,std::size_t n,T v){std::memcpy(s.data()+n,&v,sizeof(v));}
// Large chunks: group=log2-7, index from the next five bits. Small chunks (<256): group0, index size/8.
std::pair<unsigned,unsigned> bin(std::uint64_t n){
 if(n<256)return {0u,static_cast<unsigned>(n>>3)};
 auto m=31u-static_cast<unsigned>(std::countl_zero(static_cast<std::uint32_t>(n)));return {m-7,static_cast<unsigned>((n>>(m-5))^32)};
}
std::size_t head_of(std::uint64_t n){const auto [g,i]=bin(n);return 0x88+static_cast<std::size_t>(g)*256+i*8;}
struct Chunk{std::size_t offset;std::uint64_t size;bool free;int block;};
std::span<std::byte> owned_resource(std::span<std::byte> p,SystemPoolBackend& b,std::uint64_t expected){
 auto bytes=b.memory.bytes();const auto base=reinterpret_cast<std::uintptr_t>(bytes.data()),object=reinterpret_cast<std::uintptr_t>(p.data());
 if(p.size()!=192||object<base||object-base>bytes.size()||192>bytes.size()-(object-base)||get<std::uint64_t>(p,0)!=0x025baa70||get<std::uint64_t>(p,0x10)||get<std::uint32_t>(p,0x1c)!=1||p[0x18]!=std::byte{1}||p[0x4b]!=std::byte{0})throw std::invalid_argument("Activated same-creator derived owner required");
 const auto h=get<std::uint64_t>(p,0xa8);
 if((expected&&h!=expected)||h!=get<std::uint64_t>(p,0x20)||get<std::uint64_t>(p,0x28)!=8192||h<base||h-base>bytes.size()||8192>bytes.size()-(h-base))throw std::invalid_argument("Owned8192 resource required");
 return bytes.subspan(static_cast<std::size_t>(h-base),8192);
}
void publish_bins(std::span<std::byte> s,std::uint64_t h,const std::vector<Chunk>& chunks){
 for(std::size_t j=0x88;j<0x1988;j+=8)put(s,j,h);
 for(unsigned j=0;j<25;++j)put(s,0x24+j*4,0u);
 std::uint32_t groups=0;
 for(const auto& c:chunks)if(c.free){const auto [g,i]=bin(c.size);put(s,head_of(c.size),h+c.offset);put(s,0x24+static_cast<std::size_t>(g)*4,get<std::uint32_t>(s,0x24+static_cast<std::size_t>(g)*4)|(1u<<i));groups|=1u<<g;}
 put(s,0x20,groups);
}
}
BitmapGroup2RemainderSmallBinCycle::BitmapGroup2RemainderSmallBinCycle(std::span<std::byte> p,SystemPoolBackend& b):pool_(p),backend_(&b){
 (void)owned_resource(p,b,0);resource_=get<std::uint64_t>(p,0xa8);
 peak_request_=get<std::uint64_t>(p,0x68);peak_occupied_=get<std::uint64_t>(p,0x78);
 if(get<std::uint64_t>(p,0x58)!=4||peak_request_<768+129+129||peak_request_>896+192+192||peak_occupied_<0x1998+528+272+2*152||peak_occupied_>0x1998+656+272+2*208)throw std::invalid_argument("Measured four-live retained peaks required");
 validate();
}
// Expected physical layout from captured allocations and frees: adjacent free chunks are coalesced.
static std::vector<Chunk> expected_layout(unsigned allocated,const std::array<std::uint64_t,4>& size,const std::array<bool,4>& freed){
 std::vector<Chunk> raw;std::size_t off=0x1980;std::uint64_t remain=1640;
 for(unsigned k=0;k<allocated;++k){raw.push_back({off,size[k],freed[k],static_cast<int>(k)});off+=static_cast<std::size_t>(size[k])+8;remain-=size[k]+8;}
 raw.push_back({off,remain,true,-1});
 std::vector<Chunk> out;
 for(const auto& c:raw){if(c.free&&!out.empty()&&out.back().free)out.back().size+=c.size+8;else out.push_back(c);}
 return out;
}
void BitmapGroup2RemainderSmallBinCycle::validate() const {
 auto s=owned_resource(pool_,*backend_,resource_);const auto h=resource_;const unsigned allocated=std::min(stage_,4u);
 const auto layout=expected_layout(allocated,size_,freed_);
 std::uint64_t requested=0,occupied=0x1998;unsigned live=0;
 for(std::size_t n=0;n<layout.size();++n){
  const auto& c=layout[n];const bool prevfree=n>0&&layout[n-1].free;
  if(get<std::uint64_t>(s,c.offset+8)!=(c.size|(c.free?1u:0u)|(prevfree?2u:0u)))throw std::invalid_argument("Actual chunk header chain required");
  if(prevfree&&get<std::uint64_t>(s,c.offset)!=h+layout[n-1].offset)throw std::invalid_argument("Actual prev-free pointer required");
  if(c.free){if(get<std::uint64_t>(s,c.offset+16)!=h||get<std::uint64_t>(s,c.offset+24)!=h||get<std::uint64_t>(s,head_of(c.size))!=h+c.offset)throw std::invalid_argument("Actual singleton free links/head required");}
  else{const auto k=static_cast<unsigned>(c.block);
   if(address_[k]!=h+c.offset+16||get<std::uint64_t>(s,c.offset+static_cast<std::size_t>(c.size)+8)!=request_[k]||bitmap_requested_size_large(s,address_[k])!=request_[k])throw std::invalid_argument("Actual captured live block with footer required");
   requested+=request_[k];occupied+=size_[k]+8;++live;}
 }
 const auto& last=layout.back();
 if(!last.free||last.offset+static_cast<std::size_t>(last.size)+8!=0x1ff0||get<std::uint64_t>(s,0x1ff0)!=h+last.offset||get<std::uint64_t>(s,0x1ff8)!=2||get<std::uint64_t>(s,0x10)!=h||get<std::uint64_t>(s,0x18)!=h+last_binned_)throw std::invalid_argument("Actual free tail, terminal guard and last-binned cursor required");
 std::uint32_t groups=0;std::array<std::uint32_t,25> maps{};
 for(const auto& c:layout)if(c.free){const auto [g,i]=bin(c.size);if(maps[g]&(1u<<i))throw std::invalid_argument("Distinct singleton bins required");maps[g]|=1u<<i;groups|=1u<<g;}
 if(get<std::uint32_t>(s,0x20)!=groups)throw std::invalid_argument("Actual group map required");
 for(unsigned j=0;j<25;++j)if(get<std::uint32_t>(s,0x24+j*4)!=maps[j])throw std::invalid_argument("Actual bin maps required");
 for(std::size_t j=0x88;j<0x1988;j+=8){bool used=false;for(const auto& c:layout)used|=c.free&&j==head_of(c.size);if(!used&&get<std::uint64_t>(s,j)!=h)throw std::invalid_argument("All actual bin heads required");}
 std::uint64_t maxrequest=0,maxoccupied=0x1998;for(unsigned k=0;k<allocated;++k){maxrequest+=request_[k];maxoccupied+=size_[k]+8;}
 const auto p=pool_;
 if(get<std::uint64_t>(p,0x50)!=live||get<std::uint64_t>(p,0x60)!=requested||get<std::uint64_t>(p,0x70)!=occupied||get<std::uint64_t>(p,0x58)!=4||get<std::uint64_t>(p,0x68)!=std::max(peak_request_,maxrequest)||get<std::uint64_t>(p,0x78)!=std::max(peak_occupied_,allocated?maxoccupied:0))throw std::invalid_argument("Measured current and retained peak statistics required");
}
BitmapAllocation BitmapGroup2RemainderSmallBinCycle::allocate(std::uint64_t request,std::uint32_t alignment){
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
 publish_bins(s,h,{{tail,newremain,true,-1}});put(s,0x18,h+tail);put(s,0x1ff0,h+tail);last_binned_=tail;
 auto p=pool_;const auto count=get<std::uint64_t>(p,0x50)+1,requested=get<std::uint64_t>(p,0x60)+request,occupied=get<std::uint64_t>(p,0x70)+size+8;
 put(p,0x70,occupied);put(p,0x78,std::max(get<std::uint64_t>(p,0x78),occupied));
 put(p,0x60,requested);put(p,0x68,std::max(get<std::uint64_t>(p,0x68),requested));
 put(p,0x50,count);put(p,0x58,std::max(get<std::uint64_t>(p,0x58),count));
 request_[stage_]=request;address_[stage_]=h+off+16;size_[stage_]=size;++stage_;validate();
 return {h+off+16,s.subspan(off+16,static_cast<std::size_t>(request))};
}
std::uint32_t BitmapGroup2RemainderSmallBinCycle::free(std::uint64_t pointer){
 static constexpr std::array<unsigned,4> order{0,2,1,3};
 if(stage_<4||stage_>=8)throw std::invalid_argument("Declared free stage required");
 const unsigned k=order[stage_-4];if(pointer!=address_[k])throw std::invalid_argument("Actual captured allocation in declared free order required");
 validate();
 auto s=owned_resource(pool_,*backend_,resource_);const auto h=resource_;
 const auto off=static_cast<std::size_t>(pointer-h-16),next=off+static_cast<std::size_t>(size_[k])+8;
 const auto header=get<std::uint64_t>(s,off+8);const bool prevfree=(header&2)!=0;
 const auto start=prevfree?static_cast<std::size_t>(get<std::uint64_t>(s,off)-h):off;
 const auto nexthdr=get<std::uint64_t>(s,next+8);const bool nextfree=(nexthdr&1)!=0;
 const auto end=nextfree?next+static_cast<std::size_t>(nexthdr&~std::uint64_t{3})+8:next;
 const auto merged=static_cast<std::uint64_t>(end-start)-8;
 // Original transient writes: absorbed headers keep in-use/prev-free bits, absorbed next gets the start pointer.
 if(prevfree)put(s,off+8,header|1);
 if(nextfree){put(s,next,h+start);put(s,next+8,nexthdr|2);}
 put(s,start+8,merged|1);put(s,start+16,h);put(s,start+24,h);
 if(end<0x1ff0){put(s,end,h+start);put(s,end+8,get<std::uint64_t>(s,end+8)|2);}else put(s,0x1ff0,h+start);
 freed_[k]=true;
 publish_bins(s,h,expected_layout(4,size_,freed_));put(s,0x18,h+start);last_binned_=start;
 auto p=pool_;put(p,0x70,get<std::uint64_t>(p,0x70)-size_[k]-8);put(p,0x60,get<std::uint64_t>(p,0x60)-request_[k]);put(p,0x50,get<std::uint64_t>(p,0x50)-1);
 ++stage_;validate();return 0;
}
}
