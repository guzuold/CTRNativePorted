#include "ctr/core/matrix_vector_transform.h"
#include <immintrin.h>
namespace ctr {
void transform_matrix_vector(std::span<const float,12> matrix,std::span<const float,3> vector,std::span<float,3> output) noexcept {
 const auto x=_mm_set1_ps(vector[0]), y=_mm_set1_ps(vector[1]), z=_mm_set1_ps(vector[2]);
 const auto a=_mm_mul_ps(x,_mm_loadu_ps(matrix.data()));
 const auto b=_mm_mul_ps(y,_mm_loadu_ps(matrix.data()+4));
 const auto c=_mm_mul_ps(z,_mm_loadu_ps(matrix.data()+8));
 const auto result=_mm_add_ps(_mm_add_ps(a,b),c);
 float lanes[4];_mm_storeu_ps(lanes,result);
 output[0]=lanes[0];output[1]=lanes[1];output[2]=lanes[2];
}
}
