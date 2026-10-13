#include "ctr/core/allocation_dispatch.h"
#include <stdexcept>
namespace ctr {
std::uint64_t dispatch_allocation(std::span<const std::byte> descriptor,std::uint64_t size,std::uint64_t argument) {
 if(descriptor.size()<25)throw std::runtime_error("Missing allocation descriptor byte+18");
 if(descriptor[24]==std::byte{0})return 0;
 if(size==0&&argument==0)return 0;
 throw std::runtime_error("Missing whole014dabb0 active virtual+e0, lock, provider+108/+168 and allocation services");
}
}
