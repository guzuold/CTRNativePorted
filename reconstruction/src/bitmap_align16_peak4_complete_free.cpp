#include "ctr/bitmap_align16_peak4_complete_free.h"
#include <bit>
#include <cstring>
#include <stdexcept>
namespace ctr { namespace {
template<class T>T read(std::span<std::byte> s,std::size_t n){T v;std::memcpy(&v,s.data()+n,sizeof(v));return v;}
template<class T>void write(std::span<std::byte> s,std::size_t n,T v){std::memcpy(s.data()+n,&v,sizeof(v));}
auto bin(std::uint64_t n){auto m=31u-static_cast<unsigned>(std::countl_zero(static_cast<std::uint32_t>(n)));return std::pair{m-7,static_cast<unsigned>((n>>(m-5))^32)};}
}
BitmapAlign16Peak4CompleteFree::BitmapAlign16Peak4CompleteFree(std::span<std::byte> p,SystemPoolBackend& b):object_(p),backend_(b){validate();}
void BitmapAlign16Peak4CompleteFree::validate() const {
 auto p=object_;auto bytes=backend_.memory.bytes();const auto base=reinterpret_cast<std::uintptr_t>(bytes.data()),object=reinterpret_cast<std::uintptr_t>(p.data());
 if(step_>=2||p.size()!=192||object<base||object-base>bytes.size()||192>bytes.size()-(object-base)||read<std::uint64_t>(p,0)!=0x025baa70||read<std::uint64_t>(p,0x10)||read<std::uint32_t>(p,0x1c)!=1||p[0x18]!=std::byte{1}||p[0x4b]!=std::byte{0})throw std::invalid_argument("Live bounded samecreator derived owner required");
 const auto h=read<std::uint64_t>(p,0xa8);
 if((h&15)||h!=read<std::uint64_t>(p,0x20)||read<std::uint64_t>(p,0x28)!=8192||h<base||h-base>bytes.size()||8192>bytes.size()-(h-base)||(resource_&&h!=resource_))throw std::invalid_argument("Actual owned naturally aligned resource required");
 auto s=bytes.subspan(static_cast<std::size_t>(h-base),8192);const auto firstmarker=read<std::uint64_t>(s,0x1988),first=firstmarker&~std::uint64_t{3};
 if(read<std::uint64_t>(p,0x50)!=2-step_||read<std::uint64_t>(p,0x58)!=4)throw std::invalid_argument("Exact remaining order/peak required");
 std::uint64_t original{},prior{},smallsize{};std::size_t split{};
 if(step_==0){
  if(first<144||first>192||(first&15)||firstmarker!=first)throw std::invalid_argument("Declared live first required");
  split=0x1980+static_cast<std::size_t>(first)+8;const auto marker=read<std::uint64_t>(s,split+8);smallsize=marker&~std::uint64_t{3};prior=read<std::uint64_t>(s,split);
  if(smallsize<128||smallsize>240||(smallsize&7)||marker!=(smallsize|1)||prior<129||prior>177||((prior-129)&15)||((prior+15)&~std::uint64_t{7})!=first||read<std::uint64_t>(s,split+16)!=h||read<std::uint64_t>(s,split+24)!=h)throw std::invalid_argument("Actual restored small remainder and requested footer required");
  original=first+smallsize+8;
 }else{original=first;if(firstmarker!=(first|1)||original!=original_size_||read<std::uint64_t>(s,0x1990)!=h||read<std::uint64_t>(s,0x1998)!=h)throw std::invalid_argument("Actual merged large prefix required");}
 if(original<328||original>392||(original&7))throw std::invalid_argument("Declared original prefix required");
 const auto second=0x1980+static_cast<std::size_t>(original)+8,tail=second+272,remain=1640-original-280;const auto [tg,ti]=bin(remain);const auto [pg,pi]=bin(original);
 const auto smallindex=static_cast<unsigned>(smallsize/8),head=step_==0?0x88+smallindex*8:0x88+pg*256+pi*8,thead=0x88+tg*256+ti*8;
 if((tg!=2&&tg!=3)||pg!=1||read<std::uint64_t>(s,second)!=h+(step_==0?split:0x1980)||read<std::uint64_t>(s,second+8)!=266||read<std::uint64_t>(s,second+272)!=256||read<std::uint64_t>(s,tail+8)!=(remain|1)||read<std::uint64_t>(p,0x60)!=256+prior||read<std::uint64_t>(p,0x70)!=0x1998+272+(step_==0?first+8:0)||read<std::uint32_t>(s,0x20)!=((1u<<tg)|(step_==0?1u:2u))||read<std::uint32_t>(s,0x24)!=(step_==0?1u<<smallindex:0u)||read<std::uint32_t>(s,0x24+4)!=(step_==1?1u<<pi:0u)||read<std::uint32_t>(s,0x24+tg*4)!=(1u<<ti)||read<std::uint64_t>(s,head)!=h+(step_==0?split:0x1980)||read<std::uint64_t>(s,thead)!=h+tail||read<std::uint64_t>(s,0x18)!=h+(step_==0?split:0x1980)||read<std::uint64_t>(s,0x10)!=h)throw std::invalid_argument("Exact actual chain/maps/stats/sentinel required");
 for(unsigned j=2;j<25;++j)if(j!=tg&&read<std::uint32_t>(s,0x24+j*4))throw std::invalid_argument("Other groups unsupported");
 for(std::size_t j=0x88;j<0x1988;j+=8)if(j!=head&&j!=thead&&read<std::uint64_t>(s,j)!=h)throw std::invalid_argument("Other heads unsupported");
 if(read<std::uint64_t>(s,tail+16)!=h||read<std::uint64_t>(s,tail+24)!=h||read<std::uint64_t>(s,0x1ff0)!=h+tail||(read<std::uint64_t>(s,0x1ff8)&3)!=2)throw std::invalid_argument("Actual terminal links/guard required");
 if(second_pointer_&&second_pointer_!=h+second+16)throw std::invalid_argument("Retained second identity required");
}
std::uint32_t BitmapAlign16Peak4CompleteFree::free(std::uint64_t pointer){
 validate();const auto h=read<std::uint64_t>(object_,0xa8);auto bytes=backend_.memory.bytes();const auto base=reinterpret_cast<std::uintptr_t>(bytes.data());auto s=bytes.subspan(static_cast<std::size_t>(h-base),8192);
 if(step_==0){const auto first=read<std::uint64_t>(s,0x1988),split=0x1980+static_cast<std::size_t>(first)+8,size=read<std::uint64_t>(s,split+8)&~std::uint64_t{3};if(pointer!=h+0x1990)throw std::invalid_argument("FIRST must be freed first");resource_=h;first_pointer_=pointer;original_size_=first+size+8;second_pointer_=h+0x1980+original_size_+24;}else if(pointer!=second_pointer_)throw std::invalid_argument("Retained SECOND must be freed second");
 std::uint32_t result{};
 if(step_==0){
  const auto first=read<std::uint64_t>(s,0x1988),split=0x1980+static_cast<std::size_t>(first)+8,size=read<std::uint64_t>(s,split+8)&~std::uint64_t{3},next=split+static_cast<std::size_t>(size)+8,prior=read<std::uint64_t>(s,split);const auto small=static_cast<unsigned>(size/8),head=0x88+small*8;
  const auto tail=next+272,remain=1640-original_size_-280;const auto [tg,ti]=bin(remain);(void)ti;
  // Exact original forward-small unlink and merged large-prefix insertion; peak4 unchanged.
  write(s,split,h+0x1980);s[split+8]|=std::byte{2};write(s,0x1988,first|1);
  write(s,0x18,h);write(s,0x10,h);write(s,head,h);write(s,0x24,0u);write(s,0x20,1u<<tg);
  write(s,0x1988,original_size_|1);write(s,next,h+0x1980);const auto [g,i]=bin(original_size_);
  write(s,0x1990,h);write(s,0x1998,h);write(s,0x18,h+0x1980);write(s,0x88+g*256+i*8,h+0x1980);write(s,0x24+g*4,1u<<i);write(s,0x20,(1u<<tg)|(1u<<g));
  write(object_,0x70,read<std::uint64_t>(object_,0x70)-first-8);write(object_,0x60,read<std::uint64_t>(object_,0x60)-prior);write(object_,0x50,std::uint64_t{1});(void)tail;
 }else result=free_normal_bitmap_large_pair(object_,backend_,pointer);
 ++step_;return result;
}
}
