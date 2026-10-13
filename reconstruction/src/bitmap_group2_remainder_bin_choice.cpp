#include "ctr/bitmap_group2_remainder_bin_choice.h"
#include <algorithm>
#include <bit>
#include <cstring>
#include <stdexcept>
#include <utility>
#include <vector>
// Original 014db6b0 NORMAL allocation, 014db250 getter, 014db270 NORMAL free -> 015c3a20 bitmap free,
// 015c3c50 group-map clear, 015c3c7f small-bin index. Write sets derived from genuine original snapshots
// (bitmap_group2_remainder_bin_choice_original.bin).
namespace ctr {
namespace {
template<class T>T get(std::span<const std::byte> s,std::size_t n){T v;std::memcpy(&v,s.data()+n,sizeof(v));return v;}
template<class T>void put(std::span<std::byte> s,std::size_t n,T v){std::memcpy(s.data()+n,&v,sizeof(v));}
std::pair<unsigned,unsigned> bin(std::uint64_t n){
 if(n<256)return {0u,static_cast<unsigned>(n>>3)};
 auto m=31u-static_cast<unsigned>(std::countl_zero(static_cast<std::uint32_t>(n)));return {m-7,static_cast<unsigned>((n>>(m-5))^32)};
}
std::size_t head_of(std::uint64_t n){const auto [g,i]=bin(n);return 0x88+static_cast<std::size_t>(g)*256+i*8;}
std::span<std::byte> owned_resource(std::span<std::byte> p,SystemPoolBackend& b,std::uint64_t expected){
 auto bytes=b.memory.bytes();const auto base=reinterpret_cast<std::uintptr_t>(bytes.data()),object=reinterpret_cast<std::uintptr_t>(p.data());
 if(p.size()!=192||object<base||object-base>bytes.size()||192>bytes.size()-(object-base)||get<std::uint64_t>(p,0)!=0x025baa70||get<std::uint64_t>(p,0x10)||get<std::uint32_t>(p,0x1c)!=1||p[0x18]!=std::byte{1}||p[0x4b]!=std::byte{0})throw std::invalid_argument("Activated same-creator derived owner required");
 const auto h=get<std::uint64_t>(p,0xa8);
 if((expected&&h!=expected)||h!=get<std::uint64_t>(p,0x20)||get<std::uint64_t>(p,0x28)!=8192||h<base||h-base>bytes.size()||8192>bytes.size()-(h-base))throw std::invalid_argument("Owned8192 resource required");
 return bytes.subspan(static_cast<std::size_t>(h-base),8192);
}
// Original list operations; the sentinel is the resource base (next at +0x10, prev at +0x18).
void unlink(std::span<std::byte> s,std::uint64_t h,std::size_t node,std::uint64_t size){
 const auto next=get<std::uint64_t>(s,node+16),prev=get<std::uint64_t>(s,node+24);
 // A head node moves the bin head to its successor (the sentinel next at +0x10 is never written).
 if(prev==h){put(s,head_of(size),next);if(next==h){const auto [g,i]=bin(size);const auto map=get<std::uint32_t>(s,0x24+g*4)&~(1u<<i);put(s,0x24+g*4,map);if(!map)put(s,0x20,get<std::uint32_t>(s,0x20)&~(1u<<g));}}
 else put(s,static_cast<std::size_t>(prev-h)+16,next);
 if(next==h)put(s,0x18,prev);else put(s,static_cast<std::size_t>(next-h)+24,prev);
}
void insert(std::span<std::byte> s,std::uint64_t h,std::size_t node,std::uint64_t size){
 const auto head=head_of(size),old=get<std::uint64_t>(s,head);
 put(s,node+16,old);put(s,node+24,h);
 if(old==h)put(s,0x18,h+node);else put(s,static_cast<std::size_t>(old-h)+24,h+node);
 put(s,head,h+node);const auto [g,i]=bin(size);put(s,0x24+g*4,get<std::uint32_t>(s,0x24+g*4)|(1u<<i));put(s,0x20,get<std::uint32_t>(s,0x20)|(1u<<g));
}
}
std::uint64_t bitmap_requested_size_large_slack(std::span<const std::byte> s,std::uint64_t pointer){
 const auto base=static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(s.data()));
 if(s.size()!=8192||pointer<base+0x1990||pointer-base>s.size()-8||pointer&7)throw std::invalid_argument("Actual positive normal bitmap pointer required");
 std::uint64_t marker;std::memcpy(&marker,s.data()+pointer-base-8,8);const auto size=marker&~std::uint64_t{3};
 if(marker&1||(size<136||size>1032)||size>s.size()-(pointer-base))throw std::invalid_argument("Bounded allocated bitmap marker required");
 std::uint64_t requested;std::memcpy(&requested,s.data()+((pointer+size-8)&~std::uint64_t{7})-base,8);
 const auto rounded=(requested+15)&~std::uint64_t{7};
 if(requested<129||requested>1024||size<rounded||size-rounded>=32)throw std::invalid_argument("Requested-size footer within whole-handout slack required");
 return requested;
}
BitmapGroup2RemainderBinChoiceCycle::BitmapGroup2RemainderBinChoiceCycle(std::span<std::byte> p,SystemPoolBackend& b):pool_(p),backend_(&b){
 (void)owned_resource(p,b,0);resource_=get<std::uint64_t>(p,0xa8);
 peak_request_=get<std::uint64_t>(p,0x68);peak_occupied_=get<std::uint64_t>(p,0x78);
 if(get<std::uint64_t>(p,0x58)!=5||peak_request_<768+129+129||peak_request_>896+192+192||peak_occupied_<0x1998+528+272+2*152||peak_occupied_>0x1998+656+272+2*208)throw std::invalid_argument("Measured five-live retained peaks required");
 model_.push_back({0x1980,1640,true,-1});
 validate();
}
void BitmapGroup2RemainderBinChoiceCycle::validate() const {
 auto s=owned_resource(pool_,*backend_,resource_);const auto h=resource_;
 std::uint64_t requested=0,occupied=0x1998;unsigned live=0;std::size_t expect=0x1980;
 for(std::size_t n=0;n<model_.size();++n){
  const auto& c=model_[n];const bool prevfree=n>0&&model_[n-1].free;
  if(c.offset!=expect||(prevfree&&c.free))throw std::invalid_argument("Physical chunk model inconsistent");
  if(get<std::uint64_t>(s,c.offset+8)!=(c.size|(c.free?1u:0u)|(prevfree?2u:0u)))throw std::invalid_argument("Actual chunk header chain required");
  if(prevfree&&get<std::uint64_t>(s,c.offset)!=h+model_[n-1].offset)throw std::invalid_argument("Actual prev-free pointer required");
  if(!c.free){const auto k=static_cast<unsigned>(c.block);
   if(address_[k]!=h+c.offset+16||get<std::uint64_t>(s,c.offset+static_cast<std::size_t>(c.size)+8)!=request_[k]||bitmap_requested_size_large_slack(s,address_[k])!=request_[k])throw std::invalid_argument("Actual captured live block with footer required");
   requested+=request_[k];occupied+=c.size+8;++live;}
  expect=c.offset+static_cast<std::size_t>(c.size)+8;
 }
 const auto& last=model_.back();
 if(expect!=0x1ff0||!last.free||get<std::uint64_t>(s,0x1ff0)!=h+last.offset||get<std::uint64_t>(s,0x1ff8)!=2||get<std::uint64_t>(s,0x10)!=h||get<std::uint64_t>(s,0x18)!=h+sentinel_prev_)throw std::invalid_argument("Actual free tail, terminal guard and sentinel links required");
 // Every non-empty bin is a well-formed list holding exactly the expected free chunks of that size class.
 std::uint32_t groups=0;std::array<std::uint32_t,25> maps{};std::vector<std::size_t> heads;
 for(const auto& c:model_)if(c.free){const auto [g,i]=bin(c.size);maps[g]|=1u<<i;groups|=1u<<g;const auto hd=head_of(c.size);if(std::find(heads.begin(),heads.end(),hd)==heads.end())heads.push_back(hd);}
 if(get<std::uint32_t>(s,0x20)!=groups)throw std::invalid_argument("Actual group map required");
 for(unsigned j=0;j<25;++j)if(get<std::uint32_t>(s,0x24+j*4)!=maps[j])throw std::invalid_argument("Actual bin maps required");
 for(std::size_t j=0x88;j<0x1988;j+=8)if(std::find(heads.begin(),heads.end(),j)==heads.end()&&get<std::uint64_t>(s,j)!=h)throw std::invalid_argument("All actual empty bin heads required");
 for(const auto hd:heads){
  std::vector<std::size_t> members;for(const auto& c:model_)if(c.free&&head_of(c.size)==hd)members.push_back(c.offset);
  auto node=get<std::uint64_t>(s,hd);std::uint64_t prev=h;std::size_t seen=0;
  while(node!=h){
   if(node<h+0x1980||node>=h+0x1ff0||seen>=members.size())throw std::invalid_argument("Bin list leaves the resource or is too long");
   const auto off=static_cast<std::size_t>(node-h);
   if(std::find(members.begin(),members.end(),off)==members.end()||get<std::uint64_t>(s,off+24)!=prev)throw std::invalid_argument("Actual bin list membership and back links required");
   prev=node;node=get<std::uint64_t>(s,off+16);++seen;
  }
  if(seen!=members.size())throw std::invalid_argument("Bin list misses an expected free chunk");
 }
 const auto p=pool_;
 if(get<std::uint64_t>(p,0x50)!=live||get<std::uint64_t>(p,0x60)!=requested||get<std::uint64_t>(p,0x70)!=occupied||get<std::uint64_t>(p,0x58)!=5||get<std::uint64_t>(p,0x68)!=std::max(peak_request_,max_requested_)||get<std::uint64_t>(p,0x78)!=std::max(peak_occupied_,max_occupied_))throw std::invalid_argument("Measured current and retained peak statistics required");
}
void BitmapGroup2RemainderBinChoiceCycle::account_allocation(std::uint64_t request,std::uint64_t size){
 auto p=pool_;const auto count=get<std::uint64_t>(p,0x50)+1,requested=get<std::uint64_t>(p,0x60)+request,occupied=get<std::uint64_t>(p,0x70)+size+8;
 put(p,0x70,occupied);put(p,0x78,std::max(get<std::uint64_t>(p,0x78),occupied));
 put(p,0x60,requested);put(p,0x68,std::max(get<std::uint64_t>(p,0x68),requested));
 put(p,0x50,count);put(p,0x58,std::max(get<std::uint64_t>(p,0x58),count));
 max_requested_=std::max(max_requested_,requested);max_occupied_=std::max(max_occupied_,occupied);
}
// Original split of a free chunk at its start: unlink it, in-use header, footer in the remainder's
// prev-size word, remainder header/links inserted, successor prev pointer moved to the remainder.
BitmapAllocation BitmapGroup2RemainderBinChoiceCycle::split(std::size_t index,std::uint64_t request,unsigned block){
 auto s=owned_resource(pool_,*backend_,resource_);const auto h=resource_;
 const auto chunk=model_[index];const auto size=(request+15)&~std::uint64_t{7};const auto rem=chunk.size-size-8;
 const auto off=chunk.offset,tail=off+static_cast<std::size_t>(size)+8,end=chunk.offset+static_cast<std::size_t>(chunk.size)+8;
 unlink(s,h,off,chunk.size);put(s,off+8,size);put(s,tail,request);put(s,tail+8,rem|1);insert(s,h,tail,rem);
 if(end<0x1ff0)put(s,end,h+tail);else put(s,0x1ff0,h+tail);
 sentinel_prev_=get<std::uint64_t>(s,0x18)-h;account_allocation(request,size);
 request_[block]=request;address_[block]=h+off+16;
 model_[index]={off,size,false,static_cast<int>(block)};model_.insert(model_.begin()+static_cast<std::ptrdiff_t>(index)+1,ModelChunk{tail,rem,true,-1});
 ++stage_;validate();return {h+off+16,s.subspan(off+16,static_cast<std::size_t>(request))};
}
// Original whole-chunk handout when less than 32 bytes would remain: unlink, in-use header keeps the chunk
// size, footer in the successor's prev-size word, successor prev-free bit cleared.
BitmapAllocation BitmapGroup2RemainderBinChoiceCycle::whole(std::size_t index,std::uint64_t request,unsigned block){
 auto s=owned_resource(pool_,*backend_,resource_);const auto h=resource_;
 const auto chunk=model_[index];const auto off=chunk.offset,next=off+static_cast<std::size_t>(chunk.size)+8;
 unlink(s,h,off,chunk.size);put(s,off+8,chunk.size);put(s,next,request);put(s,next+8,get<std::uint64_t>(s,next+8)&~std::uint64_t{2});
 sentinel_prev_=get<std::uint64_t>(s,0x18)-h;account_allocation(request,chunk.size);
 request_[block]=request;address_[block]=h+off+16;model_[index]={off,chunk.size,false,static_cast<int>(block)};
 ++stage_;validate();return {h+off+16,s.subspan(off+16,static_cast<std::size_t>(request))};
}
BitmapAllocation BitmapGroup2RemainderBinChoiceCycle::allocate(std::uint64_t request,std::uint32_t alignment){
 const bool small=request>=129&&request<=192;
 const bool domain=stage_==0||stage_==3?request==256:stage_==1||stage_==2||stage_==4||stage_==7?small:false;
 if(alignment!=8||!domain)throw std::invalid_argument("Declared bin-choice allocation order/domain required");
 validate();
 const auto size=(request+15)&~std::uint64_t{7};
 if(stage_==7){
  // Original choice: the free chunk in the smallest fitting bin; split if at least 32 bytes remain.
  std::size_t index=model_.size();std::pair<unsigned,unsigned> key{99u,0u};bool tie=false;
  for(std::size_t n=0;n<model_.size();++n)if(model_[n].free&&model_[n].size>=size){const auto k=bin(model_[n].size);if(k<key){key=k;index=n;tie=false;}else if(k==key)tie=true;}
  if(index==model_.size()||tie||index+1==model_.size())throw std::invalid_argument("Single non-tail smallest-fitting bin chunk required");
  return model_[index].size-size>=32?split(index,request,5):whole(index,request,5);
 }
 if(model_.back().size<size+8+256)throw std::invalid_argument("Split must leave a large free tail");
 return split(model_.size()-1,request,stage_);
}
std::uint32_t BitmapGroup2RemainderBinChoiceCycle::free(std::uint64_t pointer){
 // Stage5 frees A, stage6 SECOND; stages8..11 free D, C, X, FIRST.
 const unsigned k=stage_==5?1u:stage_==6?3u:stage_==8?5u:stage_==9?4u:stage_==10?2u:stage_==11?0u:6u;
 if(k==6u)throw std::invalid_argument("Declared free stage required");
 if(pointer!=address_[k])throw std::invalid_argument("Actual captured allocation in declared free order required");
 validate();
 std::size_t index=0;while(model_[index].block!=static_cast<int>(k)||model_[index].free)++index;
 auto s=owned_resource(pool_,*backend_,resource_);const auto h=resource_;
 const auto size=model_[index].size;const auto request=request_[k];
 const auto off=static_cast<std::size_t>(pointer-h-16),next=off+static_cast<std::size_t>(size)+8;
 const auto header=get<std::uint64_t>(s,off+8);const bool prevfree=(header&2)!=0;
 const auto start=prevfree?static_cast<std::size_t>(get<std::uint64_t>(s,off)-h):off;
 const auto nexthdr=get<std::uint64_t>(s,next+8);const bool nextfree=(nexthdr&1)!=0;
 const auto end=nextfree?next+static_cast<std::size_t>(nexthdr&~std::uint64_t{3})+8:next;
 const auto merged=static_cast<std::uint64_t>(end-start)-8;
 if(prevfree)unlink(s,h,start,get<std::uint64_t>(s,start+8)&~std::uint64_t{3});
 if(nextfree)unlink(s,h,next,nexthdr&~std::uint64_t{3});
 // Original transient writes: absorbed headers keep in-use/prev-free bits, absorbed next gets the start pointer.
 if(prevfree)put(s,off+8,header|1);
 if(nextfree){put(s,next,h+start);put(s,next+8,nexthdr|2);}
 put(s,start+8,merged|1);
 if(end<0x1ff0){put(s,end,h+start);put(s,end+8,get<std::uint64_t>(s,end+8)|2);}else put(s,0x1ff0,h+start);
 insert(s,h,start,merged);sentinel_prev_=get<std::uint64_t>(s,0x18)-h;
 auto p=pool_;put(p,0x70,get<std::uint64_t>(p,0x70)-size-8);put(p,0x60,get<std::uint64_t>(p,0x60)-request);put(p,0x50,get<std::uint64_t>(p,0x50)-1);
 // Model: the freed chunk absorbs a free predecessor and successor.
 std::size_t first=index,last=index;if(prevfree)--first;if(nextfree)++last;
 model_[first]={start,merged,true,-1};model_.erase(model_.begin()+static_cast<std::ptrdiff_t>(first)+1,model_.begin()+static_cast<std::ptrdiff_t>(last)+1);
 ++stage_;validate();return 0;
}
}
