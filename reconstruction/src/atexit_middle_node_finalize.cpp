#include "ctr/atexit_single_finalize.h"
#include "ctr/atexit_embedded_registration.h"
#include "ctr/native_finalize_thread_services.h"
#include <cstring>
#include <stdexcept>
#include <utility>
namespace ctr {namespace {
template<class T>T get(std::span<std::byte> b,std::size_t off){T v{};std::memcpy(&v,b.data()+off,sizeof(v));return v;}
template<class T>void put(std::span<std::byte> b,std::size_t off,T v){std::memcpy(b.data()+off,&v,sizeof(v));}
void need(bool ok){if(!ok)throw std::invalid_argument("Exclusive three-node registry: disabled endpoints and live kind2 middle required");}
struct Services{NativeFinalizeThreadServices* owner{create_finalize_thread_services(0x50124268)};~Services(){if(owner)(void)release_finalize_thread_services(owner);}};
}
std::array<std::byte,8192> serialize_middle_node_registry(const NativeAtexitBss& registry,bool live_removed){
 auto b=registry.page();const auto base=reinterpret_cast<std::uint64_t>(b.data());
 const std::array<std::pair<std::size_t,std::size_t>,7> links{{
  {0xc60,0xcd0},{0xc70,0},{0xc78,live_removed?0xcd0:0xca0},
  {0xca0,0xc70},{0xca8,0xcd0},{0xcd0,live_removed?0xc70:0xca0},{0xcd8,0}}};
 std::array<std::byte,8192> out{};std::memcpy(out.data(),b.data(),b.size());auto mapped=std::span<std::byte>(out);
 for(const auto& [cell,target]:links){need(get<std::uint64_t>(b,cell)==(target?base+target:0));put(mapped,cell,target?NativeAtexitBss::logical_base+target:0);}
 return out;
}
SingleFinalizeResult finalize_three_node_live_middle(const std::shared_ptr<NativeAtexitBss>& registry,
 const std::shared_ptr<NativeTypedGameHeapCallback>& callback,std::uint64_t dso_filter,std::uint64_t thread_exit_list){
 need(registry&&callback&&dso_filter==0&&thread_exit_list==0);
 const auto retained_registry=registry;const auto retained_callback=callback;
 auto b=registry->page();const auto base=reinterpret_cast<std::uint64_t>(b.data());
 need(get<std::uint32_t>(b,0x1870)==0&&get<std::uint32_t>(b,0xc68)==3);
 (void)serialize_middle_node_registry(*registry,false);
 need(get<std::uint32_t>(b,0xc80)==0&&get<std::uint32_t>(b,0xcb0)==2&&get<std::uint32_t>(b,0xce0)==0);
 for(auto node:{std::size_t{0xc70},std::size_t{0xca0},std::size_t{0xcd0}})
  need(get<std::uint64_t>(b,node+24)==original_game_heap_callback_id&&get<std::uint64_t>(b,node+32)==original_game_heap_argument_id&&get<std::uint64_t>(b,node+40)==original_game_dso_id);
 for(std::size_t i=0xd00;i<0xc70+64*48;++i)need(b[i]==std::byte{});
 for(std::size_t i=NativeAtexitBss::source_extent;i<b.size();++i)need(b[i]==std::byte{0xa5});
 const auto state=callback->status();need(!state.invoked&&!state.backend_released&&state.code_module&&state.dso_address);
 callback->validate_finalizer_frame();Services services;need(services.owner!=nullptr);const auto cookie=atexit_actual_cookie();
 // ada70/adae0: prepared head kind0 is skipped BEFORE one real callback.
 need(get<std::uint64_t>(b,0xcd0)==base+0xca0);put(b,0xcb0,std::uint32_t{});
 const auto callback_registry=serialize_middle_node_registry(*registry,false);
 auto snapshots=callback->dispatch_finalizer(original_game_heap_callback_id,original_game_heap_argument_id,original_game_dso_id);
 (void)serialize_middle_node_registry(*registry,false);
 // Non-head removal, BOTH reciprocal writes, head unchanged. The removed
 // middle keeps both owned neighbour identities; only32data bytes clear.
 put(b,0xcd0,base+0xc70);put(b,0xc78,base+0xcd0);put(b,0xc68,std::uint32_t{2});std::memset(b.data()+0xcb0,0,32);
 // ada70/adae0: prepared first predecessor skipped AFTER callback return.
 need(get<std::uint32_t>(b,0xc80)==0&&get<std::uint64_t>(b,0xc70)==0);
 std::uint64_t thread{};
 if(finalize_thread_self(services.owner,&thread)!=0||!thread)throw std::runtime_error("Actual thread self failed");
 if(finalize_mutex_lock(services.owner,0x50124268)!=0)throw std::runtime_error("Actual helper lock failed");
 if(finalize_mutex_unlock(services.owner,0x50124268)!=0)throw std::runtime_error("Actual helper unlock failed");
 if(cookie!=atexit_actual_cookie()||release_finalize_thread_services(services.owner)!=0)throw std::runtime_error("Actual helper cookie or release failed");
 services.owner=nullptr;need(retained_callback->status().backend_released);
 return {std::move(snapshots),callback_registry,thread,1};
}
}
