#include "ctr/atexit_reused_head_finalize.h"
#include "ctr/native_finalize_thread_services.h"
#include <cstring>
#include <stdexcept>
namespace ctr {namespace {
constexpr std::uint32_t none=64;
template<class T>T get(std::span<const std::byte>b,std::size_t off){T v{};std::memcpy(&v,b.data()+off,sizeof(v));return v;}
template<class T>void put(std::span<std::byte>b,std::size_t off,T v){std::memcpy(b.data()+off,&v,sizeof(v));}
void need(bool ok){if(!ok)throw std::invalid_argument("Owned reused head with exactly one fresh typed callback required");}
std::size_t off(std::uint32_t i){return 0xc70+48*static_cast<std::size_t>(i);}
std::uint64_t ptr(std::uint64_t base,std::uint32_t i){return i==none?0:base+off(i);}
std::uint32_t oldhead(BoundedWalkLayout l){return l.live==l.count-1?(l.live?l.live-1:none):l.count-1;}
void validate(const NativeAtexitBss& owner,BoundedWalkLayout l){
 auto b=owner.page();need(l.count>=1&&l.count<=64&&l.live<l.count&&get<std::uint32_t>(b,0xc68)==l.count&&get<std::uint32_t>(b,0x1870)==0);
 (void)serialize_reregistered(owner,l);
 for(std::uint32_t i=0;i<l.count;++i){const auto start=off(i);need(get<std::uint32_t>(b,start+16)==(i==l.live?2u:0u)&&get<std::uint64_t>(b,start+24)==original_game_heap_callback_id&&get<std::uint64_t>(b,start+32)==original_game_heap_argument_id&&get<std::uint64_t>(b,start+40)==original_game_dso_id);}
 for(std::size_t j=off(l.count);j<off(64);++j)need(b[j]==std::byte{});
 for(std::size_t j=NativeAtexitBss::source_extent;j<b.size();++j)need(b[j]==std::byte{0xa5});
}
struct Services{NativeFinalizeThreadServices* owner{create_finalize_thread_services(0x50124268)};~Services(){if(owner)(void)release_finalize_thread_services(owner);}};
}
FinalizeCallerFrame generate_reused_head_call(const NativeAtexitBss& owner,BoundedWalkLayout l,const FinalizeCallerFrame& entry){
 validate(owner,l);const auto& e=entry.registers;need(e[14]==0x2000fff8&&e[5]==0&&get<std::uint64_t>(entry.stack,1016)==0x30000000);
 FinalizeCallerFrame out=entry;auto s=std::span<std::byte>(out.stack);auto& r=out.registers;
 put(s,1008,e[15]);put(s,1000,e[13]);put(s,992,e[12]);put(s,984,e[11]);put(s,976,e[10]);put(s,968,e[1]);put(s,960,atexit_actual_cookie());put(s,532,std::uint32_t{});
 r[0]=0x50127870;r[1]=ptr(NativeAtexitBss::logical_base,l.live);r[5]=original_game_heap_argument_id;r[10]=0;r[11]=original_game_heap_callback_id;r[12]=original_game_heap_argument_id;r[13]=2;r[14]=0x2000fe08;r[15]=0x2000fff0;
 put(s,520,std::uint64_t{0x500adad9});out.flags=(entry.flags&~std::uint64_t{0x8d5})|0x44;return out;
}
FinalizeCallerFrame complete_reused_head_return(const FinalizeCallerFrame& entry,const FinalizeCallerFrame& generated,std::span<const std::byte> callback,BoundedWalkLayout l){
 need(l.count>=1&&l.count<=64&&l.live<l.count);auto out=complete_single_finalize_return(entry,generated,callback);
 // Every surviving disabled record is visited AFTER the live reused HEAD.
 put(std::span<std::byte>(out.stack),488,std::uint64_t{l.count==1?2u:0u});return out;
}
std::array<std::byte,8192> serialize_reused_head_removed(const NativeAtexitBss& owner,BoundedWalkLayout l){
 need(l.count>=1&&l.count<=64&&l.live<l.count);auto b=owner.page();const auto base=reinterpret_cast<std::uint64_t>(b.data());std::array<std::byte,8192> out{};std::memcpy(out.data(),b.data(),8192);auto target=std::span<std::byte>(out);
 auto map=[&](std::size_t cell,std::uint32_t index){need(get<std::uint64_t>(b,cell)==ptr(base,index));put(target,cell,ptr(NativeAtexitBss::logical_base,index));};map(0xc60,oldhead(l));
 for(std::uint32_t i=0;i<l.count;++i){auto prev=i?i-1:none,next=i+1<l.count?i+1:none;if(i==l.live){prev=oldhead(l);next=none;}else{if(prev==l.live)prev=l.live?l.live-1:none;if(next==l.live)next=l.live+1<l.count?l.live+1:none;}map(off(i),prev);map(off(i)+8,next);}
 return out;
}
BoundedWalkResult finalize_reused_head(const std::shared_ptr<NativeAtexitBss>& owner,BoundedWalkLayout l,const std::shared_ptr<NativeTypedGameHeapCallback>& callback,std::uint64_t dso_filter,std::uint64_t thread_exit_list){
 need(owner&&callback&&dso_filter==0&&thread_exit_list==0);const auto retained_owner=owner;const auto retained_callback=callback;validate(*owner,l);
 auto state=callback->status();need(!state.invoked&&!state.backend_released&&state.code_module&&state.dso_address);callback->validate_finalizer_frame();
 auto b=owner->page();const auto base=reinterpret_cast<std::uint64_t>(b.data());Services services;need(services.owner!=nullptr);const auto cookie=atexit_actual_cookie();SingleFinalizeResult result{};std::vector<std::uint32_t> order;order.reserve(l.count);bool called=false;
 for(auto i=l.live;i!=none;){const auto previous=get<std::uint64_t>(b,off(i));std::uint32_t next_index=none;
  if(previous){need(previous>=base+off(0)&&previous<base+off(l.count)&&(previous-base-off(0))%48==0);next_index=static_cast<std::uint32_t>((previous-base-off(0))/48);}
  need(order.size()<l.count);order.push_back(i);const auto kind=get<std::uint32_t>(b,off(i)+16);
  if(kind==0){need(called&&i!=l.live);i=next_index;continue;}
  need(kind==2&&i==l.live&&!called);called=true;put(b,off(i)+16,std::uint32_t{});result.callback_registry=serialize_reregistered(*owner,l);
  result.callback_snapshots=callback->dispatch_finalizer(original_game_heap_callback_id,original_game_heap_argument_id,original_game_dso_id);(void)serialize_reregistered(*owner,l);
  put(b,0xc60,previous);if(previous)put(b,off(next_index)+8,std::uint64_t{});put(b,0xc68,l.count-1);std::memset(b.data()+off(i)+16,0,32);i=next_index;
 }
 need(called&&order.size()==l.count&&callback->status().backend_released);
 if(finalize_thread_self(services.owner,&result.actual_thread)!=0||!result.actual_thread)throw std::runtime_error("Actual thread self failed");
 if(finalize_mutex_lock(services.owner,0x50124268)!=0||finalize_mutex_unlock(services.owner,0x50124268)!=0)throw std::runtime_error("Actual helper lock failed");
 if(cookie!=atexit_actual_cookie()||release_finalize_thread_services(services.owner)!=0)throw std::runtime_error("Actual cookie/release failed");services.owner=nullptr;result.selected_callbacks=1;(void)serialize_reused_head_removed(*owner,l);return {std::move(result),std::move(order)};
}
}
