#include "ctr/core/matrix_transpose.h"
#include <utility>
namespace ctr {
void transpose_matrix(std::span<std::uint32_t,16> matrix) noexcept {
 for(unsigned row=0;row<4;++row)
  for(unsigned col=row+1;col<4;++col)std::swap(matrix[row*4+col],matrix[col*4+row]);
}
}
