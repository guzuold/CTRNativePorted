#include "ctr/mspace_usable_size.h"
#include <cstring>
#include <stdexcept>
namespace ctr {
std::uint64_t mspace_usable_size(std::span<const std::byte> backing,
 std::optional<std::size_t> payload_offset) {
 if(!payload_offset) return 0;
 const auto offset=*payload_offset;
 if(offset<8 || offset>backing.size()) throw std::invalid_argument("Usable-size marker outside backing");
 std::uint64_t marker;std::memcpy(&marker,backing.data()+offset-8,8);
 if(!(marker&2)) return 0;
 const auto size=marker&0xff00fffffffffff8ull;
 std::uint64_t correction=0;
 if(marker&4) {
  if(size<17 || size-17>=backing.size()-offset)
   throw std::invalid_argument("Usable-size padding byte outside backing");
  correction=std::to_integer<std::uint64_t>(backing[offset+static_cast<std::size_t>(size-17)])|0xffffffffffffff00ull;
 }
 return size+correction-16; // Unsigned arithmetic preserves the original 64-bit ABI.
}
}
