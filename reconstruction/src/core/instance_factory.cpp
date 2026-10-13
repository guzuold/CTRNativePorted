#include "ctr/core/instance_factory.h"
#include "ctr/core/allocation_dispatch.h"
#include <cstring>
#include <stdexcept>
namespace ctr {
std::uint64_t create_instance(std::span<const std::byte> metadata,std::span<const std::byte> allocator,std::uint32_t& aligned_failures,std::uint32_t& adjusted_failures){
 if(metadata.size()<96)throw std::runtime_error("Missing factory metadata layout");
 std::uint16_t flags,size;std::memcpy(&flags,metadata.data()+92,2);
 if(flags&32)throw std::runtime_error("Missing factory metadata substitution registry/callback");
 if(flags&16)return 0;
 if(allocator.empty())throw std::runtime_error("Missing whole014d8550 real allocation context");
 if(allocator.size()<52)throw std::runtime_error("Missing allocator layout");
 std::memcpy(&size,metadata.data()+90,2);
 const auto result=dispatch_allocation(allocator,0,size);
 if(result==0){std::uint16_t required;std::uint32_t alignment;std::memcpy(&required,metadata.data()+94,2);std::memcpy(&alignment,allocator.data()+48,4);if(alignment>=required)++aligned_failures;else ++adjusted_failures;return 0;}
 throw std::runtime_error("Missing factory successful allocation/memset/wholeconstruction/services");
}
}

