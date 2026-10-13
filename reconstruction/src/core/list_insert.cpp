#include "ctr/core/list_insert.h"
#include <bit>
#include <cstring>
#include <stdexcept>
namespace ctr {
std::uint32_t list_insert(std::span<std::byte> header, std::span<std::byte> backing,
                         std::int32_t index, std::int32_t added, std::uint32_t stride) {
 if(header.size()<40)throw std::runtime_error("Missing list header");
 std::int32_t count,capacity;std::memcpy(&count,header.data()+12,4);std::memcpy(&capacity,header.data()+16,4);
 const auto next=std::bit_cast<std::int32_t>(static_cast<std::uint32_t>(count)+static_cast<std::uint32_t>(added));
 if(next>capacity)throw std::runtime_error("Missing original 014e19b0 growth");
 if(count<0||index<0||index>count||added<0||next<count)throw std::runtime_error("Missing invalid signed insertion domain");
 const auto offset=static_cast<std::uint32_t>(index)*stride;
 const auto shift=static_cast<std::uint32_t>(added)*stride;
 const auto bytes=static_cast<std::uint32_t>(count-index)*stride;
 if(bytes && (static_cast<std::uint64_t>(offset)+shift+bytes>backing.size()))throw std::runtime_error("Missing backing extent");
 std::memcpy(header.data()+12,&next,4);
 if(bytes)std::memmove(backing.data()+offset+shift,backing.data()+offset,bytes);
 return offset;
}
}
