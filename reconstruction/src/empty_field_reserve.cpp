#include "ctr/empty_field_reserve.h"
#include <cstring>
#include <stdexcept>
namespace ctr {
SystemPoolAllocation reserve_empty_field_array(std::span<std::byte> header,
 std::span<const std::byte> manager,std::span<std::byte> pool,
 SystemPoolBackend& backend,std::uint32_t fields,bool poison){
 if(header.size()!=24||manager.size()!=112||pool.size()!=176||manager[0x6c]!=std::byte{0}||fields<1||fields>64)
  throw std::invalid_argument("Empty array24/nonthreaded pool1/1..64 slots required");
 std::uint32_t count;std::uint64_t tag,old,selected;
 std::memcpy(&count,header.data(),4);std::memcpy(&tag,header.data()+8,8);std::memcpy(&old,header.data()+16,8);std::memcpy(&selected,manager.data()+0x10,8);
 if(count||old||tag!=0x0010000000000002ull||selected!=reinterpret_cast<std::uintptr_t>(pool.data()))
  throw std::invalid_argument("Initial pool1 token2/header backing required; reallocation missing");
 const auto request=static_cast<std::uint64_t>(fields)*8;
 auto allocation=dispatch_null_reallocation_system_pool(pool,backend,request,8,poison);
 const auto capacity=0x0010000000000000ull|request;
 std::memcpy(header.data()+8,&capacity,8);std::memcpy(header.data()+16,&allocation.address,8);
 return allocation;
}
}
