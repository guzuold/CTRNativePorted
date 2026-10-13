#pragma once
#include <cstdint>
#include <span>
namespace ctr { void transpose_matrix(std::span<std::uint32_t,16> matrix) noexcept; }
