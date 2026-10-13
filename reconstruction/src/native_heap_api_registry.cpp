#include "ctr/native_heap_api_registry.h"
#include <stdexcept>
namespace ctr {
NativeGameHeapApi::NativeGameHeapApi(std::shared_ptr<SystemPoolBackend> backend,GameHeapApiContinuation continuation,std::array<std::byte,192> slots,bool poison)
 :backend_(std::move(backend)),continuation_(std::move(continuation)),binding_{backend_.get(),continuation_.installation,slots,poison}{
 if(!backend_||!continuation_.parameter)throw std::invalid_argument("Actual owned backend and API parameter required");
 const auto expected=prepare_game_heap_api_publication(continuation_.installation,continuation_.parameter,0x50000000);
 if(expected.boundary_registers!=continuation_.boundary_registers||expected.boundary_registers[5]!=0x501218d0)throw std::invalid_argument("Original zero-API publication boundary required");
 std::array<std::byte,8> pub{};auto expected_slots=slots;
 const auto installed=install_game_global_callbacks(binding_.installation.source,expected_slots,pub,0x2000ffc0,0x2000fff0);
 if(expected_slots!=slots||installed.malloc_identity!=0x01437990||installed.free_identity!=0x014379b0)throw std::invalid_argument("Only installed proven malloc/free adapters supported");
}
void NativeHeapApiRegistry::publish(std::shared_ptr<const NativeGameHeapApi> api){
 if(!api)throw std::invalid_argument("Live owned API required");
 std::shared_ptr<const NativeGameHeapApi> absent{};
 if(!current_.compare_exchange_strong(absent,std::move(api)))throw std::invalid_argument("Existing registry publication must be retired first");
}
std::shared_ptr<const NativeGameHeapApi> NativeHeapApiRegistry::acquire() const{
 auto api=current_.load();if(!api)throw std::invalid_argument("No published native heap API");return api;
}
void NativeHeapApiRegistry::unpublish() noexcept{current_.store({});}
}
