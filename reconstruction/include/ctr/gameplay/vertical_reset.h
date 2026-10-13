#pragma once
#include <array>
#include <cstddef>
#include <cstdint>
namespace ctr {
struct VerticalResetHandle;
// Adapted borrowed views; fields correspond to original offsets, not ABI layout.
struct VerticalResetPayload {
    std::array<std::byte,12> header; // count at original +8
    VerticalResetHandle* canonical; // original +10
    std::uint32_t value;            // original +e0
    std::uint32_t mode;             // original +e8
};
struct VerticalResetHandle {
    std::uint32_t flags; // original +0
    VerticalResetPayload* payload; // original +28
};
// Whole00ef9b40, stable borrowed handles, callback-free object release only.
void reset_vertical_payload(VerticalResetHandle*& slot);
}
