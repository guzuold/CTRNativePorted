#include "ctr/bitmap_post_split_cycle.h"
#include <bit>
#include <cstring>
#include <stdexcept>
namespace ctr {
namespace {
template<class T>T read(std::span<std::byte> s,std::size_t n){T v;std::memcpy(&v,s.data()+n,sizeof(v));return v;}
auto bin(std::uint64_t n){auto m=31u-static_cast<unsigned>(std::countl_zero(static_cast<std::uint32_t>(n)));return std::pair{m-7,static_cast<unsigned>((n>>(m-5))^32)};}
}
BitmapPostSplitCycle::BitmapPostSplitCycle(std::span<std::byte> p,SystemPoolBackend& b):pool_(p),backend_(&b){validate();}
void BitmapPostSplitCycle::validate() const {
 auto p=pool_;if(p.size()!=192)throw std::invalid_argument("Actual derived192 required");auto bytes=backend_->memory.bytes();const auto base=reinterpret_cast<std::uintptr_t>(bytes.data()),object=reinterpret_cast<std::uintptr_t>(p.data());
 if(object<base||object-base>bytes.size()||192>bytes.size()-(object-base)||read<std::uint64_t>(p,0)!=0x025baa70||read<std::uint64_t>(p,0x10)||read<std::uint32_t>(p,0x1c)!=1||p[0x18]!=std::byte{1}||p[0x4b]!=std::byte{0})throw std::invalid_argument("Activated same-creator owner required");
 const auto h=read<std::uint64_t>(p,0xa8);
 if(h!=read<std::uint64_t>(p,0x20)||read<std::uint64_t>(p,0x28)!=8192||h<base||h-base>bytes.size()||8192>bytes.size()-(h-base))throw std::invalid_argument("Owned resource required");
 auto s=bytes.subspan(static_cast<std::size_t>(h-base),8192);const auto count=stage_==1||stage_==3?1u:stage_==2?2u:0u;
 const auto size=(first_request_+15)&~std::uint64_t{7};const auto requested=stage_==1?first_request_:stage_==2?first_request_+256:stage_==3?256u:0u;
 const auto occupied=0x1998+(stage_==1?size+8:stage_==2?size+280:stage_==3?272u:0u);
 if(read<std::uint64_t>(p,0x50)!=count||read<std::uint64_t>(p,0x58)!=3||read<std::uint64_t>(p,0x60)!=requested||read<std::uint64_t>(p,0x70)!=occupied||read<std::uint64_t>(p,0x68)<requested||read<std::uint64_t>(p,0x78)<occupied)throw std::invalid_argument("Measured current and retained peak statistics required");
 std::size_t tail=0x1980;std::uint64_t remaining=1640;
 if(stage_>=1&&stage_<=3){
  if(first_request_<256||first_request_>384||first_address_!=h+0x1990||read<std::uint64_t>(s,0x1988)!=(size|(stage_==3?1u:0u)))throw std::invalid_argument("Actual captured first allocation required");
  tail+=static_cast<std::size_t>(size)+8;remaining-=size+8;
  if(stage_>=2){if(second_address_!=h+tail+16||read<std::uint64_t>(s,tail+8)!=(stage_==3?266u:264u)||read<std::uint64_t>(s,tail)!=(stage_==3?h+0x1980:first_request_))throw std::invalid_argument("Actual captured second allocation and predecessor required");tail+=272;remaining-=272;}
  if(read<std::uint64_t>(s,tail)!=(stage_==1?first_request_:256u))throw std::invalid_argument("Actual requested footer required");
 }
 if(read<std::uint64_t>(s,tail+8)!=(remaining|1)||read<std::uint64_t>(s,tail+16)!=h||read<std::uint64_t>(s,tail+24)!=h||read<std::uint64_t>(s,0x1ff0)!=h+tail||(read<std::uint64_t>(s,0x1ff8)&3)!=2)throw std::invalid_argument("Actual free tail and terminal guard required");
 const auto [tg,ti]=bin(remaining);const auto thead=0x88+tg*256+ti*8;const auto [pg,pi]=bin(stage_==3?size:264);const auto phead=0x88+pg*256+pi*8;
 if((tg!=2&&tg!=3)||read<std::uint32_t>(s,0x20)!=((1u<<tg)|(stage_==3?1u<<pg:0u))||read<std::uint32_t>(s,0x24+tg*4)!=(1u<<ti)||read<std::uint64_t>(s,thead)!=h+tail||read<std::uint64_t>(s,0x18)!=(stage_==3?h+0x1980:h+tail))throw std::invalid_argument("Actual singleton maps required");
 for(unsigned j=0;j<25;++j)if(j!=tg&&read<std::uint32_t>(s,0x24+j*4)!=(stage_==3&&j==pg?1u<<pi:0u))throw std::invalid_argument("Other groups unsupported");
 for(std::size_t j=0x88;j<0x1988;j+=8)if(j!=thead&&!(stage_==3&&j==phead)&&read<std::uint64_t>(s,j)!=h)throw std::invalid_argument("Other heads unsupported");
 if(stage_==3&&(read<std::uint64_t>(s,phead)!=h+0x1980||read<std::uint64_t>(s,0x1990)!=h||read<std::uint64_t>(s,0x1998)!=h))throw std::invalid_argument("Actual free prefix singleton required");
}
BitmapAllocation BitmapPostSplitCycle::allocate(std::uint64_t request,std::uint32_t alignment){
 if(stage_>1||alignment!=8||request<256||request>384||(stage_==1&&request!=256))throw std::invalid_argument("One declared second pair cycle required");validate();
 auto allocation=allocate_normal_bitmap_large_pair(pool_,*backend_,request,alignment);
 if(stage_==0){first_request_=request;first_address_=allocation.address;}else second_address_=allocation.address;
 ++stage_;validate();return allocation;
}
std::uint32_t BitmapPostSplitCycle::free(std::uint64_t pointer){
 if(stage_!=2&&stage_!=3)throw std::invalid_argument("Four-step cycle free order required");
 if(pointer!=(stage_==2?first_address_:second_address_))throw std::invalid_argument("Actual captured allocation identity required");validate();
 const auto result=free_normal_bitmap_large_pair(pool_,*backend_,pointer);++stage_;validate();return result;
}
}
