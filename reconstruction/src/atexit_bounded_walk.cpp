#include "ctr/atexit_bounded_walk.h"
#include "ctr/atexit_embedded_registration.h"
#include "ctr/native_finalize_thread_services.h"
#include <cstring>
#include <stdexcept>
namespace ctr {namespace {
constexpr std::uint32_t none=64;constexpr std::size_t first=0xc70,stride=48;
template<class T>T get(std::span<const std::byte> b,std::size_t off){T v{};std::memcpy(&v,b.data()+off,sizeof(v));return v;}
template<class T>void put(std::span<std::byte> b,std::size_t off,T v){std::memcpy(b.data()+off,&v,sizeof(v));}
void need(bool ok){if(!ok)throw std::invalid_argument("Owned bounded registry requires exactly one kind2 and prepared kind0 records");}
std::size_t offset(std::uint32_t i){return first+stride*i;}
std::uint64_t pointer(std::uint64_t base,std::uint32_t i){return i==none?0:base+offset(i);}
struct Services{NativeFinalizeThreadServices* owner{create_finalize_thread_services(0x50124268)};~Services(){if(owner)(void)release_finalize_thread_services(owner);}};
}
std::array<std::byte,8192> serialize_bounded_walk(const NativeAtexitBss& registry,BoundedWalkLayout layout,bool removed){
 need(layout.count>=1&&layout.count<=64&&layout.live<layout.count);
 auto b=registry.page();const auto base=reinterpret_cast<std::uint64_t>(b.data());
 std::array<std::byte,8192> out{};std::memcpy(out.data(),b.data(),b.size());auto mapped=std::span<std::byte>(out);
 auto map=[&](std::size_t cell,std::uint32_t target){need(get<std::uint64_t>(b,cell)==pointer(base,target));put(mapped,cell,pointer(NativeAtexitBss::logical_base,target));};
 auto head=layout.count-1;if(removed&&head==layout.live)head=head?head-1:none;map(0xc60,head);
 for(std::uint32_t i=0;i<layout.count;++i){
  auto prev=i?i-1:none,next=i+1<layout.count?i+1:none;
  if(removed&&i!=layout.live){if(prev==layout.live)prev=layout.live?layout.live-1:none;if(next==layout.live)next=layout.live+1<layout.count?layout.live+1:none;}
  map(offset(i),prev);map(offset(i)+8,next);
 }
 return out;
}
BoundedWalkLayout validate_bounded_walk(const NativeAtexitBss& registry){
 auto b=registry.page();BoundedWalkLayout layout{get<std::uint32_t>(b,0xc68),none};
 need(layout.count>=1&&layout.count<=64&&get<std::uint32_t>(b,0x1870)==0);
 for(std::uint32_t i=0;i<layout.count;++i){const auto off=offset(i);const auto kind=get<std::uint32_t>(b,off+16);
  need(kind==0||kind==2);if(kind==2){need(layout.live==none);layout.live=i;}
  need(get<std::uint64_t>(b,off+24)==original_game_heap_callback_id&&get<std::uint64_t>(b,off+32)==original_game_heap_argument_id&&get<std::uint64_t>(b,off+40)==original_game_dso_id);
 }
 need(layout.live!=none);(void)serialize_bounded_walk(registry,layout,false);
 for(std::size_t i=offset(layout.count);i<offset(64);++i)need(b[i]==std::byte{});
 for(std::size_t i=NativeAtexitBss::source_extent;i<b.size();++i)need(b[i]==std::byte{0xa5});
 return layout;
}
FinalizeCallerFrame generate_bounded_walk_call(const NativeAtexitBss& registry,const FinalizeCallerFrame& entry){
 const auto layout=validate_bounded_walk(registry);const auto& e=entry.registers;
 need(e[14]==0x2000fff8&&e[5]==0&&get<std::uint64_t>(entry.stack,1016)==0x30000000);
 FinalizeCallerFrame out=entry;auto s=std::span<std::byte>(out.stack);auto& r=out.registers;
 put(s,1008,e[15]);put(s,1000,e[13]);put(s,992,e[12]);put(s,984,e[11]);put(s,976,e[10]);put(s,968,e[1]);put(s,960,atexit_actual_cookie());put(s,532,std::uint32_t{});
 r[0]=0x50127870;r[1]=pointer(NativeAtexitBss::logical_base,layout.live);r[5]=original_game_heap_argument_id;
 r[10]=0;r[11]=original_game_heap_callback_id;r[12]=original_game_heap_argument_id;r[13]=2;r[14]=0x2000fe08;r[15]=0x2000fff0;
 put(s,520,std::uint64_t{0x500adad9});out.flags=(entry.flags&~std::uint64_t{0x8d5})|0x44;return out;
}
FinalizeCallerFrame complete_bounded_walk_return(const FinalizeCallerFrame& entry,const FinalizeCallerFrame& generated,std::span<const std::byte> callback_output,BoundedWalkLayout layout){
 need(layout.count>=1&&layout.count<=64&&layout.live<layout.count);
 auto out=complete_single_finalize_return(entry,generated,callback_output);
 put(std::span<std::byte>(out.stack),488,std::uint64_t{layout.live==0?2u:0u});
 // RCX is overwritten by adb1f only when the live node has a next neighbour.
 if(layout.live+1<layout.count)out.registers[2]=pointer(NativeAtexitBss::logical_base,layout.live?layout.live-1:none);
 return out;
}
BoundedWalkResult finalize_bounded_walk(const std::shared_ptr<NativeAtexitBss>& registry,const std::shared_ptr<NativeTypedGameHeapCallback>& callback,std::uint64_t dso_filter,std::uint64_t thread_exit_list){
 need(registry&&callback&&dso_filter==0&&thread_exit_list==0);
 const auto retained_registry=registry;const auto retained_callback=callback;
 const auto layout=validate_bounded_walk(*registry);auto b=registry->page();const auto base=reinterpret_cast<std::uint64_t>(b.data());
 const auto state=callback->status();need(!state.invoked&&!state.backend_released&&state.code_module&&state.dso_address);callback->validate_finalizer_frame();
 Services services;need(services.owner!=nullptr);const auto cookie=atexit_actual_cookie();
 std::vector<std::uint32_t> order;order.reserve(layout.count);SingleFinalizeResult result{};bool called=false;
 for(auto i=layout.count-1;i!=none;){
  const auto off=offset(i);const auto prev=i?i-1:none,next=i+1<layout.count?i+1:none;
  // No foreign pointer dereference: validate physical owned previous link.
  need(get<std::uint64_t>(b,off)==pointer(base,prev));order.push_back(i);
  const auto kind=get<std::uint32_t>(b,off+16);
  if(kind==0){i=prev;continue;} // original ada70 -> adae0 -> adb68
  need(kind==2&&i==layout.live&&!called);called=true;put(b,off+16,std::uint32_t{});
  result.callback_registry=serialize_bounded_walk(*registry,layout,false);
  result.callback_snapshots=callback->dispatch_finalizer(original_game_heap_callback_id,original_game_heap_argument_id,original_game_dso_id);
  (void)serialize_bounded_walk(*registry,layout,false);
  if(get<std::uint64_t>(b,0xc60)==pointer(base,i))put(b,0xc60,pointer(base,prev));
  if(next!=none)put(b,offset(next),pointer(base,prev));
  if(prev!=none)put(b,offset(prev)+8,pointer(base,next));
  put(b,0xc68,layout.count-1);std::memset(b.data()+off+16,0,32);i=prev;
 }
 need(called&&order.size()==layout.count&&callback->status().backend_released);
 if(finalize_thread_self(services.owner,&result.actual_thread)!=0||!result.actual_thread)throw std::runtime_error("Actual thread self failed");
 if(finalize_mutex_lock(services.owner,0x50124268)!=0)throw std::runtime_error("Actual helper lock failed");
 if(finalize_mutex_unlock(services.owner,0x50124268)!=0)throw std::runtime_error("Actual helper unlock failed");
 if(cookie!=atexit_actual_cookie()||release_finalize_thread_services(services.owner)!=0)throw std::runtime_error("Actual cookie/release failed");
 services.owner=nullptr;result.selected_callbacks=1;(void)serialize_bounded_walk(*registry,layout,true);
 return {std::move(result),std::move(order)};
}
}
