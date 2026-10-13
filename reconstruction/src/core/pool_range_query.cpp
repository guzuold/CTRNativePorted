#include "ctr/core/pool_range_query.h"
namespace ctr {
bool pool_range_query(std::uint64_t base,std::uint64_t size,std::uint64_t pointer) noexcept {
 if(base>pointer)return false;
 return base+size>pointer;
}
}
