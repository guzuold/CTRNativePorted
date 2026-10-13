#pragma once
#include "ctr/game_heap_api_continuation.h"
#include "ctr/game_global_installed_heap.h"
#include <atomic>
namespace ctr {
// Native context ABI is explicit. Only proven malloc/free are callable.
// Retained leases own backend, table and source; unsupported API entries stay missing.
class NativeGameHeapApi final {
public:
 using Malloc = MspaceAllocation (*)(const InstalledGameGlobalHeap&,std::uint64_t);
 using Free = std::uint32_t (*)(const InstalledGameGlobalHeap&,std::uint64_t);
 NativeGameHeapApi(std::shared_ptr<SystemPoolBackend>,GameHeapApiContinuation,
                   std::array<std::byte,192> installed_slots,bool poison);
 MspaceAllocation allocate(std::uint64_t size) const{return malloc_(binding_,size);}
 std::uint32_t release(std::uint64_t address) const{return free_(binding_,address);}
 const InstalledGameGlobalHeap& binding() const noexcept{return binding_;}
private:
 std::shared_ptr<SystemPoolBackend> backend_;
 GameHeapApiContinuation continuation_;
 InstalledGameGlobalHeap binding_;
 Malloc malloc_{allocate_installed_game_global_top};
 Free free_{free_installed_game_global_top};
};
class NativeHeapApiRegistry final {
public:
 void publish(std::shared_ptr<const NativeGameHeapApi>);
 std::shared_ptr<const NativeGameHeapApi> acquire() const;
 void unpublish() noexcept;
private:
 std::atomic<std::shared_ptr<const NativeGameHeapApi>> current_{};
};
}
