#include "ctr/bitmap_normal_allocator.h"
#include <algorithm>
#include <bit>
#include <cstring>
#include <stdexcept>
#include <utility>
// Rules consolidated from the per-history units (bitmap_group2_*) and validated against the genuine
// original outputs of the whole cumulative history (bitmap_group2_remainder_three_node_original.bin).
namespace ctr {
namespace {
template<class T>T get(std::span<const std::byte> s,std::size_t n){T v;std::memcpy(&v,s.data()+n,sizeof(v));return v;}
template<class T>void put(std::span<std::byte> s,std::size_t n,T v){std::memcpy(s.data()+n,&v,sizeof(v));}
constexpr std::size_t kFirst=0x1980,kGuard=0x1ff0;
// Bin of a chunk size: small (<256) group0 index size/8; large group log2-7, index from the next five bits.
std::pair<unsigned,unsigned> bin(std::uint64_t n){
 if(n<256)return {0u,static_cast<unsigned>(n>>3)};
 auto m=31u-static_cast<unsigned>(std::countl_zero(static_cast<std::uint32_t>(n)));return {m-7,static_cast<unsigned>((n>>(m-5))^32)};
}
// Original lookup rounds the size up to its bin width (bin(size+15) in group2), so any chunk found fits.
std::pair<unsigned,unsigned> lookup(std::uint64_t n){
 if(n<256)return bin(n);
 const auto m=31u-static_cast<unsigned>(std::countl_zero(static_cast<std::uint32_t>(n)));return bin(n+(std::uint64_t{1}<<(m-5))-1);
}
std::size_t head_of(std::uint64_t n){const auto [g,i]=bin(n);return 0x88+static_cast<std::size_t>(g)*256+i*8;}
// Bins are doubly linked lists whose sentinel is the resource base (node+16 next, node+24 prev, sentinel
// next/prev at +0x10/+0x18); a head node moves the bin head and never writes the sentinel next.
void unlink(std::span<std::byte> s,std::uint64_t h,std::size_t node,std::uint64_t size){
 const auto next=get<std::uint64_t>(s,node+16),prev=get<std::uint64_t>(s,node+24);
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
BitmapNormalAllocator::BitmapNormalAllocator(std::span<std::byte> p,SystemPoolBackend& b):pool_(p),backend_(&b){
 auto bytes=b.memory.bytes();const auto base=reinterpret_cast<std::uintptr_t>(bytes.data()),object=reinterpret_cast<std::uintptr_t>(p.data());
 if(p.size()!=192||object<base||object-base>bytes.size()||192>bytes.size()-(object-base))throw std::invalid_argument("Derived owner inside the same-creator backend required");
 resource_=get<std::uint64_t>(p,0xa8);(void)scan();
}
std::span<std::byte> BitmapNormalAllocator::resource() const {
 auto bytes=backend_->memory.bytes();const auto base=reinterpret_cast<std::uintptr_t>(bytes.data());const auto p=pool_;
 if(get<std::uint64_t>(p,0)!=0x025baa70||get<std::uint64_t>(p,0x10)||get<std::uint32_t>(p,0x1c)!=1||p[0x18]!=std::byte{1}||p[0x4b]!=std::byte{0})throw std::invalid_argument("Activated derived type1 debug0 owner required");
 const auto h=get<std::uint64_t>(p,0xa8);
 if(h!=resource_||h!=get<std::uint64_t>(p,0x20)||get<std::uint64_t>(p,0x28)!=8192||h<base||h-base>bytes.size()||8192>bytes.size()-(h-base)||(h&7))throw std::invalid_argument("Owned8192 bitmap resource required");
 return bytes.subspan(static_cast<std::size_t>(h-base),8192);
}
std::vector<BitmapNormalAllocator::Chunk> BitmapNormalAllocator::scan() const {
 const auto s=resource();const auto h=resource_;
 std::vector<Chunk> chain;std::uint64_t count=0,requested=0,occupied=0x1998;std::size_t off=kFirst,prevoff=0;bool prevfree=false;
 while(off<kGuard){
  const auto hdr=get<std::uint64_t>(s,off+8),size=hdr&~std::uint64_t{3};const bool free=(hdr&1)!=0,pf=(hdr&2)!=0;
  if(size<24||(size&7)||size>kGuard-off-8)throw std::invalid_argument("Actual chunk header chain required");
  if(pf!=prevfree||(pf&&free)||(pf&&get<std::uint64_t>(s,off)!=h+prevoff))throw std::invalid_argument("Actual prev-free bits/pointers and coalesced free chunks required");
  if(!free){
   const auto request=get<std::uint64_t>(s,off+static_cast<std::size_t>(size)+8),rounded=(request+15)&~std::uint64_t{7};
   if(request<129||request>1024||size<rounded||size-rounded>=32)throw std::invalid_argument("Actual NORMAL footer within observed slack required");
   ++count;requested+=request;occupied+=size+8;
  }
  chain.push_back({off,size,free});prevfree=free;prevoff=off;off+=static_cast<std::size_t>(size)+8;
 }
 if(off!=kGuard||!prevfree)throw std::invalid_argument("Free last chunk required (allocated last chunk unobserved)");
 if(get<std::uint64_t>(s,kGuard)!=h+prevoff||get<std::uint64_t>(s,kGuard+8)!=2||get<std::uint64_t>(s,0x10)!=h)throw std::invalid_argument("Actual terminal guard and sentinel next required");
 const auto cursor=get<std::uint64_t>(s,0x18);
 if(cursor!=h&&(cursor<h+kFirst||cursor>=h+kGuard||(cursor&7)))throw std::invalid_argument("Sentinel prev inside the resource required");
 // Maps and lists must hold exactly the free chunks of the chain.
 std::uint32_t groups=0;std::array<std::uint32_t,25> maps{};std::vector<std::size_t> heads;
 for(const auto& c:chain)if(c.free){const auto [g,i]=bin(c.size);if(g>=25)throw std::invalid_argument("Free chunk outside the bin table");maps[g]|=1u<<i;groups|=1u<<g;const auto hd=head_of(c.size);if(std::find(heads.begin(),heads.end(),hd)==heads.end())heads.push_back(hd);}
 if(get<std::uint32_t>(s,0x20)!=groups)throw std::invalid_argument("Actual group map required");
 for(unsigned j=0;j<25;++j)if(get<std::uint32_t>(s,0x24+j*4)!=maps[j])throw std::invalid_argument("Actual bin maps required");
 for(std::size_t j=0x88;j<kFirst;j+=8)if(std::find(heads.begin(),heads.end(),j)==heads.end()&&get<std::uint64_t>(s,j)!=h)throw std::invalid_argument("All actual empty bin heads required");
 for(const auto hd:heads){
  std::vector<std::size_t> members;for(const auto& c:chain)if(c.free&&head_of(c.size)==hd)members.push_back(c.offset);
  auto node=get<std::uint64_t>(s,hd);std::uint64_t prev=h;std::size_t seen=0;
  while(node!=h){
   if(node<h+kFirst||node>=h+kGuard||seen>=members.size())throw std::invalid_argument("Bin list leaves the resource or is too long");
   const auto o=static_cast<std::size_t>(node-h);
   if(std::find(members.begin(),members.end(),o)==members.end()||get<std::uint64_t>(s,o+24)!=prev)throw std::invalid_argument("Actual bin list membership and back links required");
   prev=node;node=get<std::uint64_t>(s,o+16);++seen;
  }
  if(seen!=members.size())throw std::invalid_argument("Bin list misses a free chunk");
 }
 const auto p=pool_;
 if(get<std::uint64_t>(p,0x50)!=count||get<std::uint64_t>(p,0x60)!=requested||get<std::uint64_t>(p,0x70)!=occupied||get<std::uint64_t>(p,0x58)<count||get<std::uint64_t>(p,0x68)<requested||get<std::uint64_t>(p,0x78)<occupied)throw std::invalid_argument("Actual current statistics and retained maxima required");
 return chain;
}
BitmapAllocation BitmapNormalAllocator::allocate(std::uint64_t request,std::uint32_t alignment){
 if(alignment!=8||request<129||request>1024)throw std::invalid_argument("Observed NORMAL request129..1024 alignment8 required");
 (void)scan();auto s=resource();const auto h=resource_;const auto need=(request+15)&~std::uint64_t{7};
 const auto [lg,li]=lookup(need);
 unsigned g=lg,i=0;const auto inbin=get<std::uint32_t>(s,0x24+lg*4)&(li<32?~0u<<li:0u);
 if(inbin)i=static_cast<unsigned>(std::countr_zero(inbin));
 else{const auto higher=get<std::uint32_t>(s,0x20)&~((2u<<lg)-1u);if(!higher)throw std::invalid_argument("No fitting bin chunk: original owner-pool fallback not reconstructed here");g=static_cast<unsigned>(std::countr_zero(higher));i=static_cast<unsigned>(std::countr_zero(get<std::uint32_t>(s,0x24+g*4)));}
 const auto off=static_cast<std::size_t>(get<std::uint64_t>(s,0x88+static_cast<std::size_t>(g)*256+i*8)-h);
 if(get<std::uint64_t>(s,off+16)!=h)throw std::invalid_argument("Allocation from a multi-chunk bin is unobserved");
 const auto size=get<std::uint64_t>(s,off+8)&~std::uint64_t{3},end=off+static_cast<std::size_t>(size)+8;
 if(size<need)throw std::invalid_argument("Lookup returned a chunk smaller than the request");
 std::uint64_t charged=need;
 if(size-need>=32){
  // Split at the chunk start: in-use header, footer in the remainder prev-size word, remainder binned,
  // successor (or terminal guard) prev pointer moved to the remainder.
  const auto rem=size-need-8,tail=off+static_cast<std::size_t>(need)+8;
  unlink(s,h,off,size);put(s,off+8,need);put(s,tail,request);put(s,tail+8,rem|1);insert(s,h,tail,rem);put(s,end,h+tail);
 } else {
  if(end==kGuard)throw std::invalid_argument("Whole handout of the last chunk is unobserved");
  // Whole handout: header keeps the chunk size, footer in the successor prev-size word, successor bit1 cleared.
  unlink(s,h,off,size);put(s,off+8,size);put(s,end,request);put(s,end+8,get<std::uint64_t>(s,end+8)&~std::uint64_t{2});charged=size;
 }
 auto p=pool_;const auto count=get<std::uint64_t>(p,0x50)+1,requested=get<std::uint64_t>(p,0x60)+request,occupied=get<std::uint64_t>(p,0x70)+charged+8;
 put(p,0x70,occupied);put(p,0x78,std::max(get<std::uint64_t>(p,0x78),occupied));
 put(p,0x60,requested);put(p,0x68,std::max(get<std::uint64_t>(p,0x68),requested));
 put(p,0x50,count);put(p,0x58,std::max(get<std::uint64_t>(p,0x58),count));
 (void)scan();return {h+off+16,s.subspan(off+16,static_cast<std::size_t>(request))};
}
std::uint32_t BitmapNormalAllocator::free(std::uint64_t pointer){
 const auto chain=scan();auto s=resource();const auto h=resource_;
 if(pointer<h+kFirst+16||pointer>=h+kGuard)throw std::invalid_argument("Live NORMAL block identity required");
 const auto off=static_cast<std::size_t>(pointer-h-16);
 const auto it=std::find_if(chain.begin(),chain.end(),[&](const Chunk& c){return c.offset==off;});
 if(it==chain.end()||it->free)throw std::invalid_argument("Live NORMAL block identity required");
 const auto size=it->size,next=off+static_cast<std::size_t>(size)+8,request=get<std::uint64_t>(s,next);
 const auto header=get<std::uint64_t>(s,off+8);const bool prevfree=(header&2)!=0;
 const auto start=prevfree?static_cast<std::size_t>(get<std::uint64_t>(s,off)-h):off;
 const auto nexthdr=get<std::uint64_t>(s,next+8);const bool nextfree=next<kGuard&&(nexthdr&1)!=0;
 const auto end=nextfree?next+static_cast<std::size_t>(nexthdr&~std::uint64_t{3})+8:next;
 const auto merged=static_cast<std::uint64_t>(end-start)-8;
 if(prevfree)unlink(s,h,start,get<std::uint64_t>(s,start+8)&~std::uint64_t{3});
 if(nextfree)unlink(s,h,next,nexthdr&~std::uint64_t{3});
 // Original transient writes: absorbed headers keep their bits plus in-use clear / prev-free set,
 // the absorbed successor and the next live chunk (or terminal guard) point back at the merged start.
 if(prevfree)put(s,off+8,header|1);
 if(nextfree){put(s,next,h+start);put(s,next+8,nexthdr|2);}
 put(s,start+8,merged|1);put(s,end,h+start);put(s,end+8,get<std::uint64_t>(s,end+8)|2);
 insert(s,h,start,merged);
 auto p=pool_;put(p,0x70,get<std::uint64_t>(p,0x70)-size-8);put(p,0x60,get<std::uint64_t>(p,0x60)-request);put(p,0x50,get<std::uint64_t>(p,0x50)-1);
 (void)scan();return 0;
}
std::uint64_t BitmapNormalAllocator::requested_size(std::uint64_t pointer) const {
 const auto chain=scan();const auto s=resource();const auto off=static_cast<std::size_t>(pointer-resource_-16);
 if(std::none_of(chain.begin(),chain.end(),[&](const Chunk& c){return c.offset==off&&!c.free;}))throw std::invalid_argument("Live NORMAL block identity required");
 return bitmap_requested_size_large_slack(s,pointer);
}
}
