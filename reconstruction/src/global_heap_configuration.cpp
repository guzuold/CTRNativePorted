#include "ctr/global_heap_configuration.h"
#include <cstdint>
#include <cstring>
#include <stdexcept>
namespace ctr {
void configure_global_heap_header(std::span<std::byte> header,std::span<std::byte> globals){
 const auto h=reinterpret_cast<std::uintptr_t>(header.data()),g=reinterpret_cast<std::uintptr_t>(globals.data());
 if(header.size()<0x3b0||h%8||globals.size()!=0x61||(h<=g?g-h<header.size():h-g<globals.size()))throw std::invalid_argument("Header3b0 and separate global block61 required");
 auto byte=[&](std::size_t offset){return std::to_integer<unsigned>(globals[offset]);};
 if(!byte(0x48)){for(auto offset:{0x40,0x58,0x48,0x50,0x60})globals[offset]=std::byte{1};}
 const std::uint64_t signature=byte(0x40)==1?0x58585858ull:0;
 const std::uint32_t flags=(byte(0x60)&1)?7u:0u;
 const std::uint64_t page=0x4000,threshold=(byte(0x50)&1)?0x10000ull:0;
 std::memcpy(header.data()+0x38,&signature,8);std::memcpy(header.data()+0x370,&flags,4);
 std::memcpy(header.data()+0x3a0,&page,8);std::memcpy(header.data()+0x3a8,&threshold,8);
 std::memset(globals.data()+8,0,0x28);const std::uint64_t granularity=0x40000;const std::uint32_t count=1;
 std::memcpy(globals.data(),&granularity,8);std::memcpy(globals.data()+8,&count,4);
}
}
