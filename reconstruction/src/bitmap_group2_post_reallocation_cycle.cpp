#include "ctr/bitmap_group2_post_reallocation_cycle.h"
#include <bit>
#include <algorithm>
#include <cstring>
#include <stdexcept>
namespace ctr {
namespace {
template<class T>T read(std::span<std::byte> s,std::size_t n){T v;std::memcpy(&v,s.data()+n,sizeof(v));return v;}
auto bin(std::uint64_t n){auto m=31u-static_cast<unsigned>(std::countl_zero(static_cast<std::uint32_t>(n)));return std::pair{m-7,static_cast<unsigned>((n>>(m-5))^32)};}
template<class T>T get(std::span<std::byte> s,std::size_t n){return read<T>(s,n);}
template<class T>void put(std::span<std::byte> s,std::size_t n,T v){std::memcpy(s.data()+n,&v,sizeof(v));}
auto index(std::uint64_t n){return bin(n);}

}
static std::uint32_t cycle_prefix_free(std::span<std::byte> pool,SystemPoolBackend& b,std::uint64_t pointer){
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
 if(get<std::uint64_t>(pool,0x50)!=2||get<std::uint64_t>(pool,0x58)!=2||get<std::uint64_t>(pool,0x60)!=requested+256||get<std::uint64_t>(pool,0x68)<requested+256||get<std::uint64_t>(pool,0x70)!=occupied||get<std::uint64_t>(pool,0x78)<occupied)throw std::invalid_argument("Exact two-live statistics required");
 // All owned shape checks precede mutation. Preserve the other bin in this group.
 put(s,next,header+0x1980);put(s,next+8,std::uint64_t{266});put(s,0x1988,size|1);
 put(s,0x1990,header);put(s,0x1998,header);put(s,0x18,header+0x1980);put(s,head,header+0x1980);
 put(s,0x2c,(1u<<tailbin)|(1u<<bin));
 put(pool,0x70,std::uint64_t{0x1998+272});put(pool,0x60,std::uint64_t{256});put(pool,0x50,std::uint64_t{1});return 0;
}
static std::uint32_t cycle_second_free(std::span<std::byte> pool,SystemPoolBackend& b,std::uint64_t pointer){
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
 if(get<std::uint64_t>(pool,0x50)!=1||get<std::uint64_t>(pool,0x58)!=2||get<std::uint64_t>(pool,0x60)!=256||peak<768||peak>896||get<std::uint64_t>(pool,0x70)!=occupied||get<std::uint64_t>(pool,0x78)<0x1998+size+280)throw std::invalid_argument("Exact remaining second statistics required");
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
BitmapGroup2PostReallocationCycle::BitmapGroup2PostReallocationCycle(std::span<std::byte> p,SystemPoolBackend& b):pool_(p),backend_(&b){
 auto bytes=b.memory.bytes();const auto base=reinterpret_cast<std::uintptr_t>(bytes.data()),object=reinterpret_cast<std::uintptr_t>(p.data());
 if(p.size()!=192||object<base||object-base>bytes.size()||192>bytes.size()-(object-base))throw std::invalid_argument("Retained actual derived owner required");
 resource_=read<std::uint64_t>(p,0xa8);peak_request_=read<std::uint64_t>(p,0x68);peak_occupied_=read<std::uint64_t>(p,0x78);
 if(peak_request_<768||peak_request_>896||peak_occupied_!=0x1998+((peak_request_-256+15)&~std::uint64_t{7})+280)throw std::invalid_argument("Measured retained pair peaks required");validate();}
void BitmapGroup2PostReallocationCycle::validate() const {
 auto p=pool_;if(p.size()!=192)throw std::invalid_argument("Actual derived192 required");auto bytes=backend_->memory.bytes();const auto base=reinterpret_cast<std::uintptr_t>(bytes.data()),object=reinterpret_cast<std::uintptr_t>(p.data());
 if(object<base||object-base>bytes.size()||192>bytes.size()-(object-base)||read<std::uint64_t>(p,0)!=0x025baa70||read<std::uint64_t>(p,0x10)||read<std::uint32_t>(p,0x1c)!=1||p[0x18]!=std::byte{1}||p[0x4b]!=std::byte{0})throw std::invalid_argument("Activated same-creator owner required");
 const auto h=read<std::uint64_t>(p,0xa8);
 if(h!=resource_||h!=read<std::uint64_t>(p,0x20)||read<std::uint64_t>(p,0x28)!=8192||h<base||h-base>bytes.size()||8192>bytes.size()-(h-base))throw std::invalid_argument("Owned resource required");
 auto s=bytes.subspan(static_cast<std::size_t>(h-base),8192);const auto count=stage_==1||stage_==3?1u:stage_==2?2u:0u;
 const auto size=(first_request_+15)&~std::uint64_t{7};const auto requested=stage_==1?first_request_:stage_==2?first_request_+256:stage_==3?256u:0u;
 const auto occupied=0x1998+(stage_==1?size+8:stage_==2?size+280:stage_==3?272u:0u);
 if(read<std::uint64_t>(p,0x50)!=count||read<std::uint64_t>(p,0x58)!=2||read<std::uint64_t>(p,0x60)!=requested||read<std::uint64_t>(p,0x70)!=occupied||read<std::uint64_t>(p,0x68)!=std::max(peak_request_,stage_==0?std::uint64_t{0}:stage_==1?first_request_:first_request_+256)||read<std::uint64_t>(p,0x78)!=std::max(peak_occupied_,stage_==0?std::uint64_t{0}:stage_==1?std::uint64_t{0x1998}+size+8:std::uint64_t{0x1998}+size+280))throw std::invalid_argument("Measured current and retained peak statistics required");
 std::size_t tail=0x1980;std::uint64_t remaining=1640;
 if(stage_>=1&&stage_<=3){
  if(first_request_<512||first_request_>640||first_address_!=h+0x1990||read<std::uint64_t>(s,0x1988)!=(size|(stage_==3?1u:0u)))throw std::invalid_argument("Actual captured first allocation required");
  tail+=static_cast<std::size_t>(size)+8;remaining-=size+8;
  if(stage_>=2){if(second_address_!=h+tail+16||read<std::uint64_t>(s,tail+8)!=(stage_==3?266u:264u)||read<std::uint64_t>(s,tail)!=(stage_==3?h+0x1980:first_request_))throw std::invalid_argument("Actual captured second allocation and predecessor required");tail+=272;remaining-=272;}
  if(read<std::uint64_t>(s,tail)!=(stage_==1?first_request_:256u))throw std::invalid_argument("Actual requested footer required");
 }
 if(read<std::uint64_t>(s,0x10)!=h||read<std::uint64_t>(s,tail+8)!=(remaining|1)||read<std::uint64_t>(s,tail+16)!=h||read<std::uint64_t>(s,tail+24)!=h||read<std::uint64_t>(s,0x1ff0)!=h+tail||read<std::uint64_t>(s,0x1ff8)!=2)throw std::invalid_argument("Actual free tail and terminal guard required");
 const auto [tg,ti]=bin(remaining);const auto thead=0x88+tg*256+ti*8;const auto [pg,pi]=bin(stage_==3?size:520);const auto phead=0x88+pg*256+pi*8;
 if((tg!=2&&tg!=3)||read<std::uint32_t>(s,0x20)!=((1u<<tg)|(stage_==3?1u<<pg:0u))||read<std::uint32_t>(s,0x24+tg*4)!=((1u<<ti)|(stage_==3&&pg==tg?1u<<pi:0u))||read<std::uint64_t>(s,thead)!=h+tail||read<std::uint64_t>(s,0x18)!=(stage_==3?h+0x1980:h+tail))throw std::invalid_argument("Actual singleton maps required");
 for(unsigned j=0;j<25;++j)if(j!=tg&&read<std::uint32_t>(s,0x24+j*4)!=(stage_==3&&j==pg?1u<<pi:0u))throw std::invalid_argument("Other groups unsupported");
 for(std::size_t j=0x88;j<0x1988;j+=8)if(j!=thead&&!(stage_==3&&j==phead)&&read<std::uint64_t>(s,j)!=h)throw std::invalid_argument("Other heads unsupported");
 if(stage_==3&&(read<std::uint64_t>(s,phead)!=h+0x1980||read<std::uint64_t>(s,0x1990)!=h||read<std::uint64_t>(s,0x1998)!=h))throw std::invalid_argument("Actual free prefix singleton required");
}
BitmapAllocation BitmapGroup2PostReallocationCycle::allocate(std::uint64_t request,std::uint32_t alignment){
 if(stage_>1||alignment!=8||(stage_==0&&(request<512||request>640))||(stage_==1&&request!=256))throw std::invalid_argument("One declared second pair cycle required");validate();
 auto allocation=allocate_normal_bitmap_group2_pair(pool_,*backend_,request,alignment);
 if(stage_==0){first_request_=request;first_address_=allocation.address;}else second_address_=allocation.address;
 ++stage_;validate();return allocation;
}
std::uint32_t BitmapGroup2PostReallocationCycle::free(std::uint64_t pointer){
 if(stage_!=2&&stage_!=3)throw std::invalid_argument("Four-step cycle free order required");
 if(pointer!=(stage_==2?first_address_:second_address_))throw std::invalid_argument("Actual captured allocation identity required");validate();
 const auto result=stage_==2?cycle_prefix_free(pool_,*backend_,pointer):cycle_second_free(pool_,*backend_,pointer);++stage_;validate();return result;
}
}
