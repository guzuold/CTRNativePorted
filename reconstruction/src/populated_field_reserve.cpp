#include "ctr/populated_field_reserve.h"
#include <cstring>
#include <stdexcept>
namespace ctr {
SystemPoolAllocation reserve_populated_field_array(std::span<std::byte> header,
 std::span<const std::byte> manager,std::span<std::byte> pool,SystemPoolBackend& b,
 const SystemPoolAllocation& old,std::uint32_t fields,bool poison){
 if(header.size()!=24 || manager.size()!=112 || pool.size()!=176 || fields<1 || fields>64 ||
    manager[0x6c]!=std::byte{0} || old.payload.empty() || old.payload.size()>256 || old.payload.size()%8)
  throw std::invalid_argument("Populated pool1 array24, manager112 and bounded pointer table required");
 std::uint32_t count;std::uint64_t tag,address,selected;
 std::memcpy(&count,header.data(),4);std::memcpy(&tag,header.data()+8,8);std::memcpy(&address,header.data()+16,8);std::memcpy(&selected,manager.data()+0x10,8);
 for(std::size_t at=0x18;at<0x30;at+=8){std::uint64_t other;std::memcpy(&other,manager.data()+at,8);if(other)throw std::invalid_argument("Alternate original pool queries remain missing");}
 if(tag!=(0x0010000000000000ull|old.payload.size()) || address!=old.address || count!=old.payload.size()/8 ||
    selected!=reinterpret_cast<std::uintptr_t>(pool.data()) || fields<=count)
  throw std::invalid_argument("Full populated tagged array and actual pool1 owner required");
 // 014de6e0 nonthreaded scan skips null alternate pools; concrete014d8050
 // compares low10 bits of [old-16] with pool+1c, checked by grow module.
 auto result=dispatch_grow_reallocation_system_pool(pool,b,old,static_cast<std::uint64_t>(fields)*8,8,poison);
 const auto capacity=0x0010000000000000ull|static_cast<std::uint64_t>(fields)*8;
 std::memcpy(header.data()+8,&capacity,8);std::memcpy(header.data()+16,&result.address,8);
 return result; // Existing count and pointer bytes remain intact; fields not queried.
}
}
