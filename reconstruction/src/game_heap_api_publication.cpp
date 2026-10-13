#include "ctr/game_heap_api_publication.h"
#include <cstring>
#include <stdexcept>
namespace ctr {
std::shared_ptr<const NativeGameHeapApi> publish_game_heap_api(NativeHeapApiRegistry& registry,std::shared_ptr<SystemPoolBackend> backend,const GameHeapApiContinuation& boundary,std::span<const std::byte> slots,std::uint64_t argument,bool poison){
 if(slots.size()!=192||argument!=0x501218d0||boundary.boundary_registers[5]!=argument)throw std::invalid_argument("Original installed table argument required");
 std::array<std::byte,192> owned_slots{};std::memcpy(owned_slots.data(),slots.data(),192);
 auto table=std::make_shared<const NativeGameHeapApi>(std::move(backend),boundary,owned_slots,poison);
 registry.publish(table);return table;
}
}
