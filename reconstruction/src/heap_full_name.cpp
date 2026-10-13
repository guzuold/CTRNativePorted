#include "ctr/heap_full_name.h"
#include "ctr/heap_pointer_name.h"
#include <algorithm>
#include <stdexcept>
namespace ctr {
std::uint64_t make_heap_pointer_name(std::span<std::byte> name,std::string_view prefix,std::uint64_t token){
 if(name.size()!=32||prefix.size()>15||prefix.find('\0')!=std::string_view::npos)throw std::invalid_argument("Name32 and prefix0..15 required");
 // Original strncpy writes 31 bytes, including zero padding, before strlen.
 for(std::size_t i=0;i<31;++i)name[i]=i<prefix.size()?static_cast<std::byte>(prefix[i]):std::byte{0};
 append_heap_pointer_name(name,static_cast<std::uint32_t>(prefix.size()),token);
 return prefix.size();
}
}
