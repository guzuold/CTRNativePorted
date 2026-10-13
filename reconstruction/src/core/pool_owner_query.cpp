#include "ctr/core/pool_owner_query.h"
#include "ctr/core/pool_range_query.h"
#include <stdexcept>
namespace ctr {
bool pool_owner_query(std::uint64_t original_vtable,std::uint64_t base,std::uint64_t size,std::uint64_t pointer){
 switch(original_vtable){
 case 0x25b9e68:case 0x25ba8f0:case 0x25baa70:case 0x25bae40:case 0x25bafc0:
  return pool_range_query(base,size,pointer);
 default:throw std::runtime_error("Missing original virtual+118 target identity");
 }
}
}
