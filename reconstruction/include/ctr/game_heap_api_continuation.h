#pragma once
#include "ctr/game_global_installation.h"
namespace ctr {
class GameZeroHeapApiParameter final {
public:
 explicit GameZeroHeapApiParameter(std::span<const std::byte> original_bytes);
 std::span<const std::byte> bytes() const noexcept{return data_;}
private:
 std::array<std::byte,56> data_{};
};
struct GameHeapApiContinuation {
 GameGlobalInstallation installation;
 std::shared_ptr<const GameZeroHeapApiParameter> parameter;
 std::array<std::uint64_t,16> boundary_registers{};
};
// Original58c65 -> zero callback branch58d8a ->58cc8, stop before4c8.
// Module-base translation is a declared comparison adapter, not host callable IDs.
GameHeapApiContinuation prepare_game_heap_api_publication(
 const GameGlobalInstallation&,std::shared_ptr<const GameZeroHeapApiParameter>,
 std::uint64_t module_base);
}
