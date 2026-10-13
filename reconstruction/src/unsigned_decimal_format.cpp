#include "ctr/unsigned_decimal_format.h"
#include <algorithm>
#include <array>
#include <charconv>
#include <cstring>
namespace ctr {
std::uint32_t format_unsigned_long_decimal(std::span<char> destination,std::uint64_t value){
 std::array<char,20> digits{};auto result=std::to_chars(digits.data(),digits.data()+digits.size(),value);
 const auto length=static_cast<std::size_t>(result.ptr-digits.data());
 if(!destination.empty()){
  const auto copied=std::min(length,destination.size()-1);
  std::memcpy(destination.data(),digits.data(),copied);destination[copied]='\0';
 }
 return static_cast<std::uint32_t>(length);
}
}
