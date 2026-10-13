#pragma once
#include <array>
#include <cstddef>
#include <cstdint>
namespace ctr {
struct InputLookupHandle;
struct InputLookupPayload {std::array<std::byte,12> header;const InputLookupHandle* canonical;};
struct InputLookupHandle {std::uint32_t flags;InputLookupPayload* payload;};
struct InputLookupSlot {std::array<std::byte,12> header;const InputLookupHandle* handle;};
// Whole00edecd0, single-thread borrowed views; terminal release remains missing.
InputLookupPayload* lookup_input(const std::array<InputLookupSlot*,8>& slots,std::uint32_t index);
}
