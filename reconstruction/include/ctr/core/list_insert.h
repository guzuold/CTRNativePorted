#pragma once
#include <cstddef>
#include <cstdint>
#include <span>
namespace ctr {
std::uint32_t list_insert(std::span<std::byte> header, std::span<std::byte> backing,
                         std::int32_t index, std::int32_t added, std::uint32_t stride);
}
