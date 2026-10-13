#include "ctr/gameplay/body_record.h"
#include <cstring>
#include <stdexcept>
namespace ctr {
std::size_t body_record_offset(std::span<const std::byte> mapping,std::int32_t index,std::size_t body_extent){
 if(index<0||static_cast<std::size_t>(index)>=mapping.size()/144)throw std::invalid_argument("Missing mapping row");
 std::int32_t body;std::memcpy(&body,mapping.data()+static_cast<std::size_t>(index)*144+104,4);
 if(body<0||static_cast<std::size_t>(body)>=body_extent/128)throw std::invalid_argument("Missing body record");
 return static_cast<std::size_t>(body)*128+64;
}
}
