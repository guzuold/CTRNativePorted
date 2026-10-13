#pragma once
#include <span>
namespace ctr {
// Whole original014cb750; exact ordered SIMD arithmetic, no determinant division.
void matrix_adjugate(std::span<const float,16> input,std::span<float,16> output);
}
