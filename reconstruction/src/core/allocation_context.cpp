#include "ctr/core/allocation_context.h"
#include <cstring>
#include <stdexcept>
namespace ctr {
std::uint64_t allocation_context(std::span<const std::byte> manager,std::int32_t tls_key){
 if(tls_key!=-1)throw std::runtime_error("Missing original TLS01bdc080, lifetime014d8710, virtual synchronization and pool014db760/014db7a0");
 if(manager.size()<56)throw std::runtime_error("Missing original manager+30 context cell");
 std::uint64_t context;std::memcpy(&context,manager.data()+48,8);return context;
}
}
