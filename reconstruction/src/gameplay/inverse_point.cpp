// SPDX-License-Identifier: GPL-2.0-only
#include "ctr/gameplay/inverse_point.h"
#include <cmath>
namespace ctr {
Vec3 inverse_point(const EntityLocalView& entity,const LocalScaleTransform* scale,
 Vec3 default_scale,Vec3 point,const std::array<float,7>* initialized_globals){
 std::array<float,16> matrix{};
 entity_matrix(matrix,entity,scale,default_scale,0,initialized_globals);
 std::array<float,4> result{};
 // Original bd42b7..bd42cf: (x*row0+y*row1)+(z*row2+row3).
 for(unsigned lane=0;lane<4;++lane){
  const float x=point.x*matrix[lane];
  const float y=point.y*matrix[4+lane];
  const float z=point.z*matrix[8+lane];
  const float xy=x+y;
  const float zt=z+matrix[12+lane];
  result[lane]=xy+zt;
 }
 // Original unordered comparison also enters this path; nonfinite domain
 // remains unverified, with no successful missing-interface substitution.
 if(result[3]!=1.0f){
  constexpr float epsilon=4.999999987376214e-7f;
  const float divisor=std::fabs(result[3])<epsilon?epsilon:result[3];
  const float reciprocal=1.0f/divisor;
  for(unsigned lane=0;lane<3;++lane)result[lane]=result[lane]*reciprocal;
 }
 return {result[0],result[1],result[2]};
}
}
