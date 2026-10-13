#include "ctr/core/contact_curve_collection.h"
#include "ctr/core/contact_curve_search.h"
#include <array>
#include <bit>
#include <cstring>
#include <stdexcept>
namespace ctr {
std::uint32_t contact_curve_collection(std::span<const std::byte,80> receiver,std::span<const float,3> query,float& position,float& squared_distance,std::int32_t axis,const std::uint64_t& guard){
 const auto saved_guard=guard;std::int32_t count{};std::memcpy(&count,receiver.data()+0x18,4);
 std::uint32_t result=1;
 if(count!=0){
  if(axis < -1 || axis>2)throw std::runtime_error("Missing out-of-range axis contract");
  std::array<float,3> projected_query{query[0],query[1],query[2]};if(axis!=-1)projected_query[static_cast<std::size_t>(axis)]=0.f;
  squared_distance=std::bit_cast<float>(std::uint32_t{0x7f7fffff});
  if(count>0){std::uintptr_t backing{};std::memcpy(&backing,receiver.data()+0x40,8);if(!backing)throw std::runtime_error("Missing borrowed curve collection backing");
   for(std::int32_t i=0;i<count;++i){float distance{};const auto*curve=reinterpret_cast<const std::byte*>(backing)+static_cast<std::size_t>(i)*0x88;
    const float found=contact_curve_search(std::span<const std::byte,128>(curve,128),projected_query,distance,axis,guard);
    if(squared_distance>distance){squared_distance=distance;position=found;}
   }
  }result=0;
 }
 if(saved_guard!=guard)throw std::runtime_error("Missing original guard failure1adb0d0");return result;
}
}
