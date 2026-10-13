#include "ctr/atexit_reregistration.h"
#include <cstring>
#include <stdexcept>
namespace ctr {namespace {
constexpr std::uint32_t none=64;
template<class T>T get(std::span<const std::byte>b,std::size_t off){T v{};std::memcpy(&v,b.data()+off,sizeof(v));return v;}
template<class T>void put(std::span<std::byte>b,std::size_t off,T v){std::memcpy(b.data()+off,&v,sizeof(v));}
void need(bool ok){if(!ok)throw std::invalid_argument("Single removed embedded slot and numeric identities required");}
std::size_t off(std::uint32_t i){return 0xc70+48*static_cast<std::size_t>(i);}
std::uint64_t ptr(std::uint64_t base,std::uint32_t i){return i==none?0:base+off(i);}
std::uint32_t oldhead(BoundedWalkLayout l){return l.live==l.count-1?(l.live?l.live-1:none):l.count-1;}
void bytes(NativeAtexitBss& owner,BoundedWalkLayout l,bool replaced){
 auto b=owner.page();need(l.count>=1&&l.count<=64&&l.live<l.count&&get<std::uint32_t>(b,0xc68)==l.count-(replaced?0:1)&&get<std::uint32_t>(b,0x1870)==0);
 for(std::uint32_t i=0;i<l.count;++i){const auto start=off(i);
  if(i==l.live&&!replaced){for(std::size_t j=16;j<48;++j)need(b[start+j]==std::byte{});}
  else need(get<std::uint32_t>(b,start+16)==(i==l.live?2u:0u)&&get<std::uint64_t>(b,start+24)==original_game_heap_callback_id&&get<std::uint64_t>(b,start+32)==original_game_heap_argument_id&&get<std::uint64_t>(b,start+40)==original_game_dso_id);
 }
 for(std::size_t j=off(l.count);j<off(64);++j)need(b[j]==std::byte{});
 for(std::size_t j=NativeAtexitBss::source_extent;j<b.size();++j)need(b[j]==std::byte{0xa5});
}
}
std::array<std::byte,8192> serialize_reregistered(const NativeAtexitBss& owner,BoundedWalkLayout l){
 auto b=owner.page();const auto base=reinterpret_cast<std::uint64_t>(b.data());need(l.count>=1&&l.count<=64&&l.live<l.count);
 std::array<std::byte,8192> out{};std::memcpy(out.data(),b.data(),8192);auto target=std::span<std::byte>(out);
 auto map=[&](std::size_t cell,std::uint32_t index){need(get<std::uint64_t>(b,cell)==ptr(base,index));put(target,cell,ptr(NativeAtexitBss::logical_base,index));};map(0xc60,l.live);
 for(std::uint32_t i=0;i<l.count;++i){auto prev=i?i-1:none,next=i+1<l.count?i+1:none;
  if(i==l.live){prev=oldhead(l);next=none;}else{if(prev==l.live)prev=l.live?l.live-1:none;if(next==l.live)next=l.live+1<l.count?l.live+1:none;if(i==oldhead(l))next=l.live;}
  map(off(i),prev);map(off(i)+8,next);
 }
 return out;
}
ReregistrationResult register_removed_embedded(const std::shared_ptr<NativeAtexitBss>& owner,BoundedWalkLayout l,const std::array<std::uint64_t,16>& entry,std::span<std::byte> stack,std::uint64_t entry_flags){
 need(static_cast<bool>(owner));const auto retained=owner;auto b=owner->page();const auto base=reinterpret_cast<std::uint64_t>(b.data()),address=reinterpret_cast<std::uint64_t>(stack.data());
 need(stack.size()==128&&entry[14]==0x2000fff8&&entry[5]==original_game_heap_callback_id&&entry[4]==original_game_heap_argument_id&&entry[3]==original_game_dso_id&&(address>=base+8192||(address<=base&&128<=base-address)));
 bytes(*owner,l,false);(void)serialize_bounded_walk(*owner,l,true);
 std::vector<std::uint32_t> scan;scan.reserve(l.count);std::uint32_t selected=none;
 // Original ad8d0 tests callback qword; kind0 with metadata is occupied.
 for(std::uint32_t i=0;i<64;++i){scan.push_back(i);if(get<std::uint64_t>(b,off(i)+24)==0){selected=i;break;}}
 need(selected==l.live&&scan.size()==l.live+1);const auto cookie=atexit_actual_cookie();
 put(stack,112,entry[15]);put(stack,104,entry[1]);put(stack,96,cookie);put(stack,64,std::uint32_t{2});put(stack,72,entry[5]);put(stack,80,entry[4]);put(stack,88,entry[3]);put(stack,56,std::uint64_t{0x500ad9d3});put(stack,48,std::uint64_t{0x2000fff0});put(stack,40,entry[13]);put(stack,32,entry[12]);put(stack,24,entry[10]);put(stack,16,atexit_actual_cookie_cell());
 const auto previous=get<std::uint64_t>(b,0xc60);put(b,off(selected),previous);put(b,off(selected)+8,std::uint64_t{});if(previous)put(b,off(oldhead(l))+8,base+off(selected));put(b,0xc60,base+off(selected));put(b,0xc68,l.count);std::memcpy(b.data()+off(selected)+16,stack.data()+64,32);
 if(get<std::uint64_t>(stack,96)!=atexit_actual_cookie())throw std::runtime_error("Actual cookie changed");bytes(*owner,l,true);(void)serialize_reregistered(*owner,l);
 AtexitCallResult result{entry};result.registers[0]=0;result.registers[2]=cookie;result.registers[5]=0x2000ffc0;result.registers[14]=0x20010000;
 // Last arithmetic is original add RSP,28: ffc0 -> ffe8 (PF set).
 return {result,(entry_flags&~std::uint64_t{0x8d5})|0x4,std::move(scan)};
}
}
