#include "ctr/pool_activation.h"
#include <algorithm>
#include <cstring>
#include <stdexcept>
namespace ctr {
std::uint32_t activate_bootstrap_system_pool(std::span<std::byte> storage,
    const SystemPoolBootstrapContext& context) {
    if (storage.size()<0xb0) throw std::out_of_range("System pool requires 176 bytes");
    if (storage[0x18]!=std::byte{0}) return 1;
    std::uint64_t owner;std::memcpy(&owner,storage.data()+0x10,8);
    if ((!owner&&context.descriptor_initialized)||context.threaded_manager||context.tls_key!=-1)
        throw std::invalid_argument("System pool owner, registry or TLS lifecycle missing");
    std::uint32_t alignment;std::memcpy(&alignment,storage.data()+0x30,4);
    alignment=std::max(alignment,8u);std::memcpy(storage.data()+0x30,&alignment,4);
    std::memset(storage.data()+0x50,0,0x30);
    if (!owner&&storage[0x39]!=std::byte{0}) storage[0x38]=std::byte{1};
    std::memset(storage.data()+0x40,0,8);
    std::memcpy(storage.data()+0x28,&context.backing_size,8);
    std::memcpy(storage.data()+0x20,&context.backing_token,8);
    std::memcpy(storage.data()+0xa8,&context.allocator_token,8);
    storage[0x18]=std::byte{1};return 0;
}
}
