#include "ctr/core/allocation_owner_query.h"
#include <cstring>
#include "ctr/core/pool_owner_query.h"
#include <stdexcept>
namespace ctr {
std::uint64_t allocation_owner_query(std::span<const std::byte> manager,
 std::uint64_t allocation,std::uint32_t mode,std::span<const AllocationPoolView> pools){
 // 13de6fa precedes every manager access and the query-mode branches.
 if(!allocation)return 0;
 if(manager.size()<112)throw std::runtime_error("Missing manager112 layout");
 if(manager[0x6c]!=std::byte{0})
  throw std::runtime_error("Missing original collections+40/+48 and synchronization virtual+f8/+e8");
 // Both low-byte mode branches scan pools+18,+20,+28,+10 in that order.
 for(std::size_t offset:{0x18u,0x20u,0x28u,0x10u}){
  std::uint64_t pool;std::memcpy(&pool,manager.data()+offset,8);
  if(!pool)continue;
  const AllocationPoolView* view=nullptr;
  for(const auto& item:pools)if(item.address==pool){view=&item;break;}
  if(!view||view->bytes.size()<48)throw std::runtime_error("Missing concrete pool input");
  if(view->bytes[0x18]==std::byte{0})continue;
  std::uint64_t vtable,base,size;
  std::memcpy(&vtable,view->bytes.data(),8);
  std::memcpy(&base,view->bytes.data()+0x20,8);
  std::memcpy(&size,view->bytes.data()+0x28,8);
  // The five pinned identities have +118=13dea30 and +120=13e2620.
  static_cast<void>(mode);
  if(pool_owner_query(vtable,base,size,allocation))return pool;
 }
 return 0;
}
}

