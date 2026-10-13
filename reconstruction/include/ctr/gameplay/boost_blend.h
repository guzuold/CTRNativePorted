#pragma once
#include <array>
namespace ctr {
struct BoostBlendView { std::array<float,5> values; std::array<float,6>* nested; };
void blend_boost_record(BoostBlendView& target,const BoostBlendView* source,float factor);
}
