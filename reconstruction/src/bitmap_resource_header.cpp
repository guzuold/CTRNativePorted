#include "ctr/bitmap_resource_header.h"
#include <cstring>
#include <stdexcept>
namespace ctr {
std::uint64_t initialize_bitmap_resource_header(std::span<std::byte> storage){
 const auto address=reinterpret_cast<std::uint64_t>(storage.data());
 if(storage.size()<0x1988 || (address&7))throw std::invalid_argument("Aligned6536-byte bitmap resource header required");
 std::memcpy(storage.data()+0x10,&address,8);std::memcpy(storage.data()+0x18,&address,8);
 std::memset(storage.data()+0x20,0,0x68);
 for(std::size_t offset=0x88;offset<0x1988;offset+=8)std::memcpy(storage.data()+offset,&address,8);
 return address;
}
}
