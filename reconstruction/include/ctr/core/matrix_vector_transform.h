#pragma once
#include <span>
namespace ctr { void transform_matrix_vector(std::span<const float,12> matrix,std::span<const float,3> vector,std::span<float,3> output) noexcept; }
