#include "ctr/system_pool_backend.h"
#include <stdexcept>
namespace ctr {
std::uint32_t activate_owned_system_pool(std::span<std::byte> pool,SystemPoolBackend& backend,
 const SystemPoolBootstrapContext& lifecycle){
 if(!backend.heap.header_token || (backend.capacity!=0x4000&&backend.capacity!=0x8000) || backend.memory.bytes().size()!=backend.capacity)
  throw std::invalid_argument("Actual initialized owned backend required");
 auto context=lifecycle;context.backing_size=backend.capacity;
 context.backing_token=static_cast<std::uint64_t>(backend.physical);
 context.allocator_token=backend.heap.header_token;
 return activate_bootstrap_system_pool(pool,context);
}
}
