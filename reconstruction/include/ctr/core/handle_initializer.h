#pragma once
#include <cstddef>
#include <cstdint>
#include <span>
namespace ctr {
void initialize_handle(std::span<std::byte> destination,
 std::span<const std::byte> source, std::span<std::byte> old_record,
 std::uint64_t old_record_token);
}
