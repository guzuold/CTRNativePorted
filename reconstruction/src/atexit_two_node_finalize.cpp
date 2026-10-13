#include "ctr/atexit_single_finalize.h"
#include "ctr/atexit_embedded_registration.h"
#include "ctr/native_finalize_thread_services.h"
#include <cstring>
#include <stdexcept>
namespace ctr {namespace {
template<class T>T get(std::span<std::byte> b,std::size_t off){T v{};std::memcpy(&v,b.data()+off,sizeof(v));return v;}
template<class T>void put(std::span<std::byte> b,std::size_t off,T v){std::memcpy(b.data()+off,&v,sizeof(v));}
void need(bool ok){if(!ok)throw std::invalid_argument("Exclusive two embedded nodes: disabled predecessor and live kind2 required");}
struct Services{NativeFinalizeThreadServices* owner{create_finalize_thread_services(0x50124268)};~Services(){if(owner)(void)release_finalize_thread_services(owner);}};
}
static std::array<std::byte,8192> serialize_two_layout(const NativeAtexitBss& registry,bool live_removed,bool disabled_head){
 auto b=registry.page();const auto base=reinterpret_cast<std::uint64_t>(b.data());
 const auto head=disabled_head?0xca0:live_removed?0xc70:0xca0;
 const auto first_next=(disabled_head||!live_removed)?base+0xca0:0;
 const auto second_previous=(disabled_head&&live_removed)?0:base+0xc70;
 need(get<std::uint64_t>(b,0xc60)==base+head);
 need(get<std::uint64_t>(b,0xc70)==0&&get<std::uint64_t>(b,0xc78)==first_next);
 need(get<std::uint64_t>(b,0xca0)==second_previous&&get<std::uint64_t>(b,0xca8)==0);
 std::array<std::byte,8192> out{};std::memcpy(out.data(),b.data(),b.size());
 auto mapped=std::span<std::byte>(out);put(mapped,0xc60,NativeAtexitBss::logical_base+head);
 if(first_next)put(mapped,0xc78,NativeAtexitBss::logical_base+0xca0);
 if(second_previous)put(mapped,0xca0,NativeAtexitBss::logical_base+0xc70);return out;
}
std::array<std::byte,8192> serialize_two_node_registry(const NativeAtexitBss& registry,bool live_removed){return serialize_two_layout(registry,live_removed,false);}
std::array<std::byte,8192> serialize_disabled_head_registry(const NativeAtexitBss& registry,bool live_removed){return serialize_two_layout(registry,live_removed,true);}
static SingleFinalizeResult finalize_two_layout(const std::shared_ptr<NativeAtexitBss>& registry,
 const std::shared_ptr<NativeTypedGameHeapCallback>& callback,std::uint64_t dso_filter,std::uint64_t thread_exit_list,bool disabled_head){
 need(registry&&callback&&dso_filter==0&&thread_exit_list==0);
 const auto retained_registry=registry;const auto retained_callback=callback;
 auto b=registry->page();const auto base=reinterpret_cast<std::uint64_t>(b.data());
 need(get<std::uint32_t>(b,0x1870)==0&&get<std::uint32_t>(b,0xc68)==2);
 (void)serialize_two_layout(*registry,false,disabled_head);
 const std::size_t live=disabled_head?0xc70:0xca0,disabled=disabled_head?0xca0:0xc70;
 need(get<std::uint32_t>(b,live+16)==2&&get<std::uint32_t>(b,disabled+16)==0);
 for(auto node:{std::size_t{0xc70},std::size_t{0xca0}}){
  need(get<std::uint64_t>(b,node+24)==original_game_heap_callback_id&&get<std::uint64_t>(b,node+32)==original_game_heap_argument_id&&get<std::uint64_t>(b,node+40)==original_game_dso_id);
 }
 for(std::size_t i=0xcd0;i<0xc70+64*48;++i)need(b[i]==std::byte{});
 for(std::size_t i=NativeAtexitBss::source_extent;i<b.size();++i)need(b[i]==std::byte{0xa5});
 const auto state=callback->status();need(!state.invoked&&!state.backend_released&&state.code_module&&state.dso_address);
 callback->validate_finalizer_frame();Services services;need(services.owner!=nullptr);const auto cookie=atexit_actual_cookie();
 // Source head is second node. Optional head-kind0 skip selects its live
 // predecessor; disable the selected live node before the one real call.
 if(disabled_head)need(get<std::uint32_t>(b,0xcb0)==0&&get<std::uint64_t>(b,0xca0)==base+live); // actual kind0 skip-before-call
 put(b,live+16,std::uint32_t{});const auto callback_registry=serialize_two_layout(*registry,false,disabled_head);
 auto snapshots=callback->dispatch_finalizer(original_game_heap_callback_id,original_game_heap_argument_id,original_game_dso_id);
 need(get<std::uint64_t>(b,0xc60)==base+0xca0&&get<std::uint64_t>(b,0xca0)==base+0xc70&&get<std::uint64_t>(b,0xca8)==0);
 // adb03..adb53: remove only the selected live record. Preserve its link
 // words, clear ONLY32bytes, and update the corresponding neighbour link.
 if(disabled_head){
  // adb03 branch is non-head: preserve head, next.previous=live.previous=0.
  need(get<std::uint64_t>(b,live)==0&&get<std::uint64_t>(b,live+8)==base+disabled);
  put(b,disabled,std::uint64_t{});
 }else{put(b,0xc60,base+0xc70);put(b,0xc78,std::uint64_t{});}
 put(b,0xc68,std::uint32_t{1});std::memset(b.data()+live+16,0,32);
 // With disabled predecessor, ada70 kind0 -> adae0 skips it AFTER callback.
 // With disabled head, live.previous==0 terminates without revisiting head.
 // In both layouts retain disabled padding and numeric callback metadata.
 need(get<std::uint32_t>(b,disabled+16)==0&&get<std::uint64_t>(b,live)==(disabled_head?0:base+disabled));
 std::uint64_t thread{};
 if(finalize_thread_self(services.owner,&thread)!=0||!thread)throw std::runtime_error("Real thread self failed");
 if(finalize_mutex_lock(services.owner,0x50124268)!=0)throw std::runtime_error("Real helper lock failed");
 if(finalize_mutex_unlock(services.owner,0x50124268)!=0)throw std::runtime_error("Real helper unlock failed");
 if(cookie!=atexit_actual_cookie()||release_finalize_thread_services(services.owner)!=0)throw std::runtime_error("Helper cookie or owner release failed");
 services.owner=nullptr;need(retained_callback->status().backend_released);
 return {std::move(snapshots),callback_registry,thread,1};
}
SingleFinalizeResult finalize_two_node_disabled_predecessor(const std::shared_ptr<NativeAtexitBss>& registry,const std::shared_ptr<NativeTypedGameHeapCallback>& callback,std::uint64_t dso_filter,std::uint64_t thread_exit_list){return finalize_two_layout(registry,callback,dso_filter,thread_exit_list,false);}
SingleFinalizeResult finalize_two_node_disabled_head(const std::shared_ptr<NativeAtexitBss>& registry,const std::shared_ptr<NativeTypedGameHeapCallback>& callback,std::uint64_t dso_filter,std::uint64_t thread_exit_list){return finalize_two_layout(registry,callback,dso_filter,thread_exit_list,true);}
}
