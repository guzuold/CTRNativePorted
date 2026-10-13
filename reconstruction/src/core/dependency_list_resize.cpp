#include "ctr/core/dependency_list_resize.h"
#include <cstring>
#include <stdexcept>
namespace ctr {
void dependency_list_resize(std::span<std::byte> header,
                            std::span<std::byte> backing, std::int32_t requested) {
 if(header.size()<40) throw std::runtime_error("Missing list header");
 std::int32_t old_count, capacity;
 std::memcpy(&old_count,header.data()+12,4);
 std::memcpy(&capacity,header.data()+16,4);
 if(capacity<requested) throw std::runtime_error("Missing original 014e19b0 growth");
 if(old_count<0 || requested<0) throw std::runtime_error("Missing signed negative backing domain");
 const auto extent=static_cast<std::size_t>(old_count>requested?old_count:requested)*16;
 if(old_count!=requested && backing.size()<extent) throw std::runtime_error("Missing list backing extent");
 std::memcpy(header.data()+12,&requested,4);
 if(old_count<requested) {
  for(auto i=old_count;i<requested;++i) std::memset(backing.data()+static_cast<std::size_t>(i)*16,0,12);
 } else {
  for(auto i=requested;i<old_count;++i) {
   auto* record=backing.data()+static_cast<std::size_t>(i)*16;
   std::uint64_t name;std::memcpy(&name,record,8);
   if(name) throw std::runtime_error("Missing original 0154df00 name release");
   std::memset(record,0,8);
  }
 }
}
}
