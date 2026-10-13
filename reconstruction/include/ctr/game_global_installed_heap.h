#pragma once
#include "ctr/game_global_installation.h"
#include "ctr/game_global_free.h"
namespace ctr {
// Native-owned dispatch storage and immutable graph; backend must outlive it.
// Only the two proven game callbacks are callable through explicit adapters.
struct InstalledGameGlobalHeap {
 SystemPoolBackend* backend{};
 GameGlobalInstallation installation;
 std::array<std::byte,192> slots{};
 bool poison{};
};
MspaceAllocation allocate_installed_game_global_top(const InstalledGameGlobalHeap&,std::uint64_t request);
std::uint32_t free_installed_game_global_top(const InstalledGameGlobalHeap&,std::uint64_t pointer);
}
