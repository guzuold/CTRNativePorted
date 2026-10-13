#include "ctr/atexit_single_finalize.h"
#include "ctr/atexit_embedded_registration.h"
#include "ctr/native_finalize_thread_services.h"
#include <cstring>
#include <stdexcept>
namespace ctr {namespace {
template<class T>T read(std::span<std::byte> page,std::size_t off){T value{};std::memcpy(&value,page.data()+off,sizeof(value));return value;}
template<class T>void write(std::span<std::byte> page,std::size_t off,T value){std::memcpy(page.data()+off,&value,sizeof(value));}
void require(bool ok){if(!ok)throw std::invalid_argument("Exclusive single kind-2 embedded finalizer contract required");}
struct Services {
 NativeFinalizeThreadServices* owner{create_finalize_thread_services(0x50124268)};
 ~Services(){if(owner)(void)release_finalize_thread_services(owner);}
};
}
SingleFinalizeResult finalize_single_embedded(const std::shared_ptr<NativeAtexitBss>& registry,
 const std::shared_ptr<NativeTypedGameHeapCallback>& callback,
 std::uint64_t dso_filter,std::uint64_t thread_exit_list){
 require(registry&&callback&&dso_filter==0&&thread_exit_list==0);
 const auto retained_registry=registry;const auto retained_callback=callback;
 auto page=retained_registry->page();const auto base=reinterpret_cast<std::uint64_t>(page.data());
 constexpr std::size_t node=0xc70;
 require(read<std::uint32_t>(page,0x1870)==0&&read<std::uint32_t>(page,0xc68)==1);
 require(read<std::uint64_t>(page,0xc60)==base+node);
 require(read<std::uint64_t>(page,node)==0&&read<std::uint64_t>(page,node+8)==0);
 require(read<std::uint32_t>(page,node+16)==2);
 const auto code=read<std::uint64_t>(page,node+24),argument=read<std::uint64_t>(page,node+32),dso=read<std::uint64_t>(page,node+40);
 require(code==original_game_heap_callback_id&&argument==original_game_heap_argument_id&&dso==original_game_dso_id);
 for(std::size_t i=node+48;i<node+64*48;++i)require(page[i]==std::byte{});
 for(std::size_t i=NativeAtexitBss::source_extent;i<page.size();++i)require(page[i]==std::byte{0xa5});
 const auto state=retained_callback->status();require(!state.invoked&&!state.backend_released&&state.code_module&&state.dso_address);
 retained_callback->validate_finalizer_frame();
 Services services;require(services.owner!=nullptr);const auto cookie=atexit_actual_cookie();
 // adaaf disables the selected record BEFORE the typed call. The three IDs
 // are validated and resolved by the compiled adapter, never cast to code.
 write(page,node+16,std::uint32_t{});
 std::array<std::byte,8192> callback_registry{};
 std::memcpy(callback_registry.data(),page.data(),page.size());
 // Assert the actual owned head before serializing its declared logical map.
 require(read<std::uint64_t>(page,0xc60)==reinterpret_cast<std::uint64_t>(page.data()+node));
 const std::uint64_t logical_head=NativeAtexitBss::logical_base+node;
 std::memcpy(callback_registry.data()+0xc60,&logical_head,8);
 auto snapshots=retained_callback->dispatch_finalizer(code,argument,dso);
 // adb03..adb53: one genuine embedded first node, with no neighbours.
 // The bounds preflight above excludes the external-node free branch.
 require(read<std::uint64_t>(page,0xc60)==base+node&&read<std::uint64_t>(page,node)==0&&read<std::uint64_t>(page,node+8)==0);
 write(page,0xc60,std::uint64_t{});write(page,0xc68,std::uint32_t{});
 std::memset(page.data()+node+16,0,32);
 // 2ebc0 -> 2ebd0 empty-list path still calls REAL self/lock/unlock.
 // No nonempty-list service or destructor is silently skipped.
 std::uint64_t thread{};
 if(finalize_thread_self(services.owner,&thread)!=0||thread==0)throw std::runtime_error("Actual thread identity unavailable");
 if(finalize_mutex_lock(services.owner,0x50124268)!=0)throw std::runtime_error("Actual finalize mutex lock failed");
 if(finalize_mutex_unlock(services.owner,0x50124268)!=0)throw std::runtime_error("Actual finalize mutex unlock failed");
 if(cookie!=atexit_actual_cookie())throw std::runtime_error("Actual finalizer cookie changed");
 if(release_finalize_thread_services(services.owner)!=0)throw std::runtime_error("Finalize mutex remained locked");
 services.owner=nullptr;
 require(retained_callback->status().backend_released);
 return {std::move(snapshots),callback_registry,thread,1};
}
}
