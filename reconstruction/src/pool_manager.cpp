#include "ctr/pool_manager.h"
#include <cstring>
#include <cstdint>
#include <stdexcept>
namespace ctr {
void reset_unowned_pool_manager(std::span<std::byte> manager) {
    if (manager.size()<0x70) throw std::out_of_range("Pool manager requires 112 bytes");
    for (const auto offset:{0x30,0x38,0x40,0x48}) {
        std::uint64_t owned;std::memcpy(&owned,manager.data()+offset,8);
        if (owned) throw std::invalid_argument("Pool manager ownership release missing");
    }
    manager[0x6c]=std::byte{0};
    std::memset(manager.data()+0x10,0,0x40);
    const std::uint32_t capacity=0x400;
    std::memcpy(manager.data()+0x50,&capacity,4);
    std::memset(manager.data()+0x60,0,12);
}
void initialize_unowned_pool_storage(std::span<std::byte> pool) {
    if (pool.size()<0xa1) throw std::out_of_range("Pool defaults require 161 bytes");
    std::uint64_t owner;std::memcpy(&owner,pool.data()+0x10,8);
    if (owner) throw std::invalid_argument("Pool owner release missing");
    pool[0x18]=std::byte{0};
    const std::uint32_t invalid_index=0xffffffffu, flags=0x101;
    std::memcpy(pool.data()+0x1c,&invalid_index,4);
    std::memset(pool.data()+0x20,0,16);
    std::memset(pool.data()+0x30,0,4);
    std::memset(pool.data()+0x10,0,8);
    std::memset(pool.data()+0x40,0,8);
    std::memcpy(pool.data()+0x49,&flags,4);
    pool[0x4d]=std::byte{0};
    std::memset(pool.data()+0x90,0,8);
    pool[0xa0]=std::byte{0};
    std::memset(pool.data()+0x50,0,0x34);
}
}
