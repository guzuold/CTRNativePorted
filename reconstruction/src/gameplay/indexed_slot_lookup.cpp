// SPDX-License-Identifier: GPL-2.0-only
#include "ctr/gameplay/indexed_slot_lookup.h"
#include <stdexcept>
namespace ctr {
std::uint64_t indexed_slot_lookup(std::uint32_t token,std::uint32_t sentinel,std::int32_t count,
 std::span<const std::uint16_t> tags,std::span<const std::uint64_t> values){
 if(token==sentinel||!(token&0x8000u))return 0;
 const auto index=token&0x7fffu;
 if(count<=static_cast<std::int32_t>(index))return 0;
 if(index>=tags.size())throw std::invalid_argument("Missing slot tag storage");
 if(tags[index]!=(index|0x8000u))return 0;
 if(index>=values.size())throw std::invalid_argument("Missing slot value storage");
 return values[index];
}
}
