#include "ctr/descriptor_pool_selection.h"
#include <cstring>
#include <stdexcept>
namespace ctr {
std::uint64_t select_descriptor_manager_pool(std::span<const std::byte> descriptor,
    std::span<const std::byte> manager, std::int32_t selector) {
    if(descriptor.size()<176 || manager.size()<112) throw std::invalid_argument("Descriptor176 and manager112 required");
    if(descriptor[0]==std::byte{0}) throw std::invalid_argument("Cold descriptor GLOBALmalloc dependency missing");
    if(manager[0x6c]!=std::byte{0}) throw std::invalid_argument("Thread context pool lookup dependency missing");
    const std::size_t offset=selector==1?0x10:selector==2?0x20:selector==4?0x28:0x18;
    std::uint64_t pool;std::memcpy(&pool,manager.data()+offset,sizeof(pool));
    return pool;
}
}
