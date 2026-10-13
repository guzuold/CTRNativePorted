#include "ctr/system_pool_backend.h"
#include <stdexcept>
namespace ctr {
void initialize_preconfigured_system_pool(SystemPoolBackend& b,std::uint64_t capacity,MspaceGlobalConfiguration& globals){
 if((capacity!=0x4000&&capacity!=0x8000) || b.heap.header_token || b.capacity)throw std::invalid_argument("Only fresh preconfigured16KB/32KB bootstrap verified");
 b.capacity=capacity;b.original_descriptor_identity=0x025b9c90;b.initialized_flag=0;
 const auto end=b.memory.size();
 if(b.memory.allocate(0,static_cast<std::int64_t>(end),capacity,0x200000,0,&b.physical))throw std::runtime_error("Actual system pool reservation failed");
 void* address=nullptr;
 if(b.memory.map(&address,capacity,0x33,0,b.physical,0x200000))throw std::runtime_error("Actual system pool mapping failed");
 b.heap=create_default_named_mspace(b.memory.bytes(),reinterpret_cast<std::uint64_t>(address),capacity,"System Pool",globals,{});
 if(!b.heap.header_token)throw std::runtime_error("Verified system pool create failed");
 b.original_manager_identity=0x026ee0b8;
}
}
