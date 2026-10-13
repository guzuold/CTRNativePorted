#pragma once
#include <cstdint>
namespace ctr {
struct GameplayPayload;
struct GameplayHandle {std::uint32_t flags; const GameplayPayload* payload;};
struct GameplayPayload {const GameplayHandle* canonical;};
// Whole00cc8cd0: owner+140 borrowed handle; mutating resolution missing.
const GameplayPayload* gameplay_handle_payload(const GameplayHandle* handle);
}
