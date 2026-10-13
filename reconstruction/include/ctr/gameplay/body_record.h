#pragma once
#include <cstddef>
#include <cstdint>
#include <span>
namespace ctr {
//007dc960: bounded caller views of144-byte mapping rows and128-byte bodies.
std::size_t body_record_offset(std::span<const std::byte> mapping,std::int32_t index,std::size_t body_extent);
}
