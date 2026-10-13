#include "ctr/bitmap_pool_dispatch.h"
#include <cstring>
#include <stdexcept>
namespace ctr {
BitmapAllocation dispatch_new_derived_bitmap_allocation(std::span<std::byte> s,SystemPoolBackend& b,std::uint64_t request,std::uint32_t alignment){
 if(s.size()!=192||!request||request>128||alignment!=8)throw std::invalid_argument("Derived default dispatcher positive1..128/align8 required");
 std::uint64_t owner,table;std::uint32_t minimum,calls;
 std::memcpy(&owner,s.data()+0x10,8);std::memcpy(&table,s.data(),8);std::memcpy(&minimum,s.data()+0x30,4);std::memcpy(&calls,s.data()+0x80,4);
 if(owner||table!=0x025baa70||minimum!=8||s[0x18]!=std::byte{1}||s[0x4b]!=std::byte{0}||s[0x4c]!=std::byte{0}||s[0x4d]!=std::byte{0})throw std::invalid_argument("Concrete active derived owner0/debug0/minimum8 required");
 const auto next=calls+1;std::memcpy(s.data()+0x80,&next,4);
 // Original slot+140 equals014db610 and dispatcher INLINES its stats/allocator.
 // Native calls the separately verified same concrete operation; no generic vtable.
 try{return allocate_derived_bitmap_pool(s,b,request,alignment);}
 catch(...){std::memcpy(s.data()+0x80,&calls,4);throw;} // Native unsupported-path rejection, not original failure semantics.
}
}
