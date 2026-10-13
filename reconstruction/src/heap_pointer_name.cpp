#include "ctr/heap_pointer_name.h"
#include <stdexcept>
namespace ctr {
void append_heap_pointer_name(std::span<std::byte> name,std::uint32_t length,std::uint64_t token){
 if(name.size()!=32||length>15||name[length]!=std::byte{0})throw std::invalid_argument("Prepared32-byte name/prefix0..15 required");
 static constexpr char digits[]="0123456789ABCDEF";
 for(unsigned i=0;i<16;++i)name[length+i]=static_cast<std::byte>(digits[(token>>(60-i*4))&15]);
 // The original terminates fixed byte31, not prefix_length+16.
 name[31]=std::byte{0};
}
}
