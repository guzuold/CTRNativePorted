#include "ctr/bootstrap_table_storage.h"
#include <cstring>
#include <stdexcept>
namespace ctr {
void initialize_bootstrap_table_storage(std::span<std::byte> storage){
 if(storage.size()!=64)throw std::invalid_argument("Bootstrap table storage64 required");
 const std::uint64_t table=0x025c4510,zero64=0,capacity=0x400;
 const std::uint32_t one=1,zero=0;
 std::memcpy(storage.data(),&table,8);
 std::memcpy(storage.data()+8,&one,4);
 std::memcpy(storage.data()+0xc,&zero,4);
 std::memcpy(storage.data()+0x10,&zero64,8);
 std::memcpy(storage.data()+0x18,&capacity,8);
 std::memcpy(storage.data()+0x20,&zero,4);
 // Original xchg+24: value only; concurrent atomic semantics outside this scope.
 std::memcpy(storage.data()+0x24,&zero,4);
 storage[0x28]=std::byte{0};
}
void assign_bootstrap_table_pool_index(std::span<std::byte> storage,std::uint32_t index){
 if(storage.size()!=64)throw std::invalid_argument("Bootstrap table storage64 required");
 std::uint32_t value;std::memcpy(&value,storage.data()+8,4);
 value=(value&0x3fffffU)|(index<<22);
 std::memcpy(storage.data()+8,&value,4);
}
}
