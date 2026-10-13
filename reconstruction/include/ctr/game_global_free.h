#pragma once
#include "ctr/game_global_malloc.h"
namespace ctr {
struct PreparedGameGlobalFree {SystemPoolBackend* backend{};std::uint64_t original_free_target{0x014379b0};bool poison{};};
// Only actual LIFO top free or null, prepared original callback installation.
std::uint32_t free_game_global_top(const PreparedGameGlobalFree& binding,std::uint64_t pointer);
}
