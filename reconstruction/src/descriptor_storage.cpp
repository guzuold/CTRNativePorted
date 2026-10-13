#include "ctr/descriptor_storage.h"
#include <cstring>
#include <stdexcept>
namespace ctr {
void initialize_allocated_dispatch_descriptor(std::span<std::byte> s,std::uint64_t& publication){
 if(s.size()!=176)throw std::invalid_argument("Actual descriptor176 storage required");
 const auto base=reinterpret_cast<std::uintptr_t>(s.data()),out=reinterpret_cast<std::uintptr_t>(&publication);
 if(out<base+176&&out+sizeof(publication)>base)throw std::invalid_argument("Descriptor publication must not overlap storage");
 // Preserve the original sparse store order. Unspecified bytes retain their values.
 std::memset(s.data()+0x14,0,2);s[0]=std::byte{0};std::memset(s.data()+0x18,0,4);
 std::memset(s.data()+0x64,0,8);std::memset(s.data()+0xa0,0,2);std::memset(s.data()+0xa8,0,4);
 std::memset(s.data()+0x90,0,8);std::memset(s.data()+0x30,0,16);std::memset(s.data()+0x48,0,16);
 std::memset(s.data()+0x58,0,8);std::memset(s.data()+0x70,0,16);std::memset(s.data()+0x80,0,8);
 publication=static_cast<std::uint64_t>(base);
}
}
